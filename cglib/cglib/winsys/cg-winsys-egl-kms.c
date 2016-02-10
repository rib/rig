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
 *
 * Authors:
 *   Rob Bradford <rob@linux.intel.com>
 *   Kristian Høgsberg (from eglkms.c)
 *   Benjamin Franzke (from eglkms.c)
 *   Robert Bragg <robert@linux.intel.com>
 *   Neil Roberts <neil@linux.intel.com>
 */

#include <cglib-config.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stddef.h>
#include <drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <clib.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "cg-winsys-egl-kms-private.h"
#include "cg-winsys-egl-private.h"
#include "cg-renderer-private.h"
#include "cg-framebuffer-private.h"
#include "cg-onscreen-private.h"
#include "cg-kms-renderer.h"
#include "cg-kms-display.h"
#include "cg-version.h"
#include "cg-error-private.h"
#include "cg-loop-private.h"

static const cg_winsys_egl_vtable_t _cg_winsys_egl_vtable;

static const cg_winsys_vtable_t *parent_vtable;

typedef struct _cg_renderer_kms_t {
    int fd;
    int opened_fd;
    struct gbm_device *gbm;
} cg_renderer_kms_t;

typedef struct _cg_output_kms_t {
    drmModeConnector *connector;
    drmModeEncoder *encoder;
    drmModeCrtc *saved_crtc;
    drmModeModeInfo *modes;
    int n_modes;
    drmModeModeInfo mode;
} cg_output_kms_t;

typedef struct _cg_display_kms_t {
    c_llist_t *outputs;
    c_llist_t *crtcs;

    int width, height;
    bool pending_set_crtc;
    struct gbm_surface *dummy_gbm_surface;

    cg_onscreen_t *onscreen;
} cg_display_kms_t;

typedef struct _cg_flip_kms_t {
    cg_onscreen_t *onscreen;
    int pending;
} cg_flip_kms_t;

typedef struct _cg_onscreen_kms_t {
    struct gbm_surface *surface;
    uint32_t current_fb_id;
    uint32_t next_fb_id;
    struct gbm_bo *current_bo;
    struct gbm_bo *next_bo;
} cg_onscreen_kms_t;

static const char device_name[] = "/dev/dri/card0";

static void
_cg_winsys_renderer_disconnect(cg_renderer_t *renderer)
{
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    cg_renderer_kms_t *kms_renderer = egl_renderer->platform;

    eglTerminate(egl_renderer->edpy);

    if (kms_renderer->opened_fd >= 0)
        close(kms_renderer->opened_fd);

    c_slice_free(cg_renderer_kms_t, kms_renderer);
    c_slice_free(cg_renderer_egl_t, egl_renderer);
}

static void
free_current_bo(cg_onscreen_t *onscreen)
{
    cg_onscreen_egl_t *egl_onscreen = onscreen->winsys;
    cg_onscreen_kms_t *kms_onscreen = egl_onscreen->platform;
    cg_device_t *dev = CG_FRAMEBUFFER(onscreen)->dev;
    cg_renderer_t *renderer = dev->display->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    cg_renderer_kms_t *kms_renderer = egl_renderer->platform;

    if (kms_onscreen->current_fb_id) {
        drmModeRmFB(kms_renderer->fd, kms_onscreen->current_fb_id);
        kms_onscreen->current_fb_id = 0;
    }
    if (kms_onscreen->current_bo) {
        gbm_surface_release_buffer(kms_onscreen->surface,
                                   kms_onscreen->current_bo);
        kms_onscreen->current_bo = NULL;
    }
}

static void
page_flip_handler(int fd,
                  unsigned int frame,
                  unsigned int sec,
                  unsigned int usec,
                  void *data)
{
    cg_flip_kms_t *flip = data;

    /* We're only ready to dispatch a swap notification once all outputs
     * have flipped... */
    flip->pending--;
    if (flip->pending == 0) {
        cg_onscreen_t *onscreen = flip->onscreen;
        cg_onscreen_egl_t *egl_onscreen = onscreen->winsys;
        cg_onscreen_kms_t *kms_onscreen = egl_onscreen->platform;
        cg_device_t *dev = CG_FRAMEBUFFER(onscreen)->dev;
        cg_renderer_t *renderer = dev->display->renderer;
        cg_renderer_egl_t *egl_renderer = renderer->winsys;
        cg_renderer_kms_t *kms_renderer = egl_renderer->platform;
        cg_frame_info_t *info =
            c_queue_pop_head(&onscreen->pending_frame_infos);

        info->presentation_time =
            (int64_t)sec * (int64_t)1000000000 + usec * 1000;

        _cg_onscreen_notify_frame_sync(onscreen, info);
        _cg_onscreen_notify_complete(onscreen, info);

        cg_object_unref(info);

        free_current_bo(onscreen);

        kms_onscreen->current_fb_id = kms_onscreen->next_fb_id;
        kms_onscreen->next_fb_id = 0;

        kms_onscreen->current_bo = kms_onscreen->next_bo;
        kms_onscreen->next_bo = NULL;

        cg_object_unref(flip->onscreen);

        c_slice_free(cg_flip_kms_t, flip);
    }
}

