/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012,2013,2014  Intel Corporation
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

#include <rut-config.h>

#if 0
#include <signal.h>
#endif

#include <clib.h>

#include <cglib/cglib.h>
#ifdef USE_SDL
#include <cglib/cg-sdl.h>
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "rut-transform-private.h"
#include "rut-shell.h"
#include "rut-util.h"
#include "rut-inputable.h"
#include "rut-transform.h"
#include "rut-input-region.h"
#include "rut-mimable.h"
#include "rut-camera.h"
#include "rut-poll.h"
#include "rut-geometry.h"
#include "rut-texture-cache.h"
#include "rut-headless-shell.h"

#ifdef USE_SDL
#include "rut-sdl-shell.h"
#endif
#ifdef USE_X11
#include "rut-x11-shell.h"
#endif
#ifdef __EMSCRIPTEN__
#include "rut-emscripten-shell.h"
#endif

#ifdef USE_GSTREAMER
#include "gstmemsrc.h"
#endif

#ifdef USE_XLIB
#include <X11/Xlib.h>
#endif

#ifdef __ANDROID__
#include <android/log.h>
#endif

/* Mainly for logging/debugging purposes, we keep track of a
 * per-thread current shell */
static c_tls_t rut_shell_tls;

static void
_rut_shell_fini(rut_shell_t *shell)
{
    rut_object_unref(shell);
}

rut_closure_t *
rut_shell_add_input_callback(rut_shell_t *shell,
                             rut_input_callback_t callback,
                             void *user_data,
                             rut_closure_destroy_callback_t destroy_cb)
{
    return rut_closure_list_add_FIXME(
        &shell->input_cb_list, callback, user_data, destroy_cb);
}

rut_object_t *
rut_input_event_get_camera(rut_input_event_t *event)
{
    c_return_val_if_reached(NULL);
}

rut_input_event_type_t
rut_input_event_get_type(rut_input_event_t *event)
{
    return event->type;
}

rut_shell_onscreen_t *
rut_input_event_get_onscreen(rut_input_event_t *event)
{
    return event->onscreen;
}

int32_t
rut_key_event_get_keysym(rut_input_event_t *event)
{
    rut_shell_t *shell = event->onscreen->shell;

    return shell->platform.key_event_get_keysym(event);
}

rut_key_event_action_t
rut_key_event_get_action(rut_input_event_t *event)
{
    rut_shell_t *shell = event->onscreen->shell;

    return shell->platform.key_event_get_action(event);
}

rut_motion_event_action_t
rut_motion_event_get_action(rut_input_event_t *event)
{
    rut_shell_t *shell = event->onscreen->shell;

    return shell->platform.motion_event_get_action(event);
}

rut_button_state_t
rut_motion_event_get_button(rut_input_event_t *event)
{
    rut_shell_t *shell = event->onscreen->shell;

    return shell->platform.motion_event_get_button(event);
}

rut_button_state_t
rut_motion_event_get_button_state(rut_input_event_t *event)
{
    rut_shell_t *shell = event->onscreen->shell;

    return shell->platform.motion_event_get_button_state(event);
}

rut_modifier_state_t
rut_key_event_get_modifier_state(rut_input_event_t *event)
{
    rut_shell_t *shell = event->onscreen->shell;

    return shell->platform.key_event_get_modifier_state(event);
}

rut_modifier_state_t
rut_motion_event_get_modifier_state(rut_input_event_t *event)
{
    rut_shell_t *shell = event->onscreen->shell;

    return shell->platform.motion_event_get_modifier_state(event);
}

static void
rut_motion_event_get_transformed_xy(rut_input_event_t *event,
                                    float *x,
                                    float *y)
{
    const c_matrix_t *transform = event->input_transform;
    rut_shell_t *shell = event->onscreen->shell;

    shell->platform.motion_event_get_transformed_xy(event, x, y);

    if (transform) {
        *x = transform->xx * *x + transform->xy * *y + transform->xw;
        *y = transform->yx * *x + transform->yy * *y + transform->yw;
    }
}

float
rut_motion_event_get_x(rut_input_event_t *event)
{
    float x, y;

    rut_motion_event_get_transformed_xy(event, &x, &y);

    return x;
}

float
rut_motion_event_get_y(rut_input_event_t *event)
{
    float x, y;

    rut_motion_event_get_transformed_xy(event, &x, &y);

    return y;
}

bool
rut_motion_event_unproject(rut_input_event_t *event,
                           rut_object_t *graphable,
                           float *x,
                           float *y)
{
    c_matrix_t transform;
    c_matrix_t inverse_transform;
    rut_object_t *camera = rut_input_event_get_camera(event);

    rut_graphable_get_modelview(graphable, camera, &transform);

    if (!c_matrix_get_inverse(&transform, &inverse_transform))
        return false;

    *x = rut_motion_event_get_x(event);
    *y = rut_motion_event_get_y(event);
    rut_camera_unproject_coord(camera,
                               &transform,
                               &inverse_transform,
                               0, /* object_coord_z */
                               x,
                               y);

    return true;
}

rut_object_t *
rut_drop_offer_event_get_payload(rut_input_event_t *event)
{
    return event->onscreen->shell->drag_payload;
}

