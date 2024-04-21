/* Implementing coroutines with setjmp/longjmp */
#include <ctype.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "agents/mcts.h"
#include "agents/negamax.h"
#include "game.h"
#include "list.h"


struct task {
    jmp_buf env;
    struct list_head list;
    char task_name[10];
    char turn;
    char *table;
};

struct arg {
    char turn;
    char *table;
    char *task_name;
};

static LIST_HEAD(tasklist);
static void (**tasks)(void *);
static struct arg *args;
static int ntasks;
static jmp_buf sched;
static struct task *cur_task;

static int move_record[N_GRIDS];
static int move_count = 0;

static void task_add(struct task *task)
{
    list_add_tail(&task->list, &tasklist);
}

static void task_switch()
{
    if (!list_empty(&tasklist)) {
        // cppcheck-suppress nullPointer
        struct task *t = list_first_entry(&tasklist, struct task, list);
        if (t == NULL) {
            return;
        }
        list_del(&t->list);
        cur_task = t;
        longjmp(t->env, 1);
    }
}


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
void schedule(void)
{
    static int i;

    setjmp(sched);

    while (ntasks-- > 0) {
        struct arg arg = args[i];
        tasks[i++](&arg);
        // printf("Never reached\n");
    }

    task_switch();
}

/*negamax*/
void task0(void *arg)
{
    struct task *task = malloc(sizeof(struct task));
    strncpy(task->task_name, ((struct arg *) arg)->task_name,
            sizeof(task->task_name) - 1);
    task->task_name[sizeof(task->task_name) - 1] = '\0';
    task->table = ((struct arg *) arg)->table;
    task->turn = ((struct arg *) arg)->turn;
    INIT_LIST_HEAD(&task->list);
    // he don't have the above code

    if (setjmp(task->env) == 0) {
        task_add(task);
        longjmp(sched, 1);
    }
    while (1) {
        task = cur_task;
        if (setjmp(task->env) == 0) {
            char win = check_win(task->table);
            if (win == 'D') {
                draw_board(task->table);
                printf("It is a draw!\n");
                break;
            }

            draw_board(task->table);
            int move = negamax_predict(task->table, task->turn).move;
            if (move != -1) {
                task->table[move] = task->turn;
                record_move(move);
            }

            task_add(task);
            task_switch();
        }
    }

    printf("%s: complete\n", task->task_name);
    longjmp(sched, 1);
}
/*mcts*/
void task1(void *arg)
{
    struct task *task = malloc(sizeof(struct task));
    strncpy(task->task_name, ((struct arg *) arg)->task_name,
            sizeof(task->task_name) - 1);
    task->task_name[sizeof(task->task_name) - 1] = '\0';
    task->table = ((struct arg *) arg)->table;
    task->turn = ((struct arg *) arg)->turn;
    INIT_LIST_HEAD(&task->list);
    // he don't have the above code

    if (setjmp(task->env) == 0) {
        task_add(task);
        longjmp(sched, 1);
    }
    while (1) {
        task = cur_task;
        if (setjmp(task->env) == 0) {
            char win = check_win(task->table);
            if (win == 'D') {
                draw_board(task->table);
                printf("It is a draw!\n");
                break;
            }

            draw_board(task->table);
            int move = mcts(task->table, task->turn);
            if (move != -1) {
                task->table[move] = task->turn;
                record_move(move);
            }

            task_add(task);
            task_switch();
        }
    }

    printf("%s: complete\n", task->task_name);
    longjmp(sched, 1);
}

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
void ttt(int mode)
{
    srand(time(NULL));
    char table[N_GRIDS];
    memset(table, ' ', N_GRIDS);

    void (*registered_task[])(void *) = {task0, task1};

    struct arg arg0 = {.turn = 'X', .table = table, .task_name = "Task 0"};
    struct arg arg1 = {.turn = 'O', .table = table, .task_name = "Task 1"};
    struct arg registered_arg[] = {arg0, arg1};
    tasks = registered_task;
    args = registered_arg;
    ntasks = ARRAY_SIZE(registered_task);

    char turn = 'X';
    char ai = 'O';

    negamax_init();
    while (1) {
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
                    printf("Invalid operation: the position has been marked\n");
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
        }
        // schedule();

        turn = turn == 'X' ? 'O' : 'X';
    }
    print_moves();
}

// singinpore -> yogat (Scoot)