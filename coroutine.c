#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>
#include "agents/mcts.h"
#include "agents/negamax.h"
#include "game.h"
#include "list.h"


#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef void(task_callback_t)(void *arg);

struct task_struct {
    struct list_head list;
    ucontext_t context;
    void *stack;
    task_callback_t *callback;
    void *arg;
    bool reap_self;
};

static struct task_struct *task_current, task_main;
static LIST_HEAD(task_reap);

union task_ptr {
    void *p;
    int i[2];
};

struct arg {
    char turn;
    char *table;
    char *task_name;
};


static int move_record[N_GRIDS];
static int move_count = 0;

bool isPause = false;
bool isVisible = true;

static void record_move(int move)
{
    move_record[move_count++] = move;
}

static void print_moves()
{
    printf("Moves: ");
    for (int i = 0; i < move_count; i++) {
        printf("%c%d", 'A' + GET_COL(move_record[i]),
               1 + GET_ROW(move_record[i]));
        if (i < move_count - 1) {
            printf(" -> ");
        }
    }
    printf("\n");
}

static int get_input(char player)
{
    char *line = NULL;
    size_t line_length = 0;
    int parseX = 1;

    int x = -1, y = -1;
    while (x < 0 || x > (BOARD_SIZE - 1) || y < 0 || y > (BOARD_SIZE - 1)) {
        printf("%c> ", player);
        int r = getline(&line, &line_length, stdin);
        if (r == -1)
            exit(1);
        if (r < 2)
            continue;
        x = 0;
        y = 0;
        parseX = 1;
        for (int i = 0; i < (r - 1); i++) {
            if (isalpha(line[i]) && parseX) {
                x = x * 26 + (tolower(line[i]) - 'a' + 1);
                if (x > BOARD_SIZE) {
                    // could be any value in [BOARD_SIZE + 1, INT_MAX]
                    x = BOARD_SIZE + 1;
                    printf("Invalid operation: index exceeds board size\n");
                    break;
                }
                continue;
            }
            // input does not have leading alphabets
            if (x == 0) {
                printf("Invalid operation: No leading alphabet\n");
                y = 0;
                break;
            }
            parseX = 0;
            if (isdigit(line[i])) {
                y = y * 10 + line[i] - '0';
                if (y > BOARD_SIZE) {
                    // could be any value in [BOARD_SIZE + 1, INT_MAX]
                    y = BOARD_SIZE + 1;
                    printf("Invalid operation: index exceeds board size\n");
                    break;
                }
                continue;
            }
            // any other character is invalid
            // any non-digit char during digit parsing is invalid
            // TODO: Error message could be better by separating these two cases
            printf("Invalid operation\n");
            x = y = 0;
            break;
        }
        x -= 1;
        y -= 1;
    }
    free(line);
    return GET_INDEX(y, x);
}

static int preempt_count = 0;

static void preempt_disable(void)
{
    preempt_count++;
}
static void preempt_enable(void)
{
    preempt_count--;
}
void task0(void *arg)
{
    char *table = ((struct arg *) arg)->table;
    char turn = ((struct arg *) arg)->turn;
    preempt_disable();
    int move = negamax_predict(table, turn).move;

    if (move != -1) {
        table[move] = turn;
        record_move(move);
    }
    if (isVisible)
        draw_board(table);
    preempt_enable();
}

void task1(void *arg)
{
    char *table = ((struct arg *) arg)->table;
    char turn = ((struct arg *) arg)->turn;
    preempt_disable();
    int move = mcts(table, turn);

    if (move != -1) {
        table[move] = turn;
        record_move(move);
    }
    if (isVisible)
        draw_board(table);
    preempt_enable();
}

static void my_timer_create(unsigned int usecs)
{
    ualarm(usecs, usecs);
}
static void timer_cancel(void)
{
    ualarm(0, 0);
}

static void timer_wait(void)
{
    sigset_t mask;
    sigprocmask(0, NULL, &mask);
    sigdelset(&mask, SIGALRM);
    sigsuspend(&mask);
}
static void local_irq_save(sigset_t *sig_set)
{
    sigset_t block_set;
    /*sigset_t
       Include: <signal.h>.  Alternatively, <spawn.h>, or <sys/select.h>.

       This is a type that represents a set of signals.  According to POSIX,
       this shall be an in‐ teger or structure type.*/
    sigfillset(&block_set);  // initializes set to full, including all signals.
    sigdelset(
        &block_set,
        SIGINT);  // delete respectively signal signum from set. // SIGINT?
    sigprocmask(SIG_BLOCK, &block_set, sig_set);
}
static void local_irq_restore(sigset_t *sig_set)
{
    sigprocmask(SIG_SETMASK, sig_set, NULL);
}
static void local_irq_restore_trampoline(struct task_struct *task)
{
    sigdelset(&task->context.uc_sigmask, SIGALRM);
    local_irq_restore(&task->context.uc_sigmask);
}
static void task_destroy(struct task_struct *task)
{
    list_del(&task->list);
    free(task->stack);
    free(task);
}

static void task_switch_to(struct task_struct *from, struct task_struct *to)
{
    task_current = to;
    swapcontext(&from->context, &to->context);
}

