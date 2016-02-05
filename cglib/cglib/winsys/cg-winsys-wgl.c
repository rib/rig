/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010,2011 Intel Corporation.
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
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#include <cglib-config.h>

#include <windows.h>

#include "cg-util.h"
#include "cg-winsys-private.h"
#include "cg-device-private.h"
#include "cg-framebuffer.h"
#include "cg-onscreen-private.h"
#include "cg-renderer-private.h"
#include "cg-display-private.h"
#include "cg-onscreen-template-private.h"
#include "cg-private.h"
#include "cg-feature-private.h"
#include "cg-win32-renderer.h"
#include "cg-winsys-wgl-private.h"
#include "cg-error-private.h"
#include "cg-loop-private.h"

/* This magic handle will cause c_poll to wakeup when there is a
 * pending message */
#define WIN32_MSG_HANDLE 19981206

typedef struct _cg_renderer_wgl_t {
    c_module_t *gl_module;

/* Function pointers for GLX specific extensions */
#define CG_WINSYS_FEATURE_BEGIN(a, b, c, d, e)

#define CG_WINSYS_FEATURE_FUNCTION(ret, name, args)                            \
    ret(APIENTRY *pf_##name) args;

#define CG_WINSYS_FEATURE_END()

#include "cg-winsys-wgl-feature-functions.h"

#undef CG_WINSYS_FEATURE_BEGIN
#undef CG_WINSYS_FEATURE_FUNCTION
#undef CG_WINSYS_FEATURE_END
} cg_renderer_wgl_t;

typedef struct _cg_display_wgl_t {
    ATOM window_class;
    HGLRC wgl_context;
    HWND dummy_hwnd;
    HDC dummy_dc;
} cg_display_wgl_t;

typedef struct _cg_onscreen_win32_t {
    HWND hwnd;
    bool is_foreign_hwnd;
} cg_onscreen_win32_t;

typedef struct _cg_device_wgl_t {
    HDC current_dc;
} cg_device_wgl_t;

typedef struct _cg_onscreen_wgl_t {
    cg_onscreen_win32_t _parent;

    HDC client_dc;

} cg_onscreen_wgl_t;

/* Define a set of arrays containing the functions required from GL
   for each winsys feature */
#define CG_WINSYS_FEATURE_BEGIN(                                               \
        name, namespaces, extension_names, feature_flags_private, winsys_feature)  \
    static const cg_feature_function_t cg_wgl_feature_##name##_funcs[] = {
#define CG_WINSYS_FEATURE_FUNCTION(ret, name, args)                            \
    {                                                                          \
        C_STRINGIFY(name), C_STRUCT_OFFSET(cg_renderer_wgl_t, pf_##name)       \
    }                                                                          \
    ,
#define CG_WINSYS_FEATURE_END()                                                \
    {                                                                          \
        NULL, 0                                                                \
    }                                                                          \
    ,                                                                          \
    }                                                                          \
    ;
#include "cg-winsys-wgl-feature-functions.h"

/* Define an array of features */
#undef CG_WINSYS_FEATURE_BEGIN
#define CG_WINSYS_FEATURE_BEGIN(                                               \
        name, namespaces, extension_names, feature_flags_private, winsys_feature)  \
    {                                                                          \
        255, 255, 0, namespaces, extension_names, feature_flags_private,       \
        winsys_feature, cg_wgl_feature_##name##_funcs                      \
    }                                                                          \
    ,
#undef CG_WINSYS_FEATURE_FUNCTION
#define CG_WINSYS_FEATURE_FUNCTION(ret, name, args)
#undef CG_WINSYS_FEATURE_END
#define CG_WINSYS_FEATURE_END()

static const cg_feature_data_t winsys_feature_data[] = {
#include "cg-winsys-wgl-feature-functions.h"
};

