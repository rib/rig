/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2011 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.og/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * Contributor(s):
 *      Robert Bragg <robert@linux.intel.com>
 */

#ifndef _RUT_GRADIENT_H_
#define _RUT_GRADIENT_H_

#include <cogl/cogl.h>

#include "rut-object.h"
#include "rut-shell.h"

typedef enum {
    RUT_EXTEND_NONE,
    RUT_EXTEND_REPEAT,
    RUT_EXTEND_REFLECT,
    RUT_EXTEND_PAD
} rut_extend_t;

typedef struct _rut_gradient_stop {
    cg_color_t color;
    float offset;
} rut_gradient_stop_t;

struct _rut_linear_gradient {
    rut_object_base_t _base;

    rut_extend_t extend_mode;

    /* NB: these stops will have premultiplied colors */
    int	n_stops;
    rut_gradient_stop_t *stops;

    cg_texture_2d_t *texture;
    float translate_x;
    float scale_x;
};


typedef struct _rut_linear_gradient rut_linear_gradient_t;

extern rut_type_t rut_linear_gradient_type;

/* XXX: the input stop colors should be unpremultiplied */
rut_linear_gradient_t *
rut_linear_gradient_new(rut_shell_t *shell,
                        rut_extend_t extend_mode,
                        int n_stops,
                        const rut_gradient_stop_t *stops);

bool
rut_linear_gradient_equal(const void *key_a, const void *key_b);

#endif /* _RUT_GRADIENT_H_ */
