/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
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
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#include <cglib-config.h>

#include <stdlib.h>
#include <string.h>

#include "cg-util.h"
#include "cg-private.h"
#include "cg-object.h"
#include "cg-device-private.h"
#include "cg-util-gl-private.h"

#include "cg-renderer.h"
#include "cg-renderer-private.h"
#include "cg-display-private.h"
#include "cg-winsys-private.h"
#include "cg-winsys-stub-private.h"
#include "cg-config-private.h"
#include "cg-error-private.h"

#ifdef CG_HAS_WEBGL_SUPPORT
#include "cg-winsys-webgl-private.h"
#endif
#ifdef CG_HAS_EGL_PLATFORM_XLIB_SUPPORT
#include "cg-winsys-egl-x11-private.h"
#endif
#ifdef CG_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
#include "cg-winsys-egl-wayland-private.h"
#endif
#ifdef CG_HAS_EGL_PLATFORM_KMS_SUPPORT
#include "cg-winsys-egl-kms-private.h"
#endif
#ifdef CG_HAS_EGL_PLATFORM_ANDROID_SUPPORT
#include "cg-winsys-egl-android-private.h"
#endif
#ifdef CG_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT
#include "cg-winsys-egl-null-private.h"
#endif
#ifdef CG_HAS_GLX_SUPPORT
#include "cg-winsys-glx-private.h"
#endif
#ifdef CG_HAS_WGL_SUPPORT
#include "cg-winsys-wgl-private.h"
#endif
#ifdef CG_HAS_SDL_SUPPORT
#include "cg-winsys-sdl-private.h"
#endif

#ifdef CG_HAS_XLIB_SUPPORT
#include "cg-xlib-renderer.h"
#endif

typedef const cg_winsys_vtable_t *(*cg_winsys_vtable_getter_t)(void);

#ifdef CG_HAS_GL_SUPPORT
extern const cg_texture_driver_t _cg_texture_driver_gl;
extern const cg_driver_vtable_t _cg_driver_gl;
#endif
#ifdef CG_HAS_GLES2_SUPPORT
extern const cg_texture_driver_t _cg_texture_driver_gles;
extern const cg_driver_vtable_t _cg_driver_gles;
#endif

extern const cg_driver_vtable_t _cg_driver_nop;

typedef struct _cg_driver_description_t {
    cg_driver_t id;
    const char *name;
    cg_renderer_constraint_t constraints;
    /* It would be nice to make this a pointer and then use a compound
     * literal from C99 to initialise it but we probably can't get away
     * with using C99 here. Instead we'll just use a fixed-size array.
     * GCC should complain if someone adds an 8th feature to a
     * driver. */
    const cg_private_feature_t private_features[8];
    const cg_driver_vtable_t *vtable;
    const cg_texture_driver_t *texture_driver;
    const char *libgl_name;
} cg_driver_description_t;

static cg_driver_description_t _cg_drivers[] = {
#ifdef CG_HAS_GL_SUPPORT
    { CG_DRIVER_GL3,
      "gl3",
      0,
      { CG_PRIVATE_FEATURE_ANY_GL, CG_PRIVATE_FEATURE_GL_PROGRAMMABLE, -1 },
      &_cg_driver_gl,
      &_cg_texture_driver_gl,
      CG_GL_LIBNAME, },
    { CG_DRIVER_GL,
      "gl",
      0,
      { CG_PRIVATE_FEATURE_ANY_GL, CG_PRIVATE_FEATURE_GL_PROGRAMMABLE, -1 },
      &_cg_driver_gl,
      &_cg_texture_driver_gl,
      CG_GL_LIBNAME, },
#endif
#ifdef CG_HAS_GLES2_SUPPORT
    { CG_DRIVER_GLES2, "gles2", CG_RENDERER_CONSTRAINT_SUPPORTS_CG_GLES2,
      { CG_PRIVATE_FEATURE_ANY_GL,          CG_PRIVATE_FEATURE_GL_EMBEDDED,
        CG_PRIVATE_FEATURE_GL_PROGRAMMABLE, -1 },
      &_cg_driver_gles, &_cg_texture_driver_gles, CG_GLES2_LIBNAME, },
#endif
#ifdef EMSCRIPTEN
    { CG_DRIVER_WEBGL, "webgl", 0,
      { CG_PRIVATE_FEATURE_ANY_GL,          CG_PRIVATE_FEATURE_GL_EMBEDDED,
        CG_PRIVATE_FEATURE_GL_PROGRAMMABLE, CG_PRIVATE_FEATURE_GL_WEB,
        -1 },
      &_cg_driver_gles, &_cg_texture_driver_gles, NULL, },
#endif
    { CG_DRIVER_NOP,
      "nop",
      0, /* constraints satisfied */
      { -1 },
      &_cg_driver_nop,
      NULL, /* texture driver */
      NULL /* libgl_name */
    }
};