const char *
rut_text_event_get_text(rut_input_event_t *event)
{
    return event->onscreen->shell->platform.text_event_get_text(event);
}

static void
cancel_current_drop_offer_taker(rut_shell_onscreen_t *onscreen)
{
    rut_shell_t *shell = onscreen->shell;
    rut_input_event_t drop_cancel;
    rut_input_event_status_t status;

    if (!shell->drop_offer_taker)
        return;

    drop_cancel.type = RUT_INPUT_EVENT_TYPE_DROP_CANCEL;
    drop_cancel.onscreen = onscreen;
    drop_cancel.native = NULL;
    drop_cancel.camera_entity = NULL;
    drop_cancel.input_transform = NULL;

    status = rut_inputable_handle_event(shell->drop_offer_taker, &drop_cancel);

    c_warn_if_fail(status == RUT_INPUT_EVENT_STATUS_HANDLED);

    rut_object_unref(shell->drop_offer_taker);
    shell->drop_offer_taker = NULL;
}

rut_input_event_status_t
rut_shell_dispatch_input_event(rut_shell_t *shell, rut_input_event_t *event)
{
    rut_input_event_status_t status = RUT_INPUT_EVENT_STATUS_UNHANDLED;
    rut_shell_onscreen_t *onscreen = rut_input_event_get_onscreen(event);
    rut_closure_t *c, *tmp;
    rut_shell_grab_t *grab;

    /* Keep track of the last known position of the mouse so that we can
     * send key events to whatever is under the mouse when there is no
     * key focus */
    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION) {
        shell->mouse_x = rut_motion_event_get_x(event);
        shell->mouse_y = rut_motion_event_get_y(event);

        /* Keep track of whether any handlers set a cursor in response to
         * the motion event */
        if (onscreen)
            onscreen->cursor_set = false;

        if (shell->drag_payload) {
            event->type = RUT_INPUT_EVENT_TYPE_DROP_OFFER;
            rut_shell_dispatch_input_event(shell, event);
            event->type = RUT_INPUT_EVENT_TYPE_MOTION;
        }
    } else if (rut_input_event_get_type(event) ==
               RUT_INPUT_EVENT_TYPE_DROP_OFFER)
        cancel_current_drop_offer_taker(onscreen);
    else if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_DROP) {
        if (shell->drop_offer_taker) {
            rut_input_event_status_t status =
                rut_inputable_handle_event(shell->drop_offer_taker, event);

            if (status == RUT_INPUT_EVENT_STATUS_HANDLED)
                goto handled;
        }
    }

    c_list_for_each_safe(grab, shell->next_grab, &shell->grabs, list_node)
    {
        rut_object_t *old_camera = event->camera_entity;
        rut_input_event_status_t grab_status;

        if (grab->camera_entity)
            event->camera_entity = grab->camera_entity;

        grab_status = grab->callback(event, grab->user_data);

        event->camera_entity = old_camera;

        if (grab_status == RUT_INPUT_EVENT_STATUS_HANDLED) {
            status = RUT_INPUT_EVENT_STATUS_HANDLED;
            goto handled;
        }
    }

    c_list_for_each_safe(c, tmp, &shell->input_cb_list, list_node)
    {
        rut_input_callback_t cb = c->function;

        status = cb(event, c->user_data);
        if (status == RUT_INPUT_EVENT_STATUS_HANDLED)
            goto handled;
    }

handled:

    /* If nothing set a cursor in response to the motion event then
     * we'll reset it back to the default pointer */
    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION &&
        onscreen && !onscreen->cursor_set)
        rut_shell_onscreen_set_cursor(onscreen, RUT_CURSOR_ARROW);

    return status;
}

static void
_rut_shell_remove_grab_link(rut_shell_t *shell,
                            rut_shell_grab_t *grab)
{
    if (grab->camera_entity)
        rut_object_unref(grab->camera_entity);

    /* If we are in the middle of iterating the grab callbacks then this
     * will make it cope with removing arbritrary nodes from the list
     * while iterating it */
    if (shell->next_grab == grab)
        shell->next_grab =
            rut_container_of(grab->list_node.next, grab, list_node);

    c_list_remove(&grab->list_node);

    c_slice_free(rut_shell_grab_t, grab);
}

static void
_rut_shell_free(void *object)
{
    rut_shell_t *shell = object;

    while (!c_list_empty(&shell->grabs)) {
        rut_shell_grab_t *first_grab =
            c_container_of(shell->grabs.next, rut_shell_grab_t, list_node);

        _rut_shell_remove_grab_link(shell, first_grab);
    }

    rut_closure_list_disconnect_all_FIXME(&shell->input_cb_list);

    _rut_shell_fini(shell);

#ifdef USE_PANGO
    g_object_unref(shell->pango_context);
    g_object_unref(shell->pango_font_map);
    pango_font_description_free(shell->pango_font_desc);
#endif

    if (shell->platform.cleanup)
        shell->platform.cleanup(shell);

    cg_object_unref(shell->cg_device);

    rut_settings_destroy(shell->settings);

    rut_object_free(rut_shell_t, shell);
}

rut_type_t rut_shell_type;

static void
_rut_shell_init_type(void)
{
    rut_type_init(&rut_shell_type, "rut_shell_t", _rut_shell_free);
}

