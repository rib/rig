/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013,2014  Intel Corporation.
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

#ifndef _RUT_DISPLAY_LIST_H_
#define _RUT_DISPLAY_LIST_H_

#include <clib.h>

#include <cogl/cogl.h>
#include <cogl-pango/cogl-pango.h>

typedef enum _rut_cmd_type_t {
    RUT_CMD_TYPE_NOP,
    RUT_CMD_TYPE_TRANSFORM_PUSH,
    RUT_CMD_TYPE_TRANSFORM_POP,
    RUT_CMD_TYPE_TRANSFORM,
    RUT_CMD_TYPE_PRIMITIVE,
    RUT_CMD_TYPE_TEXT,
    RUT_CMD_TYPE_RECTANGLE
} rut_cmd_type_t;

typedef struct _rut_cmd_t {
    rut_cmd_type_t type;
} rut_cmd_t;
#define RUT_CMD(x) ((rut_cmd_t *)X)

typedef struct _rut_transform_cmd_t {
    rut_cmd_t _parent;
    c_matrix_t matrix;
} rut_transform_cmd_t;
#define RUT_TRANSFORM_CMD(X) ((rut_transform_cmd_t *)X)

typedef struct _rut_primitive_cmd_t {
    rut_cmd_t _parent;
    cg_pipeline_t *pipeline;
    cg_primitive_t *primitive;
} rut_primitive_cmd_t;
#define RUT_PRIMITIVE_CMD(X) ((rut_primitive_cmd_t *)X)

typedef struct _rut_text_cmd_t {
    rut_cmd_t _parent;
    PangoLayout *layout;
    cg_color_t color;
    int x;
    int y;
} rut_text_cmd_t;
#define RUT_TEXT_CMD(X) ((rut_text_cmd_t *)X)

typedef struct _rut_rectangle_cmd_t {
    rut_cmd_t _parent;
    cg_pipeline_t *pipeline;
    float width, height;
} rut_rectangle_cmd_t;
#define RUT_RECTANGLE_CMD(X) ((rut_rectangle_cmd_t *)X)

typedef struct _rut_display_list_t {
    /* PRIVATE */
    c_llist_t *head;
    c_llist_t *tail;
} rut_display_list_t;

void rut_display_list_unsplice(rut_display_list_t *list);

void rut_display_list_splice(c_llist_t *after, rut_display_list_t *sub_list);

void rut_display_list_append(rut_display_list_t *list, void *data);

c_llist_t *rut_display_list_insert_before(c_llist_t *sibling, void *data);

void rut_display_list_delete_link(c_llist_t *link);

void rut_display_list_init(rut_display_list_t *list);

void rut_display_list_destroy(rut_display_list_t *list);

void rut_display_list_paint(rut_display_list_t *display_list,
                            cg_framebuffer_t *fb);

#endif /* _RUT_DISPLAY_LIST_H_ */
