/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011,2012,2013 Intel Corporation.
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
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_ONSCREEN_H
#define __CG_ONSCREEN_H

#include <cglib/cg-device.h>
#include <cglib/cg-framebuffer.h>
#include <cglib/cg-frame-info.h>
#include <cglib/cg-object.h>

CG_BEGIN_DECLS

typedef struct _cg_onscreen_t cg_onscreen_t;
#define CG_ONSCREEN(X) ((cg_onscreen_t *)(X))

/**
 * cg_onscreen_new: (constructor)
 * @dev: A #cg_device_t
 * @width: The desired framebuffer width
 * @height: The desired framebuffer height
 *
 * Instantiates an "unallocated" #cg_onscreen_t framebuffer that may be
 * configured before later being allocated, either implicitly when
 * it is first used or explicitly via cg_framebuffer_allocate().
 *
 * Return value: (transfer full): A newly instantiated #cg_onscreen_t
 * framebuffer
 * Stability: unstable
 */
cg_onscreen_t *cg_onscreen_new(cg_device_t *dev, int width, int height);

#ifdef CG_HAS_X11_SUPPORT
typedef void (*cg_onscreen_x11_mask_callback_t)(cg_onscreen_t *onscreen,
                                                uint32_t event_mask,
                                                void *user_data);

/**
 * cg_x11_onscreen_set_foreign_window_xid:
 * @onscreen: The unallocated framebuffer to associated with an X
 *            window.
 * @xid: The XID of an existing X window
 * @update: A callback that notifies of updates to what CGlib requires
 *          to be in the core X protocol event mask.
 * @user_data: user data passed to @update
 *
 * Ideally we would recommend that you let CGlib be responsible for
 * creating any X window required to back an onscreen framebuffer but
 * if you really need to target a window created manually this
 * function can be called before @onscreen has been allocated to set a
 * foreign XID for your existing X window.
 *
 * Since CGlib needs, for example, to track changes to the size of an X
 * window it requires that certain events be selected for via the core
 * X protocol. This requirement may also be changed asynchronously so
 * you must pass in an @update callback to inform you of CGlib's
 * required event mask.
 *
 * For example if you are using Xlib you could use this API roughly
 * as follows:
 * [{
 * static void
 * my_update_cg_x11_event_mask (cg_onscreen_t *onscreen,
 *                                uint32_t event_mask,
 *                                void *user_data)
 * {
 *   XSetWindowAttributes attrs;
 *   MyData *data = user_data;
 *   attrs.event_mask = event_mask | data->my_event_mask;
 *   XChangeWindowAttributes (data->xdpy,
 *                            data->xwin,
 *                            CWEventMask,
 *                            &attrs);
 * }
 *
 * {
 *   *snip*
 *   cg_x11_onscreen_set_foreign_window_xid (onscreen,
 *                                             data->xwin,
 *                                             my_update_cg_x11_event_mask,
 *                                             data);
 *   *snip*
 * }
 * }]
 *
 * Stability: Unstable
 */
void
cg_x11_onscreen_set_foreign_window_xid(cg_onscreen_t *onscreen,
                                       uint32_t xid,
                                       cg_onscreen_x11_mask_callback_t update,
                                       void *user_data);

/**
 * cg_x11_onscreen_get_window_xid:
 * @onscreen: A #cg_onscreen_t framebuffer
 *
 * Assuming you know the given @onscreen framebuffer is based on an x11 window
 * this queries the XID of that window. If
 * cg_x11_onscreen_set_foreign_window_xid() was previously called then it
 * will return that same XID otherwise it will be the XID of a window CGlib
 * created internally. If the window has not been allocated yet and a foreign
 * xid has not been set then it's undefined what value will be returned.
 *
 * It's undefined what this function does if called when not using an x11 based
 * renderer.
 *
 * Stability: unstable
 */
uint32_t cg_x11_onscreen_get_window_xid(cg_onscreen_t *onscreen);