rut_shell_t *
rut_shell_new(rut_shell_t *main_shell,
              rut_shell_paint_callback_t paint,
              void *user_data)
{
    rut_shell_t *shell =
        rut_object_alloc0(rut_shell_t, &rut_shell_type, _rut_shell_init_type);

    shell->input_queue = rut_input_queue_new(shell);

    c_list_init(&shell->grabs);
    c_list_init(&shell->input_cb_list);
    c_list_init(&shell->onscreens);

    _rut_matrix_entry_identity_init(&shell->identity_entry);

    shell->paint_cb = paint;
    shell->user_data = user_data;

    rut_poll_init(shell, main_shell);
    rut_closure_init(&shell->paint_idle, rut_shell_paint, shell);

    return shell;
}

void
rut_shell_set_is_headless(rut_shell_t *shell, bool headless)
{
    shell->headless = headless;
}

void
rut_shell_set_on_run_callback(rut_shell_t *shell,
                              void (*callback)(rut_shell_t *shell, void *data),
                              void *user_data)
{
    shell->on_run_cb = callback;
    shell->on_run_data = user_data;
}

void
rut_shell_set_on_quit_callback(rut_shell_t *shell,
                               void (*callback)(rut_shell_t *shell, void *data),
                               void *user_data)
{
    shell->on_quit_cb = callback;
    shell->on_quit_data = user_data;
}

#ifdef USE_UV
uv_loop_t *
rut_uv_shell_get_loop(rut_shell_t *shell)
{
    c_return_val_if_fail(shell->uv_loop, NULL);

    return shell->uv_loop;
}
#endif

#ifdef __ANDROID__
void
rut_android_shell_set_application(rut_shell_t *shell,
                                  struct android_app *application)
{
    shell->android_application = application;
}
#endif

bool
rut_shell_get_headless(rut_shell_t *shell)
{
    return shell->headless;
}

#if 0
static int signal_write_fd;

void
signal_handler (int sig)
{
    switch (sig)
    {
    case SIGCHLD:
        write (signal_write_fd, &sig, sizeof (sig));
        break;
    }
}

bool
dispatch_signal_source (GSource *source,
                        GSourceFunc callback,
                        void *user_data)
{
    rut_shell_t *shell = user_data;

    do {
        int sig;
        int ret = read (shell->signal_read_fd, &sig, sizeof (sig));
        if (ret != sizeof (sig))
        {
            if (ret == 0)
                return true;
            else if (ret < 0 && errno == EINTR)
                continue;
            else
            {
                c_warning ("Error reading signal fd: %s", strerror (errno));
                return false;
            }
        }

        c_debug ("Signal received: %d\n", sig);

        rut_closure_list_invoke (&shell->signal_cb_list,
                                 rut_shell_signal_callback_t,
                                 shell,
                                 sig);
    } while (1);

    return true;
}
#endif

static void
_rut_shell_init(rut_shell_t *shell)
{
    /* Only initialize once */
    if (shell->settings)
        return;

    shell->settings = rut_settings_new();

    if (shell->headless)
        rut_headless_shell_init(shell);
    else {
#ifdef USE_GSTREAMER
        gst_init(NULL, NULL);
        gst_element_register(NULL, "memsrc", 0, gst_mem_src_get_type());
#endif

#ifdef USE_SDL
        rut_sdl_shell_init(shell);
#elif defined(USE_X11)
        rut_x11_shell_init(shell);
#elif defined(__ANDROID__)
        rut_android_shell_init(shell);
#elif defined(__EMSCRIPTEN__)
        rut_emscripten_shell_init(shell);
#endif

#ifdef USE_UV
        rut_poll_shell_integrate_cg_events_via_libuv(shell);
#endif

        shell->nine_slice_indices =
            cg_indices_new(shell->cg_device,
                           CG_INDICES_TYPE_UNSIGNED_BYTE,
                           _rut_nine_slice_indices_data,
                           sizeof(_rut_nine_slice_indices_data) /
                           sizeof(_rut_nine_slice_indices_data[0]));

        rut_texture_cache_init(shell);

        shell->single_texture_2d_template =
            cg_pipeline_new(shell->cg_device);
        cg_pipeline_set_layer_null_texture(
            shell->single_texture_2d_template, 0, CG_TEXTURE_TYPE_2D);

        shell->circle_texture =
            rut_create_circle_texture(shell,
                                      CIRCLE_TEX_RADIUS /* radius */,
                                      CIRCLE_TEX_PADDING /* padding */);

        c_matrix_init_identity(&shell->identity_matrix);

#ifdef USE_PANGO
        shell->pango_font_map =
            CG_PANGO_FONT_MAP(cg_pango_font_map_new(shell->cg_device));

        cg_pango_font_map_set_use_mipmapping(shell->pango_font_map, true);

        shell->pango_context = pango_font_map_create_context(
            PANGO_FONT_MAP(shell->pango_font_map));

        shell->pango_font_desc = pango_font_description_new();
        pango_font_description_set_family(shell->pango_font_desc, "Sans");
        pango_font_description_set_size(shell->pango_font_desc,
                                        14 * PANGO_SCALE);
#endif

    }
}