static void schedule(void)
{
    sigset_t set;
    local_irq_save(&set);

    struct task_struct *next_task =
        // cppcheck-suppress nullPointer
        list_first_entry(&task_current->list, struct task_struct, list);
    if (next_task) {
        if (task_current->reap_self)
            list_move(&task_current->list, &task_reap);
        task_switch_to(task_current, next_task);
    }

    struct task_struct *task, *tmp;
    list_for_each_entry_safe (task, tmp, &task_reap, list) /* clean reaps */
        task_destroy(task);

    local_irq_restore(&set);
}
__attribute__((noreturn)) static void task_trampoline(int i0, int i1)
{
    union task_ptr ptr = {.i = {i0, i1}};
    struct task_struct *task = ptr.p;

    /* We switch to trampoline with blocked timer.  That is safe.
     * So the first thing that we have to do is to unblock timer signal.
     * Paired with task_add().
     */
    local_irq_restore_trampoline(task);
    task->callback(task->arg);
    task->reap_self = true;
    schedule();

    __builtin_unreachable(); /* shall not reach here */
}


static void timer_handler(int signo, siginfo_t *info, ucontext_t *ctx)
{
    if (preempt_count) /* once preemption is disabled */
        return;
    /* We can schedule directly from sighandler because Linux kernel cares only
     * about proper sigreturn frame in the stack.
     */
    schedule();
}

void enableRawMode()
{
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);  // Disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &term);
}

void disableRawMode()
{
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag |= (ICANON | ECHO);  // Enable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &term);
}

void process_key()
{
    enableRawMode();
    preempt_disable();
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) {
        preempt_enable();
        return;
    }
    if (c == '\x71') {  // TODO: Ctrl + Q + pause the game
        isVisible ^= true;
    } else if (c == '\x70') {  // TODO:  Ctrl + P
        isPause ^= true;
    }
    preempt_enable();
    disableRawMode();
}

static void task_init(void)
{
    INIT_LIST_HEAD(&task_main.list);
    task_current = &task_main;
}

static struct task_struct *task_alloc(task_callback_t *func, void *arg)
{
    struct task_struct *task = calloc(1, sizeof(*task));
    task->stack = calloc(1, 1 << 20);
    task->callback = func;
    task->arg = arg;
    return task;
}

static void task_add(task_callback_t *func, void *param)
{
    struct task_struct *task = task_alloc(func, param);
    if (getcontext(&task->context) == -1)
        abort();

    task->context.uc_stack.ss_sp = task->stack;
    task->context.uc_stack.ss_size = 1 << 20;
    task->context.uc_stack.ss_flags = 0;
    task->context.uc_link = NULL;

    union task_ptr ptr = {.p = task};
    makecontext(&task->context, (void (*)(void)) task_trampoline, 2, ptr.i[0],
                ptr.i[1]);

    /* When we switch to it for the first time, timer signal must be blocked.
     * Paired with task_trampoline().
     */
    sigaddset(&task->context.uc_sigmask, SIGALRM);

    preempt_disable();
    list_add_tail(&task->list, &task_main.list);
    preempt_enable();
}

static void timer_init(void)
{
    struct sigaction sa = {
        .sa_handler = (void (*)(int)) timer_handler,
        .sa_flags = SA_SIGINFO,
    };
    sigfillset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);
}



void ttt(int mode)
{
    srand(time(NULL));
    char table[N_GRIDS];

    memset(table, ' ', N_GRIDS);

    struct arg arg0 = {.turn = 'X', .table = table, .task_name = "Task 0"};
    struct arg arg1 = {.turn = 'O', .table = table, .task_name = "Task 1"};

    char turn = 'X';
    char ai = 'O';
    int game_round = mode == 2 ? 10 : 1;
    negamax_init();
    for (int i = 0; i < game_round; i++) {
        while (!isPause) {
            char win = check_win(table);
            if (win == 'D') {
                draw_board(table);
                printf("It is a draw!\n");
                break;
            } else if (win != ' ') {
                draw_board(table);
                printf("%c won!\n", win);
                break;
            }
            // human vs. ai
            if (mode == 0) {
                if (turn == ai) {
                    int move = negamax_predict(table, ai).move;
                    if (move != -1) {
                        table[move] = ai;
                        record_move(move);
                    }
                } else {
                    draw_board(table);
                    int move;
                    while (1) {
                        move = get_input(turn);
                        if (table[move] == ' ') {
                            break;
                        }
                        printf(
                            "Invalid operation: the position has been "
                            "marked\n");
                    }
                    table[move] = turn;
                    record_move(move);
                }
            }
            // ai vs. ai
            else if (mode == 1) {
                if (turn == 'O') {
                    int move = mcts(table, turn);
                    if (move != -1) {
                        table[move] = turn;
                        record_move(move);
                    }
                    draw_board(table);
                } else {
                    int move = negamax_predict(table, turn).move;
                    if (move != -1) {
                        table[move] = turn;
                        record_move(move);
                    }
                    draw_board(table);
                }
            } else if (mode == 2) {
                // schedule();
                timer_init();
                task_init();
                task_add(task0, &arg0);
                task_add(process_key, NULL);
                task_add(task1, &arg1);
                task_add(process_key, NULL);

                preempt_disable();
                my_timer_create(100000);
                while (!list_empty(&task_main.list) ||
                       !list_empty(&task_reap)) {
                    preempt_enable();
                    timer_wait();
                    preempt_disable();
                }
                preempt_enable();
                timer_cancel();
            }

            turn = turn == 'X' ? 'O' : 'X';
        }

        print_moves();
        memset(table, ' ', N_GRIDS);
        memset(move_record, 0, sizeof(move_record));
        move_count = 0;
        isPause = false;
    }
}
