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

typedef enum _RutCmdType
{
  RUT_CMD_TYPE_NOP,
  RUT_CMD_TYPE_TRANSFORM_PUSH,
  RUT_CMD_TYPE_TRANSFORM_POP,
  RUT_CMD_TYPE_TRANSFORM,
  RUT_CMD_TYPE_PRIMITIVE,
  RUT_CMD_TYPE_TEXT,
  RUT_CMD_TYPE_RECTANGLE
} RutCmdType;

typedef struct _RutCmd
{
  RutCmdType type;
} RutCmd;
#define RUT_CMD(x) ((RutCmd *)X)

typedef struct _RutTransformCmd
{
  RutCmd _parent;
  cg_matrix_t matrix;
} RutTransformCmd;
#define RUT_TRANSFORM_CMD(X) ((RutTransformCmd *)X)

typedef struct _RutPrimitiveCmd
{
  RutCmd _parent;
  cg_pipeline_t *pipeline;
  cg_primitive_t *primitive;
} RutPrimitiveCmd;
#define RUT_PRIMITIVE_CMD(X) ((RutPrimitiveCmd *)X)

typedef struct _RutTextCmd
{
  RutCmd _parent;
  PangoLayout *layout;
  cg_color_t color;
  int x;
  int y;
} RutTextCmd;
#define RUT_TEXT_CMD(X) ((RutTextCmd *)X)

typedef struct _RutRectangleCmd
{
  RutCmd _parent;
  cg_pipeline_t *pipeline;
  float width, height;
} RutRectangleCmd;
#define RUT_RECTANGLE_CMD(X) ((RutRectangleCmd *)X)

typedef struct _RutDisplayList
{
  /* PRIVATE */
  c_list_t *head;
  c_list_t *tail;
} RutDisplayList;

void
rut_display_list_unsplice (RutDisplayList *list);

void
rut_display_list_splice (c_list_t *after, RutDisplayList *sub_list);

void
rut_display_list_append (RutDisplayList *list, void *data);

c_list_t *
rut_display_list_insert_before (c_list_t *sibling,
                                 void *data);

void
rut_display_list_delete_link (c_list_t *link);

void
rut_display_list_init (RutDisplayList *list);

void
rut_display_list_destroy (RutDisplayList *list);

void
rut_display_list_paint (RutDisplayList *display_list,
                        cg_framebuffer_t *fb);

#endif /* _RUT_DISPLAY_LIST_H_ */
