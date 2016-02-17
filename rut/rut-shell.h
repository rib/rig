/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012, 2013 Intel Corporation.
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
 */

#pragma once

#include <stdbool.h>

#if defined(USE_SDL)
#include "rut-sdl-keysyms.h"
#include "SDL_syswm.h"
#endif

#if defined(USE_X11)
#include <X11/Xlib.h>
#include <X11/extensions/XKB.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xlib-xcb.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xkbcommon/xkbcommon-compose.h>
#endif

#ifdef USE_UV
#include <uv.h>
#endif

#ifdef USE_GLIB
#include <glib.h>
#endif

#include <cglib/cglib.h>
#ifdef USE_PANGO
#include <cogl-pango/cogl-pango.h>
#endif

#include "rut-settings.h"
#include "rut-matrix-stack.h"
#include "rut-keysyms.h"
#include "rut-object.h"
#include "rut-types.h"
#include "rut-closure.h"
#include "rut-poll.h"

typedef void (*rut_shell_init_callback_t)(rut_shell_t *shell, void *user_data);
typedef void (*rut_shell_fini_callback_t)(rut_shell_t *shell, void *user_data);
typedef void (*rut_shell_paint_callback_t)(rut_shell_t *shell, void *user_data);

typedef enum {
    RUT_CURSOR_DEFAULT,
    RUT_CURSOR_INVISIBLE,
    RUT_CURSOR_ARROW,
    RUT_CURSOR_IBEAM,
    RUT_CURSOR_WAIT,
    RUT_CURSOR_CROSSHAIR,
    RUT_CURSOR_SIZE_WE,
    RUT_CURSOR_SIZE_NS
} rut_cursor_t;

typedef struct {
    c_list_t link;

    rut_shell_t *shell;

    int width;
    int height;
    bool resizable;

    cg_onscreen_t *cg_onscreen;

    cg_onscreen_resize_closure_t *resize_closure;

    int64_t presentation_time_earlier;
    int64_t presentation_time_latest;

    /* When predicting the target timestamp for the next frame it
     * important to know how old the historic presentation timestamps
     * are so we can multiply the predicited delta for one frame by
     * the number of in-flight frames to determine a target timestamp
     * for the next frame.
     */
    int64_t presentation_time_latest_frame;

    /* A circular history of frame presentation deltas, which we take
     * the median of to determine whether we're failing to keep up
     * with the target fps. */
    int64_t presentation_deltas[11];

    /* monotonically increments (not limited by _deltas[] array len as
     * the modulus is done when recording a new delta). It can
     * thus also be used to tell when the array is not yet full */
    uint32_t presentation_delta_index;

    /* As a fallback for predicting how to progress time if
     * we don't have presentation times to go on then we
     * may know the refresh rate of the monitor we're
     * displaying on... */
    float refresh_rate;

    bool is_dirty; /* true if we need to redraw something */
    bool is_ready; /* indicates that the onscreen is not tied
                    * up in the display pipeline. It's updated
                    * based on compositor or swap events to
                    * throttle to the display refresh rate */

    rut_cursor_t current_cursor;
    /* This is used to record whether anything set a cursor while
     * handling a mouse motion event. If nothing sets one then the shell
     * will put the cursor back to the default pointer. */
    bool cursor_set;

    bool fullscreen;

    rut_object_t *input_camera;

    union {
#ifdef USE_SDL
        struct {
            SDL_SysWMinfo sdl_info;
            SDL_Window *sdl_window;
            SDL_Cursor *sdl_cursor_image;
        } sdl;
#endif
#ifdef USE_X11
        struct {
            Window *xwindow;
            XID frame_sync_xcounters[2];
            int64_t frame_sync_counters[2];
            int64_t last_sync_request_value;
        } x11;
#endif
        int dummy;
    };

    c_list_t frame_closures;
} rut_shell_onscreen_t;