static void
handle_drm_event(cg_renderer_kms_t *kms_renderer)
{
    drmEventContext evdev;

    memset(&evdev, 0, sizeof evdev);
    evdev.version = DRM_EVENT_CONTEXT_VERSION;
    evdev.page_flip_handler = page_flip_handler;
    drmHandleEvent(kms_renderer->fd, &evdev);
}

static void
dispatch_kms_events(void *user_data, int revents)
{
    cg_renderer_t *renderer = user_data;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    cg_renderer_kms_t *kms_renderer = egl_renderer->platform;

    if (!revents)
        return;

    handle_drm_event(kms_renderer);
}

static bool
_cg_winsys_renderer_connect(cg_renderer_t *renderer,
                            cg_error_t **error)
{
    cg_renderer_egl_t *egl_renderer;
    cg_renderer_kms_t *kms_renderer;

    renderer->winsys = c_slice_new0(cg_renderer_egl_t);
    egl_renderer = renderer->winsys;

    egl_renderer->platform_vtable = &_cg_winsys_egl_vtable;
    egl_renderer->platform = c_slice_new0(cg_renderer_kms_t);
    kms_renderer = egl_renderer->platform;

    kms_renderer->fd = -1;
    kms_renderer->opened_fd = -1;

    if (renderer->kms_fd >= 0) {
        kms_renderer->fd = renderer->kms_fd;
    } else {
        kms_renderer->opened_fd = open(device_name, O_RDWR);
        kms_renderer->fd = kms_renderer->opened_fd;
        if (kms_renderer->fd < 0) {
            /* Probably permissions error */
            _cg_set_error(error,
                          CG_WINSYS_ERROR,
                          CG_WINSYS_ERROR_INIT,
                          "Couldn't open %s",
                          device_name);
            return false;
        }
    }

    kms_renderer->gbm = gbm_create_device(kms_renderer->fd);
    if (kms_renderer->gbm == NULL) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_INIT,
                      "Couldn't create gbm device");
        goto close_fd;
    }

    egl_renderer->edpy = eglGetDisplay((EGLNativeDisplayType)kms_renderer->gbm);
    if (egl_renderer->edpy == EGL_NO_DISPLAY) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_INIT,
                      "Couldn't get eglDisplay");
        goto destroy_gbm_device;
    }

    if (!_cg_winsys_egl_renderer_connect_common(renderer, error))
        goto egl_terminate;

    _cg_loop_add_fd(renderer,
                             kms_renderer->fd,
                             CG_POLL_FD_EVENT_IN,
                             NULL, /* no prepare callback */
                             dispatch_kms_events,
                             renderer);

    return true;

egl_terminate:
    eglTerminate(egl_renderer->edpy);
destroy_gbm_device:
    gbm_device_destroy(kms_renderer->gbm);
close_fd:
    if (kms_renderer->opened_fd >= 0)
        close(kms_renderer->opened_fd);

    _cg_winsys_renderer_disconnect(renderer);

    return false;
}

static bool
is_connector_excluded(int id,
                      int *excluded_connectors,
                      int n_excluded_connectors)
{
    int i;
    for (i = 0; i < n_excluded_connectors; i++)
        if (excluded_connectors[i] == id)
            return true;
    return false;
}

static drmModeConnector *
find_connector(int fd,
               drmModeRes *resources,
               int *excluded_connectors,
               int n_excluded_connectors)
{
    int i;

    for (i = 0; i < resources->count_connectors; i++) {
        drmModeConnector *connector =
            drmModeGetConnector(fd, resources->connectors[i]);

        if (connector && connector->connection == DRM_MODE_CONNECTED &&
            connector->count_modes > 0 &&
            !is_connector_excluded(connector->connector_id,
                                   excluded_connectors,
                                   n_excluded_connectors))
            return connector;
        drmModeFreeConnector(connector);
    }
    return NULL;
}