static cg_func_ptr_t
_cg_winsys_renderer_get_proc_address(
    cg_renderer_t *renderer, const char *name, bool in_core)
{
    cg_renderer_wgl_t *wgl_renderer = renderer->winsys;
    void *proc = wglGetProcAddress((LPCSTR)name);

    /* The documentation for wglGetProcAddress implies that it only
       returns pointers to extension functions so if it fails we'll try
       resolving the symbol directly from the the GL library. We could
       completely avoid using wglGetProcAddress if in_core is true but
       on WGL any function that is in GL > 1.1 is considered an
       extension and is not directly exported from opengl32.dll.
       Therefore we currently just assume wglGetProcAddress will return
       NULL for GL 1.1 functions and we can fallback to querying them
       directly from the library */

    if (proc == NULL) {
        if (wgl_renderer->gl_module == NULL)
            wgl_renderer->gl_module = c_module_open("opengl32");

        if (wgl_renderer->gl_module)
            c_module_symbol(wgl_renderer->gl_module, name, &proc);
    }

    return proc;
}

static void
_cg_winsys_renderer_disconnect(cg_renderer_t *renderer)
{
    cg_renderer_wgl_t *wgl_renderer = renderer->winsys;

    if (renderer->win32_enable_event_retrieval)
        _cg_loop_remove_fd(renderer, WIN32_MSG_HANDLE);

    if (wgl_renderer->gl_module)
        c_module_close(wgl_renderer->gl_module);

    c_slice_free(cg_renderer_wgl_t, renderer->winsys);
}

static cg_onscreen_t *
find_onscreen_for_hwnd(cg_device_t *dev, HWND hwnd)
{
    cg_display_wgl_t *display_wgl = dev->display->winsys;
    c_llist_t *l;

    /* If the hwnd has CGlib's window class then we can lookup the
       onscreen pointer directly by reading the extra window data */
    if (GetClassLongPtr(hwnd, GCW_ATOM) == display_wgl->window_class) {
        cg_onscreen_t *onscreen = (cg_onscreen_t *)GetWindowLongPtr(hwnd, 0);

        if (onscreen)
            return onscreen;
    }

    for (l = dev->framebuffers; l; l = l->next) {
        cg_framebuffer_t *framebuffer = l->data;

        if (framebuffer->type == CG_FRAMEBUFFER_TYPE_ONSCREEN) {
            cg_onscreen_win32_t *win32_onscreen =
                CG_ONSCREEN(framebuffer)->winsys;

            if (win32_onscreen->hwnd == hwnd)
                return CG_ONSCREEN(framebuffer);
        }
    }

    return NULL;
}

static cg_filter_return_t
win32_event_filter_cb(MSG *msg, void *data)
{
    cg_device_t *dev = data;

    if (msg->message == WM_SIZE) {
        cg_onscreen_t *onscreen = find_onscreen_for_hwnd(dev, msg->hwnd);

        if (onscreen) {
            cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);

            /* Ignore size changes resulting from the stage being
               minimized - otherwise it will think the window has been
               resized to 0,0 */
            if (msg->wParam != SIZE_MINIMIZED) {
                WORD new_width = LOWORD(msg->lParam);
                WORD new_height = HIWORD(msg->lParam);
                _cg_framebuffer_winsys_update_size(
                    framebuffer, new_width, new_height);
            }
        }
    } else if (msg->message == WM_PAINT) {
        cg_onscreen_t *onscreen = find_onscreen_for_hwnd(dev, msg->hwnd);
        RECT rect;

        if (onscreen && GetUpdateRect(msg->hwnd, &rect, false)) {
            cg_onscreen_dirty_info_t info;

            /* Apparently this removes the dirty region from the window
             * so that it won't be included in the next WM_PAINT
             * message. This is also what SDL does to emit dirty
             * events */
            ValidateRect(msg->hwnd, &rect);

            info.x = rect.left;
            info.y = rect.top;
            info.width = rect.right - rect.left;
            info.height = rect.bottom - rect.top;

            _cg_onscreen_queue_dirty(onscreen, &info);
        }
    }

    return CG_FILTER_CONTINUE;
}

static int64_t
prepare_messages(void *user_data)
{
    MSG msg;

    return PeekMessageW(&msg, NULL, 0, 0, PM_NOREMOVE) ? 0 : -1;
}

