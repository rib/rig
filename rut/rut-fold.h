/*
 * Rut
 *
 * Copyright (C) 2013  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _RUT_FOLD_H_
#define _RUT_FOLD_H_

#include "rut-type.h"
#include "rut-box-layout.h"
#include "rut-text.h"

#include "components/rut-nine-slice.h"

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