static bool
find_mirror_modes(drmModeModeInfo *modes0,
                  int n_modes0,
                  drmModeModeInfo *modes1,
                  int n_modes1,
                  drmModeModeInfo *mode1_out,
                  drmModeModeInfo *mode0_out)
{
    int i;
    for (i = 0; i < n_modes0; i++) {
        int j;
        drmModeModeInfo *mode0 = &modes0[i];
        for (j = 0; j < n_modes1; j++) {
            drmModeModeInfo *mode1 = &modes1[j];
            if (mode1->hdisplay == mode0->hdisplay &&
                mode1->vdisplay == mode0->vdisplay) {
                *mode0_out = *mode0;
                *mode1_out = *mode1;
                return true;
            }
        }
    }
    return false;
}

static drmModeModeInfo builtin_1024x768 = { 63500, /* clock */
                                            1024, 1072, 1176, 1328, 0, 768, 771,
                                            775, 798, 0, 59920,
                                            DRM_MODE_FLAG_NHSYNC |
                                            DRM_MODE_FLAG_PVSYNC,
                                            0, "1024x768" };

static bool
is_panel(int type)
{
    return (type == DRM_MODE_CONNECTOR_LVDS || type == DRM_MODE_CONNECTOR_eDP);
}

static cg_output_kms_t *
find_output(int _index,
            int fd,
            drmModeRes *resources,
            int *excluded_connectors,
            int n_excluded_connectors,
            cg_error_t **error)
{
    char *connector_env_name = c_strdup_printf("CG_KMS_CONNECTOR%d", _index);
    char *mode_env_name;
    drmModeConnector *connector;
    drmModeEncoder *encoder;
    cg_output_kms_t *output;
    drmModeModeInfo *modes;
    int n_modes;

    if (getenv(connector_env_name)) {
        unsigned long id = strtoul(getenv(connector_env_name), NULL, 10);
        connector = drmModeGetConnector(fd, id);
    } else
        connector = NULL;
    c_free(connector_env_name);

    if (connector == NULL)
        connector = find_connector(
            fd, resources, excluded_connectors, n_excluded_connectors);
    if (connector == NULL) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_INIT,
                      "No currently active connector found");
        return NULL;
    }

    /* XXX: At this point it seems connector->encoder_id may be an invalid id of
     * 0
     * even though the connector is marked as connected. Referencing
     * ->encoders[0]
     * seems more reliable. */
    encoder = drmModeGetEncoder(fd, connector->encoders[0]);

    output = c_slice_new0(cg_output_kms_t);
    output->connector = connector;
    output->encoder = encoder;
    output->saved_crtc = drmModeGetCrtc(fd, encoder->crtc_id);

    if (is_panel(connector->connector_type)) {
        n_modes = connector->count_modes + 1;
        modes = c_new(drmModeModeInfo, n_modes);
        memcpy(modes,
               connector->modes,
               sizeof(drmModeModeInfo) * connector->count_modes);
        /* TODO: parse EDID */
        modes[n_modes - 1] = builtin_1024x768;
    } else {
        n_modes = connector->count_modes;
        modes = c_new(drmModeModeInfo, n_modes);
        memcpy(modes, connector->modes, sizeof(drmModeModeInfo) * n_modes);
    }

    mode_env_name = c_strdup_printf("CG_KMS_CONNECTOR%d_MODE", _index);
    if (getenv(mode_env_name)) {
        const char *name = getenv(mode_env_name);
        int i;
        bool found = false;
        drmModeModeInfo mode;

        for (i = 0; i < n_modes; i++) {
            if (strcmp(modes[i].name, name) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            c_free(mode_env_name);
            _cg_set_error(error,
                          CG_WINSYS_ERROR,
                          CG_WINSYS_ERROR_INIT,
                          "CG_KMS_CONNECTOR%d_MODE of %s could not be found",
                          _index,
                          name);
            return NULL;
        }
        n_modes = 1;
        mode = modes[i];
        c_free(modes);
        modes = c_new(drmModeModeInfo, 1);
        modes[0] = mode;
    }
    c_free(mode_env_name);

    output->modes = modes;
    output->n_modes = n_modes;

    return output;
}

