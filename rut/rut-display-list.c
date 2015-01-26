/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012,2013  Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <config.h>

#include "rut-display-list.h"

/* Given the @head and @tail for a sub-list contained within another
 * @list this unlinks the sub-list from @list and returns the new
 * head of @list if it has changed. */
static c_llist_t *
list_unsplice(c_llist_t *list, c_llist_t *head, c_llist_t *tail)
{
    c_llist_t *after;

    if (tail->next) {
        after = tail->next;
        after->prev = NULL;
        tail->next = NULL;
    } else
        after = NULL;

    if (head->prev) {
        head->prev->next = after;

        if (after)
            after->prev = head->prev;

        head->prev = NULL;
        return list;
    } else {
        c_return_val_if_fail(head == list, list);
        return after;
    }
}

/* Given the @head and @tail for a sub-list this links the sub-list
 * into @list after the @after link and returns the new list head
 * if it has changed.
 *
 * If @after is NULL the sub-list will be linked in-front of @list. This
 * would have the same result as using c_llist_concat (@head, @list)
 * although in this case there is no need to traverse the first list to
 * find its @tail. If @after is NULL then @list can also be NULL and
 * in that case the function will return @head.
 *
 * Note: this function doesn't implicitly unsplice the sub-list before
 * splicing so it's the callers responsibility to unsplice the list
 * if necessary and this function will assert that head->prev == NULL
 * and tail->next == NULL.
 */
static c_llist_t *
list_splice(c_llist_t *list, c_llist_t *after, c_llist_t *head, c_llist_t *tail)
{
    c_return_val_if_fail(head->prev == NULL, NULL);
    c_return_val_if_fail(tail->next == NULL, NULL);

    if (after) {
        c_llist_t *remainder = after->next;
        after->next = head;
        head->prev = after;
        if (remainder) {
            tail->next = remainder;
            remainder->prev = tail;
        }
        return list;
    } else {
        if (list) {
            tail->next = list;
            list->prev = tail;
        }
        return head;
    }
}

#if 0
static void
_assert_list_is (c_llist_t *list, unsigned int length, unsigned int value)
{
    c_llist_t *l;

    c_assert (list->prev == NULL);
    c_assert_cmpuint (c_llist_length (list), ==, length);

    for (l = c_llist_last (list); l; l = l->prev)
    {
        c_assert_cmpuint (GPOINTER_TO_UINT (l->data), ==, value % 10);
        value /= 10;
    }
}

static void
_rut_test_list_splice (void)
{
    c_llist_t *l0 = NULL, *l1 = NULL, *l2 = NULL;
    c_llist_t *l0_tail, *l1_tail, *l2_tail;
    c_llist_t *list = NULL;

    l0 = c_llist_append (l0, C_UINT_TO_POINTER (1));
    l0 = c_llist_append (l0, C_UINT_TO_POINTER (2));
    l0 = c_llist_append (l0, C_UINT_TO_POINTER (3));
    l0_tail = c_llist_last (l0);
    _assert_list_is (l0, 3, 123);

    l1 = c_llist_append (l1, C_UINT_TO_POINTER (4));
    l1 = c_llist_append (l1, C_UINT_TO_POINTER (5));
    l1 = c_llist_append (l1, C_UINT_TO_POINTER (6));
    l1_tail = c_llist_last (l1);
    _assert_list_is (l1, 3, 456);

    l2 = c_llist_append (l2, C_UINT_TO_POINTER (7));
    l2 = c_llist_append (l2, C_UINT_TO_POINTER (8));
    l2_tail = c_llist_last (l2);
    _assert_list_is (l2, 2, 78);

    list = l0;

    /* splice on the end */
    list = list_splice (list, l0_tail, l1, l1_tail);
    _assert_list_is (list, 6, 123456);

    /* splice in the middle */
    list = list_splice (list, l0_tail, l2, l2_tail);
    _assert_list_is (list, 8, 12378456);

    /* unsplice from the middle */
    list = list_unsplice (list, l2, l2_tail);
    _assert_list_is (list, 6, 123456);
    _assert_list_is (l2, 2, 78);

    /* unsplice the end */
    list = list_unsplice (list, l1, l1_tail);
    _assert_list_is (list, 3, 123);
    _assert_list_is (l1, 3, 456);

    /* splice at the beginning */
    list = list_splice (list, NULL, l2, l2_tail);
    _assert_list_is (list, 5, 78123);

    /* unsplice from the beginning */
    list = list_unsplice (list, l2, l2_tail);
    _assert_list_is (list, 3, 123);
    _assert_list_is (l2, 2, 78);
}
#endif