typedef enum _rut_input_event_type_t {
    RUT_INPUT_EVENT_TYPE_MOTION = 1,
    RUT_INPUT_EVENT_TYPE_KEY,
    RUT_INPUT_EVENT_TYPE_TEXT,
    RUT_INPUT_EVENT_TYPE_DROP_OFFER,
    RUT_INPUT_EVENT_TYPE_DROP_CANCEL,
    RUT_INPUT_EVENT_TYPE_DROP
} rut_input_event_type_t;

typedef enum _rut_key_event_action_t {
    RUT_KEY_EVENT_ACTION_UP = 1,
    RUT_KEY_EVENT_ACTION_DOWN
} rut_key_event_action_t;

typedef enum _rut_motion_event_action_t {
    RUT_MOTION_EVENT_ACTION_UP = 1,
    RUT_MOTION_EVENT_ACTION_DOWN,
    RUT_MOTION_EVENT_ACTION_MOVE,
} rut_motion_event_action_t;

/* XXX: Note the X11 shell backend at least currently
 * relies on the bit mask corresponding to 1 << button number
 */
typedef enum _rut_button_state_t {
    RUT_BUTTON_STATE_1 = 1 << 1,
    RUT_BUTTON_STATE_2 = 1 << 2,
    RUT_BUTTON_STATE_3 = 1 << 3,
} rut_button_state_t;

typedef enum _rut_modifier_state_t {
    RUT_MODIFIER_SHIFT_ON = 1 << 0,
    RUT_MODIFIER_CTRL_ON = 1 << 1,
    RUT_MODIFIER_ALT_ON = 1 << 2,
    RUT_MODIFIER_NUM_LOCK_ON = 1 << 3,
    RUT_MODIFIER_CAPS_LOCK_ON = 1 << 4
} rut_modifier_state_t;

#define RUT_N_MODIFIERS 5

typedef enum _rut_input_event_status_t {
    RUT_INPUT_EVENT_STATUS_UNHANDLED,
    RUT_INPUT_EVENT_STATUS_HANDLED,
} rut_input_event_status_t;

typedef struct _rut_input_event_t {
    c_list_t list_node;
    rut_input_event_type_t type;
    rut_shell_onscreen_t *onscreen;
    rut_object_t *camera_entity;
    const c_matrix_t *input_transform;

    void *native;

    uint8_t data[];
} rut_input_event_t;

/* stream_t events are optimized for IPC based on the assumption that
 * the remote process does some state tracking to know the current
 * state of pointer buttons for example.
 */
typedef enum _rut_stream_event_type_t {
    RUT_STREAM_EVENT_POINTER_MOVE = 1,
    RUT_STREAM_EVENT_POINTER_DOWN,
    RUT_STREAM_EVENT_POINTER_UP,
    RUT_STREAM_EVENT_KEY_DOWN,
    RUT_STREAM_EVENT_KEY_UP
} rut_stream_event_type_t;

typedef struct _rut_stream_event_t {
    rut_stream_event_type_t type;
    rut_object_t *camera_entity;
    uint64_t timestamp;

    union {
        struct {
            rut_button_state_t state;
            rut_modifier_state_t mod_state;
            float x;
            float y;
        } pointer_move;
        struct {
            rut_button_state_t state;
            rut_button_state_t button;
            rut_modifier_state_t mod_state;
            float x;
            float y;
        } pointer_button;
        struct {
            int keysym;
            rut_modifier_state_t mod_state;
        } key;
    };
} rut_stream_event_t;

typedef rut_input_event_status_t (*rut_input_callback_t)(
    rut_input_event_t *event, void *user_data);

typedef struct {
    c_list_t list_node;
    rut_input_callback_t callback;
    rut_object_t *camera_entity;
    void *user_data;
} rut_shell_grab_t;

typedef void (*RutPrePaintCallback)(rut_object_t *graphable, void *user_data);

typedef struct {
    c_list_t list_node;

    int depth;
    rut_object_t *graphable;

    RutPrePaintCallback callback;
    void *user_data;
} rut_shell_pre_paint_entry_t;

typedef struct _rut_input_queue_t {
    rut_shell_t *shell;
    c_list_t events;
    int n_events;
} rut_input_queue_t;