static void
setup_crtc_modes(cg_display_t *display, int fb_id)
{
    cg_display_egl_t *egl_display = display->winsys;
    cg_display_kms_t *kms_display = egl_display->platform;
    cg_renderer_egl_t *egl_renderer = display->renderer->winsys;
    cg_renderer_kms_t *kms_renderer = egl_renderer->platform;
    c_llist_t *l;

    for (l = kms_display->crtcs; l; l = l->next) {
        cg_kms_crtc_t *crtc = l->data;

        int ret = drmModeSetCrtc(kms_renderer->fd,
                                 crtc->id,
                                 fb_id,
                                 crtc->x,
                                 crtc->y,
                                 crtc->connectors,
                                 crtc->count,
                                 crtc->count ? &crtc->mode : NULL);
        if (ret)
            c_warning("Failed to set crtc mode %s: %m", crtc->mode.name);
    }
}

static void
flip_all_crtcs(cg_display_t *display, cg_flip_kms_t *flip, int fb_id)
{
    cg_display_egl_t *egl_display = display->winsys;
    cg_display_kms_t *kms_display = egl_display->platform;
    cg_renderer_egl_t *egl_renderer = display->renderer->winsys;
    cg_renderer_kms_t *kms_renderer = egl_renderer->platform;
    c_llist_t *l;

    for (l = kms_display->crtcs; l; l = l->next) {
        cg_kms_crtc_t *crtc = l->data;
        int ret;

        if (crtc->count == 0)
            continue;

        ret = drmModePageFlip(
            kms_renderer->fd, crtc->id, fb_id, DRM_MODE_PAGE_FLIP_EVENT, flip);

        if (ret) {
            c_warning("Failed to flip: %m");
            continue;
        }

        flip->pending++;
    }
}

static void
crtc_free(cg_kms_crtc_t *crtc)
{
    c_free(crtc->connectors);
    c_slice_free(cg_kms_crtc_t, crtc);
}

static cg_kms_crtc_t *
crtc_copy(cg_kms_crtc_t *from)
{
    cg_kms_crtc_t *new;

    new = c_slice_new(cg_kms_crtc_t);

    *new = *from;
    new->connectors =
        c_memdup(from->connectors, from->count * sizeof(uint32_t));

    return new;
}

static bool
_cg_winsys_egl_display_setup(cg_display_t *display,
                             cg_error_t **error)
{
    cg_display_egl_t *egl_display = display->winsys;
    cg_display_kms_t *kms_display;
    cg_renderer_egl_t *egl_renderer = display->renderer->winsys;
    cg_renderer_kms_t *kms_renderer = egl_renderer->platform;
    drmModeRes *resources;
    cg_output_kms_t *output0, *output1;
    bool mirror;
    cg_kms_crtc_t *crtc0, *crtc1;

    kms_display = c_slice_new0(cg_display_kms_t);
    egl_display->platform = kms_display;

    resources = drmModeGetResources(kms_renderer->fd);
    if (!resources) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_INIT,
                      "drmModeGetResources failed");
        return false;
    }

    output0 = find_output(0,
                          kms_renderer->fd,
                          resources,
                          NULL,
                          0, /* n excluded connectors */
                          error);
    kms_display->outputs = c_llist_append(kms_display->outputs, output0);
    if (!output0)
        return false;

    if (getenv("CG_KMS_MIRROR"))
        mirror = true;
    else
        mirror = false;

    if (mirror) {
        int exclude_connector = output0->connector->connector_id;
        output1 = find_output(1,
                              kms_renderer->fd,
                              resources,
                              &exclude_connector,
                              1, /* n excluded connectors */
                              error);
        if (!output1)
            return false;

        kms_display->outputs = c_llist_append(kms_display->outputs, output1);

        if (!find_mirror_modes(output0->modes,
                               output0->n_modes,
                               output1->modes,
                               output1->n_modes,
                               &output0->mode,
                               &output1->mode)) {
            _cg_set_error(error,
                          CG_WINSYS_ERROR,
                          CG_WINSYS_ERROR_INIT,
                          "Failed to find matching modes for mirroring");
            return false;
        }
    } else {
        output0->mode = output0->modes[0];
        output1 = NULL;
    }

    crtc0 = c_slice_new(cg_kms_crtc_t);
    crtc0->id = output0->encoder->crtc_id;
    crtc0->x = 0;
    crtc0->y = 0;
    crtc0->mode = output0->mode;
    crtc0->connectors = c_new(uint32_t, 1);
    crtc0->connectors[0] = output0->connector->connector_id;
    crtc0->count = 1;
    kms_display->crtcs = c_llist_prepend(kms_display->crtcs, crtc0);

    if (output1) {
        crtc1 = c_slice_new(cg_kms_crtc_t);
        crtc1->id = output1->encoder->crtc_id;
        crtc1->x = 0;
        crtc1->y = 0;
        crtc1->mode = output1->mode;
        crtc1->connectors = c_new(uint32_t, 1);
        crtc1->connectors[0] = output1->connector->connector_id;
        crtc1->count = 1;
        kms_display->crtcs = c_llist_prepend(kms_display->crtcs, crtc1);
    }

    kms_display->width = output0->mode.hdisplay;
    kms_display->height = output0->mode.vdisplay;

    /* We defer setting the crtc modes until the first swap_buffers request of a
     * cg_onscreen_t framebuffer. */
    kms_display->pending_set_crtc = true;

    return true;
}

