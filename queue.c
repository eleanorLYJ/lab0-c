#include "queue.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Notice: sometimes, Cppcheck would find the potential NULL pointer bugs,
 * but some of them cannot occur. You can suppress them by adding the
 * following line.
 *   cppcheck-suppress nullPointer
 */


/* Create an empty queue */
struct list_head *q_new()
{
    struct list_head *new = malloc(sizeof(struct list_head));
    if (!new)
        return NULL;
    INIT_LIST_HEAD(new);
    return new;
}

/* Free all storage used by queue */
void q_free(struct list_head *l)
{
    if (!l)
        return;
    struct list_head *node, *next;
    list_for_each_safe (node, next, l) {
        element_t *ele = container_of(node, element_t, list);
        if (ele)
            q_release_element(ele);
    }
    free(l);
}

/* Insert an element at head of queue */
bool q_insert_head(struct list_head *head, char *s)
{
    if (!head)
        return false;
    element_t *newNode = malloc(sizeof(element_t));
    if (!newNode)
        return false;
    size_t len = strlen(s);
    char *copyStr = malloc(len + 1);
    if (!copyStr) {
        free(newNode);
        return false;
    }
    copyStr[len] = '\0';
    strncpy(copyStr, s, len);
    newNode->value = copyStr;
    list_add(&newNode->list, head);
    return true;
}

/* Insert an element at tail of queue */
bool q_insert_tail(struct list_head *head, char *s)
{
    if (!head)
        return false;
    element_t *newNode = malloc(sizeof(element_t));
    if (!newNode)
        return false;

    size_t len = strlen(s);
    char *copyStr = malloc(len + 1);

    if (!copyStr) {
        free(newNode);
        return false;
    }
    copyStr[len] = '\0';
    strncpy(copyStr, s, len);
    newNode->value = copyStr;
    list_add_tail(&newNode->list, head);
    return true;
}

/* Remove an element from head of queue */
element_t *q_remove_head(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;

    element_t *rmElement = list_first_entry(head, element_t, list);
    list_del(&rmElement->list);

    if (sp && bufsize > 0) {
        strncpy(sp, rmElement->value, bufsize - 1);
        sp[bufsize - 1] = '\0';
    }

    return rmElement;
}

/* Remove an element from tail of queue */
element_t *q_remove_tail(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;

    element_t *rmElement = list_last_entry(head, element_t, list);
    list_del(&rmElement->list);

    if (sp && bufsize > 0) {
        strncpy(sp, rmElement->value, bufsize - 1);
        sp[bufsize - 1] = '\0';
    }

    return rmElement;
}

/* Return number of elements in queue */
int q_size(struct list_head *head)
{
    if (!head)
        return 0;
    int size = 0;
    struct list_head *node;
    list_for_each (node, head) {
        size++;
    }
    return size;
}

/* Delete the middle node in queue */
// https://leetcode.com/problems/delete-the-middle-node-of-a-linked-list/
bool q_delete_mid(struct list_head *head)
{
    if (!head || list_empty(head))
        return false;
    struct list_head *fast = head->next, *slow = head->next;

    // find the middle node
    while (fast != head && fast->next != head) {
        fast = fast->next->next;
        slow = slow->next;
    }

    element_t *rmElement = list_entry(slow, element_t, list);
    list_del_init(slow);
    q_release_element(rmElement);
    return true;
}

/* Delete all nodes that have duplicate string */
// https://leetcode.com/problems/remove-duplicates-from-sorted-list-ii/

bool q_delete_dup(struct list_head *head)
{
    if (!head)
        return false;
    LIST_HEAD(dup_list);
    struct list_head *node, *next;
    list_for_each_safe (node, next, head) {
        struct list_head *cur = node->prev;
        bool isDup = false;
        element_t *ele1 = list_entry(node, element_t, list);
        if (!ele1)
            return false;
        while (cur != head) {
            element_t *ele2 = list_entry(cur, element_t, list);
            if (!ele2)
                return false;

            if (!strcmp(ele1->value, ele2->value)) {
                list_del_init(node);
                q_release_element(ele1);
                list_del_init(cur);
                list_add_tail(cur, &dup_list);
                isDup = true;
                break;
            }
            cur = cur->prev;
        }
        if (isDup)
            continue;
        list_for_each (cur, &dup_list) {
            element_t *ele2 = list_entry(cur, element_t, list);
            if (!ele2)
                return false;
            if (!strcmp(ele1->value, ele2->value)) {
                list_del_init(node);
                q_release_element(ele1);
                break;
            }
        }
    }
    // delete duplicated list
    list_for_each_safe (node, next, &dup_list) {
        element_t *ele = list_entry(node, element_t, list);
        q_release_element(ele);
    }
    return true;
}

/* Swap every two adjacent nodes */
// https://leetcode.com/problems/swap-nodes-in-pairs/
void q_swap(struct list_head *head)
{
    if (!head || list_empty(head))
        return;
    for (struct list_head *cur = head->next;
         (uintptr_t) (cur) ^ (uintptr_t) (head) &&
         (uintptr_t) (cur->next) ^ (uintptr_t) (head);
         cur = cur->next->next) {
        element_t *first = list_entry(cur, element_t, list);
        element_t *second = list_entry(cur->next, element_t, list);
        // swap the value
        first->value =
            (char *) ((uintptr_t) first->value ^ (uintptr_t) second->value);
        second->value =
            (char *) ((uintptr_t) first->value ^ (uintptr_t) second->value);
        first->value =
            (char *) ((uintptr_t) first->value ^ (uintptr_t) second->value);
    }
    return;
}