/**
 * cg_x11_onscreen_get_visual_xid:
 * @onscreen: A #cg_onscreen_t framebuffer
 * @error: A #cg_error_t to catch exceptional errors
 *
 * Assuming your display is X11 based, then given a an @onscreen
 * framebuffer (which may not be allocated yet) this returns an X
 * Visual XID which is compatible with the @onscreen framebuffer's
 * current configuration. This Visual could then be used to create an
 * X Window that would be compatible with
 * cg_x11_onscreen_set_foreign_window_xid().
 *
 * It's undefined what this function does if called when not using an
 * x11 based renderer.
 *
 * Return value: A Visual XID corresponding to the current @onscreen
 *               framebuffer configuration, or %0 on error (and
 *               @error will be updated)
 */
uint32_t cg_x11_onscreen_get_visual_xid(cg_onscreen_t *onscreen,
                                        cg_error_t **error);
#endif /* CG_HAS_X11_SUPPORT */

#ifdef CG_HAS_WIN32_SUPPORT
/**
 * cg_win32_onscreen_set_foreign_window:
 * @onscreen: A #cg_onscreen_t framebuffer
 * @hwnd: A win32 window handle
 *
 * Ideally we would recommend that you let CGlib be responsible for
 * creating any window required to back an onscreen framebuffer but
 * if you really need to target a window created manually this
 * function can be called before @onscreen has been allocated to set a
 * foreign XID for your existing X window.
 *
 * Stability: unstable
 */
void cg_win32_onscreen_set_foreign_window(cg_onscreen_t *onscreen, HWND hwnd);

/**
 * cg_win32_onscreen_get_window:
 * @onscreen: A #cg_onscreen_t framebuffer
 *
 * Queries the internally created window HWND backing the given @onscreen
 * framebuffer.  If cg_win32_onscreen_set_foreign_window() has been used then
 * it will return the same handle set with that API.
 *
 * Stability: unstable
 */
HWND cg_win32_onscreen_get_window(cg_onscreen_t *onscreen);
#endif /* CG_HAS_WIN32_SUPPORT */

#if defined(CG_HAS_EGL_PLATFORM_WAYLAND_SUPPORT)
struct wl_surface *cg_wayland_onscreen_get_surface(cg_onscreen_t *onscreen);
struct wl_shell_surface *
cg_wayland_onscreen_get_shell_surface(cg_onscreen_t *onscreen);

/**
 * cg_wayland_onscreen_set_foreign_surface:
 * @onscreen: An unallocated framebuffer.
 * @surface A Wayland surface to associate with the @onscreen.
 *
 * Allows you to explicitly notify CGlib of an existing Wayland surface to use,
 * which prevents CGlib from allocating a surface and shell surface for the
 * @onscreen. An allocated surface will not be destroyed when the @onscreen is
 * freed.
 *
 * This function must be called before @onscreen is allocated.
 *
 * Stability: unstable
 */
void cg_wayland_onscreen_set_foreign_surface(cg_onscreen_t *onscreen,
                                             struct wl_surface *surface);

/**
 * cg_wayland_onscreen_resize:
 * @onscreen: A #cg_onscreen_t framebuffer
 * @width: The desired width of the framebuffer
 * @height: The desired height of the framebuffer
 * @offset_x: A relative x offset for the new framebuffer
 * @offset_y: A relative y offset for the new framebuffer
 *
 * Resizes the backbuffer of the given @onscreen framebuffer to the
 * given size. Since a buffer is usually conceptually scaled with a
 * center point the @offset_x and @offset_y arguments allow the newly
 * allocated buffer to be positioned relative to the old buffer size.
 *
 * For example a buffer that is being resized by moving the bottom right
 * corner, and the top left corner is remaining static would use x and y
 * offsets of (0, 0) since the top-left of the new buffer should have the same
 * position as the old buffer. If the center of the old buffer is being zoomed
 * into then all the corners of the new buffer move out from the center and the
 * x
 * and y offsets would be (-half_x_size_increase, -half_y_size_increase) where
 * x/y_size_increase is how many pixels bigger the buffer is on the x and y
 * axis.
 *
 * Note that if some drawing commands have been applied to the
 * framebuffer since the last swap buffers then the resize will be
 * queued and will only take effect in the next swap buffers.
 *
 * If multiple calls to cg_wayland_onscreen_resize() get queued
 * before the next swap buffers request then the relative x and y
 * offsets accumulate instead of being replaced. The @width and
 * @height values superseed the old values.
 *
 * Stability: unstable
 */