#ifdef USE_X11
struct rut_mod_index_mapping
{
    rut_modifier_state_t mod;
    int mod_index;
};
#endif

struct _rut_shell_t {
    rut_object_base_t _base;

    /* If true then this process does not handle input events directly
     * or output graphics directly. */
    bool headless;
    rut_shell_onscreen_t *headless_onscreen;

#ifdef USE_SDL
    SDL_SYSWM_TYPE sdl_subsystem;
    SDL_Keymod sdl_keymod;
    uint32_t sdl_buttons;
    bool x11_grabbed;

    /* Note we can't use SDL_WaitEvent() to block for events given
     * that we also care about non-SDL based events.
     *
     * This lets us to use poll() to block until an SDL event
     * is recieved instead of busy waiting...
     */
    SDL_mutex *event_pipe_mutex;
    int event_pipe_read;
    int event_pipe_write;
    bool wake_queued;
#endif

#ifdef USE_X11
    Display *xdpy;
    xcb_connection_t *xcon;
    rut_poll_source_t *x11_poll_source;

    int xsync_event;
    int xsync_error;
    int xsync_major;
    int xsync_minor;

    int xi2_opcode;
    int xi2_event;
    int xi2_error;
    int xi2_major;
    int xi2_minor;

    int xkb_opcode;
    int xkb_event;
    int xkb_error;
    int xkb_major;
    int xkb_minor;
    //XkbDesc *xkb_desc;
    uint32_t xkb_core_device_id;
    struct xkb_context *xkb_ctx;
    struct xkb_keymap *xkb_keymap;
    struct xkb_state *xkb_state;
    struct xkb_compose_state *xkb_compose_state;
    //unsigned int xkb_mod_state_event_serial;
    //rut_modifier_state_t xkb_mod_state_cached;
    struct rut_mod_index_mapping xkb_mod_index_map[RUT_N_MODIFIERS];

    int hmd_output_id;

    bool net_wm_frame_drawn_supported;
#if 0
    int x11_min_keycode;
    int x11_max_keycode;
    XModifierKeymap *x11_mod_map;
    KeySym *x11_keymap;
    int x11_keysyms_per_keycode;
#endif
#endif

#ifdef __ANDROID__
    struct android_app *android_application;
#endif

#ifndef RIG_SIMULATOR_ONLY
    c_array_t *cg_poll_fds;
    int cg_poll_fds_age;
#endif

    int poll_sources_age;
    c_list_t poll_sources;

    c_list_t idle_closures;

#if 0
    int signal_read_fd;
    c_list_t signal_cb_list;
#endif

    /* When running multiple shells in one thread we define one shell
     * as the "main" shell which owns the mainloop.
     */
    rut_shell_t *main_shell;

#ifdef USE_UV
    uv_loop_t *uv_loop;
    uv_idle_t uv_idle;
    uv_prepare_t cg_prepare;
    uv_timer_t cg_timer;
    uv_check_t cg_timer_check;

    uv_signal_t sigchild_handle;
    c_list_t sigchild_closures;

#ifdef __ANDROID__
    int uv_ready;
    bool quit;
#endif
#ifdef USE_GLIB
    GMainContext *glib_main_ctx;
    uv_prepare_t glib_uv_prepare;
    uv_check_t glib_uv_check;
    uv_timer_t glib_uv_timer;
    uv_check_t glib_uv_timer_check;
    GArray *pollfds;
    GArray *glib_polls;
    int n_pollfds;
#endif
#endif

    rut_settings_t *settings;

    cg_renderer_t *cg_renderer;
    cg_device_t *cg_device;

    rut_matrix_entry_t identity_entry;
    c_matrix_t identity_matrix;

    char *assets_location;

    c_hash_table_t *texture_cache;
    cg_pipeline_t *single_texture_2d_template;
    cg_texture_t *circle_texture;

    cg_indices_t *nine_slice_indices;

    c_hash_table_t *colors_hash;

#ifdef USE_PANGO
    CgPangoFontMap *pango_font_map;
    PangoContext *pango_context;
    PangoFontDescription *pango_font_desc;
#endif