void
rut_shell_onscreen_set_cursor(rut_shell_onscreen_t *onscreen,
                              rut_cursor_t cursor)
{
    c_return_if_fail(onscreen != NULL);

    if (onscreen->current_cursor == cursor)
        return;

    if (onscreen->current_cursor != cursor) {
        rut_shell_t *shell = onscreen->shell;

        if (shell->platform.onscreen_set_cursor)
            shell->platform.onscreen_set_cursor(onscreen, cursor);

        onscreen->current_cursor = cursor;
    }

    onscreen->cursor_set = true;
}

void
rut_shell_onscreen_set_title(rut_shell_onscreen_t *onscreen,
                             const char *title)
{
    if (onscreen->shell->platform.onscreen_set_title)
        onscreen->shell->platform.onscreen_set_title(onscreen, title);
}

void
rut_shell_onscreen_set_fullscreen(rut_shell_onscreen_t *onscreen,
                                  bool fullscreen)
{
    if (onscreen->shell->platform.onscreen_set_fullscreen)
        onscreen->shell->platform.onscreen_set_fullscreen(onscreen, fullscreen);
}

bool
rut_shell_onscreen_get_fullscreen(rut_shell_onscreen_t *onscreen)
{
    return onscreen->fullscreen;
}

void
rut_shell_remove_paint_idle(rut_shell_t *shell)
{
#ifdef USE_UV
    rut_poll_shell_remove_idle(shell, &shell->paint_idle);
#endif
}

static void
free_input_event(rut_shell_t *shell, rut_input_event_t *event)
{
    if (shell->platform.free_input_event)
        shell->platform.free_input_event(event);
    else
        c_slice_free(rut_input_event_t, event);
}

void
rut_shell_dispatch_input_events(rut_shell_t *shell)
{
    rut_input_queue_t *input_queue = shell->input_queue;
    rut_input_event_t *event, *tmp;

    c_list_for_each_safe(event, tmp, &input_queue->events, list_node)
    {
        /* XXX: we remove the event from the queue before dispatching it
         * so that it can potentially be deferred to another input queue
         * during the dispatch. */
        rut_input_queue_remove(shell->input_queue, event);

        rut_shell_dispatch_input_event(shell, event);

        /* Only free the event if it hasn't been re-queued
         * TODO: perhaps make rut_input_event_t into a ref-counted object
         */
        if (event->list_node.prev == NULL && event->list_node.next == NULL) {
            free_input_event(shell, event);
        }
    }

    c_warn_if_fail(input_queue->n_events == 0);
}

rut_input_queue_t *
rut_shell_get_input_queue(rut_shell_t *shell)
{
    return shell->input_queue;
}

rut_input_queue_t *
rut_input_queue_new(rut_shell_t *shell)
{
    rut_input_queue_t *queue = c_slice_new0(rut_input_queue_t);

    queue->shell = shell;
    c_list_init(&queue->events);
    queue->n_events = 0;

    return queue;
}

void
rut_input_queue_append(rut_input_queue_t *queue, rut_input_event_t *event)
{
#if 0
    if (queue->shell->headless) {
        c_debug ("input_queue_append %p %d\n", event, event->type);
        if (event->type == RUT_INPUT_EVENT_TYPE_KEY)
            c_debug ("> is key\n");
        else
            c_debug ("> not key\n");
    }
#endif
    c_list_insert(queue->events.prev, &event->list_node);
    queue->n_events++;
}

void
rut_input_queue_remove(rut_input_queue_t *queue, rut_input_event_t *event)
{
    c_list_remove(&event->list_node);
    queue->n_events--;
}

void
rut_input_queue_destroy(rut_input_queue_t *queue)
{
    rut_input_queue_clear(queue);

    c_slice_free(rut_input_queue_t, queue);
}

void
rut_input_queue_clear(rut_input_queue_t *input_queue)
{
    rut_shell_t *shell = input_queue->shell;
    rut_input_event_t *event, *tmp;

    c_list_for_each_safe(event, tmp, &input_queue->events, list_node)
    {
        c_list_remove(&event->list_node);
        free_input_event(shell, event);
    }

    input_queue->n_events = 0;
}

void
rut_shell_paint(rut_shell_t *shell)
{
    shell->paint_cb(shell, shell->user_data);
}

static void
_rut_shell_onscreen_free(void *object)
{
    rut_shell_onscreen_t *onscreen = object;

    if (onscreen->cg_onscreen) {
        cg_onscreen_remove_resize_callback(onscreen->cg_onscreen,
                                           onscreen->resize_closure);
        onscreen->resize_closure = NULL;
    }

    if (onscreen->input_camera)
        rut_object_unref(onscreen->input_camera);

    c_list_remove(&onscreen->link);

    rut_object_free(rut_shell_onscreen_t, object);
}

rut_type_t rut_shell_onscreen_type;

static void
_rut_shell_onscreen_init_type(void)
{
    rut_type_init(&rut_shell_onscreen_type,
                  "rut_shell_onscreen_t", _rut_shell_onscreen_free);
}

rut_shell_onscreen_t *
rut_shell_onscreen_new(rut_shell_t *shell,
                       int width, int height)
{
    rut_shell_onscreen_t *onscreen =
        rut_object_alloc0(rut_shell_onscreen_t, &rut_shell_onscreen_type,
                          _rut_shell_onscreen_init_type);

    onscreen->shell = shell;

    onscreen->width = width;
    onscreen->height = height;
    onscreen->is_ready = true;

    onscreen->presentation_time_latest_frame = -1;

    c_list_init(&onscreen->frame_closures);

    c_list_insert(shell->onscreens.prev, &onscreen->link);

    return onscreen;
}