void cg_wayland_onscreen_resize(
    cg_onscreen_t *onscreen, int width, int height, int offset_x, int offset_y);
#endif /* CG_HAS_EGL_PLATFORM_WAYLAND_SUPPORT */

#if defined(CG_HAS_EGL_PLATFORM_ANDROID_SUPPORT)
/**
 * cg_android_onscreen_update_size:
 * @onscreen: A #cg_onscreen_t framebuffer
 * @width: The desired width of the framebuffer
 * @height: The desired height of the framebuffer
 *
 * Allows applications to notify CGlib when an Android window has been
 * resized. Android applications get notified by the window system if
 * an onscreen window has been resized and since CGlib is not hooked
 * into these events directly the application needs to inform CGlib
 * when the framebuffer size has changed.
 *
 * Stability: unstable
 */
void
cg_android_onscreen_update_size(cg_onscreen_t *onscreen, int width, int height);
#endif /* CG_HAS_EGL_PLATFORM_ANDROID_SUPPORT */

/**
 * cg_onscreen_set_swap_throttled:
 * @onscreen: A #cg_onscreen_t framebuffer
 * @throttled: Whether swap throttling is wanted or not.
 *
 * Requests that the given @onscreen framebuffer should have swap buffer
 * requests (made using cg_onscreen_swap_buffers()) throttled either by a
 * displays vblank period or perhaps some other mechanism in a composited
 * environment.
 *
 * Stability: unstable
 */
void cg_onscreen_set_swap_throttled(cg_onscreen_t *onscreen, bool throttled);

/**
 * cg_onscreen_show:
 * @onscreen: The onscreen framebuffer to make visible
 *
 * This requests to make @onscreen visible to the user.
 *
 * Actually the precise semantics of this function depend on the
 * window system currently in use, and if you don't have a
 * multi-windowining system this function may in-fact do nothing.
 *
 * This function will implicitly allocate the given @onscreen
 * framebuffer before showing it if it hasn't already been allocated.
 *
 * When using the Wayland winsys calling this will set the surface to
 * a toplevel type which will make it appear. If the application wants
 * to set a different type for the surface, it can avoid calling
 * cg_onscreen_show() and set its own type directly with the Wayland
 * client API via cg_wayland_onscreen_get_surface().
 *
 * <note>Since CGlib doesn't explicitly track the visibility status of
 * onscreen framebuffers it wont try to avoid redundant window system
 * requests e.g. to show an already visible window. This also means
 * that it's acceptable to alternatively use native APIs to show and
 * hide windows without confusing CGlib.</note>
 *
 * Stability: Unstable
 */
void cg_onscreen_show(cg_onscreen_t *onscreen);

/**
 * cg_onscreen_hide:
 * @onscreen: The onscreen framebuffer to make invisible
 *
 * This requests to make @onscreen invisible to the user.
 *
 * Actually the precise semantics of this function depend on the
 * window system currently in use, and if you don't have a
 * multi-windowining system this function may in-fact do nothing.
 *
 * This function does not implicitly allocate the given @onscreen
 * framebuffer before hiding it.
 *
 * <note>Since CGlib doesn't explicitly track the visibility status of
 * onscreen framebuffers it wont try to avoid redundant window system
 * requests e.g. to show an already visible window. This also means
 * that it's acceptable to alternatively use native APIs to show and
 * hide windows without confusing CGlib.</note>
 *
 * Stability: Unstable
 */
void cg_onscreen_hide(cg_onscreen_t *onscreen);

/**
 * cg_onscreen_swap_buffers:
 * @onscreen: A #cg_onscreen_t framebuffer
 *
 * Swaps the current back buffer being rendered too, to the front for display.
 *
 * This function also implicitly discards the contents of the color, depth and
 * stencil buffers as if cg_framebuffer_discard_buffers() were used. The
 * significance of the discard is that you should not expect to be able to
 * start a new frame that incrementally builds on the contents of the previous
 * frame.
 *
 * <note>It is highly recommended that applications use
 * cg_onscreen_swap_buffers_with_damage() instead whenever possible
 * and also use the cg_onscreen_get_buffer_age() api so they can
 * perform incremental updates to older buffers instead of having to
 * render a full buffer for every frame.</note>
 *
 * Stability: unstable
 */