    rut_property_context_t property_ctx;

#ifdef __EMSCRIPTEN__
    bool paint_loop_running;
#else
    rut_closure_t paint_idle;
#endif

    rut_input_queue_t *input_queue;
    int input_queue_len;

    void (*on_run_cb)(rut_shell_t *shell, void *user_data);
    void *on_run_data;
    bool running;

    void (*on_quit_cb)(rut_shell_t *shell, void *user_data);
    void *on_quit_data;

    rut_shell_paint_callback_t paint_cb;
    void *user_data;

    c_list_t input_cb_list;

    /* Last known position of the mouse */
    float mouse_x;
    float mouse_y;

    rut_object_t *drag_payload;
    rut_object_t *drop_offer_taker;
    rut_shell_onscreen_t *drag_onscreen;

    /* List of grabs that are currently in place. This are in order from
     * highest to lowest priority. */
    c_list_t grabs;
    /* A pointer to the next grab to process. This is only used while
     * invoking the grab callbacks so that we can cope with multiple
     * grabs being removed from the list while one is being processed */
    rut_shell_grab_t *next_grab;

    rut_object_t *clipboard;

    void (*queue_redraw_callback)(rut_shell_t *shell, void *user_data);
    void *queue_redraw_data;

    /* A list of onscreen windows that the shell is manipulating */
    c_list_t onscreens;

    rut_object_t *selection;

    struct {
        enum {
            RUT_SHELL_HEADLESS = 1,
            RUT_SHELL_X11_PLATFORM,
            RUT_SHELL_ANDROID_PLATFORM,
            RUT_SHELL_WEB_PLATFORM,
            RUT_SHELL_SDL_PLATFORM,
        } type;


        bool (*check_for_hmd)(rut_shell_t *shell);

        cg_onscreen_t *(*allocate_onscreen)(rut_shell_onscreen_t *onscreen);
        void (*onscreen_resize)(rut_shell_onscreen_t *onscreen,
                                int width, int height);
        void (*onscreen_set_title)(rut_shell_onscreen_t *onscreen,
                                   const char *title);
        void (*onscreen_set_cursor)(rut_shell_onscreen_t *onscreen,
                                    rut_cursor_t cursor);
        void (*onscreen_set_fullscreen)(rut_shell_onscreen_t *onscreen,
                                        bool fullscreen);

        int32_t (*key_event_get_keysym)(rut_input_event_t *event);
        rut_key_event_action_t (*key_event_get_action)(rut_input_event_t *event);
        rut_modifier_state_t (*key_event_get_modifier_state)(rut_input_event_t *event);

        rut_motion_event_action_t (*motion_event_get_action)(rut_input_event_t *event);
        rut_button_state_t (*motion_event_get_button)(rut_input_event_t *event);
        rut_button_state_t (*motion_event_get_button_state)(rut_input_event_t *event);
        rut_modifier_state_t (*motion_event_get_modifier_state)(rut_input_event_t *event);
        void (*motion_event_get_transformed_xy)(rut_input_event_t *event,
                                                float *x,
                                                float *y);

        const char *(*text_event_get_text)(rut_input_event_t *event);

        void (*free_input_event)(rut_input_event_t *event);

        void (*cleanup)(rut_shell_t *shell);
    } platform;
};

typedef enum _rut_input_transform_type_t {
    RUT_INPUT_TRANSFORM_TYPE_NONE,
    RUT_INPUT_TRANSFORM_TYPE_MATRIX,
    RUT_INPUT_TRANSFORM_TYPE_GRAPHABLE
} rut_input_transform_type_t;

typedef struct _rut_input_transform_any_t {
    rut_input_transform_type_t type;
} rut_input_transform_any_t;

typedef struct _rut_input_transform_matrix_t {
    rut_input_transform_type_t type;
    c_matrix_t *matrix;
} rut_input_transform_matrix_t;

typedef struct _rut_input_transform_graphable_t {
    rut_input_transform_type_t type;
} rut_input_transform_graphable_t;