void
rut_shell_onscreen_begin_frame(rut_shell_onscreen_t *onscreen)
{
    c_warn_if_fail(onscreen->is_ready);

    onscreen->is_dirty = false;
    onscreen->is_ready = false;
}

static void
onscreen_maybe_queue_redraw(rut_shell_onscreen_t *onscreen)
{
    if (onscreen->is_dirty && onscreen->is_ready)
        rut_shell_queue_redraw(onscreen->shell);
}

static void
onscreen_frame_event_cb(cg_onscreen_t *cg_onscreen,
                        cg_frame_event_t event,
                        cg_frame_info_t *info,
                        void *user_data)
{
    rut_shell_onscreen_t *onscreen = user_data;

    if (event == CG_FRAME_EVENT_SYNC) {
        float refresh_rate = cg_frame_info_get_refresh_rate(info);

        onscreen->is_ready = true;

        if (refresh_rate)
            onscreen->refresh_rate = refresh_rate;

        onscreen_maybe_queue_redraw(onscreen);

    } else if (event == CG_FRAME_EVENT_COMPLETE) {
        int64_t frame = cg_frame_info_get_frame_counter(info);
        float refresh_rate = cg_frame_info_get_refresh_rate(info);

        if(frame > onscreen->presentation_time_latest_frame) {
            int64_t presentation_time = cg_frame_info_get_presentation_time(info);

            if (presentation_time)
            {
                int64_t delta;
                int delta_idx = onscreen->presentation_delta_index++ %
                    C_N_ELEMENTS(onscreen->presentation_deltas);

                onscreen->presentation_time_earlier = onscreen->presentation_time_latest;
                onscreen->presentation_time_latest = presentation_time;

                onscreen->presentation_time_latest_frame = frame;

                delta = onscreen->presentation_time_latest -
                    onscreen->presentation_time_earlier;

                onscreen->presentation_deltas[delta_idx] = delta;
                c_debug("onscreen_frame_event_cb: latest =%"PRIi64" earlier=%"PRIi64" no=%"PRIi64,
                        onscreen->presentation_time_latest,
                        onscreen->presentation_time_earlier,
                        onscreen->presentation_time_latest_frame);
            }
        } else {
            c_debug("Spurious out of order _FRAME_COMPLETE (current frame = %"PRIi64", last frame = %"PRIi64")",
                    frame,
                    onscreen->presentation_time_latest_frame);
        }

        if (refresh_rate) {
            c_debug("onscreen_frame_event_cb: refresh_rate = %f", refresh_rate);
            onscreen->refresh_rate = refresh_rate;
        }
    }
}

static void
onscreen_dirty_cb(cg_onscreen_t *cg_onscreen,
                  const cg_onscreen_dirty_info_t *info,
                  void *user_data)
{
    rut_shell_onscreen_t *onscreen = user_data;

    onscreen->is_dirty = true;

    onscreen_maybe_queue_redraw(onscreen);
}

static void
onscreen_resize_callback(cg_onscreen_t *cg_onscreen,
                         int width,
                         int height,
                         void *user_data)
{
    rut_shell_onscreen_t *onscreen = user_data;

    onscreen->width = width;
    onscreen->height = height;

    rut_shell_queue_redraw(onscreen->shell);
}

bool
rut_shell_onscreen_allocate(rut_shell_onscreen_t *onscreen)
{
    onscreen->cg_onscreen =
        onscreen->shell->platform.allocate_onscreen(onscreen);

    if (!onscreen->cg_onscreen)
        return false;

    cg_onscreen_set_resizable(onscreen->cg_onscreen,
                              onscreen->resizable);

    cg_onscreen_add_dirty_callback(onscreen->cg_onscreen,
                                   onscreen_dirty_cb,
                                   onscreen, /* user data */
                                   NULL); /* destroy notify */
    cg_onscreen_add_frame_callback(onscreen->cg_onscreen,
                                   onscreen_frame_event_cb,
                                   onscreen, /* user data */
                                   NULL); /* destroy notify */

    onscreen->resize_closure =
        cg_onscreen_add_resize_callback(onscreen->cg_onscreen,
                                        onscreen_resize_callback,
                                        onscreen,
                                        NULL); /* destroy */

    return true;
}

void
rut_shell_onscreen_set_resizable(rut_shell_onscreen_t *onscreen,
                                 bool resizable)
{
    c_return_if_fail(onscreen->cg_onscreen);

    onscreen->resizable = resizable;
}

void
rut_shell_onscreen_resize(rut_shell_onscreen_t *onscreen,
                          int width,
                          int height)
{
    onscreen->shell->platform.onscreen_resize(onscreen, width, height);
}

void
rut_shell_onscreen_show(rut_shell_onscreen_t *onscreen)
{
    cg_onscreen_show(onscreen->cg_onscreen);
}