void cg_onscreen_swap_buffers(cg_onscreen_t *onscreen);

/**
 * cg_onscreen_get_buffer_age:
 * @onscreen: A #cg_onscreen_t framebuffer
 *
 * Gets the current age of the buffer contents.
 *
 * This function allows applications to query the age of the current
 * back buffer contents for a #cg_onscreen_t as the number of frames
 * elapsed since the contents were most recently defined.
 *
 * These age values exposes enough information to applications about
 * how CGlib internally manages back buffers to allow applications to
 * re-use the contents of old frames and minimize how much must be
 * redrawn for the next frame.
 *
 * The back buffer contents can either be reported as invalid (has an
 * age of 0) or it may be reported to be the same contents as from n
 * frames prior to the current frame.
 *
 * The queried value remains valid until the next buffer swap.
 *
 * <note>One caveat is that under X11 the buffer age does not reflect
 * changes to buffer contents caused by the window systems. X11
 * applications must track Expose events to determine what buffer
 * regions need to additionally be repaired each frame.</note>
 *
 * The recommended way to take advantage of this buffer age api is to
 * build up a circular buffer of length 3 for tracking damage regions
 * over the last 3 frames and when starting a new frame look at the
 * age of the buffer and combine the damage regions for the current
 * frame with the damage regions of previous @age frames so you know
 * everything that must be redrawn to update the old contents for the
 * new frame.
 *
 * <note>If the system doesn't not support being able to track the age
 * of back buffers then this function will always return 0 which
 * implies that the contents are undefined.</note>
 *
 * Return value: The age of the buffer contents or 0 when the buffer
 *               contents are undefined.
 *
 * Stability: stable
 */
int cg_onscreen_get_buffer_age(cg_onscreen_t *onscreen);

/**
 * cg_onscreen_swap_buffers_with_damage:
 * @onscreen: A #cg_onscreen_t framebuffer
 * @rectangles: An array of integer 4-tuples representing damaged
 *              rectangles as (x, y, width, height) tuples.
 * @n_rectangles: The number of 4-tuples to be read from @rectangles
 *
 * Swaps the current back buffer being rendered too, to the front for
 * display and provides information to any system compositor about
 * what regions of the buffer have changed (damage) with respect to
 * the last swapped buffer.
 *
 * This function has the same semantics as
 * cg_framebuffer_swap_buffers() except that it additionally allows
 * applications to pass a list of damaged rectangles which may be
 * passed on to a compositor so that it can minimize how much of the
 * screen is redrawn in response to this applications newly swapped
 * front buffer.
 *
 * For example if your application is only animating a small object in
 * the corner of the screen and everything else is remaining static
 * then it can help the compositor to know that only the bottom right
 * corner of your newly swapped buffer has really changed with respect
 * to your previously swapped front buffer.
 *
 * If @n_rectangles is 0 then the whole buffer will implicitly be
 * reported as damaged as if cg_onscreen_swap_buffers() had been
 * called.
 *
 * This function also implicitly discards the contents of the color,
 * depth and stencil buffers as if cg_framebuffer_discard_buffers()
 * were used. The significance of the discard is that you should not
 * expect to be able to start a new frame that incrementally builds on
 * the contents of the previous frame. If you want to perform
 * incremental updates to older back buffers then please refer to the
 * cg_onscreen_get_buffer_age() api.
 *
 * Whenever possible it is recommended that applications use this
 * function instead of cg_onscreen_swap_buffers() to improve
 * performance when running under a compositor.
 *
 * <note>It is highly recommended to use this API in conjunction with
 * the cg_onscreen_get_buffer_age() api so that your application can
 * perform incremental rendering based on old back buffers.</note>
 *
 * Stability: unstable
 */
void cg_onscreen_swap_buffers_with_damage(cg_onscreen_t *onscreen,
                                          const int *rectangles,
                                          int n_rectangles);

