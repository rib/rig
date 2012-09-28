/*
 * Copyright © 2008 Kristian Høgsberg
 * Copyright © 2012 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

/* This list implementation is based on the Wayland source code */

#ifndef RUT_LIST_H
#define RUT_LIST_H

/**
 * RutList - linked list
 *
 * The list head is of "RutList" type, and must be initialized
 * using rut_list_init().  All entries in the list must be of the same
 * type.  The item type must have a "RutList" member. This
 * member will be initialized by rut_list_insert(). There is no need to
 * call rut_list_init() on the individual item. To query if the list is
 * empty in O(1), use rut_list_empty().
 *
 * Let's call the list reference "RutList foo_list", the item type as
 * "item_t", and the item member as "RutList link". The following code
 *
 * The following code will initialize a list:
 *
 *      rut_list_init (foo_list);
 *      rut_list_insert (foo_list, item1);      Pushes item1 at the head
 *      rut_list_insert (foo_list, item2);      Pushes item2 at the head
 *      rut_list_insert (item2, item3);         Pushes item3 after item2
 *
 * The list now looks like [item2, item3, item1]
 *
 * Will iterate the list in ascending order:
 *
 *      item_t *item;
 *      rut_list_for_each(item, foo_list, link) {
 *              Do_something_with_item(item);
 *      }
 */

typedef struct _RutList RutList;

struct _RutList
{
  RutList *prev;
  RutList *next;
};

void
rut_list_init (RutList *list);

void
rut_list_insert (RutList *list,
                 RutList *elm);

void
rut_list_remove (RutList *elm);

int
rut_list_length (RutList *list);

int
rut_list_empty (RutList *list);

void
rut_list_insert_list (RutList *list,
                      RutList *other);

#ifdef __GNUC__
#define rut_container_of(ptr, sample, member)                           \
  (__typeof__(sample))((char *)(ptr)    -                               \
                       ((char *)&(sample)->member - (char *)(sample)))
#else
#define rut_container_of(ptr, sample, member)                   \
  (void *)((char *)(ptr)        -                               \
           ((char *)&(sample)->member - (char *)(sample)))
#endif

#define rut_list_for_each(pos, head, member)                            \
  for (pos = 0, pos = rut_container_of((head)->next, pos, member);      \
       &pos->member != (head);                                          \
       pos = rut_container_of(pos->member.next, pos, member))

#define rut_list_for_each_safe(pos, tmp, head, member)                  \
  for (pos = 0, tmp = 0,                                                \
         pos = rut_container_of((head)->next, pos, member),             \
         tmp = rut_container_of((pos)->member.next, tmp, member);       \
       &pos->member != (head);                                          \
       pos = tmp,                                                       \
         tmp = rut_container_of(pos->member.next, tmp, member))

#define rut_list_for_each_reverse(pos, head, member)                    \
  for (pos = 0, pos = rut_container_of((head)->prev, pos, member);      \
       &pos->member != (head);                                          \
       pos = rut_container_of(pos->member.prev, pos, member))

#define rut_list_for_each_reverse_safe(pos, tmp, head, member)          \
  for (pos = 0, tmp = 0,                                                \
         pos = rut_container_of((head)->prev, pos, member),             \
         tmp = rut_container_of((pos)->member.prev, tmp, member);       \
       &pos->member != (head);                                          \
       pos = tmp,                                                       \
         tmp = rut_container_of(pos->member.prev, tmp, member))

#endif /* RUT_LIST_H */