void
rut_shell_onscreen_set_input_camera(rut_shell_onscreen_t *onscreen,
                                    rut_object_t *camera)
{
    if (onscreen->input_camera == camera)
        return;

    if (onscreen->input_camera) {
        rut_object_unref(onscreen->input_camera);
        onscreen->input_camera = NULL;
    }

    if (camera)
        onscreen->input_camera = rut_object_ref(camera);
}

rut_object_t *
rut_shell_onscreen_get_input_camera(rut_shell_onscreen_t *onscreen)
{
    return onscreen->input_camera;
}

void
rut_shell_main(rut_shell_t *shell)
{
    _rut_shell_init(shell);

    rut_poll_run(shell);

    if (shell->on_quit_cb)
        shell->on_quit_cb(shell, shell->on_quit_data);
}

void
rut_shell_grab_input(rut_shell_t *shell,
                     rut_object_t *camera_entity,
                     rut_input_callback_t callback,
                     void *user_data)
{
    rut_shell_grab_t *grab = c_slice_new(rut_shell_grab_t);

    grab->callback = callback;
    grab->user_data = user_data;

    if (camera_entity)
        grab->camera_entity = rut_object_ref(camera_entity);
    else
        grab->camera_entity = NULL;

    c_list_insert(&shell->grabs, &grab->list_node);
}

void
rut_shell_ungrab_input(rut_shell_t *shell,
                       rut_input_callback_t callback,
                       void *user_data)
{
    rut_shell_grab_t *grab;

    c_list_for_each(grab, &shell->grabs, list_node)
    if (grab->callback == callback && grab->user_data == user_data) {
        _rut_shell_remove_grab_link(shell, grab);
        break;
    }
}

typedef struct _pointer_grab_t {
    rut_shell_t *shell;
    rut_input_event_status_t (*callback)(rut_input_event_t *event,
                                         void *user_data);
    void *user_data;
    rut_button_state_t button_mask;
    bool x11_grabbed;
} pointer_grab_t;

static rut_input_event_status_t
pointer_grab_cb(rut_input_event_t *event,
                void *user_data)
{
    pointer_grab_t *grab = user_data;
    rut_input_event_status_t status = grab->callback(event, grab->user_data);

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION) {
        rut_button_state_t current = rut_motion_event_get_button_state(event);
        rut_button_state_t last_button = 1 << (ffs(current) - 1);

        if (rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_UP &&
            ((rut_motion_event_get_button_state(event) & last_button) == 0)) {
            rut_shell_t *shell = grab->shell;
            c_slice_free(pointer_grab_t, grab);
            rut_shell_ungrab_input(shell, pointer_grab_cb, user_data);

#if 0 // USE_XLIB
            /* X11 doesn't implicitly grab the mouse on pointer-down events
             * so we have to do it explicitly... */
            if (grab->x11_grabbed)
            {
                rut_shell_onscreen_t *shell_onscreen =
                    c_list_first(&shell->onscreens, rut_shell_onscreen_t, link);
                Display *dpy = shell_onscreen->sdl_info.info.x11.display;

                if (shell->x11_grabbed)
                {
                    XUngrabPointer (dpy, CurrentTime);
                    XUngrabKeyboard (dpy, CurrentTime);
                    shell->x11_grabbed = false;
                }
            }
#endif
        }
    }

    return status;
}

void
rut_shell_grab_pointer(rut_shell_t *shell,
                       rut_object_t *camera,
                       rut_input_event_status_t (*callback)(
                           rut_input_event_t *event, void *user_data),
                       void *user_data)
{

    pointer_grab_t *grab = c_slice_new0(pointer_grab_t);

    grab->shell = shell;
    grab->callback = callback;
    grab->user_data = user_data;

    rut_shell_grab_input(shell, camera, pointer_grab_cb, grab);

#if 0 // USE_XLIB
    /* X11 doesn't implicitly grab the mouse on pointer-down events
     * so we have to do it explicitly... */
    if (shell->sdl_subsystem == SDL_SYSWM_X11)
    {
        rut_shell_onscreen_t *shell_onscreen =
            c_list_first(&shell->onscreens, rut_shell_onscreen_t, link);
        Display *dpy = shell_onscreen->sdl_info.info.x11.display;
        Window win = shell_onscreen->sdl_info.info.x11.window;

        c_warn_if_fail (shell->x11_grabbed == false);

        if (XGrabPointer (dpy, win, False,
                          PointerMotionMask|ButtonPressMask|ButtonReleaseMask,
                          GrabModeAsync, /* pointer mode */
                          GrabModeAsync, /* keyboard mode */
                          None, /* confine to */
                          None, /* cursor */
                          CurrentTime) == GrabSuccess)
        {
            grab->x11_grabbed = true;
            shell->x11_grabbed = true;
        }

        XGrabKeyboard(dpy, win, False,
                      GrabModeAsync,
                      GrabModeAsync,
                      CurrentTime);
    }
#endif
}

void
rut_shell_queue_redraw_real(rut_shell_t *shell)
{
#ifndef __EMSCRIPTEN__
    rut_poll_shell_add_idle(shell, &shell->paint_idle);
#else
    /* If we haven't called emscripten_set_main_loop_arg() yet then
     * emscripten will get upset if we try and resume a mainloop
     * that doesn't exist yet... */
    if (!shell->running)
        return;

    if (!shell->paint_loop_running) {
        emscripten_resume_main_loop();
        shell->paint_loop_running = true;
    }
#endif
}