static void
dispatch_messages(void *user_data, int revents)
{
    MSG msg;

    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        /* This should cause the message to be sent to our window proc */
        DispatchMessageW(&msg);
}

static bool
_cg_winsys_renderer_connect(cg_renderer_t *renderer,
                            cg_error_t **error)
{
    renderer->winsys = c_slice_new0(cg_renderer_wgl_t);

    if (renderer->win32_enable_event_retrieval) {
        /* We'll add a magic handle that will cause a GLib main loop to
         * wake up when there are messages. This will only work if the
         * application is using GLib but it shouldn't matter if it
         * doesn't work in other cases because the application shouldn't
         * be using the cg_poll_* functions on non-Unix systems
         * anyway */
        _cg_loop_add_fd(renderer,
                                 WIN32_MSG_HANDLE,
                                 CG_POLL_FD_EVENT_IN,
                                 prepare_messages,
                                 dispatch_messages,
                                 renderer);
    }

    return true;
}

static LRESULT CALLBACK
window_proc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam)
{
    bool message_handled = false;
    cg_onscreen_t *onscreen;

    /* It's not clear what the best thing to do with messages sent to
       the window proc is. We want the application to forward on all
       messages through CGlib so that it can have a chance to process
       them which might mean that that in it's GetMessage loop it could
       call cg_win32_renderer_handle_event for every message. However
       the message loop would usually call DispatchMessage as well which
       mean this window proc would be invoked and CGlib would see the
       message twice. However we can't just ignore messages in the
       window proc because some messages are sent directly from windows
       without going through the message queue. This function therefore
       just forwards on all messages directly. This means that the
       application is not expected to forward on messages if it has let
       CGlib create the window itself because it will already see them
       via the window proc. This limits the kinds of messages that CGlib
       can handle to ones that are sent to the windows it creates, but I
       think that is a reasonable restriction */

    /* Convert the message to a MSG struct and pass it through the CGlib
       message handling mechanism */

    /* This window proc is only called for messages created with CGlib's
       window class so we should be able to work out the corresponding
       window class by looking in the extra window data. Windows will
       send some extra messages before we get a chance to set this value
       so we have to ignore these */
    onscreen = (cg_onscreen_t *)GetWindowLongPtr(hwnd, 0);

    if (onscreen != NULL) {
        cg_renderer_t *renderer;
        DWORD message_pos;
        MSG msg;

        msg.hwnd = hwnd;
        msg.message = umsg;
        msg.wParam = wparam;
        msg.lParam = lparam;
        msg.time = GetMessageTime();
        /* Neither MAKE_POINTS nor GET_[XY]_LPARAM is defined in MinGW
           headers so we need to convert to a signed type explicitly */
        message_pos = GetMessagePos();
        msg.pt.x = (SHORT)LOWORD(message_pos);
        msg.pt.y = (SHORT)HIWORD(message_pos);

        renderer = CG_FRAMEBUFFER(onscreen)->dev->display->renderer;

        message_handled = cg_win32_renderer_handle_event(renderer, &msg);
    }

    if (!message_handled)
        return DefWindowProcW(hwnd, umsg, wparam, lparam);
    else
        return 0;
}

static bool
pixel_format_is_better(const PIXELFORMATDESCRIPTOR *pfa,
                       const PIXELFORMATDESCRIPTOR *pfb)
{
    /* Always prefer a format with a stencil buffer */
    if (pfa->cStencilBits == 0) {
        if (pfb->cStencilBits > 0)
            return true;
    } else if (pfb->cStencilBits == 0)
        return false;

    /* Prefer a bigger color buffer */
    if (pfb->cColorBits > pfa->cColorBits)
        return true;
    else if (pfb->cColorBits < pfa->cColorBits)
        return false;

    /* Prefer a bigger depth buffer */
    return pfb->cDepthBits > pfa->cDepthBits;
}