/* A display-list is a list of sequential drawing commands including
 * transformation commands and primitive drawing commands.
 *
 * A display list is currently represented as a linked list of c_llist_t nodes
 * although the api we want is a cross between the c_llist_ api and the
 * rut_queue_ api so we have a wrapper api instead to make display list
 * manipulation less confusing and error prone.
 *
 * Two common manipulations to do on display lists are "splicing" and
 * "unsplicing" which means to insert a linked sub-list into a certain
 * position within another display list or to unlink a linked sub-list
 * from a larger display list respectively.
 *
 * A notable requirement for these operations though is that you don't
 * need access to the head pointer for the larger list being spliced
 * into or being unspliced from. This is unlike the above list_splice
 * and list_unsplice functions. Below you'll see that we essentially
 * pass in dummy head arguments to these functions and assert that
 * modification of the head wasn't required.
 *   Note: to make this work it requires that the outermost display
 *   list owned by a rut_object_t must add at least one link into the
 *   display list before allowing any splice or unsplice operations.
 *
 * A complete command sequence is created from a scene-graph by
 * traversing in a depth first order and asking the children of each
 * node to splice their commands into a given position of the
 * display list. Once a node has spliced in its own list of commands
 * then it associates each child with a position within that list
 * and recursively asks the child to splice its commands into that
 * position.
 */

void
rut_display_list_unsplice(rut_display_list_t *list)
{
    if (list->head->prev)
        c_assert(list_unsplice((void *)0xDEADBEEF, list->head, list->tail) ==
                 (c_llist_t *)0xDEADBEEF);
}

void
rut_display_list_splice(c_llist_t *after, rut_display_list_t *sub_list)
{
    rut_display_list_unsplice(sub_list);
    c_assert(list_splice(after, after, sub_list->head, sub_list->tail) ==
             after);
}

void
rut_display_list_append(rut_display_list_t *list, void *data)
{
    c_llist_t *link = c_llist_alloc();
    link->data = data;
    link->prev = list->tail;
    link->next = NULL;

    if (list->tail)
        list->tail->next = link;
    else {
        c_assert(list->head == NULL);
        list->head = link;
    }
    list->tail = link;
}

/* Note: unlike the similar c_llist_t api this returns the newly inserted
 * link not the head of the list. */
c_llist_t *
rut_display_list_insert_before(c_llist_t *sibling, void *data)
{
    c_llist_t *link = c_llist_alloc();
    link->data = data;
    link->next = sibling;
    link->prev = sibling->prev;
    link->prev->next = link;
    sibling->prev = link;

    return link;
}

void
rut_display_list_delete_link(c_llist_t *link)
{
    link->prev->next = link->next;
    link->next->prev = link->prev;
    c_llist_free_1(link);
}

void
rut_display_list_init(rut_display_list_t *list)
{
    list->head = NULL;
    list->tail = NULL;
}

void
rut_display_list_destroy(rut_display_list_t *list)
{
    rut_display_list_unsplice(list);
    c_llist_free(list->head);
    list->head = NULL;
    list->tail = NULL;
}

void
rut_display_list_paint(rut_display_list_t *display_list,
                       cg_framebuffer_t *fb)
{
    c_llist_t *l;

    for (l = display_list->head; l; l = l->next) {
        rut_cmd_t *cmd = l->data;

        if (!cmd)
            continue;

        switch (cmd->type) {
        case RUT_CMD_TYPE_NOP:
            continue;
        case RUT_CMD_TYPE_TRANSFORM_PUSH:
            cg_framebuffer_push_matrix(fb);
            break;
        case RUT_CMD_TYPE_TRANSFORM_POP:
            cg_framebuffer_pop_matrix(fb);
            break;
        case RUT_CMD_TYPE_TRANSFORM: {
            rut_transform_cmd_t *transform_cmd = RUT_TRANSFORM_CMD(cmd);
            cg_framebuffer_transform(fb, &transform_cmd->matrix);
            break;
        }
        case RUT_CMD_TYPE_PRIMITIVE: {
            rut_primitive_cmd_t *prim_cmd = RUT_PRIMITIVE_CMD(cmd);
            cg_primitive_draw(prim_cmd->primitive, fb, prim_cmd->pipeline);
            break;
        }
        case RUT_CMD_TYPE_TEXT: {
            rut_text_cmd_t *text_cmd = RUT_TEXT_CMD(cmd);
            cg_pango_show_layout(fb,
                                 text_cmd->layout,
                                 text_cmd->x,
                                 text_cmd->y,
                                 &text_cmd->color);
            break;
        }
        case RUT_CMD_TYPE_RECTANGLE: {
            rut_rectangle_cmd_t *rect_cmd = RUT_RECTANGLE_CMD(cmd);
            cg_framebuffer_draw_rectangle(fb,
                                          rect_cmd->pipeline,
                                          0,
                                          0,
                                          rect_cmd->width,
                                          rect_cmd->height);
            break;
        }
        }
    }
}