void
rut_shell_queue_redraw(rut_shell_t *shell)
{
    shell->queue_redraw_callback(shell, shell->queue_redraw_data);
}

bool
rut_shell_is_redraw_queued(rut_shell_t *shell)
{
    rut_shell_onscreen_t *first_onscreen =
        c_list_first(&shell->onscreens, rut_shell_onscreen_t, link);

    if (first_onscreen && first_onscreen->is_dirty)
        return true;

#ifdef __EMSCRIPTEN__
    return shell->paint_loop_running;
#else
    if (shell->paint_idle.list_node.next)
        return true;
    else
        return false;
#endif
}

void
rut_shell_set_queue_redraw_callback(rut_shell_t *shell,
                                    void (*callback)(rut_shell_t *shell,
                                                     void *user_data),
                                    void *user_data)
{
    shell->queue_redraw_callback = callback;
    shell->queue_redraw_data = user_data;
}

void
rut_shell_quit(rut_shell_t *shell)
{
    rut_poll_quit(shell);
}

static void
_rut_shell_onscreen_paste(rut_shell_onscreen_t *onscreen, rut_object_t *data)
{
    rut_shell_t *shell = onscreen->shell;
    rut_input_event_t drop_event;

    drop_event.type = RUT_INPUT_EVENT_TYPE_DROP;
    drop_event.onscreen = onscreen;
    drop_event.native = data;
    drop_event.camera_entity = NULL;
    drop_event.input_transform = NULL;

    /* Note: This assumes input handlers are re-entrant and hopefully
     * that's ok. */
    rut_shell_dispatch_input_event(shell, &drop_event);
}

void
rut_shell_onscreen_drop(rut_shell_onscreen_t *onscreen)
{
    rut_shell_t *shell = onscreen->shell;
    rut_input_event_t drop_event;
    rut_input_event_status_t status;

    if (!shell->drop_offer_taker)
        return;

    drop_event.type = RUT_INPUT_EVENT_TYPE_DROP;
    drop_event.onscreen = onscreen;
    drop_event.native = shell->drag_payload;
    drop_event.camera_entity = NULL;
    drop_event.input_transform = NULL;

    status = rut_inputable_handle_event(shell->drop_offer_taker, &drop_event);

    c_warn_if_fail(status == RUT_INPUT_EVENT_STATUS_HANDLED);

    rut_shell_onscreen_cancel_drag(onscreen);
}

rut_object_t *
rut_drop_event_get_data(rut_input_event_t *drop_event)
{
    return drop_event->native;
}

