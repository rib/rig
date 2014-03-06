/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013  Intel Corporation
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

#ifndef _RUT_FOLD_H_
#define _RUT_FOLD_H_

#include "rut-type.h"
#include "rut-box-layout.h"
#include "rut-text.h"

#include "rut-nine-slice.h"

extern RutType rut_fold_type;


enum {
  RUT_FOLD_PROP_LABEL,
  RUT_FOLD_N_PROPS
};

typedef struct _RutFold
{
  RutObjectBase _base;

  RutContext *context;

  RutBoxLayout *vbox;
  RutBoxLayout *header_hbox_right;

  RutText *label;
  RutFixed *fold_icon_shim;
  RutNineSlice *fold_up_icon;
  RutNineSlice *fold_down_icon;

  RutInputRegion *input_region;

  CoglBool folded;

  RutObject *child;
  RutObject *header_child;

  RutGraphableProps graphable;

  RutIntrospectableProps introspectable;
  RutProperty properties[RUT_FOLD_N_PROPS];

} RutFold;

RutFold *
rut_fold_new (RutContext *ctx, const char *label);

void
rut_fold_set_child (RutFold *fold, RutObject *child);

void
rut_fold_set_header_child (RutFold *fold, RutObject *child);

void
rut_fold_set_folded (RutFold *fold, CoglBool folded);

void
rut_fold_set_folder_color (RutFold *fold, const CoglColor *color);

void
rut_fold_set_label_color (RutFold *fold, const CoglColor *color);

void
rut_fold_set_label (RutObject *object, const char *label);

const char *
rut_fold_get_label (RutObject *object);

void
rut_fold_set_font_name (RutFold *fold, const char *font);

#endif /* _RUT_FOLD_H_ */
