/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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
 *
 *
 */

#include <cglib-config.h>

#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "cg-i18n-private.h"
#include "cg-debug.h"
#include "cg-util.h"
#include "cg-device-private.h"
#include "cg-pipeline-private.h"
#include "cg-pipeline-opengl-private.h"
#include "cg-winsys-private.h"
#include "cg-framebuffer-private.h"
#include "cg-bitmap-private.h"
#include "cg-texture-private.h"
#include "cg-texture-driver.h"
#include "cg-attribute-private.h"
#include "cg-framebuffer-private.h"
#include "cg-renderer-private.h"
#include "cg-config-private.h"
#include "cg-private.h"
#include "cg-offscreen.h"

bool
_cg_check_extension(const char *name, char *const *ext)
{
    while (*ext)
        if (!strcmp(name, *ext))
            return true;
        else
            ext++;

    return false;
}

bool
cg_has_feature(cg_device_t *dev, cg_feature_id_t feature)
{
    return CG_FLAGS_GET(dev->features, feature);
}

bool
cg_has_features(cg_device_t *dev, ...)
{
    va_list args;
    cg_feature_id_t feature;

    va_start(args, dev);
    while ((feature = va_arg(args, cg_feature_id_t)))
        if (!cg_has_feature(dev, feature))
            return false;
    va_end(args);

    return true;
}

void
cg_foreach_feature(cg_device_t *dev,
                   cg_feature_callback_t callback,
                   void *user_data)
{
    int i;
    for (i = 0; i < _CG_N_FEATURE_IDS; i++)
        if (CG_FLAGS_GET(dev->features, i))
            callback(i, user_data);
}

void
_cg_flush(cg_device_t *dev)
{
    c_llist_t *l;

    for (l = dev->framebuffers; l; l = l->next)
        _cg_framebuffer_flush(l->data);
}

uint32_t
_cg_driver_error_domain(void)
{
    return c_quark_from_static_string("cg-driver-error-quark");
}

/* Scale from OpenGL normalized device coordinates (ranging from -1 to 1)
 * to CGlib window/framebuffer coordinates (ranging from 0 to buffer-size) with
 * (0,0) being top left. */
#define VIEWPORT_TRANSFORM_X(x, vp_origin_x, vp_width)                         \
    ((((x) + 1.0) * ((vp_width) / 2.0)) + (vp_origin_x))
/* Note: for Y we first flip all coordinates around the X axis while in
 * normalized device coodinates */
#define VIEWPORT_TRANSFORM_Y(y, vp_origin_y, vp_height)                        \
    ((((-(y)) + 1.0) * ((vp_height) / 2.0)) + (vp_origin_y))

/* Transform a homogeneous vertex position from model space to CGlib
 * window coordinates (with 0,0 being top left) */
void
_cg_transform_point(const c_matrix_t *matrix_mv,
                    const c_matrix_t *matrix_p,
                    const float *viewport,
                    float *x,
                    float *y)
{
    float z = 0;
    float w = 1;

    /* Apply the modelview matrix transform */
    c_matrix_transform_point(matrix_mv, x, y, &z, &w);

    /* Apply the projection matrix transform */
    c_matrix_transform_point(matrix_p, x, y, &z, &w);

    /* Perform perspective division */
    *x /= w;
    *y /= w;

    /* Apply viewport transform */
    *x = VIEWPORT_TRANSFORM_X(*x, viewport[0], viewport[2]);
    *y = VIEWPORT_TRANSFORM_Y(*y, viewport[1], viewport[3]);
}

#undef VIEWPORT_TRANSFORM_X
#undef VIEWPORT_TRANSFORM_Y

uint32_t
_cg_system_error_domain(void)
{
    return c_quark_from_static_string("cg-system-error-quark");
}

void
_cg_init(void)
{
    static bool initialized = false;

    if (initialized == false) {
#if defined(ENABLE_NLS) && defined(USE_GLIB)
        bindtextdomain(GETTEXT_PACKAGE, CG_LOCALEDIR);
        bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
#endif

        _cg_config_read();
        _cg_debug_check_environment();
        initialized = true;
    }
}