static void
output_free(int fd, cg_output_kms_t *output)
{
    if (output->modes)
        c_free(output->modes);

    if (output->encoder)
        drmModeFreeEncoder(output->encoder);

    if (output->connector) {
        if (output->saved_crtc) {
            int ret = drmModeSetCrtc(fd,
                                     output->saved_crtc->crtc_id,
                                     output->saved_crtc->buffer_id,
                                     output->saved_crtc->x,
                                     output->saved_crtc->y,
                                     &output->connector->connector_id,
                                     1,
                                     &output->saved_crtc->mode);
            if (ret)
                c_warning(C_STRLOC ": Error restoring saved CRTC");
        }
        drmModeFreeConnector(output->connector);
    }

    c_slice_free(cg_output_kms_t, output);
}

static void
_cg_winsys_egl_display_destroy(cg_display_t *display)
{
    cg_display_egl_t *egl_display = display->winsys;
    cg_display_kms_t *kms_display = egl_display->platform;
    cg_renderer_t *renderer = display->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    cg_renderer_kms_t *kms_renderer = egl_renderer->platform;
    c_llist_t *l;

    for (l = kms_display->outputs; l; l = l->next)
        output_free(kms_renderer->fd, l->data);
    c_llist_free(kms_display->outputs);
    kms_display->outputs = NULL;

    c_llist_free_full(kms_display->crtcs, (c_destroy_func_t)crtc_free);

    c_slice_free(cg_display_kms_t, egl_display->platform);
}

static bool
_cg_winsys_egl_device_created(cg_display_t *display,
                               cg_error_t **error)
{
    cg_display_egl_t *egl_display = display->winsys;
    cg_display_kms_t *kms_display = egl_display->platform;
    cg_renderer_t *renderer = display->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    cg_renderer_kms_t *kms_renderer = egl_renderer->platform;

    if ((egl_renderer->private_features &
         CG_EGL_WINSYS_FEATURE_SURFACELESS_CONTEXT) == 0) {
        kms_display->dummy_gbm_surface =
            gbm_surface_create(kms_renderer->gbm,
                               16,
                               16,
                               GBM_FORMAT_XRGB8888,
                               GBM_BO_USE_RENDERING);
        if (!kms_display->dummy_gbm_surface) {
            _cg_set_error(error,
                          CG_WINSYS_ERROR,
                          CG_WINSYS_ERROR_CREATE_CONTEXT,
                          "Failed to create dummy GBM surface");
            return false;
        }

        egl_display->dummy_surface = eglCreateWindowSurface(
            egl_renderer->edpy,
            egl_display->egl_config,
            (NativeWindowType)kms_display->dummy_gbm_surface,
            NULL);
        if (egl_display->dummy_surface == EGL_NO_SURFACE) {
            _cg_set_error(error,
                          CG_WINSYS_ERROR,
                          CG_WINSYS_ERROR_CREATE_CONTEXT,
                          "Failed to create dummy EGL surface");
            return false;
        }
    }

    if (!_cg_winsys_egl_make_current(display,
                                     egl_display->dummy_surface,
                                     egl_display->dummy_surface,
                                     egl_display->egl_context)) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_CONTEXT,
                      "Failed to make context current");
        return false;
    }

    return true;
}