static int
choose_pixel_format(cg_framebuffer_config_t *config,
                    HDC dc,
                    PIXELFORMATDESCRIPTOR *pfd)
{
    int i, num_formats, best_pf = 0;
    PIXELFORMATDESCRIPTOR best_pfd;

    num_formats = DescribePixelFormat(dc, 0, sizeof(best_pfd), NULL);

    /* XXX: currently we don't support multisampling on windows... */
    if (config->samples_per_pixel)
        return best_pf;

    for (i = 1; i <= num_formats; i++) {
        memset(pfd, 0, sizeof(*pfd));

        if (DescribePixelFormat(dc, i, sizeof(best_pfd), pfd) &&
            /* Check whether this format is useable by CGlib */
            ((pfd->dwFlags & (PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW |
                              PFD_DOUBLEBUFFER | PFD_GENERIC_FORMAT)) ==
             (PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_DRAW_TO_WINDOW)) &&
            pfd->iPixelType == PFD_TYPE_RGBA && pfd->cColorBits >= 16 &&
            pfd->cColorBits <= 32 && pfd->cDepthBits >= 16 &&
            pfd->cDepthBits <= 32 &&
            /* Check whether this is a better format than one we've
               already found */
            (best_pf == 0 || pixel_format_is_better(&best_pfd, pfd))) {
            if (config->has_alpha && pfd->cAlphaBits == 0)
                continue;
            if (config->need_stencil && pfd->cStencilBits == 0)
                continue;

            best_pf = i;
            best_pfd = *pfd;
        }
    }

    *pfd = best_pfd;

    return best_pf;
}

static bool
create_window_class(cg_display_t *display, cg_error_t **error)
{
    cg_display_wgl_t *wgl_display = display->winsys;
    char *class_name_ascii, *src;
    WCHAR *class_name_wchar, *dst;
    WNDCLASSW wndclass;

    /* We create a window class per display so that we have an
       opportunity to clean up the class when the display is
       destroyed */

    /* Generate a unique name containing the address of the display */
    class_name_ascii = c_strdup_printf("CGlibWindow0x%0*" G_GINTPTR_MODIFIER "x",
                                       sizeof(guintptr) * 2,
                                       (guintptr)display);
    /* Convert it to WCHARs */
    class_name_wchar = c_malloc((strlen(class_name_ascii) + 1) * sizeof(WCHAR));
    for (src = class_name_ascii, dst = class_name_wchar; *src; src++, dst++)
        *dst = *src;
    *dst = L'\0';

    memset(&wndclass, 0, sizeof(wndclass));
    wndclass.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc = window_proc;
    /* We reserve extra space in the window data for a pointer back to
       the cg_onscreen_t */
    wndclass.cbWndExtra = sizeof(LONG_PTR);
    wndclass.hInstance = GetModuleHandleW(NULL);
    wndclass.hIcon = LoadIconW(NULL, (LPWSTR)IDI_APPLICATION);
    wndclass.hCursor = LoadCursorW(NULL, (LPWSTR)IDC_ARROW);
    wndclass.hbrBackground = NULL;
    wndclass.lpszMenuName = NULL;
    wndclass.lpszClassName = class_name_wchar;
    wgl_display->window_class = RegisterClassW(&wndclass);

    c_free(class_name_wchar);
    c_free(class_name_ascii);

    if (wgl_display->window_class == 0) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_CONTEXT,
                      "Unable to register window class");
        return false;
    }

    return true;
}

