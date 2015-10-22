/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Intel Corporation.
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

#ifndef __CG_WEBGL_H__
#define __CG_WEBGL_H__

/* NB: this is a top-level header that can be included directly but we
 * want to be careful not to define __CG_H_INSIDE__ when this is
 * included internally while building CGlib itself since
 * __CG_H_INSIDE__ is used in headers to guard public vs private api
 * definitions
 */
#ifndef CG_COMPILATION
#define __CG_H_INSIDE__
#endif

#include <cglib/cg-device.h>
#include <cglib/cg-onscreen.h>


CG_BEGIN_DECLS

/**
 * SECTION:cg-webgl
 * @short_description: Integration api for Emscripten
 */

const char *cg_webgl_onscreen_get_id(cg_onscreen_t *onscreen);

void cg_webgl_onscreen_resize(cg_onscreen_t *onscreen,
                              int width,
                              int height);

/**
 * cg_webgl_image_t:
 *
 * A #cg_object_t that represents a HTMLImageElement that can be used
 * to asynchronously load an image.
 */
typedef struct _cg_webgl_image cg_webgl_image_t;

/**
 * cg_webgl_image_callback_t:
 * @image: A #cg_webgl_image_t pointer
 * @user_data: Application private data
 *
 * A callback type used for notifications of an image loading or of an
 * error while loading an image. These callbacks are registered using
 * cg_webgl_image_add_onload_callback() and
 * cg_webgl_add_onerror_callback().
 */
typedef void (* cg_webgl_image_callback_t)(cg_webgl_image_t *image,
                                           void *user_data);

/**
 * cg_webgl_image_new:
 * @ctx: A #CGlibContext pointer
 * @url: An image URL to load asynchronously
 *
 * This creates a HTMLImageElement that will start loading an image at
 * the given @url. To be notified when loading is complete or if there
 * has been an error loading the image you can use
 * cg_webgl_image_add_onload_callback() and
 * cg_webgl_image_onerror_callback().
 *
 * Return value: A newly allocated #cg_webgl_image_t
 */
cg_webgl_image_t *
cg_webgl_image_new(cg_device_t *dev, const char *url);

/**
 * cg_webgl_image_closure_t:
 *
 * A handle representing a registered onload or onerror callback
 * function and some associated private data.
 */
typedef struct _cg_closure cg_webgl_image_closure_t;

/**
 * cg_webgl_image_add_onload_callback:
 * @image: A #cg_webgl_image_t pointer
 * @callback: The function to call when the image has loaded
 * @user_data: Private data to pass to the callback
 * @destroy: A function to destroy the @user_data when removing the
 *           @callback or if @image is destroyed.
 *
 * Registers a @callback function that will be called when the image
 * has finished loading. One the image has loaded then you can create
 * a #CGlibTexture by calling cg_webgl_texture_2d_new_from_image().
 *
 * Return value: A #cg_webgl_image_closure_t that can be used to remove
 *               the callback using
 *               cg_webgl_image_remove_onload_callback().
 */
cg_webgl_image_closure_t *
cg_webgl_image_add_onload_callback(cg_webgl_image_t *image,
                                   cg_webgl_image_callback_t callback,
                                   void *user_data,
                                   cg_user_data_destroy_callback_t destroy);

/**
 * cg_webgl_image_remove_onload_callback:
 * @image: A #cg_webgl_image_t pointer
 * @closure: A #cg_webgl_image_closure_t returned from
 *           cg_webgl_image_add_onload_callback()
 *
 * Unregisters an onload callback from the given @image.
 */
void
cg_webgl_image_remove_onload_callback(cg_webgl_image_t *image,
                                        cg_webgl_image_closure_t *closure);

/**
 * cg_webgl_image_add_onerror_callback:
 * @image: A #cg_webgl_image_t pointer
 * @callback: The function to call if there is an error loading the
 *            image
 * @user_data: Private data to pass to the callback
 * @destroy: A function to destroy the @user_data when removing the
 *           @callback or if @image is destroyed.
 *
 * Registers a @callback function that will be called if there is an
 * error while loading the image. If this is called then you should
 * not try to create a #CGlibTexture from the image.
 *
 * Return value: A #cg_webgl_image_closure_t that can be used to remove
 *               the callback using
 *               cg_webgl_image_remove_onerror_callback().
 */
cg_webgl_image_closure_t *
cg_webgl_image_add_onerror_callback(cg_webgl_image_t *image,
                                    cg_webgl_image_callback_t callback,
                                    void *user_data,
                                    cg_user_data_destroy_callback_t destroy);

/**
 * cg_webgl_image_remove_onerror_callback:
 * @image: A #cg_webgl_image_t pointer
 * @closure: A #cg_webgl_image_closure_t returned from
 *           cg_webgl_image_add_onerror_callback()
 *
 * Unregisters an onerror callback from the given @image.
 */
void
cg_webgl_image_remove_onerror_callback(cg_webgl_image_t *image,
                                       cg_webgl_image_closure_t *closure);

/**
 * cg_webgl_image_get_width:
 * @image: A #cg_webgl_image_t pointer
 *
 * Queries the width of the given @image, which is only known once the
 * image has been loaded.
 *
 * Return value: The width of the given @image or 0 if the image has
 *               not yet loaded
 */
int
cg_webgl_image_get_width(cg_webgl_image_t *image);

/**
 * cg_webgl_image_get_height:
 * @image: A #cg_webgl_image_t pointer
 *
 * Queries the height of the given @image, which is only known once the
 * image has been loaded.
 *
 * Return value: The width of the given @image or 0 if the image has
 *               not yet loaded
 */
int
cg_webgl_image_get_height(cg_webgl_image_t *image);

/**
 * cg_webgl_texture_2d_new_from_image:
 * @dev: A #cg_device_t pointer
 * @image: A #cg_webgl_image_t that has successfully been loaded
 *
 * Creates a #cg_texture_2d_t texture from a loaded @image element.
 *
 * The storage for the texture is not allocated before this function
 * returns. You can call cg_texture_allocate() to explicitly
 * allocate the underlying storage or preferably let CGlib
 * automatically allocate storage lazily when it may know more about
 * how the texture is being used and can optimize how it is allocated.
 *
 * The texture is still configurable until it has been allocated so
 * for example you can influence the internal format of the texture
 * using cg_texture_set_components() and
 * cg_texture_set_premultiplied().
 *
 * <note>Many GPUs only support power of two sizes for #cg_texture_2d_t
 * textures. You can check support for non power of two textures by
 * checking for the %CG_FEATURE_ID_TEXTURE_NPOT feature via
 * cg_has_feature().</note>
 *
 * Returns: (transfer full): A newly allocated #cg_texture_2d_t
 */
cg_texture_2d_t *
cg_webgl_texture_2d_new_from_image(cg_device_t *dev, cg_webgl_image_t *image);

CG_END_DECLS

#ifndef CG_COMPILATION
#undef __CG_H_INSIDE__
#endif

#endif /* __CG_WEBGL_H__ */