static void
_cg_winsys_egl_cleanup_device(cg_display_t *display)
{
    cg_display_egl_t *egl_display = display->winsys;
    cg_display_kms_t *kms_display = egl_display->platform;
    cg_renderer_t *renderer = display->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;

    if (egl_display->dummy_surface != EGL_NO_SURFACE) {
        eglDestroySurface(egl_renderer->edpy, egl_display->dummy_surface);
        egl_display->dummy_surface = EGL_NO_SURFACE;
    }

    if (kms_display->dummy_gbm_surface != NULL) {
        gbm_surface_destroy(kms_display->dummy_gbm_surface);
        kms_display->dummy_gbm_surface = NULL;
    }
}

static void
_cg_winsys_onscreen_swap_buffers_with_damage(
    cg_onscreen_t *onscreen, const int *rectangles, int n_rectangles)
{
    cg_device_t *dev = CG_FRAMEBUFFER(onscreen)->dev;
    cg_display_egl_t *egl_display = dev->display->winsys;
    cg_display_kms_t *kms_display = egl_display->platform;
    cg_renderer_t *renderer = dev->display->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    cg_renderer_kms_t *kms_renderer = egl_renderer->platform;
    cg_onscreen_egl_t *egl_onscreen = onscreen->winsys;
    cg_onscreen_kms_t *kms_onscreen = egl_onscreen->platform;
    uint32_t handle, stride;
    cg_flip_kms_t *flip;

    /* If we already have a pending swap then block until it completes */
    while (kms_onscreen->next_fb_id != 0)
        handle_drm_event(kms_renderer);

    parent_vtable->onscreen_swap_buffers_with_damage(
        onscreen, rectangles, n_rectangles);

    /* Now we need to set the CRTC to whatever is the front buffer */
    kms_onscreen->next_bo =
        gbm_surface_lock_front_buffer(kms_onscreen->surface);

#if (CG_VERSION_ENCODE(CG_GBM_MAJOR, CG_GBM_MINOR, CG_GBM_MICRO) >=            \
     CG_VERSION_ENCODE(8, 1, 0))
    stride = gbm_bo_get_stride(kms_onscreen->next_bo);
#else
    stride = gbm_bo_get_pitch(kms_onscreen->next_bo);
#endif
    handle = gbm_bo_get_handle(kms_onscreen->next_bo).u32;

    if (drmModeAddFB(kms_renderer->fd,
                     kms_display->width,
                     kms_display->height,
                     24, /* depth */
                     32, /* bpp */
                     stride,
                     handle,
                     &kms_onscreen->next_fb_id)) {
        c_warning("Failed to create new back buffer handle: %m");
        gbm_surface_release_buffer(kms_onscreen->surface,
                                   kms_onscreen->next_bo);
        kms_onscreen->next_bo = NULL;
        kms_onscreen->next_fb_id = 0;
        return;
    }

    /* If this is the first framebuffer to be presented then we now setup the
     * crtc modes, else we flip from the previous buffer */
    if (kms_display->pending_set_crtc) {
        setup_crtc_modes(dev->display, kms_onscreen->next_fb_id);
        kms_display->pending_set_crtc = false;
    }

    flip = c_slice_new0(cg_flip_kms_t);
    flip->onscreen = onscreen;

    flip_all_crtcs(dev->display, flip, kms_onscreen->next_fb_id);

    if (flip->pending == 0) {
        drmModeRmFB(kms_renderer->fd, kms_onscreen->next_fb_id);
        gbm_surface_release_buffer(kms_onscreen->surface,
                                   kms_onscreen->next_bo);
        kms_onscreen->next_bo = NULL;
        kms_onscreen->next_fb_id = 0;
        c_slice_free(cg_flip_kms_t, flip);
        flip = NULL;
    } else {
        /* Ensure the onscreen remains valid while it has any pending flips...
         */
        cg_object_ref(flip->onscreen);
    }
}

static bool
_cg_winsys_egl_device_init(cg_device_t *dev,
                           cg_error_t **error)
{
    cg_renderer_t *renderer = dev->display->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    cg_renderer_kms_t *kms_renderer = egl_renderer->platform;
    int ret;

    CG_FLAGS_SET(dev->winsys_features,
                 CG_WINSYS_FEATURE_SYNC_AND_COMPLETE_EVENT,
                 true);


    ret = drmGetCap(kms_renderer->fd, DRM_CAP_TIMESTAMP_MONOTONIC, &cap);
    if (ret ==0 && cap == 1) {
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_PRESENTATION_TIME, true);
        dev->presentation_time_seen = true;
        dev->presentation_clock_is_monotonic = true;
    }

    return true;
}