static bool
create_context(cg_display_t *display, cg_error_t **error)
{
    cg_display_wgl_t *wgl_display = display->winsys;

    c_return_val_if_fail(wgl_display->wgl_context == NULL, false);

    /* CGlib assumes that there is always a GL context selected; in order
     * to make sure that a WGL context exists and is made current, we
     * use a small dummy window that never gets shown to which we can
     * always fall back if no onscreen is available
     */
    if (wgl_display->dummy_hwnd == NULL) {
        wgl_display->dummy_hwnd =
            CreateWindowW((LPWSTR)MAKEINTATOM(wgl_display->window_class),
                          L".",
                          WS_OVERLAPPEDWINDOW,
                          CW_USEDEFAULT,
                          CW_USEDEFAULT,
                          1,
                          1,
                          NULL,
                          NULL,
                          GetModuleHandle(NULL),
                          NULL);

        if (wgl_display->dummy_hwnd == NULL) {
            _cg_set_error(error,
                          CG_WINSYS_ERROR,
                          CG_WINSYS_ERROR_CREATE_CONTEXT,
                          "Unable to create dummy window");
            return false;
        }
    }

    if (wgl_display->dummy_dc == NULL) {
        PIXELFORMATDESCRIPTOR pfd;
        int pf;

        wgl_display->dummy_dc = GetDC(wgl_display->dummy_hwnd);

        pf = choose_pixel_format(
            &display->onscreen_template->config, wgl_display->dummy_dc, &pfd);

        if (pf == 0 || !SetPixelFormat(wgl_display->dummy_dc, pf, &pfd)) {
            _cg_set_error(error,
                          CG_WINSYS_ERROR,
                          CG_WINSYS_ERROR_CREATE_CONTEXT,
                          "Unable to find suitable GL pixel format");
            ReleaseDC(wgl_display->dummy_hwnd, wgl_display->dummy_dc);
            wgl_display->dummy_dc = NULL;
            return false;
        }
    }

    if (wgl_display->wgl_context == NULL) {
        wgl_display->wgl_context = wglCreateContext(wgl_display->dummy_dc);

        if (wgl_display->wgl_context == NULL) {
            _cg_set_error(error,
                          CG_WINSYS_ERROR,
                          CG_WINSYS_ERROR_CREATE_CONTEXT,
                          "Unable to create suitable GL context");
            return false;
        }
    }

    CG_NOTE(WINSYS,
            "Selecting dummy 0x%x for the WGL context",
            (unsigned int)wgl_display->dummy_hwnd);

    wglMakeCurrent(wgl_display->dummy_dc, wgl_display->wgl_context);

    return true;
}

static void
_cg_winsys_display_destroy(cg_display_t *display)
{
    cg_display_wgl_t *wgl_display = display->winsys;

    c_return_if_fail(wgl_display != NULL);

    if (wgl_display->wgl_context) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(wgl_display->wgl_context);
    }

    if (wgl_display->dummy_dc)
        ReleaseDC(wgl_display->dummy_hwnd, wgl_display->dummy_dc);

    if (wgl_display->dummy_hwnd)
        DestroyWindow(wgl_display->dummy_hwnd);

    if (wgl_display->window_class)
        UnregisterClassW((LPWSTR)MAKEINTATOM(wgl_display->window_class),
                         GetModuleHandleW(NULL));

    c_slice_free(cg_display_wgl_t, display->winsys);
    display->winsys = NULL;
}

static bool
_cg_winsys_display_setup(cg_display_t *display, cg_error_t **error)
{
    cg_display_wgl_t *wgl_display;

    c_return_val_if_fail(display->winsys == NULL, false);

    wgl_display = c_slice_new0(cg_display_wgl_t);
    display->winsys = wgl_display;

    if (!create_window_class(display, error))
        goto error;

    if (!create_context(display, error))
        goto error;

    return true;

error:
    _cg_winsys_display_destroy(display);
    return false;
}

static const char *
get_wgl_extensions_string(cg_device_t *dev, HDC dc)
{
    const char *(APIENTRY * pf_wglGetExtensionsStringARB)(HDC);
    const char *(APIENTRY * pf_wglGetExtensionsStringEXT)(void);

    /* According to the docs for these two extensions, you are supposed
       to use wglGetProcAddress to detect their availability so
       presumably it will return NULL if they are not available */

    pf_wglGetExtensionsStringARB =
        (void *)wglGetProcAddress("wglGetExtensionsStringARB");

    if (pf_wglGetExtensionsStringARB)
        return pf_wglGetExtensionsStringARB(dc);

    pf_wglGetExtensionsStringEXT =
        (void *)wglGetProcAddress("wglGetExtensionsStringEXT");

    if (pf_wglGetExtensionsStringEXT)
        return pf_wglGetExtensionsStringEXT();

    /* The WGL_EXT_swap_control is also advertised as a GL extension as
       GL_EXT_SWAP_CONTROL so if the extension to get the list of WGL
       extensions isn't supported then we can at least fake it to
       support the swap control extension */
    {
        char **extensions = _cg_device_get_gl_extensions(dev);
        bool have_ext = _cg_check_extension("WGL_EXT_swap_control", extensions);
        c_strfreev(extensions);
        if (have_ext)
            return "WGL_EXT_swap_control";
    }

    return NULL;
}