/**
 * cg_onscreen_swap_region:
 * @onscreen: A #cg_onscreen_t framebuffer
 * @rectangles: An array of integer 4-tuples representing rectangles as
 *              (x, y, width, height) tuples.
 * @n_rectangles: The number of 4-tuples to be read from @rectangles
 *
 * Swaps a region of the back buffer being rendered too, to the front for
 * display.  @rectangles represents the region as array of @n_rectangles each
 * defined by 4 sequential (x, y, width, height) integers.
 *
 * This function also implicitly discards the contents of the color, depth and
 * stencil buffers as if cg_framebuffer_discard_buffers() were used. The
 * significance of the discard is that you should not expect to be able to
 * start a new frame that incrementally builds on the contents of the previous
 * frame.
 *
 * Stability: unstable
 */
void cg_onscreen_swap_region(cg_onscreen_t *onscreen,
                             const int *rectangles,
                             int n_rectangles);

/**
 * cg_frame_event_t:
 * @CG_FRAME_EVENT_SYNC: Notifies that the system compositor has
 *                         acknowledged a frame and is ready for a
 *                         new frame to be created.
 * @CG_FRAME_EVENT_COMPLETE: Notifies that a frame has ended. This
 *                             is a good time for applications to
 *                             collect statistics about the frame
 *                             since the #cg_frame_info_t should hold
 *                             the most data at this point. No other
 *                             events should be expected after a
 *                             @CG_FRAME_EVENT_COMPLETE event.
 *
 * Identifiers that are passed to #cg_frame_callback_t functions
 * (registered using cg_onscreen_add_frame_callback()) that
 * mark the progression of a frame in some way which usually
 * means that new information will have been accumulated in the
 * frame's corresponding #cg_frame_info_t object.
 *
 * The last event that will be sent for a frame will be a
 * @CG_FRAME_EVENT_COMPLETE event and so these are a good
 * opportunity to collect statistics about a frame since the
 * #cg_frame_info_t should hold the most data at this point.
 *
 * <note>A frame may not be completed before the next frame can start
 * so applications should avoid needing to collect all statistics for
 * a particular frame before they can start a new frame.</note>
 *
 * Stability: unstable
 */
typedef enum _cg_frame_event_t {
    CG_FRAME_EVENT_SYNC = 1,
    CG_FRAME_EVENT_COMPLETE
} cg_frame_event_t;

/**
 * cg_frame_callback_t:
 * @onscreen: The onscreen that the frame is associated with
 * @event: A #cg_frame_event_t notifying how the frame has progressed
 * @info: The meta information, such as timing information, about
 *        the frame that has progressed.
 * @user_data: The user pointer passed to
 *             cg_onscreen_add_frame_callback()
 *
 * Is a callback that can be registered via
 * cg_onscreen_add_frame_callback() to be called when a frame
 * progresses in some notable way.
 *
 * Please see the documentation for #cg_frame_event_t and
 * cg_onscreen_add_frame_callback() for more details about what
 * events can be notified.
 *
 * Stability: unstable
 */
typedef void (*cg_frame_callback_t)(cg_onscreen_t *onscreen,
                                    cg_frame_event_t event,
                                    cg_frame_info_t *info,
                                    void *user_data);

/**
 * CGlibFrameClosure:
 *
 * An opaque type that tracks a #cg_frame_callback_t and associated user
 * data. A #CGlibFrameClosure pointer will be returned from
 * cg_onscreen_add_frame_callback() and it allows you to remove a
 * callback later using cg_onscreen_remove_frame_callback().
 *
 * Stability: unstable
 */
typedef struct _cg_closure_t CGlibFrameClosure;

