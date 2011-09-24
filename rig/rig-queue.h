/*-
 * Copyright (c) 1991, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Copyright (C) 2011 Intel Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      @(#)queue.h     8.5 (Berkeley) 8/20/94
 * $FreeBSD$
 */

#ifndef _RIG_QUEUE_H_
#define _RIG_QUEUE_H_

/*
 * This file defines four types of data structures: singly-linked lists,
 * singly-linked tail queues, lists and tail queues.
 *
 * A singly-linked list is headed by a single forward pointer. The elements
 * are singly linked for minimum space and pointer manipulation overhead at
 * the expense of O(n) removal for arbitrary elements. New elements can be
 * added to the list after an existing element or at the head of the list.
 * Elements being removed from the head of the list should use the explicit
 * macro for this purpose for optimum efficiency. A singly-linked list may
 * only be traversed in the forward direction.  Singly-linked lists are ideal
 * for applications with large datasets and few or no removals or for
 * implementing a LIFO queue.
 *
 * A singly-linked tail queue is headed by a pair of pointers, one to the
 * head of the list and the other to the tail of the list. The elements are
 * singly linked for minimum space and pointer manipulation overhead at the
 * expense of O(n) removal for arbitrary elements. New elements can be added
 * to the list after an existing element, at the head of the list, or at the
 * end of the list. Elements being removed from the head of the tail queue
 * should use the explicit macro for this purpose for optimum efficiency.
 * A singly-linked tail queue may only be traversed in the forward direction.
 * Singly-linked tail queues are ideal for applications with large datasets
 * and few or no removals or for implementing a FIFO queue.
 *
 * A list is headed by a single forward pointer (or an array of forward
 * pointers for a hash table header). The elements are doubly linked
 * so that an arbitrary element can be removed without a need to
 * traverse the list. New elements can be added to the list before
 * or after an existing element or at the head of the list. A list
 * may only be traversed in the forward direction.
 *
 * A tail queue is headed by a pair of pointers, one to the head of the
 * list and the other to the tail of the list. The elements are doubly
 * linked so that an arbitrary element can be removed without a need to
 * traverse the list. New elements can be added to the list before or
 * after an existing element, at the head of the list, or at the end of
 * the list. A tail queue may be traversed in either direction.
 *
 * For details on the use of these macros, see the queue(3) manual page.
 *
 *
 *                              SLIST   LIST    STAILQ  TAILQ
 * _HEAD                        +       +       +       +
 * _HEAD_INITIALIZER            +       +       +       +
 * _ENTRY                       +       +       +       +
 * _INIT                        +       +       +       +
 * _EMPTY                       +       +       +       +
 * _FIRST                       +       +       +       +
 * _NEXT                        +       +       +       +
 * _PREV                        -       -       -       +
 * _LAST                        -       -       +       +
 * _FOREACH                     +       +       +       +
 * _FOREACH_SAFE                +       +       +       +
 * _FOREACH_REVERSE             -       -       -       +
 * _FOREACH_REVERSE_SAFE        -       -       -       +
 * _INSERT_HEAD                 +       +       +       +
 * _INSERT_BEFORE               -       +       -       +
 * _INSERT_AFTER                +       +       +       +
 * _INSERT_TAIL                 -       -       +       +
 * _CONCAT                      -       -       +       +
 * _REMOVE_AFTER                +       -       +       -
 * _REMOVE_HEAD                 +       -       +       -
 * _REMOVE                      +       +       +       +
 * _SWAP                        +       +       +       +
 *
 */
#ifdef RIG_QUEUE_MACRO_DEBUG
/* Store the last 2 places the queue element or head was altered */
struct rig_qm_trace {
        char * lastfile;
        int lastline;
        char * prevfile;
        int prevline;
};

#define RIG_TRACEBUF struct rig_qm_trace trace;
#define RIG_TRASHIT(x) do {(x) = (void *)-1;} while (0)
#define RIG_QMD_SAVELINK(name, link) void **name = (void *)&(link)

#define RIG_QMD_TRACE_HEAD(head) do {                                  \
        (head)->trace.prevline = (head)->trace.lastline;                \
        (head)->trace.prevfile = (head)->trace.lastfile;                \
        (head)->trace.lastline = __LINE__;                              \
        (head)->trace.lastfile = __FILE__;                              \
} while (0)

