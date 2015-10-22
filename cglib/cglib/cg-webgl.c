/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2015 Intel Corporation.
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

#include "cg-webgl-private.h"

static void _cg_webgl_image_free(cg_webgl_image_t *image);

CG_OBJECT_DEFINE(WebGLImage, webgl_image);

static void
_cg_webgl_image_free(cg_webgl_image_t *image)
{
    _cg_closure_list_disconnect_all(&image->onload_closures);
    _cg_closure_list_disconnect_all(&image->onerror_closures);

    _cg_webgl_image_destroy(image->image_handle);

    c_slice_free(cg_webgl_image_t, image);
}

static void
onload(cg_webgl_image_handle_t image_handle,
       void *user_data)
{
    cg_webgl_image_t *image = user_data;

    _cg_closure_list_invoke(&image->onload_closures,
                            cg_webgl_image_callback_t,
                            image);
}

static void
onerror(cg_webgl_image_handle_t image_handle,
        void *user_data)
{
    cg_webgl_image_t *image = user_data;

    _cg_closure_list_invoke(&image->onerror_closures,
                              cg_webgl_image_callback_t,
                              image);
}

cg_webgl_image_t *
cg_webgl_image_new(cg_device_t *dev, const char *url)
{
    cg_webgl_image_t *image = c_slice_new0 (cg_webgl_image_t);

    c_list_init(&image->onload_closures);
    c_list_init(&image->onerror_closures);

    image->image_handle =
        _cg_webgl_image_create(url, onload, onerror, image);

    return _cg_webgl_image_object_new(image);
}

cg_webgl_image_closure_t *
cg_webgl_image_add_onload_callback(cg_webgl_image_t *image,
                                   cg_webgl_image_callback_t callback,
                                   void *user_data,
                                   cg_user_data_destroy_callback_t destroy)
{
    return _cg_closure_list_add(&image->onload_closures,
                                callback,
                                user_data,
                                destroy);
}

void
cg_webgl_image_remove_onload_callback(cg_webgl_image_t *image,
                                      cg_webgl_image_closure_t *closure)
{
    _cg_closure_disconnect(closure);
}

cg_webgl_image_closure_t *
cg_webgl_image_add_onerror_callback(cg_webgl_image_t *image,
                                    cg_webgl_image_callback_t callback,
                                    void *user_data,
                                    cg_user_data_destroy_callback_t destroy)
{
    return _cg_closure_list_add(&image->onerror_closures,
                                callback,
                                user_data,
                                destroy);
}

void
cg_webgl_image_remove_onerror_callback(cg_webgl_image_t *image,
                                       cg_webgl_image_closure_t *closure)
{
    _cg_closure_disconnect (closure);
}

int
cg_webgl_image_get_width(cg_webgl_image_t *image)
{
    return _cg_webgl_image_get_width(image->image_handle);
}

int
cg_webgl_image_get_height(cg_webgl_image_t *image)
{
    return _cg_webgl_image_get_height(image->image_handle);
}