/**
 * cg_onscreen_add_frame_callback:
 * @onscreen: A #cg_onscreen_t framebuffer
 * @callback: A callback function to call for frame events
 * @user_data: A private pointer to be passed to @callback
 * @destroy: An optional callback to destroy @user_data when the
 *           @callback is removed or @onscreen is freed.
 *
 * Installs a @callback function that will be called for significant
 * events relating to the given @onscreen framebuffer.
 *
 * The @callback will be used to notify when the system compositor is
 * ready for this application to render a new frame. In this case
 * %CG_FRAME_EVENT_SYNC will be passed as the event argument to the
 * given @callback in addition to the #cg_frame_info_t corresponding to
 * the frame beeing acknowledged by the compositor.
 *
 * The @callback will also be called to notify when the frame has
 * ended. In this case %CG_FRAME_EVENT_COMPLETE will be passed as
 * the event argument to the given @callback in addition to the
 * #cg_frame_info_t corresponding to the newly presented frame.  The
 * meaning of "ended" here simply means that no more timing
 * information will be collected within the corresponding
 * #cg_frame_info_t and so this is a good opportunity to analyse the
 * given info. It does not necessarily mean that the GPU has finished
 * rendering the corresponding frame.
 *
 * We highly recommend throttling your application according to
 * %CG_FRAME_EVENT_SYNC events so that your application can avoid
 * wasting resources, drawing more frames than your system compositor
 * can display.
 *
 * Return value: a #CGlibFrameClosure pointer that can be used to
 *               remove the callback and associated @user_data later.
 * Stability: unstable
 */
CGlibFrameClosure *
cg_onscreen_add_frame_callback(cg_onscreen_t *onscreen,
                               cg_frame_callback_t callback,
                               void *user_data,
                               cg_user_data_destroy_callback_t destroy);

/**
 * cg_onscreen_remove_frame_callback:
 * @onscreen: A #cg_onscreen_t
 * @closure: A #CGlibFrameClosure returned from
 *           cg_onscreen_add_frame_callback()
 *
 * Removes a callback and associated user data that were previously
 * registered using cg_onscreen_add_frame_callback().
 *
 * If a destroy callback was passed to
 * cg_onscreen_add_frame_callback() to destroy the user data then
 * this will get called.
 *
 * Stability: unstable
 */
void cg_onscreen_remove_frame_callback(cg_onscreen_t *onscreen,
                                       CGlibFrameClosure *closure);

/**
 * cg_onscreen_set_resizable:
 * @onscreen: A #cg_onscreen_t framebuffer
 *
 * Lets you request CGlib to mark an @onscreen framebuffer as
 * resizable or not.
 *
 * By default, if possible, a @onscreen will be created by CGlib
 * as non resizable, but it is not guaranteed that this is always
 * possible for all window systems.
 *
 * <note>CGlib does not know whether marking the @onscreen framebuffer
 * is truly meaningful for your current window system (consider
 * applications being run fullscreen on a phone or TV) so this
 * function may not have any useful effect. If you are running on a
 * multi windowing system such as X11 or Win32 or OSX then CGlib will
 * request to the window system that users be allowed to resize the
 * @onscreen, although it's still possible that some other window
 * management policy will block this possibility.</note>
 *
 * <note>Whenever an @onscreen framebuffer is resized the viewport
 * will be automatically updated to match the new size of the
 * framebuffer with an origin of (0,0). If your application needs more
 * specialized control of the viewport it will need to register a
 * resize handler using cg_onscreen_add_resize_callback() so that it
 * can track when the viewport has been changed automatically.</note>
 *
 */
void cg_onscreen_set_resizable(cg_onscreen_t *onscreen, bool resizable);

/**
 * cg_onscreen_get_resizable:
 * @onscreen: A #cg_onscreen_t framebuffer
 *
 * Lets you query whether @onscreen has been marked as resizable via
 * the cg_onscreen_set_resizable() api.
 *
 * By default, if possible, a @onscreen will be created by CGlib
 * as non resizable, but it is not guaranteed that this is always
 * possible for all window systems.
 *
 * <note>If cg_onscreen_set_resizable(@onscreen, %true) has been
 * previously called then this function will return %true, but it's
 * possible that the current windowing system being used does not
 * support window resizing (consider fullscreen windows on a phone or
 * a TV). This function is not aware of whether resizing is truly
 * meaningful with your window system, only whether the @onscreen has
 * been marked as resizable.</note>
 *
 * Return value: Returns whether @onscreen has been marked as
 *               resizable or not.
 */
bool cg_onscreen_get_resizable(cg_onscreen_t *onscreen);