static cg_winsys_vtable_getter_t _cg_winsys_vtable_getters[] = {
#ifdef CG_HAS_WEBGL_SUPPORT
    _cg_winsys_webgl_get_vtable,
#endif
#ifdef CG_HAS_GLX_SUPPORT
    _cg_winsys_glx_get_vtable,
#endif
#ifdef CG_HAS_EGL_PLATFORM_XLIB_SUPPORT
    _cg_winsys_egl_xlib_get_vtable,
#endif
#ifdef CG_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
    _cg_winsys_egl_wayland_get_vtable,
#endif
#ifdef CG_HAS_EGL_PLATFORM_KMS_SUPPORT
    _cg_winsys_egl_kms_get_vtable,
#endif
#ifdef CG_HAS_EGL_PLATFORM_ANDROID_SUPPORT
    _cg_winsys_egl_android_get_vtable,
#endif
#ifdef CG_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT
    _cg_winsys_egl_null_get_vtable,
#endif
#ifdef CG_HAS_WGL_SUPPORT
    _cg_winsys_wgl_get_vtable,
#endif
#ifdef CG_HAS_SDL_SUPPORT
    _cg_winsys_sdl_get_vtable,
#endif
    _cg_winsys_stub_get_vtable,
};

static void _cg_renderer_free(cg_renderer_t *renderer);

CG_OBJECT_DEFINE(Renderer, renderer);

typedef struct _cg_native_filter_closure_t {
    cg_native_filter_func_t func;
    void *data;
} cg_native_filter_closure_t;

uint32_t
cg_renderer_error_domain(void)
{
    return c_quark_from_static_string("cg-renderer-error-quark");
}

static const cg_winsys_vtable_t *
_cg_renderer_get_winsys(cg_renderer_t *renderer)
{
    return renderer->winsys_vtable;
}

static void
native_filter_closure_free(cg_native_filter_closure_t *closure)
{
    c_slice_free(cg_native_filter_closure_t, closure);
}

static void
_cg_renderer_free(cg_renderer_t *renderer)
{
    const cg_winsys_vtable_t *winsys = _cg_renderer_get_winsys(renderer);

    _cg_closure_list_disconnect_all(&renderer->idle_closures);

    if (winsys)
        winsys->renderer_disconnect(renderer);

#ifndef HAVE_DIRECTLY_LINKED_GL_LIBRARY
    if (renderer->libgl_module)
        c_module_close(renderer->libgl_module);
#endif

    c_sllist_foreach(renderer->event_filters,
                    (c_iter_func_t)native_filter_closure_free, NULL);
    c_sllist_free(renderer->event_filters);

    c_array_free(renderer->poll_fds, true);

    c_free(renderer);
}

