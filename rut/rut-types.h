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

#ifndef _RUT_TYPES_H_
#define _RUT_TYPES_H_

#include "rut-type.h"

typedef struct _RutContext RutContext;
extern RutType rut_context_type;

typedef struct _RutShell RutShell;
extern RutType rut_shell_type;

typedef struct _RutInputRegion RutInputRegion;
extern RutType rut_input_region_type;

typedef struct _RutUIEnumValue
{
  int value;
  const char *nick;
  const char *blurb;
} RutUIEnumValue;

typedef struct _RutUIEnum
{
  const char *nick;
  const char *blurb;
  RutUIEnumValue values[];
} RutUIEnum;

typedef enum
{
  RUT_PROJECTION_PERSPECTIVE,
  RUT_PROJECTION_ORTHOGRAPHIC
} RutProjection;

/* XXX: Update this in rig.c if RutProjection is changed! */
extern RutUIEnum _rut_projection_ui_enum;

typedef struct _RutBox
{
  float x1, y1, x2, y2;
} RutBox;

typedef struct _RutRectangleInt
{
  int x;
  int y;
  int width;
  int height;
} RutRectangleInt;

typedef struct _RutVector3
{
  float x, y, z;
} RutVector3;

typedef enum _RutCullResult
{
  RUT_CULL_RESULT_IN,
  RUT_CULL_RESULT_OUT,
  RUT_CULL_RESULT_PARTIAL
} RutCullResult;

typedef enum _RutAxis
{
  RUT_AXIS_X,
  RUT_AXIS_Y,
  RUT_AXIS_Z
} RutAxis;

/* FIXME: avoid this Rig typedef being in rut/
 *
 * Obviously we shouldn't ideally have Rig typedefs in Rut but this is
 * currently required because we want RigAsset based properties
 */
typedef enum _RigAssetType {
  RIG_ASSET_TYPE_BUILTIN,
  RIG_ASSET_TYPE_TEXTURE,
  RIG_ASSET_TYPE_NORMAL_MAP,
  RIG_ASSET_TYPE_ALPHA_MASK,
  RIG_ASSET_TYPE_MESH,
  RIG_ASSET_TYPE_FONT,
} RigAssetType;

typedef struct _RutPreferredSize
{
  float natural_size;
  float minimum_size;
} RutPreferredSize;

#endif /* _RUT_TYPES_H_ */