/**
 * cg_onscreen_resize_callback_t:
 * @onscreen: A #cg_onscreen_t framebuffer that was resized
 * @width: The new width of @onscreen
 * @height: The new height of @onscreen
 * @user_data: The private passed to
 *             cg_onscreen_add_resize_callback()
 *
 * Is a callback type used with the
 * cg_onscreen_add_resize_callback() allowing applications to be
 * notified whenever an @onscreen framebuffer is resized.
 *
 * <note>CGlib automatically updates the viewport of an @onscreen
 * framebuffer that is resized so this callback is also an indication
 * that the viewport has been modified too</note>
 *
 * <note>A resize callback will only ever be called while dispatching
 * CGlib events from the system mainloop; so for example during
 * cg_loop_dispatch(). This is so that callbacks shouldn't
 * occur while an application might have arbitrary locks held for
 * example.</note>
 *
 */
typedef void (*cg_onscreen_resize_callback_t)(cg_onscreen_t *onscreen,
                                              int width,
                                              int height,
                                              void *user_data);

/**
 * cg_onscreen_resize_closure_t:
 *
 * An opaque type that tracks a #cg_onscreen_resize_callback_t and
 * associated user data. A #cg_onscreen_resize_closure_t pointer will be
 * returned from cg_onscreen_add_resize_callback() and it allows you
 * to remove a callback later using
 * cg_onscreen_remove_resize_callback().
 *
 * Stability: unstable
 */
typedef struct _cg_closure_t cg_onscreen_resize_closure_t;

/**
 * cg_onscreen_add_resize_callback:
 * @onscreen: A #cg_onscreen_t framebuffer
 * @callback: A #cg_onscreen_resize_callback_t to call when the @onscreen
 *            changes size.
 * @user_data: Private data to be passed to @callback.
 * @destroy: An optional callback to destroy @user_data when the
 *           @callback is removed or @onscreen is freed.
 *
 * Registers a @callback with @onscreen that will be called whenever
 * the @onscreen framebuffer changes size.
 *
 * The @callback can be removed using
 * cg_onscreen_remove_resize_callback() passing the returned closure
 * pointer.
 *
 * <note>Since CGlib automatically updates the viewport of an @onscreen
 * framebuffer that is resized, a resize callback can also be used to
 * track when the viewport has been changed automatically by CGlib in
 * case your application needs more specialized control over the
 * viewport.</note>
 *
 * <note>A resize callback will only ever be called while dispatching
 * CGlib events from the system mainloop; so for example during
 * cg_loop_dispatch(). This is so that callbacks shouldn't
 * occur while an application might have arbitrary locks held for
 * example.</note>
 *
 * Return value: a #cg_onscreen_resize_closure_t pointer that can be used to
 *               remove the callback and associated @user_data later.
 */
cg_onscreen_resize_closure_t *
cg_onscreen_add_resize_callback(cg_onscreen_t *onscreen,
                                cg_onscreen_resize_callback_t callback,
                                void *user_data,
                                cg_user_data_destroy_callback_t destroy);

/**
 * cg_onscreen_remove_resize_callback:
 * @onscreen: A #cg_onscreen_t framebuffer
 * @closure: An identifier returned from cg_onscreen_add_resize_callback()
 *
 * Removes a resize @callback and @user_data pair that were previously
 * associated with @onscreen via cg_onscreen_add_resize_callback().
 *
 */
void cg_onscreen_remove_resize_callback(cg_onscreen_t *onscreen,
                                        cg_onscreen_resize_closure_t *closure);

/**
 * cg_onscreen_dirty_info_t:
 * @x: Left edge of the dirty rectangle
 * @y: Top edge of the dirty rectangle, measured from the top of the window
 * @width: Width of the dirty rectangle
 * @height: Height of the dirty rectangle
 *
 * A structure passed to callbacks registered using
 * cg_onscreen_add_dirty_callback(). The members describe a
 * rectangle within the onscreen buffer that should be redrawn.
 *
 * Stability: unstable
 */
typedef struct _cg_onscreen_dirty_info_t cg_onscreen_dirty_info_t;

struct _cg_onscreen_dirty_info_t {
    int x, y;
    int width, height;
};