static bool
_cg_winsys_onscreen_init(cg_onscreen_t *onscreen,
                         cg_error_t **error)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_display_t *display = dev->display;
    cg_display_egl_t *egl_display = display->winsys;
    cg_display_kms_t *kms_display = egl_display->platform;
    cg_renderer_t *renderer = display->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    cg_renderer_kms_t *kms_renderer = egl_renderer->platform;
    cg_onscreen_egl_t *egl_onscreen;
    cg_onscreen_kms_t *kms_onscreen;

    c_return_val_if_fail(egl_display->egl_context, false);

    if (kms_display->onscreen) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_ONSCREEN,
                      "Cannot have multiple onscreens in the KMS platform");
        return false;
    }

    kms_display->onscreen = onscreen;

    onscreen->winsys = c_slice_new0(cg_onscreen_egl_t);
    egl_onscreen = onscreen->winsys;

    kms_onscreen = c_slice_new0(cg_onscreen_kms_t);
    egl_onscreen->platform = kms_onscreen;

    kms_onscreen->surface =
        gbm_surface_create(kms_renderer->gbm,
                           kms_display->width,
                           kms_display->height,
                           GBM_BO_FORMAT_XRGB8888,
                           GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

    if (!kms_onscreen->surface) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_ONSCREEN,
                      "Failed to allocate surface");
        return false;
    }

    egl_onscreen->egl_surface =
        eglCreateWindowSurface(egl_renderer->edpy,
                               egl_display->egl_config,
                               (NativeWindowType)kms_onscreen->surface,
                               NULL);
    if (egl_onscreen->egl_surface == EGL_NO_SURFACE) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_ONSCREEN,
                      "Failed to allocate surface");
        return false;
    }

    _cg_framebuffer_winsys_update_size(
        framebuffer, kms_display->width, kms_display->height);

    return true;
}

static void
_cg_winsys_onscreen_deinit(cg_onscreen_t *onscreen)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_display_t *display = dev->display;
    cg_display_egl_t *egl_display = display->winsys;
    cg_display_kms_t *kms_display = egl_display->platform;
    cg_renderer_t *renderer = dev->display->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    cg_onscreen_egl_t *egl_onscreen = onscreen->winsys;
    cg_onscreen_kms_t *kms_onscreen;

    /* If we never successfully allocated then there's nothing to do */
    if (egl_onscreen == NULL)
        return;

    kms_display->onscreen = NULL;

    kms_onscreen = egl_onscreen->platform;

    /* flip state takes a reference on the onscreen so there should
     * never be outstanding flips when we reach here. */
    c_return_if_fail(kms_onscreen->next_fb_id == 0);

    free_current_bo(onscreen);

    if (egl_onscreen->egl_surface != EGL_NO_SURFACE) {
        eglDestroySurface(egl_renderer->edpy, egl_onscreen->egl_surface);
        egl_onscreen->egl_surface = EGL_NO_SURFACE;
    }

    if (kms_onscreen->surface) {
        gbm_surface_destroy(kms_onscreen->surface);
        kms_onscreen->surface = NULL;
    }

    c_slice_free(cg_onscreen_kms_t, kms_onscreen);
    c_slice_free(cg_onscreen_egl_t, onscreen->winsys);
    onscreen->winsys = NULL;
}

static const cg_winsys_egl_vtable_t _cg_winsys_egl_vtable = {
    .display_setup = _cg_winsys_egl_display_setup,
    .display_destroy = _cg_winsys_egl_display_destroy,
    .device_created = _cg_winsys_egl_device_created,
    .cleanup_device = _cg_winsys_egl_cleanup_device,
    .device_init = _cg_winsys_egl_device_init,
};

