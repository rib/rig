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

#pragma once

#include "rut-type.h"

typedef struct _rut_shell_t rut_shell_t;
extern rut_type_t rut_shell_type;

typedef struct _rut_input_region_t rut_input_region_t;
extern rut_type_t rut_input_region_type;

typedef struct _rut_ui_enum_value_t {
    int value;
    const char *nick;
    const char *blurb;
} rut_ui_enum_value_t;

typedef struct _rut_ui_enum_t {
    const char *nick;
    const char *blurb;
    rut_ui_enum_value_t values[];
} rut_ui_enum_t;

typedef enum {
    RUT_PROJECTION_PERSPECTIVE,
    RUT_PROJECTION_ASYMMETRIC_PERSPECTIVE,
    RUT_PROJECTION_ORTHOGRAPHIC,
    RUT_PROJECTION_NDC
} rut_projection_t;

/* XXX: Update this in rig.c if rut_projection_t is changed! */
extern rut_ui_enum_t _rut_projection_ui_enum;

typedef struct _rut_box_t {
    float x1, y1, x2, y2;
} rut_box_t;

typedef struct _rut_vector3_t {
    float x, y, z;
} rut_vector3_t;

typedef enum _rut_cull_result_t {
    RUT_CULL_RESULT_IN,
    RUT_CULL_RESULT_OUT,
    RUT_CULL_RESULT_PARTIAL
} rut_cull_result_t;

typedef enum _rut_axis_t {
    RUT_AXIS_X,
    RUT_AXIS_Y,
    RUT_AXIS_Z
} rut_axis_t;

/* FIXME: avoid this Rig typedef being in rut/
 *
 * Obviously we shouldn't ideally have Rig typedefs in Rut but this is
 * currently required because we want rig_asset_t based properties
 */
typedef enum _rig_asset_type_t {
    RIG_ASSET_TYPE_BUILTIN,
    RIG_ASSET_TYPE_TEXTURE,
    RIG_ASSET_TYPE_NORMAL_MAP,
    RIG_ASSET_TYPE_ALPHA_MASK,
    RIG_ASSET_TYPE_MESH,
    RIG_ASSET_TYPE_FONT,
} rig_asset_type_t;

typedef struct _rut_preferred_size_t {
    float natural_size;
    float minimum_size;
} rut_preferred_size_t;

#ifdef __GNUC__
#define rut_container_of(ptr, sample, member)                                  \
    (__typeof__(sample))((char *)(ptr) -                                       \
                         ((char *)&(sample)->member - (char *)(sample)))
#else
#define rut_container_of(ptr, sample, member)                                  \
    (void *)((char *)(ptr) - ((char *)&(sample)->member - (char *)(sample)))
#endif