cg_renderer_t *
cg_renderer_new(void)
{
    cg_renderer_t *renderer = c_new0(cg_renderer_t, 1);

    _cg_init();

    renderer->connected = false;
    renderer->event_filters = NULL;

    renderer->poll_fds = c_array_new(false, true, sizeof(cg_poll_fd_t));

    c_list_init(&renderer->idle_closures);

#ifdef CG_HAS_XLIB_SUPPORT
    renderer->xlib_enable_event_retrieval = true;
#endif

#ifdef CG_HAS_WIN32_SUPPORT
    renderer->win32_enable_event_retrieval = true;
#endif

#ifdef CG_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
    renderer->wayland_enable_event_dispatch = true;
#endif

#ifdef CG_HAS_EGL_PLATFORM_KMS_SUPPORT
    renderer->kms_fd = -1;
#endif

    return _cg_renderer_object_new(renderer);
}

#ifdef CG_HAS_XLIB_SUPPORT
void
cg_xlib_renderer_set_foreign_display(cg_renderer_t *renderer,
                                     Display *xdisplay)
{
    c_return_if_fail(cg_is_renderer(renderer));

    /* NB: Renderers are considered immutable once connected */
    c_return_if_fail(!renderer->connected);

    renderer->foreign_xdpy = xdisplay;

    /* If the application is using a foreign display then we can assume
       it will also do its own event retrieval */
    cg_xlib_renderer_set_event_retrieval_enabled(renderer, false);
}

Display *
cg_xlib_renderer_get_foreign_display(cg_renderer_t *renderer)
{
    c_return_val_if_fail(cg_is_renderer(renderer), NULL);

    return renderer->foreign_xdpy;
}

void
cg_xlib_renderer_set_event_retrieval_enabled(cg_renderer_t *renderer,
                                             bool enable)
{
    c_return_if_fail(cg_is_renderer(renderer));
    /* NB: Renderers are considered immutable once connected */
    c_return_if_fail(!renderer->connected);

    renderer->xlib_enable_event_retrieval = enable;
}
#endif /* CG_HAS_XLIB_SUPPORT */

bool
cg_renderer_check_onscreen_template(cg_renderer_t *renderer,
                                    cg_onscreen_template_t *onscreen_template,
                                    cg_error_t **error)
{
    cg_display_t *display;

    if (!cg_renderer_connect(renderer, error))
        return false;

    display = cg_display_new(renderer, onscreen_template);
    if (!cg_display_setup(display, error)) {
        cg_object_unref(display);
        return false;
    }

    cg_object_unref(display);

    return true;
}

typedef bool (*driver_callback_t)(cg_driver_description_t *description,
                                  void *user_data);

static void
foreach_driver_description(cg_driver_t driver_override,
                           driver_callback_t callback,
                           void *user_data)
{
#ifdef CG_DEFAULT_DRIVER
    const cg_driver_description_t *default_driver = NULL;
#endif
    int i;

    if (driver_override != CG_DRIVER_ANY) {
        for (i = 0; i < C_N_ELEMENTS(_cg_drivers); i++) {
            if (_cg_drivers[i].id == driver_override) {
                callback(&_cg_drivers[i], user_data);
                return;
            }
        }

        c_warn_if_reached();
        return;
    }

#ifdef CG_DEFAULT_DRIVER
    for (i = 0; i < C_N_ELEMENTS(_cg_drivers); i++) {
        const cg_driver_description_t *desc = &_cg_drivers[i];
        if (c_ascii_strcasecmp(desc->name, CG_DEFAULT_DRIVER) == 0) {
            default_driver = desc;
            break;
        }
    }

    if (default_driver) {
        if (!callback(default_driver, user_data))
            return;
    }
#endif

    for (i = 0; i < C_N_ELEMENTS(_cg_drivers); i++) {
#ifdef CG_DEFAULT_DRIVER
        if (&_cg_drivers[i] == default_driver)
            continue;
#endif

        if (!callback(&_cg_drivers[i], user_data))
            return;
    }
}