/* Reverse elements in queue */
void q_reverse(struct list_head *head)
{
    if (!head || list_empty(head))
        return;

    struct list_head *currNode = head, *nextNode = currNode->next;

    while (nextNode != head) {
        nextNode = currNode->next;

        // Swap the current node's next and prev pointers
        currNode->next = currNode->prev;
        currNode->prev = nextNode;

        currNode = nextNode;
    }

    struct list_head **indirect = &head;
    *indirect = currNode->next;
}

/* Reverse the nodes of the list k at a time */
// https://leetcode.com/problems/reverse-nodes-in-k-group/
void q_reverseK(struct list_head *head, int k)
{
    if (!head || list_empty(head))
        return;
    struct list_head *it, *safe, *cut;
    int ctr = k;
    cut = head;
    list_for_each_safe (it, safe, head) {
        if (--ctr)
            continue;
        LIST_HEAD(tmp);
        list_cut_position(&tmp, cut, it);
        q_reverse(&tmp);
        list_splice(&tmp, cut);
        cut = safe->prev;
        ctr = k;
    }
}

struct list_head *mergeTwoLists(struct list_head *L1,
                                struct list_head *L2,
                                bool descend)
{
    if (!L1)
        return L2;
    if (!L2)
        return L1;
    struct list_head *L1_head = L1, *L2_head = L2;
    LIST_HEAD(head);
    L1 = L1->next;
    L2 = L2->next;
    while (L1 != L1_head && L2 != L2_head) {
        element_t *ele1 = list_entry(L1, element_t, list);
        element_t *ele2 = list_entry(L2, element_t, list);
        if ((strcmp(ele1->value, ele2->value) <= 0) != descend) {
            struct list_head *next = L1->next;
            list_move_tail(L1, &head);
            L1 = next;
        } else {
            struct list_head *next = L2->next;
            list_move_tail(L2, &head);
            L2 = next;
        }
    }
    if (L1 != L1_head) {
        list_splice_tail_init(L1_head, &head);
    } else if (L2 != L2_head) {
        list_splice_tail_init(L2_head, &head);
    }
    list_splice(&head, L1_head);
    list_splice_tail_init(L2_head, L1_head);
    return L1_head;
}

struct list_head *mergesort_list(struct list_head *head, bool descend)
{
    if (!head || list_empty(head) || list_is_singular(head)) {
        return head;
    }
    struct list_head *fast = head->next, *slow = head->next;
    do {
        fast = fast->next->next;
        slow = slow->next;
    } while (fast != head && fast->next != head);
    struct list_head *mid = slow;

    element_t ele;
    struct list_head *head2 = &(ele.list);
    INIT_LIST_HEAD(head2);
    struct list_head *prevhead = head->prev;
    struct list_head *prevmid = mid->prev;

    head2->prev = prevhead;
    prevhead->next = head2;
    head2->next = mid;
    mid->prev = head2;

    prevmid->next = head;
    head->prev = prevmid;

    struct list_head *left = mergesort_list(head, descend),
                     *right = mergesort_list(head2, descend);
    mergeTwoLists(left, right, descend);
    return left;
}

/* Sort elements of queue in ascending/descending order */
void q_sort(struct list_head *head, bool descend)
{
    if (!head || list_empty(head))
        return;
    mergesort_list(head, descend);
}

/* Remove every node which has a node with a strictly less value anywhere to
 * the right side of it */
// https://leetcode.com/problems/remove-nodes-from-linked-list/
int q_ascend(struct list_head *head)
{
    if (!head || list_empty(head)) {
        return 0;
    }
    int n = 0;
    struct list_head *cur = head->prev;
    element_t *smallest = list_entry(cur, element_t, list);
    while (cur != head) {
        struct list_head *precur = cur->prev;
        element_t *ele = list_entry(cur, element_t, list);
        if (strcmp(ele->value, smallest->value) > 0)
            list_del(cur);
        else {
            smallest = ele;
            n++;
        }
        cur = precur;
    }
    return n;
}

/* Remove every node which has a node with a strictly greater value anywhere to
 * the right side of it */
int q_descend(struct list_head *head)
{
    if (!head || list_empty(head)) {
        return 0;
    }
    int n = 0;
    struct list_head *cur = head->prev;
    element_t *biggest = list_entry(cur, element_t, list);
    while (cur != head) {
        struct list_head *precur = cur->prev;
        element_t *ele = list_entry(cur, element_t, list);
        if (strcmp(ele->value, biggest->value) < 0) {
            list_del_init(cur);
            q_release_element(list_entry(cur, element_t, list));
        } else {
            biggest = ele;
            n++;
        }
        cur = precur;
    }
    return n;
}

/* Merge all the queues into one sorted queue, which is in ascending/descending
 * order */
// https://leetcode.com/problems/merge-k-sorted-lists/
int q_merge(struct list_head *head, bool descend)
{
    if (!head || list_empty(head))
        return 0;
    else if (list_is_singular(head))
        return list_entry(head->next, queue_contex_t, chain)->size;
    queue_contex_t *target = list_entry(head->next, queue_contex_t, chain);
    queue_contex_t *que = NULL;
    list_for_each_entry (que, head, chain) {
        if (que == target)
            continue;
        list_splice_init(que->q, target->q);
        target->size = target->size + que->size;
        que->size = 0;
    }
    q_sort(target->q, descend);
    return target->size;
}