typedef union _rut_input_transform_t {
    rut_input_transform_any_t any;
    rut_input_transform_matrix_t matrix;
    rut_input_transform_graphable_t graphable;
} rut_input_transform_t;

rut_shell_t *rut_shell_new(rut_shell_t *main_shell,
                           rut_shell_paint_callback_t paint,
                           void *user_data);

/* When running a shell for a simulator we don't need any graphics support... */
void rut_shell_set_is_headless(rut_shell_t *shell, bool headless);

void rut_shell_set_on_run_callback(rut_shell_t *shell,
                                   void (*callback)(rut_shell_t *shell, void *data),
                                   void *user_data);

void rut_shell_set_on_quit_callback(rut_shell_t *shell,
                                    void (*callback)(rut_shell_t *shell, void *data),
                                    void *user_data);

#ifdef USE_UV
uv_loop_t *rut_uv_shell_get_loop(rut_shell_t *shell);
#endif

#ifdef __ANDROID__
void rut_android_shell_set_application(rut_shell_t *shell,
                                       struct android_app *application);
#endif

bool rut_shell_get_headless(rut_shell_t *shell);

void rut_shell_main(rut_shell_t *shell);

/* Should be the first thing called by the shell paint callback to
 * ensure the callback is only fired once */
void rut_shell_remove_paint_idle(rut_shell_t *shell);

void rut_shell_dispatch_input_events(rut_shell_t *shell);

/* Dispatch a single event as rut_shell_dispatch_input_events would */
rut_input_event_status_t
rut_shell_dispatch_input_event(rut_shell_t *shell, rut_input_event_t *event);

rut_input_queue_t *rut_input_queue_new(rut_shell_t *shell);

void rut_input_queue_append(rut_input_queue_t *queue, rut_input_event_t *event);

void rut_input_queue_remove(rut_input_queue_t *queue, rut_input_event_t *event);

void rut_input_queue_clear(rut_input_queue_t *queue);

rut_input_queue_t *rut_shell_get_input_queue(rut_shell_t *shell);

/**
 * rut_shell_grab_input:
 * @shell: The #rut_shell_t
 * @camera: An optional camera to set on grabbed events
 * @callback: A callback to give all events to
 * @user_data: A pointer to pass to the callback
 *
 * Adds a grab which will get a chance to see all input events before
 * anything else handles them. The callback can return
 * %RUT_INPUT_EVENT_STATUS_HANDLED if no further processing should be
 * done for the event or %RUT_INPUT_EVENT_STATUS_UNHANDLED otherwise.
 *
 * Multiple grabs can be in place at the same time. In this case the
 * events will be given to the grabs in the reverse order that they
 * were added.
 *
 * If a camera is given for the grab then this camera will be set on
 * all input events before passing them to the callback.
 */
void rut_shell_grab_input(rut_shell_t *shell,
                          rut_object_t *camera_entity,
                          rut_input_event_status_t (*callback)(
                              rut_input_event_t *event, void *user_data),
                          void *user_data);

/**
 * rut_shell_ungrab_input:
 * @shell: The #rut_shell_t
 * @callback: The callback that the grab was set with
 * @user_data: The user data that the grab was set with
 *
 * Removes a grab that was previously set with rut_shell_grab_input().
 * The @callback and @user_data must match the values passed when the
 * grab was taken.
 */
void rut_shell_ungrab_input(rut_shell_t *shell,
                            rut_input_event_status_t (*callback)(
                                rut_input_event_t *event, void *user_data),
                            void *user_data);

/*
 * Use this API for the common case of grabbing input for a pointer
 * when we receive a button press and want to grab until a
 * corresponding button release.
 */
void rut_shell_grab_pointer(rut_shell_t *shell,
                            rut_object_t *camera,
                            rut_input_event_status_t (*callback)(
                                rut_input_event_t *event, void *user_data),
                            void *user_data);