static cg_driver_t
driver_name_to_id(const char *name)
{
    int i;

    for (i = 0; i < C_N_ELEMENTS(_cg_drivers); i++) {
        if (c_ascii_strcasecmp(_cg_drivers[i].name, name) == 0)
            return _cg_drivers[i].id;
    }

    return CG_DRIVER_ANY;
}

static const char *
driver_id_to_name(cg_driver_t id)
{
    switch (id) {
    case CG_DRIVER_GL:
        return "gl";
    case CG_DRIVER_GL3:
        return "gl3";
    case CG_DRIVER_GLES2:
        return "gles2";
    case CG_DRIVER_WEBGL:
        return "webgl";
    case CG_DRIVER_NOP:
        return "nop";
    case CG_DRIVER_ANY:
        c_warn_if_reached();
        return "any";
    }

    c_warn_if_reached();
    return "unknown";
}

typedef struct _satisfy_constraints_state_t {
    c_llist_t *constraints;
    const cg_driver_description_t *driver_description;
} satisfy_constraints_state_t;

static bool
satisfy_constraints(cg_driver_description_t *description,
                    void *user_data)
{
    satisfy_constraints_state_t *state = user_data;
    c_llist_t *l;

    for (l = state->constraints; l; l = l->next) {
        cg_renderer_constraint_t constraint = C_POINTER_TO_UINT(l->data);

        /* Most of the constraints only affect the winsys selection so
         * we'll filter them out */
        if (!(constraint & CG_RENDERER_DRIVER_CONSTRAINTS))
            continue;

        /* If the driver doesn't satisfy any constraint then continue
         * to the next driver description */
        if (!(constraint & description->constraints))
            return true;
    }

    state->driver_description = description;

    return false;
}

static bool
_cg_renderer_choose_driver(cg_renderer_t *renderer,
                           cg_error_t **error)
{
    const char *driver_name = c_getenv("CG_DRIVER");
    cg_driver_t driver_override = CG_DRIVER_ANY;
    const char *invalid_override = NULL;
    const char *libgl_name;
    satisfy_constraints_state_t state;
    const cg_driver_description_t *desc;
    int i;

    if (!driver_name)
        driver_name = _cg_config_driver;

    if (driver_name) {
        driver_override = driver_name_to_id(driver_name);
        if (driver_override == CG_DRIVER_ANY)
            invalid_override = driver_name;
    }

    if (renderer->driver_override != CG_DRIVER_ANY) {
        if (driver_override != CG_DRIVER_ANY &&
            renderer->driver_override != driver_override) {
            _cg_set_error(error,
                          CG_RENDERER_ERROR,
                          CG_RENDERER_ERROR_BAD_CONSTRAINT,
                          "Application driver selection conflicts with driver "
                          "specified in configuration");
            return false;
        }

        driver_override = renderer->driver_override;
    }

    if (driver_override != CG_DRIVER_ANY) {
        bool found = false;
        int i;

        for (i = 0; i < C_N_ELEMENTS(_cg_drivers); i++) {
            if (_cg_drivers[i].id == driver_override) {
                found = true;
                break;
            }
        }
        if (!found)
            invalid_override = driver_id_to_name(driver_override);
    }

    if (invalid_override) {
        _cg_set_error(error,
                      CG_RENDERER_ERROR,
                      CG_RENDERER_ERROR_BAD_CONSTRAINT,
                      "Driver \"%s\" is not available",
                      invalid_override);
        return false;
    }

    state.driver_description = NULL;
    state.constraints = renderer->constraints;

    foreach_driver_description(driver_override, satisfy_constraints, &state);

    if (!state.driver_description) {
        _cg_set_error(error,
                      CG_RENDERER_ERROR,
                      CG_RENDERER_ERROR_BAD_CONSTRAINT,
                      "No suitable driver found");
        return false;
    }

    desc = state.driver_description;
    renderer->driver = desc->id;
    renderer->driver_vtable = desc->vtable;
    renderer->texture_driver = desc->texture_driver;
    libgl_name = desc->libgl_name;

    memset(renderer->private_features, 0, sizeof(renderer->private_features));
    for (i = 0; desc->private_features[i] != -1; i++)
        CG_FLAGS_SET(
            renderer->private_features, desc->private_features[i], true);

#ifndef HAVE_DIRECTLY_LINKED_GL_LIBRARY

    if (CG_FLAGS_GET(renderer->private_features, CG_PRIVATE_FEATURE_ANY_GL)) {
        renderer->libgl_module = c_module_open(libgl_name);

        if (renderer->libgl_module == NULL) {
            _cg_set_error(error,
                          CG_DRIVER_ERROR,
                          CG_DRIVER_ERROR_FAILED_TO_LOAD_LIBRARY,
                          "Failed to dynamically open the GL library \"%s\"",
                          libgl_name);
            return false;
        }
    }

#endif /* HAVE_DIRECTLY_LINKED_GL_LIBRARY */

    return true;
}