static rut_input_event_status_t
clipboard_input_grab_cb(rut_input_event_t *event, void *user_data)
{
    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_KEY &&
        rut_key_event_get_action(event) == RUT_KEY_EVENT_ACTION_UP) {
        if (rut_key_event_get_keysym(event) == RUT_KEY_v &&
            (rut_key_event_get_modifier_state(event) & RUT_MODIFIER_CTRL_ON)) {
            rut_shell_t *shell = event->onscreen->shell;
            rut_object_t *data = shell->clipboard;
            rut_mimable_vtable_t *mimable =
                rut_object_get_vtable(data, RUT_TRAIT_ID_MIMABLE);
            rut_object_t *copy = mimable->copy(data);

            _rut_shell_onscreen_paste(event->onscreen, copy);

            return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
set_clipboard(rut_shell_t *shell, rut_object_t *data)
{
    if (shell->clipboard == data)
        return;

    if (shell->clipboard) {
        rut_object_unref(shell->clipboard);

        rut_shell_ungrab_input(shell, clipboard_input_grab_cb, shell);
    }

    if (data) {
        shell->clipboard = rut_object_ref(data);

        rut_shell_grab_input(shell, NULL, clipboard_input_grab_cb, shell);
    } else
        shell->clipboard = NULL;
}

/* While there is an active selection then we grab input
 * to catch Ctrl-X/Ctrl-C/Ctrl-V for cut, copy and paste
 */
static rut_input_event_status_t
selection_input_grab_cb(rut_input_event_t *event, void *user_data)
{
    rut_shell_t *shell = user_data;

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_KEY) {
        if (rut_key_event_get_keysym(event) == RUT_KEY_c &&
            (rut_key_event_get_modifier_state(event) & RUT_MODIFIER_CTRL_ON)) {
            rut_object_t *selection = shell->selection;
            rut_selectable_vtable_t *selectable =
                rut_object_get_vtable(selection, RUT_TRAIT_ID_SELECTABLE);
            rut_object_t *copy = selectable->copy(selection);

            set_clipboard(shell, copy);

            rut_object_unref(copy);

            return RUT_INPUT_EVENT_STATUS_HANDLED;
        } else if (rut_key_event_get_keysym(event) == RUT_KEY_x &&
                   (rut_key_event_get_modifier_state(event) &
                    RUT_MODIFIER_CTRL_ON)) {
            rut_object_t *selection = shell->selection;
            rut_selectable_vtable_t *selectable =
                rut_object_get_vtable(selection, RUT_TRAIT_ID_SELECTABLE);
            rut_object_t *copy = selectable->copy(selection);

            selectable->del(selection);

            set_clipboard(shell, copy);

            rut_object_unref(copy);

            rut_shell_set_selection(shell, NULL);

            return RUT_INPUT_EVENT_STATUS_HANDLED;
        } else if (rut_key_event_get_keysym(event) == RUT_KEY_Delete ||
                   rut_key_event_get_keysym(event) == RUT_KEY_BackSpace) {
            rut_object_t *selection = shell->selection;
            rut_selectable_vtable_t *selectable =
                rut_object_get_vtable(selection, RUT_TRAIT_ID_SELECTABLE);

            selectable->del(selection);

            rut_shell_set_selection(shell, NULL);

            return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

rut_object_t *
rut_shell_get_clipboard(rut_shell_t *shell)
{
    return shell->clipboard;
}

void
rut_shell_set_selection(rut_shell_t *shell, rut_object_t *selection)
{
    if (shell->selection == selection)
        return;

    if (shell->selection) {
        rut_selectable_cancel(shell->selection);

        rut_object_unref(shell->selection);

        rut_shell_ungrab_input(shell, selection_input_grab_cb, shell);
    }

    if (selection) {
        shell->selection = rut_object_ref(selection);

        rut_shell_grab_input(shell, NULL, selection_input_grab_cb, shell);
    } else
        shell->selection = NULL;
}

rut_object_t *
rut_shell_get_selection(rut_shell_t *shell)
{
    return shell->selection;
}

void
rut_shell_onscreen_start_drag(rut_shell_onscreen_t *onscreen,
                              rut_object_t *payload)
{
    rut_shell_t *shell = onscreen->shell;

    c_return_if_fail(shell->drag_payload == NULL);

    if (payload) {
        shell->drag_payload = rut_object_ref(payload);
        shell->drag_onscreen = onscreen;
    }
}

void
rut_shell_onscreen_cancel_drag(rut_shell_onscreen_t *onscreen)
{
    rut_shell_t *shell = onscreen->shell;

    if (shell->drag_payload) {
        cancel_current_drop_offer_taker(onscreen);
        rut_object_unref(shell->drag_payload);
        shell->drag_payload = NULL;
        shell->drag_onscreen = NULL;
    }
}

void
rut_shell_onscreen_take_drop_offer(rut_shell_onscreen_t *onscreen,
                                   rut_object_t *taker)
{
    rut_shell_t *shell = onscreen->shell;

    c_return_if_fail(rut_object_is(taker, RUT_TRAIT_ID_INPUTABLE));

    /* shell->drop_offer_taker is always canceled at the start of
     * _rut_shell_handle_input() so it should always be NULL at
     * this point. */
    c_return_if_fail(shell->drop_offer_taker == NULL);

    c_return_if_fail(taker);

    shell->drop_offer_taker = rut_object_ref(taker);
}

#if 0
rut_closure_t *
rut_shell_add_signal_callback (rut_shell_t *shell,
                               rut_shell_signal_callback_t callback,
                               void *user_data,
                               rut_closure_destroy_callback_t destroy_cb)
{
    return rut_closure_list_add_FIXME (&shell->signal_cb_list,
                                 callback,
                                 user_data,
                                 destroy_cb);
}
#endif

rut_text_direction_t
rut_shell_get_text_direction(rut_shell_t *shell)
{
    return RUT_TEXT_DIRECTION_LEFT_TO_RIGHT;
}

void
rut_shell_set_assets_location(rut_shell_t *shell,
                        const char *assets_location)
{
    shell->assets_location = c_strdup(assets_location);
}

static const char *const *
get_system_data_dirs(void)
{
#ifdef USE_GLIB
    return g_get_system_data_dirs();
#elif defined(__linux__) && !defined(__ANDROID__)
    static char **dirs = NULL;
    if (!dirs) {
        const char *dirs_var = getenv("XDG_DATA_DIRS");
        if (dirs_var)
            dirs = c_strsplit(dirs_var, C_SEARCHPATH_SEPARATOR_S, -1);
        else
            dirs = c_malloc0(sizeof(void *));
    }
    return (const char *const *)dirs;
#elif defined (__ANDROID__)
    static const char *dirs[] = {
        "/data",
        NULL
    };
    return dirs;
#elif defined(__EMSCRIPTEN__)
    static const char *dirs[] = {
        NULL,
    };
    return dirs;
#else
#error "FIXME: Missing platform specific code to locate system data directories"
#endif
}

char *
rut_find_data_file(const char *base_filename)
{
    const char *const *dirs = get_system_data_dirs();
    const char *const *dir;

    for (dir = dirs; *dir; dir++) {
        char *full_path = c_build_filename(*dir, "rig", base_filename, NULL);

        if (c_file_test(full_path, C_FILE_TEST_EXISTS))
            return full_path;

        c_free(full_path);
    }

    return NULL;
}

void
rut_set_thread_current_shell(rut_shell_t *shell)
{
    c_tls_set(&rut_shell_tls, shell);
}

rut_shell_t *
rut_get_thread_current_shell(void)
{
    return c_tls_get(&rut_shell_tls);
}

void
rut_init(void)
{
#ifdef RUT_ENABLE_REFCOUNT_DEBUG
    rut_refcount_debug_init();
#endif

    c_tls_init(&rut_shell_tls, NULL /* destroy */);
}