#define RIG_QMD_TRACE_ELEM(elem) do {                                  \
        (elem)->trace.prevline = (elem)->trace.lastline;                \
        (elem)->trace.prevfile = (elem)->trace.lastfile;                \
        (elem)->trace.lastline = __LINE__;                              \
        (elem)->trace.lastfile = __FILE__;                              \
} while (0)

#else
#define RIG_QMD_TRACE_ELEM(elem)
#define RIG_QMD_TRACE_HEAD(head)
#define RIG_QMD_SAVELINK(name, link)
#define RIG_TRACEBUF
#define RIG_TRASHIT(x)
#endif  /* RIG_QUEUE_MACRO_DEBUG */

/*
 * Singly-linked List declarations.
 */
#define RIG_SLIST_HEAD(name, type)                                     \
typedef struct _ ## name {                                              \
  type *slh_first; /* first element */                                  \
} name

#define RIG_SLIST_HEAD_INITIALIZER(head)                               \
        { NULL }

#define RIG_SLIST_ENTRY(type)                                          \
struct {                                                                \
  type *sle_next;  /* next element */                                   \
}

/*
 * Singly-linked List functions.
 */
#define RIG_SLIST_EMPTY(head)  ((head)->slh_first == NULL)

#define RIG_SLIST_FIRST(head)  ((head)->slh_first)

#define RIG_SLIST_FOREACH(var, head, field)                            \
        for ((var) = RIG_SLIST_FIRST((head));                          \
            (var);                                                      \
            (var) = RIG_SLIST_NEXT((var), field))

#define RIG_SLIST_FOREACH_SAFE(var, head, field, tvar)                 \
        for ((var) = RIG_SLIST_FIRST((head));                          \
            (var) && ((tvar) = RIG_SLIST_NEXT((var), field), 1);       \
            (var) = (tvar))

#define RIG_SLIST_FOREACH_PREVPTR(var, varp, head, field)              \
        for ((varp) = &RIG_SLIST_FIRST((head));                        \
            ((var) = *(varp)) != NULL;                                  \
            (varp) = &RIG_SLIST_NEXT((var), field))

#define RIG_SLIST_INIT(head) do {                                      \
        RIG_SLIST_FIRST((head)) = NULL;                                \
} while (0)

#define RIG_SLIST_INSERT_AFTER(slistelm, elm, field) do {              \
        RIG_SLIST_NEXT((elm), field) = RIG_SLIST_NEXT((slistelm), field);  \
        RIG_SLIST_NEXT((slistelm), field) = (elm);                     \
} while (0)

#define RIG_SLIST_INSERT_HEAD(head, elm, field) do {                   \
        RIG_SLIST_NEXT((elm), field) = RIG_SLIST_FIRST((head));       \
        RIG_SLIST_FIRST((head)) = (elm);                               \
} while (0)

#define RIG_SLIST_NEXT(elm, field)  ((elm)->field.sle_next)

#define RIG_SLIST_REMOVE(head, elm, type, field) do {                  \
        RIG_QMD_SAVELINK(oldnext, (elm)->field.sle_next);              \
        if (RIG_SLIST_FIRST((head)) == (elm)) {                        \
                RIG_SLIST_REMOVE_HEAD((head), field);                  \
        }                                                               \
        else {                                                          \
                type *curelm = RIG_SLIST_FIRST((head));                \
                while (RIG_SLIST_NEXT(curelm, field) != (elm))         \
                        curelm = RIG_SLIST_NEXT(curelm, field);        \
                RIG_SLIST_REMOVE_AFTER(curelm, field);                 \
        }                                                               \
        RIG_TRASHIT(*oldnext);                                         \
} while (0)

#define RIG_SLIST_REMOVE_AFTER(elm, field) do {                        \
        RIG_SLIST_NEXT(elm, field) =                                   \
            RIG_SLIST_NEXT(RIG_SLIST_NEXT(elm, field), field);        \
} while (0)

#define RIG_SLIST_REMOVE_HEAD(head, field) do {                        \
    RIG_SLIST_FIRST((head)) =                                          \
      RIG_SLIST_NEXT(RIG_SLIST_FIRST((head)), field);                 \
} while (0)

#define RIG_SLIST_SWAP(head1, head2, type) do {                        \
        type *swap_first = RIG_SLIST_FIRST(head1);                     \
        RIG_SLIST_FIRST(head1) = RIG_SLIST_FIRST(head2);              \
        RIG_SLIST_FIRST(head2) = swap_first;                           \
} while (0)