/**
 * rut_shell_grab_key_focus:
 * @shell: The #rut_shell_t
 * @inputable: A #rut_object_t that implements the inputable interface
 * @ungrab_callback: A callback to invoke when the grab is lost
 *
 * Sets the shell's key grab to the given object. The object must
 * implement the inputable interface. All key events will be given to
 * the input region of this object until either something else takes
 * the key focus or rut_shell_ungrab_key_focus() is called.
 */
void rut_shell_grab_key_focus(rut_shell_t *shell,
                              rut_object_t *inputable,
                              c_destroy_func_t ungrab_callback);

void rut_shell_ungrab_key_focus(rut_shell_t *shell);

void rut_shell_queue_redraw(rut_shell_t *shell);

bool rut_shell_is_redraw_queued(rut_shell_t *shell);

void rut_shell_set_queue_redraw_callback(rut_shell_t *shell,
                                         void (*callback)(rut_shell_t *shell,
                                                          void *user_data),
                                         void *user_data);

/* You can hook into rut_shell_queue_redraw() via
 * rut_shell_set_queue_redraw_callback() but then it if you really
 * want to queue a redraw with the platform mainloop it is your
 * responsibility to call rut_shell_queue_redraw_real() */
void rut_shell_queue_redraw_real(rut_shell_t *shell);

rut_object_t *rut_input_event_get_camera(rut_input_event_t *event);

rut_input_event_type_t rut_input_event_get_type(rut_input_event_t *event);

/**
 * rut_input_event_get_onscreen:
 * @event: A #rut_input_event_t
 *
 * Return value: the onscreen window that this event was generated for
 *   or %NULL if the event does not correspond to a window.
 */
rut_shell_onscreen_t *rut_input_event_get_onscreen(rut_input_event_t *event);

rut_key_event_action_t rut_key_event_get_action(rut_input_event_t *event);

int32_t rut_key_event_get_keysym(rut_input_event_t *event);

rut_modifier_state_t rut_key_event_get_modifier_state(rut_input_event_t *event);

rut_motion_event_action_t rut_motion_event_get_action(rut_input_event_t *event);

/* For a button-up/down event which specific button changed? */
rut_button_state_t rut_motion_event_get_button(rut_input_event_t *event);

rut_button_state_t rut_motion_event_get_button_state(rut_input_event_t *event);

rut_modifier_state_t
rut_motion_event_get_modifier_state(rut_input_event_t *event);

float rut_motion_event_get_x(rut_input_event_t *event);

float rut_motion_event_get_y(rut_input_event_t *event);

/**
 * rut_motion_event_unproject:
 * @event: A motion event
 * @graphable: An object that implements #rut_graph_table
 * @x: Output location for the unprojected coordinate
 * @y: Output location for the unprojected coordinate
 *
 * Unprojects the position of the motion event so that it will be
 * relative to the coordinate space of the given graphable object.
 *
 * Return value: %false if the coordinate can't be unprojected or
 *   %true otherwise. The coordinate can't be unprojected if the
 *   transform for the graphable object object does not have an inverse.
 */
bool rut_motion_event_unproject(rut_input_event_t *event,
                                rut_object_t *graphable,
                                float *x,
                                float *y);

rut_object_t *rut_drop_offer_event_get_payload(rut_input_event_t *event);

/* Returns the text generated by the event as a null-terminated UTF-8
 * encoded string. */
const char *rut_text_event_get_text(rut_input_event_t *event);

rut_object_t *rut_drop_event_get_data(rut_input_event_t *drop_event);

rut_closure_t *
rut_shell_add_input_callback(rut_shell_t *shell,
                             rut_input_callback_t callback,
                             void *user_data,
                             rut_closure_destroy_callback_t destroy_cb);

rut_shell_onscreen_t *
rut_shell_onscreen_new(rut_shell_t *shell,
                       int width, int height);

/* TODO: accept rut_exception_t */
bool
rut_shell_onscreen_allocate(rut_shell_onscreen_t *onscreen);

void
rut_shell_onscreen_set_resizable(rut_shell_onscreen_t *onscreen,
                                 bool resizable);

void
rut_shell_onscreen_resize(rut_shell_onscreen_t *onscreen,
                          int width,
                          int height);

