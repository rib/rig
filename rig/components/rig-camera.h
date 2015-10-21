/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012  Intel Corporation
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

#ifndef __RIG_CAMERA_H__
#define __RIG_CAMERA_H__

#include <cglib/cglib.h>

#include <rut.h>

#include "rig-types.h"

typedef struct _rig_camera_t rig_camera_t;

#include "rig-entity.h"

/* NB: consider changes to _rig_camera_copy if adding
 * properties, or making existing properties
 * introspectable */
enum {
    RIG_CAMERA_PROP_MODE,
    RIG_CAMERA_PROP_VIEWPORT_X,
    RIG_CAMERA_PROP_VIEWPORT_Y,
    RIG_CAMERA_PROP_VIEWPORT_WIDTH,
    RIG_CAMERA_PROP_VIEWPORT_HEIGHT,
    RIG_CAMERA_PROP_ORTHO,
    RIG_CAMERA_PROP_FOV,
    RIG_CAMERA_PROP_NEAR,
    RIG_CAMERA_PROP_FAR,
    RIG_CAMERA_PROP_ZOOM,
    RIG_CAMERA_PROP_BG_COLOR,
    RIG_CAMERA_PROP_CLEAR,
    RIG_CAMERA_PROP_FOCAL_DISTANCE,
    RIG_CAMERA_PROP_DEPTH_OF_FIELD,
    RIG_CAMERA_N_PROPS
};

extern rut_type_t rig_camera_type;

rig_camera_t *rig_camera_new(rig_engine_t *engine,
                             float width,
                             float height,
                             cg_framebuffer_t *framebuffer); /* may be NULL */

#endif /* __RIG_CAMERA_H__ */