/*
 * Singly-linked Tail queue declarations.
 */
#define RIG_STAILQ_HEAD(name, type)                                    \
typedef struct _ ## name {                                              \
        type *stqh_first;/* first element */                            \
        type **stqh_last;/* addr of last next element */                \
} name

#define RIG_STAILQ_HEAD_INITIALIZER(head)                              \
        { NULL, &(head).stqh_first }

#define RIG_STAILQ_ENTRY(type)                                         \
struct {                                                                \
        type *stqe_next; /* next element */                             \
}

/*
 * Singly-linked Tail queue functions.
 */
#define RIG_STAILQ_CONCAT(head1, head2) do {                           \
        if (!RIG_STAILQ_EMPTY((head2))) {                              \
                *(head1)->stqh_last = (head2)->stqh_first;              \
                (head1)->stqh_last = (head2)->stqh_last;                \
                RIG_STAILQ_INIT((head2));                              \
        }                                                               \
} while (0)

#define RIG_STAILQ_EMPTY(head) ((head)->stqh_first == NULL)

#define RIG_STAILQ_FIRST(head) ((head)->stqh_first)

#define RIG_STAILQ_FOREACH(var, head, field)                           \
        for((var) = RIG_STAILQ_FIRST((head));                          \
           (var);                                                       \
           (var) = RIG_STAILQ_NEXT((var), field))


#define RIG_STAILQ_FOREACH_SAFE(var, head, field, tvar)                \
        for ((var) = RIG_STAILQ_FIRST((head));                         \
            (var) && ((tvar) = RIG_STAILQ_NEXT((var), field), 1);      \
            (var) = (tvar))

#define RIG_STAILQ_INIT(head) do {                                     \
        RIG_STAILQ_FIRST((head)) = NULL;                               \
        (head)->stqh_last = &RIG_STAILQ_FIRST((head));                 \
} while (0)

#define RIG_STAILQ_INSERT_AFTER(head, tqelm, elm, field) do {          \
    if ((RIG_STAILQ_NEXT((elm), field) =                               \
         RIG_STAILQ_NEXT((tqelm), field)) == NULL)                     \
      (head)->stqh_last = &RIG_STAILQ_NEXT((elm), field);              \
    RIG_STAILQ_NEXT((tqelm), field) = (elm);                           \
} while (0)

#define RIG_STAILQ_INSERT_HEAD(head, elm, field) do {                  \
    if ((RIG_STAILQ_NEXT((elm), field) =                               \
         RIG_STAILQ_FIRST((head))) == NULL)                            \
      (head)->stqh_last = &RIG_STAILQ_NEXT((elm), field);              \
    RIG_STAILQ_FIRST((head)) = (elm);                                  \
  } while (0)

#define RIG_STAILQ_INSERT_TAIL(head, elm, field) do {                  \
        RIG_STAILQ_NEXT((elm), field) = NULL;                          \
        *(head)->stqh_last = (elm);                                     \
        (head)->stqh_last = &RIG_STAILQ_NEXT((elm), field);            \
} while (0)

#define RIG_STAILQ_LAST(head, type, field)                             \
        (RIG_STAILQ_EMPTY((head)) ?                                    \
                NULL :                                                  \
                ((type *)(void *)                                       \
                 ((char *)((head)->stqh_last) -                         \
                  __offsetof(struct type, field))))

#define RIG_STAILQ_NEXT(elm, field) ((elm)->field.stqe_next)

#define RIG_STAILQ_REMOVE(head, elm, type, field) do {                 \
        RIG_QMD_SAVELINK(oldnext, (elm)->field.stqe_next);             \
        if (RIG_STAILQ_FIRST((head)) == (elm)) {                       \
                RIG_STAILQ_REMOVE_HEAD((head), field);                 \
        }                                                               \
        else {                                                          \
                type *curelm = RIG_STAILQ_FIRST((head));               \
                while (RIG_STAILQ_NEXT(curelm, field) != (elm))        \
                        curelm = RIG_STAILQ_NEXT(curelm, field);       \
                RIG_STAILQ_REMOVE_AFTER(head, curelm, field);          \
        }                                                               \
        RIG_TRASHIT(*oldnext);                                         \
} while (0)

