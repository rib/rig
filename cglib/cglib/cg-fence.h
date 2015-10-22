/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Collabora Ltd.
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

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_FENCE_H__
#define __CG_FENCE_H__

#include <cglib/cg-types.h>
#include <cglib/cg-framebuffer.h>

/**
 * SECTION:cg-fence
 * @short_description: Functions for notification of command completion
 *
 * CGlib allows notification of GPU command completion; users may mark
 * points in the GPU command stream and receive notification when the GPU
 * has executed to that point.
 */

/**
 * cg_fence_t:
 *
 * An opaque object representing a fence. This type is currently
 * unused but in the future may be used to pass extra information
 * about the fence completion.
 *
 * Stability: Unstable
 */
typedef struct _cg_fence_t cg_fence_t;

/**
 * cg_fence_callback_t:
 * @fence: Unused. In the future this parameter may be used to pass
 *   extra information about the fence completion but for now it
 *   should be ignored.
 * @user_data: The private data passed to cg_framebuffer_add_fence_callback()
 *
 * The callback prototype used with
 * cg_framebuffer_add_fence_callback() for notification of GPU
 * command completion.
 *
 * Stability: Unstable
 */
typedef void (*cg_fence_callback_t)(cg_fence_t *fence, void *user_data);

/**
 * cg_fence_closure_t:
 *
 * An opaque type representing one future callback to be made when the
 * GPU command stream has passed a certain point.
 *
 * Stability: Unstable
 */
typedef struct _cg_fence_closure_t cg_fence_closure_t;

/**
 * cg_frame_closure_get_user_data:
 * @closure: A #cg_fence_closure_t returned from cg_framebuffer_add_fence()
 *
 * Returns the user_data submitted to cg_framebuffer_add_fence() which
 * returned a given #cg_fence_closure_t.
 *
 * Stability: Unstable
 */
void *cg_fence_closure_get_user_data(cg_fence_closure_t *closure);

/**
 * cg_framebuffer_add_fence_callback:
 * @framebuffer: The #cg_framebuffer_t the commands have been submitted to
 * @callback: (scope notified): A #cg_fence_callback_t to be called when
 *            all commands submitted to CGlib have been executed
 * @user_data: (closure): Private data that will be passed to the callback
 *
 * Calls the provided callback when all previously-submitted commands have
 * been executed by the GPU.
 *
 * Returns non-NULL if the fence succeeded, or %NULL if it was unable to
 * be inserted and the callback will never be called.  The user does not
 * need to free the closure; it will be freed automatically when the
 * callback is called, or cancelled.
 *
 * Stability: Unstable
 */
cg_fence_closure_t *
cg_framebuffer_add_fence_callback(cg_framebuffer_t *framebuffer,
                                  cg_fence_callback_t callback,
                                  void *user_data);

/**
 * cg_framebuffer_cancel_fence_callback:
 * @framebuffer: The #cg_framebuffer_t the commands were submitted to
 * @closure: The #cg_fence_closure_t returned from
 *           cg_framebuffer_add_fence_callback()
 *
 * Removes a fence previously submitted with
 * cg_framebuffer_add_fence_callback(); the callback will not be
 * called.
 *
 * Stability: Unstable
 */
void cg_framebuffer_cancel_fence_callback(cg_framebuffer_t *framebuffer,
                                          cg_fence_closure_t *closure);

#endif /* __CG_FENCE_H__ */