static bool
update_winsys_features(cg_device_t *dev, cg_error_t **error)
{
    cg_display_wgl_t *wgl_display = dev->display->winsys;
    cg_renderer_wgl_t *wgl_renderer = dev->display->renderer->winsys;
    const char *wgl_extensions;
    int i;

    c_return_val_if_fail(wgl_display->wgl_context, false);

    if (!_cg_device_update_features(dev, error))
        return false;

    memset(dev->winsys_features, 0, sizeof(dev->winsys_features));

    CG_FLAGS_SET(dev->features, CG_FEATURE_ID_ONSCREEN_MULTIPLE, true);
    CG_FLAGS_SET(dev->winsys_features, CG_WINSYS_FEATURE_MULTIPLE_ONSCREEN,
                 true);

    wgl_extensions = get_wgl_extensions_string(dev, wgl_display->dummy_dc);

    if (wgl_extensions) {
        char **split_extensions =
            c_strsplit(wgl_extensions, " ", 0 /* max_tokens */);

        CG_NOTE(WINSYS, "  WGL Extensions: %s", wgl_extensions);

        for (i = 0; i < C_N_ELEMENTS(winsys_feature_data); i++)
            if (_cg_feature_check(dev->display->renderer,
                                  "WGL",
                                  winsys_feature_data + i,
                                  0,
                                  0,
                                  CG_DRIVER_GL,
                                  split_extensions,
                                  wgl_renderer)) {
                if (winsys_feature_data[i].winsys_feature)
                    CG_FLAGS_SET(dev->winsys_features,
                                 winsys_feature_data[i].winsys_feature,
                                 true);
            }

        c_strfreev(split_extensions);
    }

    /* We'll manually handle queueing dirty events in response to
     * WM_PAINT messages */
    CG_FLAGS_SET(dev->private_features, CG_PRIVATE_FEATURE_DIRTY_EVENTS,
                 true);

    return true;
}

static bool
_cg_winsys_device_init(cg_device_t *dev, cg_error_t **error)
{
    dev->winsys = c_new0(cg_device_wgl_t, 1);

    cg_win32_renderer_add_filter(dev->display->renderer,
                                 win32_event_filter_cb, dev);

    return update_winsys_features(dev, error);
}

static void
_cg_winsys_device_deinit(cg_device_t *dev)
{
    cg_win32_renderer_remove_filter(dev->display->renderer,
                                    win32_event_filter_cb, dev);

    c_free(dev->winsys);
}

static void
_cg_winsys_onscreen_bind(cg_onscreen_t *onscreen)
{
    cg_framebuffer_t *fb;
    cg_device_t *dev;
    cg_device_wgl_t *wgl_context;
    cg_display_wgl_t *wgl_display;
    cg_onscreen_wgl_t *wgl_onscreen;
    cg_renderer_wgl_t *wgl_renderer;

    /* The glx backend tries to bind the dummy context if onscreen ==
       NULL, but this isn't really going to work because before checking
       whether onscreen == NULL it reads the pointer to get the
       context */
    c_return_if_fail(onscreen != NULL);

    fb = CG_FRAMEBUFFER(onscreen);
    dev = fb->dev;
    wgl_context = dev->winsys;
    wgl_display = dev->display->winsys;
    wgl_onscreen = onscreen->winsys;
    wgl_renderer = dev->display->renderer->winsys;

    if (wgl_context->current_dc == wgl_onscreen->client_dc)
        return;

    wglMakeCurrent(wgl_onscreen->client_dc, wgl_display->wgl_context);

    /* According to the specs for WGL_EXT_swap_control SwapInterval()
     * applies to the current window not the context so we apply it here
     * to ensure its up-to-date even for new windows.
     */
    if (wgl_renderer->pf_wglSwapInterval) {
        if (fb->config.swap_throttled)
            wgl_renderer->pf_wglSwapInterval(1);
        else
            wgl_renderer->pf_wglSwapInterval(0);
    }

    wgl_context->current_dc = wgl_onscreen->client_dc;
}