#define RIG_STAILQ_REMOVE_AFTER(head, elm, field) do {                 \
        if ((RIG_STAILQ_NEXT(elm, field) =                             \
             RIG_STAILQ_NEXT(RIG_STAILQ_NEXT(elm, field),             \
                              field)) == NULL)                          \
                (head)->stqh_last = &RIG_STAILQ_NEXT((elm), field);    \
} while (0)

#define RIG_STAILQ_REMOVE_HEAD(head, field) do {                       \
        if ((RIG_STAILQ_FIRST((head)) =                                \
             RIG_STAILQ_NEXT(RIG_STAILQ_FIRST((head)), field)) == NULL) \
                (head)->stqh_last = &RIG_STAILQ_FIRST((head));         \
} while (0)

#define RIG_STAILQ_SWAP(head1, head2, type) do {                       \
        type *swap_first = RIG_STAILQ_FIRST(head1);                    \
        type **swap_last = (head1)->stqh_last;                          \
        RIG_STAILQ_FIRST(head1) = RIG_STAILQ_FIRST(head2);            \
        (head1)->stqh_last = (head2)->stqh_last;                        \
        RIG_STAILQ_FIRST(head2) = swap_first;                          \
        (head2)->stqh_last = swap_last;                                 \
        if (RIG_STAILQ_EMPTY(head1))                                   \
                (head1)->stqh_last = &RIG_STAILQ_FIRST(head1);         \
        if (RIG_STAILQ_EMPTY(head2))                                   \
                (head2)->stqh_last = &RIG_STAILQ_FIRST(head2);         \
} while (0)


/*
 * List declarations.
 */
#define RIG_LIST_HEAD(name, type)                                      \
typedef struct _ ## name {                                              \
        type *lh_first;  /* first element */                            \
} name

#define RIG_LIST_HEAD_INITIALIZER(head)                                \
        { NULL }

#define RIG_LIST_ENTRY(type)                                           \
struct {                                                                \
        type *le_next;   /* next element */                             \
        type **le_prev;  /* address of previous next element */         \
}

/*
 * List functions.
 */

#if (defined(_KERNEL) && defined(INVARIANTS))
#define RIG_QMD_LIST_CHECK_HEAD(head, field) do {                      \
        if (RIG_LIST_FIRST((head)) != NULL &&                          \
            RIG_LIST_FIRST((head))->field.le_prev !=                   \
             &RIG_LIST_FIRST((head)))                                  \
                panic("Bad list head %p first->prev != head", (head));  \
} while (0)

#define RIG_QMD_LIST_CHECK_NEXT(elm, field) do {                       \
        if (RIG_LIST_NEXT((elm), field) != NULL &&                     \
            RIG_LIST_NEXT((elm), field)->field.le_prev !=              \
             &((elm)->field.le_next))                                   \
                panic("Bad link elm %p next->prev != elm", (elm));      \
} while (0)

#define RIG_QMD_LIST_CHECK_PREV(elm, field) do {                       \
        if (*(elm)->field.le_prev != (elm))                             \
                panic("Bad link elm %p prev->next != elm", (elm));      \
} while (0)
#else
#define RIG_QMD_LIST_CHECK_HEAD(head, field)
#define RIG_QMD_LIST_CHECK_NEXT(elm, field)
#define RIG_QMD_LIST_CHECK_PREV(elm, field)
#endif /* (_KERNEL && INVARIANTS) */

#define RIG_LIST_EMPTY(head)   ((head)->lh_first == NULL)

#define RIG_LIST_FIRST(head)   ((head)->lh_first)

#define RIG_LIST_FOREACH(var, head, field)                             \
        for ((var) = RIG_LIST_FIRST((head));                           \
            (var);                                                      \
            (var) = RIG_LIST_NEXT((var), field))

#define RIG_LIST_FOREACH_SAFE(var, head, field, tvar)                  \
        for ((var) = RIG_LIST_FIRST((head));                           \
            (var) && ((tvar) = RIG_LIST_NEXT((var), field), 1);        \
            (var) = (tvar))

#define RIG_LIST_INIT(head) do {                                       \
        RIG_LIST_FIRST((head)) = NULL;                                 \
} while (0)

#define RIG_LIST_INSERT_AFTER(listelm, elm, field) do {                \
        RIG_QMD_LIST_CHECK_NEXT(listelm, field);                       \
        if ((RIG_LIST_NEXT((elm), field) =                             \
             RIG_LIST_NEXT((listelm), field)) != NULL)                 \
                RIG_LIST_NEXT((listelm), field)->field.le_prev =       \
                    &RIG_LIST_NEXT((elm), field);                      \
        RIG_LIST_NEXT((listelm), field) = (elm);                       \
        (elm)->field.le_prev = &RIG_LIST_NEXT((listelm), field);       \
} while (0)