/**
 * rut_shell_set_cursor:
 * @shell: The #rut_shell_t
 * @onscreen: An onscreen window to set the cursor for
 * @cursor: The new cursor
 *
 * Attempts to set the mouse cursor image to @cursor. The shell will
 * automatically reset the cursor back to the default pointer on every
 * mouse motion event if nothing else sets it. The expected usage is
 * that a widget which wants a particular cursor will listen for
 * motion events and always change the cursor when the pointer is over
 * a certain area.
 */
void rut_shell_onscreen_set_cursor(rut_shell_onscreen_t *onscreen,
                                   rut_cursor_t cursor);

void rut_shell_onscreen_set_title(rut_shell_onscreen_t *onscreen,
                                  const char *title);

void rut_shell_onscreen_set_fullscreen(rut_shell_onscreen_t *onscreen,
                                       bool fullscreen);

bool rut_shell_onscreen_get_fullscreen(rut_shell_onscreen_t *onscreen);

void rut_shell_onscreen_show(rut_shell_onscreen_t *onscreen);

void rut_shell_onscreen_set_input_camera(rut_shell_onscreen_t *onscreen,
                                         rut_object_t *camera);

rut_object_t *rut_shell_onscreen_get_input_camera(rut_shell_onscreen_t *onscreen);

/**
 * rut_shell_quit:
 * @shell: The #rut_shell_t
 *
 * Informs the shell that at the next time it returns to the main loop
 * it should quit the loop.
 */
void rut_shell_quit(rut_shell_t *shell);

/**
 * rut_shell_set_selection:
 * @selection: An object implementing the selectable interface
 *
 * This cancels any existing global selection, calling the ::cancel
 * method of the current selection and make @selection the replacement
 * selection.
 *
 * If Ctrl-C is later pressed then ::copy will be called for the
 * selectable and the returned object will be set on the clipboard.
 * If Ctrl-Z is later pressed then ::cut will be called for the
 * selectable and the returned object will be set on the clipboard.
 */
void rut_shell_set_selection(rut_shell_t *shell, rut_object_t *selection);

rut_object_t *rut_shell_get_selection(rut_shell_t *shell);

rut_object_t *rut_shell_get_clipboard(rut_shell_t *shell);

extern rut_type_t rut_text_blob_type;

typedef struct _rut_text_blob_t rut_text_blob_t;

rut_text_blob_t *rut_text_blob_new(const char *text);

void rut_shell_onscreen_start_drag(rut_shell_onscreen_t *onscreen,
                                   rut_object_t *payload);

void rut_shell_onscreen_cancel_drag(rut_shell_onscreen_t *onscreen);

void rut_shell_onscreen_drop(rut_shell_onscreen_t *onscreen);

void rut_shell_onscreen_take_drop_offer(rut_shell_onscreen_t *onscreen,
                                        rut_object_t *taker);

#if 0
typedef void (*rut_shell_signal_callback_t) (rut_shell_t *shell,
                                             int signal,
                                             void *user_data);

rut_closure_t *
rut_shell_add_signal_callback (rut_shell_t *shell,
                               rut_shell_signal_callback_t callback,
                               void *user_data,
                               rut_closure_destroy_callback_t destroy_cb);
#endif

#ifdef USE_SDL
void rut_shell_handle_sdl_event(rut_shell_t *shell, SDL_Event *sdl_event);
#endif


/* XXX: Find a better place for this to live... */
extern uint8_t _rut_nine_slice_indices_data[54];

rut_text_direction_t rut_shell_get_text_direction(rut_shell_t *shell);

void rut_shell_set_assets_location(rut_shell_t *shell,
                                   const char *assets_location);

char *rut_find_data_file(const char *base_filename);

void rut_init(void);

void rut_set_thread_current_shell(rut_shell_t *shell);
rut_shell_t *rut_get_thread_current_shell(void);

void rut_shell_paint(rut_shell_t *shell);

void rut_shell_onscreen_begin_frame(rut_shell_onscreen_t *onscreen);