static void
_cg_winsys_onscreen_deinit(cg_onscreen_t *onscreen)
{
    cg_device_t *dev = CG_FRAMEBUFFER(onscreen)->dev;
    cg_device_wgl_t *wgl_context = dev->winsys;
    cg_onscreen_win32_t *win32_onscreen = onscreen->winsys;
    cg_onscreen_wgl_t *wgl_onscreen = onscreen->winsys;

    /* If we never successfully allocated then there's nothing to do */
    if (wgl_onscreen == NULL)
        return;

    if (wgl_onscreen->client_dc) {
        if (wgl_context->current_dc == wgl_onscreen->client_dc)
            _cg_winsys_onscreen_bind(NULL);

        ReleaseDC(win32_onscreen->hwnd, wgl_onscreen->client_dc);
    }

    if (!win32_onscreen->is_foreign_hwnd && win32_onscreen->hwnd) {
        /* Drop the pointer to the onscreen in the window so that any
           further messages won't be processed */
        SetWindowLongPtrW(win32_onscreen->hwnd, 0, (LONG_PTR)0);
        DestroyWindow(win32_onscreen->hwnd);
    }

    c_slice_free(cg_onscreen_wgl_t, onscreen->winsys);
    onscreen->winsys = NULL;
}

static bool
_cg_winsys_onscreen_init(cg_onscreen_t *onscreen,
                         cg_error_t **error)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_display_t *display = dev->display;
    cg_display_wgl_t *wgl_display = display->winsys;
    cg_onscreen_wgl_t *wgl_onscreen;
    cg_onscreen_win32_t *win32_onscreen;
    PIXELFORMATDESCRIPTOR pfd;
    int pf;
    HWND hwnd;

    c_return_val_if_fail(wgl_display->wgl_context, false);

    /* XXX: Note we ignore the user's original width/height when given a
     * foreign window. */
    if (onscreen->foreign_hwnd) {
        RECT client_rect;

        hwnd = onscreen->foreign_hwnd;

        GetClientRect(hwnd, &client_rect);

        _cg_framebuffer_winsys_update_size(
            framebuffer, client_rect.right, client_rect.bottom);
    } else {
        int width, height;

        width = CG_FRAMEBUFFER(onscreen)->width;
        height = CG_FRAMEBUFFER(onscreen)->height;

        /* The size of the window passed to CreateWindow for some reason
           includes the window decorations so we need to compensate for
           that */
        width += GetSystemMetrics(SM_CXSIZEFRAME) * 2;
        height += (GetSystemMetrics(SM_CYSIZEFRAME) * 2 +
                   GetSystemMetrics(SM_CYCAPTION));

        hwnd = CreateWindowW((LPWSTR)MAKEINTATOM(wgl_display->window_class),
                             L".",
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, /* xpos */
                             CW_USEDEFAULT, /* ypos */
                             width,
                             height,
                             NULL, /* parent */
                             NULL, /* menu */
                             GetModuleHandle(NULL),
                             NULL /* lparam for the WM_CREATE message */);

        if (hwnd == NULL) {
            _cg_set_error(error,
                          CG_WINSYS_ERROR,
                          CG_WINSYS_ERROR_CREATE_ONSCREEN,
                          "Unable to create window");
            return false;
        }

        /* Store a pointer back to the onscreen in the window extra data
           so we can refer back to it quickly */
        SetWindowLongPtrW(hwnd, 0, (LONG_PTR)onscreen);
    }

    onscreen->winsys = c_slice_new0(cg_onscreen_wgl_t);
    win32_onscreen = onscreen->winsys;
    wgl_onscreen = onscreen->winsys;

    win32_onscreen->hwnd = hwnd;

    wgl_onscreen->client_dc = GetDC(hwnd);

    /* Use the same pixel format as the dummy DC from the renderer */
    pf = choose_pixel_format(
        &framebuffer->config, wgl_onscreen->client_dc, &pfd);

    if (pf == 0 || !SetPixelFormat(wgl_onscreen->client_dc, pf, &pfd)) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_ONSCREEN,
                      "Error setting pixel format on the window");

        _cg_winsys_onscreen_deinit(onscreen);

        return false;
    }

    return true;
}

