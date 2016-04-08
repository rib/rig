/*
 * Rut
 *
 * Rig Utilities
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

#ifndef _RUT_IMAGE_H_
#define _RUT_IMAGE_H_

#include "rig-property.h"

extern rut_type_t rut_image_type;

typedef struct _rut_image_t rut_image_t;

typedef enum {
    /* Don't scale the image */
    RUT_IMAGE_DRAW_MODE_1_TO_1,
    /* Fills the widget with repeats of the image */
    RUT_IMAGE_DRAW_MODE_REPEAT,
    /* Scales the image to fill the size of the widget */
    RUT_IMAGE_DRAW_MODE_SCALE,
    /* Scales the image to fill the size of the widget as much as
     * possible without breaking the aspect ratio */
    RUT_IMAGE_DRAW_MODE_SCALE_WITH_ASPECT_RATIO
} rut_image_draw_mode_t;

rut_image_t *rut_image_new(rut_shell_t *shell, cg_texture_t *texture);

void rut_image_set_draw_mode(rut_image_t *image,
                             rut_image_draw_mode_t draw_mode);

#endif /* _RUT_IMAGE_H_ */