/**
 * cg_onscreen_dirty_callback_t:
 * @onscreen: The onscreen that the frame is associated with
 * @info: A #cg_onscreen_dirty_info_t struct containing the details of the
 *   dirty area
 * @user_data: The user pointer passed to
 *             cg_onscreen_add_frame_callback()
 *
 * Is a callback that can be registered via
 * cg_onscreen_add_dirty_callback() to be called when the windowing
 * system determines that a region of the onscreen window has been
 * lost and the application should redraw it.
 *
 * Stability: unstable
 */
typedef void (*cg_onscreen_dirty_callback_t)(
    cg_onscreen_t *onscreen,
    const cg_onscreen_dirty_info_t *info,
    void *user_data);

/**
 * cg_onscreen_dirty_closure_t:
 *
 * An opaque type that tracks a #cg_onscreen_dirty_callback_t and associated
 * user data. A #cg_onscreen_dirty_closure_t pointer will be returned from
 * cg_onscreen_add_dirty_callback() and it allows you to remove a
 * callback later using cg_onscreen_remove_dirty_callback().
 *
 * Stability: unstable
 */
typedef struct _cg_closure_t cg_onscreen_dirty_closure_t;

/**
 * cg_onscreen_add_dirty_callback:
 * @onscreen: A #cg_onscreen_t framebuffer
 * @callback: A callback function to call for dirty events
 * @user_data: A private pointer to be passed to @callback
 * @destroy: An optional callback to destroy @user_data when the
 *           @callback is removed or @onscreen is freed.
 *
 * Installs a @callback function that will be called whenever the
 * window system has lost the contents of a region of the onscreen
 * buffer and the application should redraw it to repair the buffer.
 * For example this may happen in a window system without a compositor
 * if a window that was previously covering up the onscreen window has
 * been moved causing a region of the onscreen to be exposed.
 *
 * The @callback will be passed a #cg_onscreen_dirty_info_t struct which
 * decribes a rectangle containing the newly dirtied region. Note that
 * this may be called multiple times to describe a non-rectangular
 * region composed of multiple smaller rectangles.
 *
 * The dirty events are separate from %CG_FRAME_EVENT_SYNC events so
 * the application should also listen for this event before rendering
 * the dirty region to ensure that the framebuffer is actually ready
 * for rendering.
 *
 * Return value: a #cg_onscreen_dirty_closure_t pointer that can be used to
 *               remove the callback and associated @user_data later.
 * Stability: unstable
 */
cg_onscreen_dirty_closure_t *
cg_onscreen_add_dirty_callback(cg_onscreen_t *onscreen,
                               cg_onscreen_dirty_callback_t callback,
                               void *user_data,
                               cg_user_data_destroy_callback_t destroy);

/**
 * cg_onscreen_remove_dirty_callback:
 * @onscreen: A #cg_onscreen_t
 * @closure: A #cg_onscreen_dirty_closure_t returned from
 *           cg_onscreen_add_dirty_callback()
 *
 * Removes a callback and associated user data that were previously
 * registered using cg_onscreen_add_dirty_callback().
 *
 * If a destroy callback was passed to
 * cg_onscreen_add_dirty_callback() to destroy the user data then
 * this will also get called.
 *
 * Stability: unstable
 */
void cg_onscreen_remove_dirty_callback(cg_onscreen_t *onscreen,
                                       cg_onscreen_dirty_closure_t *closure);

/**
 * cg_is_onscreen:
 * @object: A #cg_object_t pointer
 *
 * Gets whether the given object references a #cg_onscreen_t.
 *
 * Return value: %true if the object references a #cg_onscreen_t
 *   and %false otherwise.
 * Stability: unstable
 */
bool cg_is_onscreen(void *object);

/**
 * cg_onscreen_get_frame_counter:
 *
 * Gets the value of the framebuffers frame counter. This is
 * a counter that increases by one each time
 * cg_onscreen_swap_buffers() or cg_onscreen_swap_region()
 * is called.
 *
 * Return value: the current frame counter value
 * Stability: unstable
 */
int64_t cg_onscreen_get_frame_counter(cg_onscreen_t *onscreen);

CG_END_DECLS

#endif /* __CG_ONSCREEN_H */