static void
_cg_winsys_onscreen_swap_buffers_with_damage(
    cg_onscreen_t *onscreen, const int *rectangles, int n_rectangles)
{
    cg_onscreen_wgl_t *wgl_onscreen = onscreen->winsys;

    SwapBuffers(wgl_onscreen->client_dc);
}

static void
_cg_winsys_onscreen_update_swap_throttled(cg_onscreen_t *onscreen)
{
    cg_device_t *dev = CG_FRAMEBUFFER(onscreen)->dev;
    cg_device_wgl_t *wgl_context = dev->winsys;
    cg_onscreen_wgl_t *wgl_onscreen = onscreen->winsys;

    if (wgl_context->current_dc != wgl_onscreen->client_dc)
        return;

    /* This will cause it to rebind the context and update the swap interval */
    wgl_context->current_dc = NULL;
    _cg_winsys_onscreen_bind(onscreen);
}

static HWND
_cg_winsys_onscreen_win32_get_window(cg_onscreen_t *onscreen)
{
    cg_onscreen_win32_t *win32_onscreen = onscreen->winsys;
    return win32_onscreen->hwnd;
}

static void
_cg_winsys_onscreen_set_visibility(cg_onscreen_t *onscreen,
                                   bool visibility)
{
    cg_onscreen_win32_t *win32_onscreen = onscreen->winsys;

    ShowWindow(win32_onscreen->hwnd, visibility ? SW_SHOW : SW_HIDE);
}

const cg_winsys_vtable_t *
_cg_winsys_wgl_get_vtable(void)
{
    static bool vtable_inited = false;
    static cg_winsys_vtable_t vtable;

    /* It would be nice if we could use C99 struct initializers here
       like the GLX backend does. However this code is more likely to be
       compiled using Visual Studio which (still!) doesn't support them
       so we initialize it in code instead */

    if (!vtable_inited) {
        memset(&vtable, 0, sizeof(vtable));

        vtable.id = CG_WINSYS_ID_WGL;
        vtable.name = "WGL";
        vtable.renderer_get_proc_address = _cg_winsys_renderer_get_proc_address;
        vtable.renderer_connect = _cg_winsys_renderer_connect;
        vtable.renderer_disconnect = _cg_winsys_renderer_disconnect;
        vtable.display_setup = _cg_winsys_display_setup;
        vtable.display_destroy = _cg_winsys_display_destroy;
        vtable.device_init = _cg_winsys_device_init;
        vtable.device_deinit = _cg_winsys_device_deinit;
        vtable.onscreen_init = _cg_winsys_onscreen_init;
        vtable.onscreen_deinit = _cg_winsys_onscreen_deinit;
        vtable.onscreen_bind = _cg_winsys_onscreen_bind;
        vtable.onscreen_swap_buffers_with_damage =
            _cg_winsys_onscreen_swap_buffers_with_damage;
        vtable.onscreen_update_swap_throttled =
            _cg_winsys_onscreen_update_swap_throttled;
        vtable.onscreen_set_visibility = _cg_winsys_onscreen_set_visibility;
        vtable.onscreen_win32_get_window = _cg_winsys_onscreen_win32_get_window;

        vtable_inited = true;
    }

    return &vtable;
}