/* Final connection API */

bool
cg_renderer_connect(cg_renderer_t *renderer, cg_error_t **error)
{
    int i;
    c_string_t *error_message;
    bool constraints_failed = false;

    if (renderer->connected)
        return true;

    /* The driver needs to be chosen before connecting the renderer
       because eglInitialize requires the library containing the GL API
       to be loaded before its called */
    if (!_cg_renderer_choose_driver(renderer, error))
        return false;

    error_message = c_string_new("");
    for (i = 0; i < C_N_ELEMENTS(_cg_winsys_vtable_getters); i++) {
        const cg_winsys_vtable_t *winsys = _cg_winsys_vtable_getters[i]();
        cg_error_t *tmp_error = NULL;
        c_llist_t *l;
        bool skip_due_to_constraints = false;

        if (renderer->winsys_id_override != CG_WINSYS_ID_ANY) {
            if (renderer->winsys_id_override != winsys->id)
                continue;
        } else {
            char *user_choice = getenv("CG_RENDERER");
            if (!user_choice)
                user_choice = _cg_config_renderer;
            if (user_choice &&
                c_ascii_strcasecmp(winsys->name, user_choice) != 0)
                continue;
        }

        for (l = renderer->constraints; l; l = l->next) {
            cg_renderer_constraint_t constraint = C_POINTER_TO_UINT(l->data);
            if (!(winsys->constraints & constraint)) {
                skip_due_to_constraints = true;
                break;
            }
        }
        if (skip_due_to_constraints) {
            constraints_failed |= true;
            continue;
        }

        /* At least temporarily we will associate this winsys with
         * the renderer in-case ->renderer_connect calls API that
         * wants to query the current winsys... */
        renderer->winsys_vtable = winsys;

        if (!winsys->renderer_connect(renderer, &tmp_error)) {
            c_string_append_c(error_message, '\n');
            c_string_append(error_message, tmp_error->message);
            cg_error_free(tmp_error);
        } else {
            renderer->connected = true;
            c_string_free(error_message, true);
            return true;
        }
    }

    if (!renderer->connected) {
        if (constraints_failed) {
            _cg_set_error(
                error,
                CG_RENDERER_ERROR,
                CG_RENDERER_ERROR_BAD_CONSTRAINT,
                "Failed to connected to any renderer due to constraints");
            return false;
        }

        renderer->winsys_vtable = NULL;
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_INIT,
                      "Failed to connected to any renderer: %s",
                      error_message->str);
        c_string_free(error_message, true);
        return false;
    }

    return true;
}