#define RIG_LIST_INSERT_BEFORE(listelm, elm, field) do {               \
        RIG_QMD_LIST_CHECK_PREV(listelm, field);                       \
        (elm)->field.le_prev = (listelm)->field.le_prev;                \
        RIG_LIST_NEXT((elm), field) = (listelm);                       \
        *(listelm)->field.le_prev = (elm);                              \
        (listelm)->field.le_prev = &RIG_LIST_NEXT((elm), field);       \
} while (0)

#define RIG_LIST_INSERT_HEAD(head, elm, field) do {                    \
        RIG_QMD_LIST_CHECK_HEAD((head), field);                        \
        if ((RIG_LIST_NEXT((elm), field) =                             \
             RIG_LIST_FIRST((head))) != NULL)                          \
          RIG_LIST_FIRST((head))->field.le_prev =                      \
            &RIG_LIST_NEXT((elm), field);                              \
        RIG_LIST_FIRST((head)) = (elm);                                \
        (elm)->field.le_prev = &RIG_LIST_FIRST((head));                \
} while (0)

#define RIG_LIST_NEXT(elm, field)   ((elm)->field.le_next)

#define RIG_LIST_REMOVE(elm, field) do {                               \
        RIG_QMD_SAVELINK(oldnext, (elm)->field.le_next);               \
        RIG_QMD_SAVELINK(oldprev, (elm)->field.le_prev);               \
        RIG_QMD_LIST_CHECK_NEXT(elm, field);                           \
        RIG_QMD_LIST_CHECK_PREV(elm, field);                           \
        if (RIG_LIST_NEXT((elm), field) != NULL)                       \
                RIG_LIST_NEXT((elm), field)->field.le_prev =           \
                    (elm)->field.le_prev;                               \
        *(elm)->field.le_prev = RIG_LIST_NEXT((elm), field);           \
        RIG_TRASHIT(*oldnext);                                         \
        RIG_TRASHIT(*oldprev);                                         \
} while (0)

#define RIG_LIST_SWAP(head1, head2, type, field) do {                  \
        type *swap_tmp = RIG_LIST_FIRST((head1));                      \
        RIG_LIST_FIRST((head1)) = RIG_LIST_FIRST((head2));            \
        RIG_LIST_FIRST((head2)) = swap_tmp;                            \
        if ((swap_tmp = RIG_LIST_FIRST((head1))) != NULL)              \
                swap_tmp->field.le_prev = &RIG_LIST_FIRST((head1));    \
        if ((swap_tmp = RIG_LIST_FIRST((head2))) != NULL)              \
                swap_tmp->field.le_prev = &RIG_LIST_FIRST((head2));    \
} while (0)

/*
 * Tail queue declarations.
 */
#define RIG_TAILQ_HEAD(name, type)                                     \
typedef struct _ ## name {                                              \
        type *tqh_first; /* first element */                            \
        type **tqh_last; /* addr of last next element */                \
        RIG_TRACEBUF                                                   \
} name

#define RIG_TAILQ_HEAD_INITIALIZER(head)                               \
        { NULL, &(head).tqh_first }

#define RIG_TAILQ_ENTRY(type)                                          \
struct {                                                                \
        type *tqe_next;  /* next element */                             \
        type **tqe_prev; /* address of previous next element */         \
        RIG_TRACEBUF                                                   \
}

/*
 * Tail queue functions.
 */
#if (defined(_KERNEL) && defined(INVARIANTS))
#define RIG_QMD_TAILQ_CHECK_HEAD(head, field) do {                     \
        if (!RIG_TAILQ_EMPTY(head) &&                                  \
            RIG_TAILQ_FIRST((head))->field.tqe_prev !=                 \
             &RIG_TAILQ_FIRST((head)))                                 \
                panic("Bad tailq head %p first->prev != head", (head)); \
} while (0)

#define RIG_QMD_TAILQ_CHECK_TAIL(head, field) do {                     \
        if (*(head)->tqh_last != NULL)                                  \
                panic("Bad tailq NEXT(%p->tqh_last) != NULL", (head));  \
} while (0)

