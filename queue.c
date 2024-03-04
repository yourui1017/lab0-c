#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

/* Notice: sometimes, Cppcheck would find the potential NULL pointer bugs,
 * but some of them cannot occur. You can suppress them by adding the
 * following line.
 *   cppcheck-suppress nullPointer
 */

/* Create an empty queue */
struct list_head *q_new()
{
    struct list_head *head = malloc(sizeof(struct list_head));
    if (!head)
        return NULL;
    INIT_LIST_HEAD(head);
    return head;
}

/* Free all storage used by queue */
void q_free(struct list_head *head)
{
    if (!head)
        return;
    element_t *entry, *safe;
    list_for_each_entry_safe (entry, safe, head, list) {
        if (entry->value)
            free(entry->value);
        free(entry);
    }
    free(head);
}

/* Insert an element at head of queue */
bool q_insert_head(struct list_head *head, char *s)
{
    if (!head)
        return false;

    element_t *new = malloc(sizeof(element_t));
    if (!new)
        return false;
    int str_size = sizeof(char) * (strlen(s) + 1);
    new->value = malloc(str_size);
    if (!new->value) {
        free(new);
        return false;
    }
    strncpy(new->value, s, str_size);
    list_add(&new->list, head);
    return true;
}

/* Insert an element at tail of queue */
bool q_insert_tail(struct list_head *head, char *s)
{
    if (!head)
        return false;

    element_t *new = malloc(sizeof(element_t));
    if (!new)
        return false;

    int str_size = sizeof(char) * (strlen(s) + 1);
    new->value = malloc(str_size);
    if (!new->value) {
        free(new);
        return false;
    }

    strncpy(new->value, s, str_size);
    list_add_tail(&new->list, head);
    return true;
}

/* Remove an element from head of queue */
element_t *q_remove_head(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;

    element_t *remove = list_first_entry(head, element_t, list);

    if (sp)
        strncpy(sp, remove->value, bufsize - 1);
    list_del(&remove->list);
    return remove;
}

/* Remove an element from tail of queue */
element_t *q_remove_tail(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;

    element_t *remove = list_last_entry(head, element_t, list);

    if (sp)
        strncpy(sp, remove->value, bufsize - 1);
    list_del(&remove->list);
    return remove;
}

/* Return number of elements in queue */
int q_size(struct list_head *head)
{
    if (!head || list_empty(head))
        return 0;

    int count = 0;
    element_t *entry;

    list_for_each_entry (entry, head, list)
        count++;

    return count;
}

/* Delete the middle node in queue */
bool q_delete_mid(struct list_head *head)
{
    if (!head || list_empty(head))
        return false;

    struct list_head **indir = &head->next;

    for (struct list_head *fast = head->next;
         fast != head && fast->next != head; fast = fast->next->next)
        indir = &(*indir)->next;
    struct list_head *del = *indir;
    list_del(del);
    free(list_entry(del, element_t, list)->value);
    free(list_entry(del, element_t, list));
    return true;
}

/* Delete all nodes that have duplicate string */
bool q_delete_dup(struct list_head *head)
{
    if (!head || list_empty(head))
        return false;
    bool flag = false;
    element_t *entry, *safe;
    struct list_head *del = malloc(sizeof(struct list_head));
    list_for_each_entry_safe (entry, safe, head, list) {
        if (entry->value == safe->value) {
            list_move(&entry->list, del);
            flag = true;
        }
        if (flag) {
            list_move(&entry->list, del);
            flag = false;
        }
    }
    q_free(del);
    return true;
}

/* Swap every two adjacent nodes */
void q_swap(struct list_head *head)
{
    if (!head || list_empty(head))
        return;

    struct list_head **indir = &head->next;
    for (; *indir != head && (*indir)->next != head;
         indir = &(*indir)->next->next)
        list_move(*indir, (*indir)->next);
}

/* Reverse elements in queue */
void q_reverse(struct list_head *head) {}

/* Reverse the nodes of the list k at a time */
void q_reverseK(struct list_head *head, int k)
{
    // https://leetcode.com/problems/reverse-nodes-in-k-group/
}

struct list_head *mergeTwoLists(struct list_head *L1,
                                struct list_head *L2,
                                bool descend)
{
    struct list_head *head = NULL, **ptr = &head, **node;

    for (node = NULL; L1 && L2; *node = (*node)->next) {
        element_t *L1_entry = list_entry(L1, element_t, list);
        element_t *L2_entry = list_entry(L2, element_t, list);
        if (descend)
            node = (strcmp(L1_entry->value, L2_entry->value) > 0) ? &L1 : &L2;
        else
            node = (strcmp(L1_entry->value, L2_entry->value) < 0) ? &L1 : &L2;

        *ptr = *node;
        ptr = &(*ptr)->next;
    }
    *ptr = (struct list_head *) ((uintptr_t) L1 | (uintptr_t) L2);
    return head;
}

struct list_head *mergesort_list(struct list_head *head, bool descend)
{
    if (!head || !head->next)
        return head;
    struct list_head *slow = head;
    for (struct list_head *fast = head->next; fast && fast->next;
         fast = fast->next->next)
        slow = slow->next;
    struct list_head *mid = slow->next;
    slow->next = NULL;

    struct list_head *left = mergesort_list(head, descend),
                     *right = mergesort_list(mid, descend);
    return mergeTwoLists(left, right, descend);
}

/* Sort elements of queue in ascending/descending order */
void q_sort(struct list_head *head, bool descend)
{
    // mergeSortList(head, descend);
    if (!head || list_empty(head) || list_is_singular(head))
        return;
    head->prev->next = NULL;
    head->next = mergesort_list(head->next, descend);

    struct list_head **indir = &head;
    while ((*indir)->next) {
        (*indir)->next->prev = *indir;
        indir = &(*indir)->next;
    }
    (*indir)->next = head;
    head->prev = *indir;
}

/* Remove every node which has a node with a strictly less value anywhere to
 * the right side of it */
int q_ascend(struct list_head *head)
{
    // https://leetcode.com/problems/remove-nodes-from-linked-list/
    return 0;
}

/* Remove every node which has a node with a strictly greater value anywhere to
 * the right side of it */
int q_descend(struct list_head *head)
{
    // https://leetcode.com/problems/remove-nodes-from-linked-list/
    return 0;
}

/* Merge all the queues into one sorted queue, which is in ascending/descending
 * order */
int q_merge(struct list_head *head, bool descend)
{
    // https://leetcode.com/problems/merge-k-sorted-lists/
    return 0;
}