cg_filter_return_t
_cg_renderer_handle_native_event(cg_renderer_t *renderer,
                                 void *event)
{
    c_sllist_t *l, *next;

    /* Pass the event on to all of the registered filters in turn */
    for (l = renderer->event_filters; l; l = next) {
        cg_native_filter_closure_t *closure = l->data;

        /* The next pointer is taken now so that we can handle the
           closure being removed during emission */
        next = l->next;

        if (closure->func(event, closure->data) == CG_FILTER_REMOVE)
            return CG_FILTER_REMOVE;
    }

    /* If the backend for the renderer also wants to see the events, it
       should just register its own filter */

    return CG_FILTER_CONTINUE;
}

void
_cg_renderer_add_native_filter(cg_renderer_t *renderer,
                               cg_native_filter_func_t func,
                               void *data)
{
    cg_native_filter_closure_t *closure;

    closure = c_slice_new(cg_native_filter_closure_t);
    closure->func = func;
    closure->data = data;

    renderer->event_filters = c_sllist_prepend(renderer->event_filters, closure);
}

void
_cg_renderer_remove_native_filter(cg_renderer_t *renderer,
                                  cg_native_filter_func_t func,
                                  void *data)
{
    c_sllist_t *l, *prev = NULL;

    for (l = renderer->event_filters; l; prev = l, l = l->next) {
        cg_native_filter_closure_t *closure = l->data;

        if (closure->func == func && closure->data == data) {
            native_filter_closure_free(closure);
            if (prev)
                prev->next = c_sllist_delete_link(prev->next, l);
            else
                renderer->event_filters =
                    c_sllist_delete_link(renderer->event_filters, l);
            break;
        }
    }
}

void
cg_renderer_set_winsys_id(cg_renderer_t *renderer,
                          cg_winsys_id_t winsys_id)
{
    c_return_if_fail(!renderer->connected);

    renderer->winsys_id_override = winsys_id;
}

cg_winsys_id_t
cg_renderer_get_winsys_id(cg_renderer_t *renderer)
{
    c_return_val_if_fail(renderer->connected, 0);

    return renderer->winsys_vtable->id;
}

void *
_cg_renderer_get_proc_address(cg_renderer_t *renderer,
                              const char *name,
                              bool in_core)
{
    const cg_winsys_vtable_t *winsys = _cg_renderer_get_winsys(renderer);

    return winsys->renderer_get_proc_address(renderer, name, in_core);
}

int
cg_renderer_get_n_fragment_texture_units(cg_renderer_t *renderer)
{
    int n = 0;

    _CG_GET_DEVICE(dev, 0);

#if defined(CG_HAS_GL_SUPPORT) || defined(CG_HAS_GLES2_SUPPORT)
    if (cg_has_feature(dev, CG_FEATURE_ID_GLSL))
        GE(dev, glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &n));
#endif

    return n;
}

void
cg_renderer_add_constraint(cg_renderer_t *renderer,
                           cg_renderer_constraint_t constraint)
{
    c_return_if_fail(!renderer->connected);
    renderer->constraints =
        c_llist_prepend(renderer->constraints, C_UINT_TO_POINTER(constraint));
}

void
cg_renderer_remove_constraint(cg_renderer_t *renderer,
                              cg_renderer_constraint_t constraint)
{
    c_return_if_fail(!renderer->connected);
    renderer->constraints =
        c_llist_remove(renderer->constraints, C_UINT_TO_POINTER(constraint));
}

void
cg_renderer_set_driver(cg_renderer_t *renderer, cg_driver_t driver)
{
    c_return_if_fail(!renderer->connected);
    renderer->driver_override = driver;
}

cg_driver_t
cg_renderer_get_driver(cg_renderer_t *renderer)
{
    c_return_val_if_fail(renderer->connected, 0);

    return renderer->driver;
}

void
cg_renderer_foreach_output(cg_renderer_t *renderer,
                           cg_output_callback_t callback,
                           void *user_data)
{
    c_llist_t *l;

    c_return_if_fail(renderer->connected);
    c_return_if_fail(callback != NULL);

    for (l = renderer->outputs; l; l = l->next)
        callback(l->data, user_data);
}