#define RIG_QMD_TAILQ_CHECK_NEXT(elm, field) do {                      \
        if (RIG_TAILQ_NEXT((elm), field) != NULL &&                    \
            RIG_TAILQ_NEXT((elm), field)->field.tqe_prev !=            \
             &((elm)->field.tqe_next))                                  \
                panic("Bad link elm %p next->prev != elm", (elm));      \
} while (0)

#define RIG_QMD_TAILQ_CHECK_PREV(elm, field) do {                      \
        if (*(elm)->field.tqe_prev != (elm))                            \
                panic("Bad link elm %p prev->next != elm", (elm));      \
} while (0)
#else
#define RIG_QMD_TAILQ_CHECK_HEAD(head, field)
#define RIG_QMD_TAILQ_CHECK_TAIL(head, headname)
#define RIG_QMD_TAILQ_CHECK_NEXT(elm, field)
#define RIG_QMD_TAILQ_CHECK_PREV(elm, field)
#endif /* (_KERNEL && INVARIANTS) */

#define RIG_TAILQ_CONCAT(head1, head2, field) do {                     \
        if (!RIG_TAILQ_EMPTY(head2)) {                                 \
                *(head1)->tqh_last = (head2)->tqh_first;                \
                (head2)->tqh_first->field.tqe_prev = (head1)->tqh_last; \
                (head1)->tqh_last = (head2)->tqh_last;                  \
                RIG_TAILQ_INIT((head2));                               \
                RIG_QMD_TRACE_HEAD(head1);                             \
                RIG_QMD_TRACE_HEAD(head2);                             \
        }                                                               \
} while (0)

#define RIG_TAILQ_EMPTY(head)  ((head)->tqh_first == NULL)

#define RIG_TAILQ_FIRST(head)  ((head)->tqh_first)

#define RIG_TAILQ_FOREACH(var, head, field)                            \
        for ((var) = RIG_TAILQ_FIRST((head));                          \
            (var);                                                      \
            (var) = RIG_TAILQ_NEXT((var), field))

#define RIG_TAILQ_FOREACH_SAFE(var, head, field, tvar)                 \
        for ((var) = RIG_TAILQ_FIRST((head));                          \
            (var) && ((tvar) = RIG_TAILQ_NEXT((var), field), 1);       \
            (var) = (tvar))

#define RIG_TAILQ_FOREACH_REVERSE(var, head, headname, field)          \
        for ((var) = RIG_TAILQ_LAST((head), headname);                 \
            (var);                                                      \
            (var) = RIG_TAILQ_PREV((var), headname, field))

#define RIG_TAILQ_FOREACH_REVERSE_SAFE(var, head, headname, field, tvar) \
        for ((var) = RIG_TAILQ_LAST((head), headname);                 \
            (var) && ((tvar) = RIG_TAILQ_PREV((var), headname, field), 1); \
            (var) = (tvar))

#define RIG_TAILQ_INIT(head) do {                                      \
        RIG_TAILQ_FIRST((head)) = NULL;                                \
        (head)->tqh_last = &RIG_TAILQ_FIRST((head));                   \
        RIG_QMD_TRACE_HEAD(head);                                      \
} while (0)

#define RIG_TAILQ_INSERT_AFTER(head, listelm, elm, field) do {         \
        RIG_QMD_TAILQ_CHECK_NEXT(listelm, field);                      \
        if ((RIG_TAILQ_NEXT((elm), field) =                            \
             RIG_TAILQ_NEXT((listelm), field)) != NULL)                \
                RIG_TAILQ_NEXT((elm), field)->field.tqe_prev =         \
                    &RIG_TAILQ_NEXT((elm), field);                     \
        else {                                                          \
                (head)->tqh_last = &RIG_TAILQ_NEXT((elm), field);      \
                RIG_QMD_TRACE_HEAD(head);                              \
        }                                                               \
        RIG_TAILQ_NEXT((listelm), field) = (elm);                      \
        (elm)->field.tqe_prev = &RIG_TAILQ_NEXT((listelm), field);     \
        RIG_QMD_TRACE_ELEM(&(elm)->field);                             \
        RIG_QMD_TRACE_ELEM(&listelm->field);                           \
} while (0)