const cg_winsys_vtable_t *
_cg_winsys_egl_kms_get_vtable(void)
{
    static bool vtable_inited = false;
    static cg_winsys_vtable_t vtable;

    if (!vtable_inited) {
        /* The EGL_KMS winsys is a subclass of the EGL winsys so we
           start by copying its vtable */

        parent_vtable = _cg_winsys_egl_get_vtable();
        vtable = *parent_vtable;

        vtable.id = CG_WINSYS_ID_EGL_KMS;
        vtable.name = "EGL_KMS";

        vtable.renderer_connect = _cg_winsys_renderer_connect;
        vtable.renderer_disconnect = _cg_winsys_renderer_disconnect;

        vtable.onscreen_init = _cg_winsys_onscreen_init;
        vtable.onscreen_deinit = _cg_winsys_onscreen_deinit;

        /* The KMS winsys doesn't support swap region */
        vtable.onscreen_swap_region = NULL;
        vtable.onscreen_swap_buffers_with_damage =
            _cg_winsys_onscreen_swap_buffers_with_damage;

        vtable_inited = true;
    }

    return &vtable;
}

void
cg_kms_renderer_set_kms_fd(cg_renderer_t *renderer, int fd)
{
    c_return_if_fail(cg_is_renderer(renderer));
    /* NB: Renderers are considered immutable once connected */
    c_return_if_fail(!renderer->connected);

    renderer->kms_fd = fd;
}

int
cg_kms_renderer_get_kms_fd(cg_renderer_t *renderer)
{
    c_return_val_if_fail(cg_is_renderer(renderer), -1);

    if (renderer->connected) {
        cg_renderer_egl_t *egl_renderer = renderer->winsys;
        cg_renderer_kms_t *kms_renderer = egl_renderer->platform;
        return kms_renderer->fd;
    } else
        return -1;
}

void
cg_kms_display_queue_modes_reset(cg_display_t *display)
{
    if (display->setup) {
        cg_display_egl_t *egl_display = display->winsys;
        cg_display_kms_t *kms_display = egl_display->platform;
        kms_display->pending_set_crtc = true;
    }
}

bool
cg_kms_display_set_layout(cg_display_t *display,
                          int width,
                          int height,
                          cg_kms_crtc_t **crtcs,
                          int n_crtcs,
                          cg_error_t **error)
{
    cg_display_egl_t *egl_display = display->winsys;
    cg_display_kms_t *kms_display = egl_display->platform;
    cg_renderer_t *renderer = display->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    cg_renderer_kms_t *kms_renderer = egl_renderer->platform;
    c_llist_t *crtc_list;
    int i;

    if ((width != kms_display->width || height != kms_display->height) &&
        kms_display->onscreen) {
        cg_onscreen_egl_t *egl_onscreen = kms_display->onscreen->winsys;
        cg_onscreen_kms_t *kms_onscreen = egl_onscreen->platform;
        struct gbm_surface *new_surface;
        EGLSurface new_egl_surface;

        /* Need to drop the GBM surface and create a new one */

        new_surface =
            gbm_surface_create(kms_renderer->gbm,
                               width,
                               height,
                               GBM_BO_FORMAT_XRGB8888,
                               GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

        if (!new_surface) {
            _cg_set_error(error,
                          CG_WINSYS_ERROR,
                          CG_WINSYS_ERROR_CREATE_ONSCREEN,
                          "Failed to allocate new surface");
            return false;
        }

        new_egl_surface = eglCreateWindowSurface(egl_renderer->edpy,
                                                 egl_display->egl_config,
                                                 (NativeWindowType)new_surface,
                                                 NULL);
        if (new_egl_surface == EGL_NO_SURFACE) {
            _cg_set_error(error,
                          CG_WINSYS_ERROR,
                          CG_WINSYS_ERROR_CREATE_ONSCREEN,
                          "Failed to allocate new surface");
            gbm_surface_destroy(new_surface);
            return false;
        }

        eglDestroySurface(egl_renderer->edpy, egl_onscreen->egl_surface);
        gbm_surface_destroy(kms_onscreen->surface);

        kms_onscreen->surface = new_surface;
        egl_onscreen->egl_surface = new_egl_surface;

        _cg_framebuffer_winsys_update_size(
            CG_FRAMEBUFFER(kms_display->onscreen), width, height);
    }

    kms_display->width = width;
    kms_display->height = height;

    c_llist_free_full(kms_display->crtcs, (c_destroy_func_t)crtc_free);

    crtc_list = NULL;
    for (i = 0; i < n_crtcs; i++) {
        crtc_list = c_llist_prepend(crtc_list, crtc_copy(crtcs[i]));
    }
    crtc_list = c_llist_reverse(crtc_list);
    kms_display->crtcs = crtc_list;

    kms_display->pending_set_crtc = true;

    return true;
}