#define RIG_TAILQ_INSERT_BEFORE(listelm, elm, field) do {              \
        RIG_QMD_TAILQ_CHECK_PREV(listelm, field);                      \
        (elm)->field.tqe_prev = (listelm)->field.tqe_prev;              \
        RIG_TAILQ_NEXT((elm), field) = (listelm);                      \
        *(listelm)->field.tqe_prev = (elm);                             \
        (listelm)->field.tqe_prev = &RIG_TAILQ_NEXT((elm), field);     \
        RIG_QMD_TRACE_ELEM(&(elm)->field);                             \
        RIG_QMD_TRACE_ELEM(&listelm->field);                           \
} while (0)

#define RIG_TAILQ_INSERT_HEAD(head, elm, field) do {                   \
        RIG_QMD_TAILQ_CHECK_HEAD(head, field);                         \
        if ((RIG_TAILQ_NEXT((elm), field) =                            \
             RIG_TAILQ_FIRST((head))) != NULL)                         \
                RIG_TAILQ_FIRST((head))->field.tqe_prev =              \
                    &RIG_TAILQ_NEXT((elm), field);                     \
        else                                                            \
                (head)->tqh_last = &RIG_TAILQ_NEXT((elm), field);      \
        RIG_TAILQ_FIRST((head)) = (elm);                               \
        (elm)->field.tqe_prev = &RIG_TAILQ_FIRST((head));              \
        RIG_QMD_TRACE_HEAD(head);                                      \
        RIG_QMD_TRACE_ELEM(&(elm)->field);                             \
} while (0)

#define RIG_TAILQ_INSERT_TAIL(head, elm, field) do {                   \
        RIG_QMD_TAILQ_CHECK_TAIL(head, field);                         \
        RIG_TAILQ_NEXT((elm), field) = NULL;                           \
        (elm)->field.tqe_prev = (head)->tqh_last;                       \
        *(head)->tqh_last = (elm);                                      \
        (head)->tqh_last = &RIG_TAILQ_NEXT((elm), field);              \
        RIG_QMD_TRACE_HEAD(head);                                      \
        RIG_QMD_TRACE_ELEM(&(elm)->field);                             \
} while (0)

#define RIG_TAILQ_LAST(head, headname)                                 \
        (*(((headname *)((head)->tqh_last))->tqh_last))

#define RIG_TAILQ_NEXT(elm, field) ((elm)->field.tqe_next)

#define RIG_TAILQ_PREV(elm, headname, field)                           \
        (*(((headname *)((elm)->field.tqe_prev))->tqh_last))

#define RIG_TAILQ_REMOVE(head, elm, field) do {                        \
        RIG_QMD_SAVELINK(oldnext, (elm)->field.tqe_next);              \
        RIG_QMD_SAVELINK(oldprev, (elm)->field.tqe_prev);              \
        RIG_QMD_TAILQ_CHECK_NEXT(elm, field);                          \
        RIG_QMD_TAILQ_CHECK_PREV(elm, field);                          \
        if ((RIG_TAILQ_NEXT((elm), field)) != NULL)                    \
                RIG_TAILQ_NEXT((elm), field)->field.tqe_prev =         \
                    (elm)->field.tqe_prev;                              \
        else {                                                          \
                (head)->tqh_last = (elm)->field.tqe_prev;               \
                RIG_QMD_TRACE_HEAD(head);                              \
        }                                                               \
        *(elm)->field.tqe_prev = RIG_TAILQ_NEXT((elm), field);         \
        RIG_TRASHIT(*oldnext);                                         \
        RIG_TRASHIT(*oldprev);                                         \
        RIG_QMD_TRACE_ELEM(&(elm)->field);                             \
} while (0)

#define RIG_TAILQ_SWAP(head1, head2, type, field) do {                 \
        type *swap_first = (head1)->tqh_first;                          \
        type **swap_last = (head1)->tqh_last;                           \
        (head1)->tqh_first = (head2)->tqh_first;                        \
        (head1)->tqh_last = (head2)->tqh_last;                          \
        (head2)->tqh_first = swap_first;                                \
        (head2)->tqh_last = swap_last;                                  \
        if ((swap_first = (head1)->tqh_first) != NULL)                  \
                swap_first->field.tqe_prev = &(head1)->tqh_first;       \
        else                                                            \
                (head1)->tqh_last = &(head1)->tqh_first;                \
        if ((swap_first = (head2)->tqh_first) != NULL)                  \
                swap_first->field.tqe_prev = &(head2)->tqh_first;       \
        else                                                            \
                (head2)->tqh_last = &(head2)->tqh_first;                \
} while (0)

#endif /* !_RIG_QUEUE_H_ */
