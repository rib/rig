#include <config.h>


#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <getopt.h>

#include <cglib/cglib.h>
#include <cglib/cg-wayland-server.h>

#include <clib.h>

#include <wayland-server.h>

struct cogland_region {
    int x1, y1, x2, y2;
};

struct cogland_shared_region {
    struct wl_resource *resource;
    struct cogland_region region;
};

struct cogland_buffer {
    struct wl_resource *resource;
    struct wl_signal destroy_signal;
    struct wl_listener destroy_listener;

    union {
        struct wl_shm_buffer *shm_buffer;
        struct wl_buffer *legacy_buffer;
    };

    int32_t width, height;
    uint32_t busy_count;
};

struct cogland_buffer_reference{
    struct cogland_buffer *buffer;
    struct wl_listener destroy_listener;
};

struct cogland_surface {
    struct cogland_compositor *compositor;

    struct wl_resource *resource;
    int x;
    int y;
    struct cogland_buffer_reference buffer_ref;
    cg_texture_2d_t *texture;

    bool has_shell_surface;

    struct wl_signal destroy_signal;

    /* All the pending state, that wl_surface.commit will apply. */
    struct {
        /* wl_surface.attach */
        bool newly_attached;
        struct cogland_buffer *buffer;
        struct wl_listener buffer_destroy_listener;
        int32_t sx;
        int32_t sy;

        /* wl_surface.damage */
        struct cogland_region damage;

        /* wl_surface.frame */
        struct wl_list frame_callback_list;
    } pending;
};

struct cogland_shell_surface {
    struct cogland_surface *surface;
    struct wl_resource *resource;
    struct wl_listener surface_destroy_listener;
};

struct cogland_mode {
    uint32_t flags;
    int width;
    int height;
    int refresh;
};

struct cogland_output {
    struct wl_object wayland_output;

    int32_t x;
    int32_t y;
    int32_t width_mm;
    int32_t height_mm;

    cg_onscreen_t *onscreen;

    c_llist_t *modes;

};

struct cogland_compositor {
    struct wl_display *wayland_display;
    struct wl_event_loop *wayland_loop;

    cg_device_t *dev;

    uv_prepare_t main_prepare;
    uv_poll_t poll;
    uv_idle_t idle;

    int virtual_width;
    int virtual_height;
    c_llist_t *outputs;

    struct wl_list frame_callbacks;

    cg_primitive_t *triangle;
    cg_pipeline_t *triangle_pipeline;

    c_llist_t *surfaces;

    unsigned int redraw_idle;
};

static bool option_multiple_outputs = false;
static bool verbose = false;

static void
help(const char *name)
{
    fprintf(stderr, "Usage: %s [args...]\n", name);
    fprintf(stderr, "  -p, --port      Port to bind too\n");
    fprintf(stderr, "  -d, --directory Directory of files to server\n");
    fprintf(stderr, "  -v, --verbose   Be verbose\n");
    fprintf(stderr, "  -h, --help      Display this help message\n");
    exit(EXIT_FAILURE);
}

static void
process_arguments(int argc, char **argv)
{
    struct option opts[] = {
        { "multiple",   no_argument, NULL, 'm' },
        { "verbose",    no_argument, NULL, 'v' },
        { "help",       no_argument, NULL, 'h' },
        { 0,            0,           NULL,  0  }
    };
    int c;
    int i;

    while ((c = getopt_long(argc, argv, "mvh", opts, &i)) != -1) {
        switch (c) {
            case 'm':
                option_multiple_outputs = true;
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                help("cogland");
        }
    }
}

static uint32_t
get_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void
region_init(struct cogland_region *region)
{
    memset(region, 0, sizeof(*region));
}

static bool
region_is_empty(const struct cogland_region *region)
{
    return region->x1 == region->x2 || region->y1 == region->y2;
}

static void
region_add(struct cogland_region *region, int x, int y, int w, int h)
{
    if (region_is_empty(region)) {
        region->x1 = x;
        region->y1 = y;
        region->x2 = x + w;
        region->y2 = y + h;
    } else {
        if (x < region->x1)
            region->x1 = x;
        if (y < region->y1)
            region->y1 = y;
        if (x + w > region->x2)
            region->x2 = x + w;
        if (y + h > region->y2)
            region->y2 = y + h;
    }
}

static void
region_subtract(struct cogland_region *region, int x, int y, int w, int h)
{
    /* FIXME */
}

static void
cogland_buffer_destroy_handler(struct wl_listener *listener,
                               void *data)
{
    struct cogland_buffer *buffer = wl_container_of(listener, buffer, destroy_listener);

    wl_signal_emit(&buffer->destroy_signal, buffer);
    c_slice_free(struct cogland_buffer, buffer);
}

static struct cogland_buffer *
cogland_buffer_from_resource(struct wl_resource *resource)
{
    struct cogland_buffer *buffer;
    struct wl_listener *listener;

    listener = wl_resource_get_destroy_listener(resource,
                                                cogland_buffer_destroy_handler);

    if (listener) {
        buffer = wl_container_of(listener, buffer, destroy_listener);
    } else {
        buffer = c_slice_new0(struct cogland_buffer);

        buffer->resource = resource;
        wl_signal_init(&buffer->destroy_signal);
        buffer->destroy_listener.notify = cogland_buffer_destroy_handler;
        wl_resource_add_destroy_listener(resource, &buffer->destroy_listener);
    }

    return buffer;
}

static void
cogland_buffer_reference_handle_destroy(struct wl_listener *listener,
                                        void *data)
{
    struct cogland_buffer_reference *ref =
        wl_container_of(listener, ref, destroy_listener);

    c_assert(data == ref->buffer);

    ref->buffer = NULL;
}

static void
cogland_buffer_reference(struct cogland_buffer_reference *ref,
                         struct cogland_buffer *buffer)
{
    if (ref->buffer && buffer != ref->buffer) {
        ref->buffer->busy_count--;

        if (ref->buffer->busy_count == 0) {
            c_assert(wl_resource_get_client(ref->buffer->resource));
            wl_resource_queue_event(ref->buffer->resource, WL_BUFFER_RELEASE);
        }

        wl_list_remove(&ref->destroy_listener.link);
    }

    if (buffer && buffer != ref->buffer) {
        buffer->busy_count++;
        wl_signal_add(&buffer->destroy_signal, &ref->destroy_listener);
    }

    ref->buffer = buffer;
    ref->destroy_listener.notify = cogland_buffer_reference_handle_destroy;
}

typedef struct _CoglandFrameCallback {
    struct wl_list link;

    /* Pointer back to the compositor */
    struct cogland_compositor *compositor;

    struct wl_resource *resource;
} CoglandFrameCallback;

static void
paint_cb(uv_idle_t *idle)
{
    struct cogland_compositor *compositor = idle->data;
    c_llist_t *l;

    for (l = compositor->outputs; l; l = l->next) {
        struct cogland_output *output = l->data;
        cg_framebuffer_t *fb = output->onscreen;
        c_llist_t *l2;

        cg_framebuffer_clear4f(fb, CG_BUFFER_BIT_COLOR, 0, 0, 0, 1);

        cg_primitive_draw(
            compositor->triangle, fb, compositor->triangle_pipeline);

        for (l2 = compositor->surfaces; l2; l2 = l2->next) {
            struct cogland_surface *surface = l2->data;

            if (surface->texture) {
                cg_texture_2d_t *texture = surface->texture;
                cg_pipeline_t *pipeline =
                    cg_pipeline_new(compositor->dev);
                cg_pipeline_set_layer_texture(pipeline, 0, texture);
                cg_framebuffer_draw_rectangle(fb, pipeline, -1, 1, 1, -1);
                cg_object_unref(pipeline);
            }
        }
        cg_onscreen_swap_buffers(CG_ONSCREEN(fb));
    }

    while (!wl_list_empty(&compositor->frame_callbacks)) {
        CoglandFrameCallback *callback =
            wl_container_of(compositor->frame_callbacks.next, callback, link);

        wl_callback_send_done(callback->resource, get_time());
        wl_resource_destroy(callback->resource);
    }

    compositor->redraw_idle = 0;

    uv_idle_stop(&compositor->idle);
}

static void
cogland_queue_redraw(struct cogland_compositor *compositor)
{
    uv_idle_start(&compositor->idle, paint_cb);
}

static void
surface_damaged(struct cogland_surface *surface,
                int32_t x,
                int32_t y,
                int32_t width,
                int32_t height)
{
    if (surface->buffer_ref.buffer && surface->texture) {
        struct wl_shm_buffer *shm_buffer =
            wl_shm_buffer_get(surface->buffer_ref.buffer->resource);

        if (shm_buffer)
            cg_wayland_texture_set_region_from_shm_buffer(surface->texture,
                                                          x,
                                                          y,
                                                          width,
                                                          height,
                                                          shm_buffer,
                                                          x,
                                                          y,
                                                          0, /* level */
                                                          NULL);
    }

    cogland_queue_redraw(surface->compositor);
}

static void
cogland_surface_destroy(struct wl_client *wayland_client,
                        struct wl_resource *wayland_resource)
{
    wl_resource_destroy(wayland_resource);
}

static void
cogland_surface_attach(struct wl_client *wayland_client,
                       struct wl_resource *wayland_surface_resource,
                       struct wl_resource *wayland_buffer_resource,
                       int32_t sx,
                       int32_t sy)
{
    struct cogland_surface *surface =
        wl_resource_get_user_data(wayland_surface_resource);
    struct cogland_buffer *buffer;

    if (wayland_buffer_resource)
        buffer = cogland_buffer_from_resource(wayland_buffer_resource);
    else
        buffer = NULL;

    /* Attach without commit in between does not went wl_buffer.release */
    if (surface->pending.buffer)
        wl_list_remove(&surface->pending.buffer_destroy_listener.link);

    surface->pending.sx = sx;
    surface->pending.sy = sy;
    surface->pending.buffer = buffer;
    surface->pending.newly_attached = true;

    if (buffer)
        wl_signal_add(&buffer->destroy_signal,
                      &surface->pending.buffer_destroy_listener);
}

static void
cogland_surface_damage(struct wl_client *client,
                       struct wl_resource *resource,
                       int32_t x,
                       int32_t y,
                       int32_t width,
                       int32_t height)
{
    struct cogland_surface *surface = wl_resource_get_user_data(resource);

    region_add(&surface->pending.damage, x, y, width, height);
}

static void
destroy_frame_callback(struct wl_resource *callback_resource)
{
    CoglandFrameCallback *callback =
        wl_resource_get_user_data(callback_resource);

    wl_list_remove(&callback->link);
    c_slice_free(CoglandFrameCallback, callback);
}

static void
cogland_surface_frame(struct wl_client *client,
                      struct wl_resource *surface_resource,
                      uint32_t callback_id)
{
    CoglandFrameCallback *callback;
    struct cogland_surface *surface = wl_resource_get_user_data(surface_resource);

    callback = c_slice_new0(CoglandFrameCallback);
    callback->compositor = surface->compositor;
    callback->resource = wl_client_add_object(client,
                                              &wl_callback_interface,
                                              NULL, /* no implementation */
                                              callback_id,
                                              callback);
    wl_resource_set_destructor(callback->resource, destroy_frame_callback);

    wl_list_insert(surface->pending.frame_callback_list.prev, &callback->link);
}

static void
cogland_surface_set_opaque_region(struct wl_client *client,
                                  struct wl_resource *resource,
                                  struct wl_resource *region)
{
}

static void
cogland_surface_set_input_region(struct wl_client *client,
                                 struct wl_resource *resource,
                                 struct wl_resource *region)
{
}

static void
cogland_surface_commit(struct wl_client *client,
                       struct wl_resource *resource)
{
    struct cogland_surface *surface = wl_resource_get_user_data(resource);
    struct cogland_compositor *compositor = surface->compositor;

    /* wl_surface.attach */
    if (surface->pending.newly_attached &&
        surface->buffer_ref.buffer != surface->pending.buffer) {
        cg_error_t *error = NULL;

        if (surface->texture) {
            cg_object_unref(surface->texture);
            surface->texture = NULL;
        }

        cogland_buffer_reference(&surface->buffer_ref, surface->pending.buffer);

        if (surface->pending.buffer) {
            struct wl_resource *buffer_resource =
                surface->pending.buffer->resource;

            surface->texture = cg_wayland_texture_2d_new_from_buffer(
                compositor->dev, buffer_resource, &error);

            if (!surface->texture) {
                c_error("Failed to create texture_2d from wayland buffer: %s",
                        error->message);
                cg_error_free(error);
            }
        }
    }
    if (surface->pending.buffer) {
        wl_list_remove(&surface->pending.buffer_destroy_listener.link);
        surface->pending.buffer = NULL;
    }
    surface->pending.sx = 0;
    surface->pending.sy = 0;
    surface->pending.newly_attached = false;

    /* wl_surface.damage */
    if (surface->buffer_ref.buffer && surface->texture &&
        !region_is_empty(&surface->pending.damage)) {
        struct cogland_region *region = &surface->pending.damage;
        cg_texture_t *texture = surface->texture;

        if (region->x2 > cg_texture_get_width(texture))
            region->x2 = cg_texture_get_width(texture);
        if (region->y2 > cg_texture_get_height(texture))
            region->y2 = cg_texture_get_height(texture);
        if (region->x1 < 0)
            region->x1 = 0;
        if (region->y1 < 0)
            region->y1 = 0;

        surface_damaged(surface,
                        region->x1,
                        region->y1,
                        region->x2 - region->x1,
                        region->y2 - region->y1);
    }
    region_init(&surface->pending.damage);

    /* wl_surface.frame */
    wl_list_insert_list(&compositor->frame_callbacks,
                        &surface->pending.frame_callback_list);
    wl_list_init(&surface->pending.frame_callback_list);
}

static void
cogland_surface_set_buffer_transform(struct wl_client *client,
                                     struct wl_resource *resource,
                                     int32_t transform)
{
}

const struct wl_surface_interface cogland_surface_interface = {
    cogland_surface_destroy,           cogland_surface_attach,
    cogland_surface_damage,            cogland_surface_frame,
    cogland_surface_set_opaque_region, cogland_surface_set_input_region,
    cogland_surface_commit,            cogland_surface_set_buffer_transform
};

static void
cogland_surface_free(struct cogland_surface *surface)
{
    struct cogland_compositor *compositor = surface->compositor;
    CoglandFrameCallback *cb, *next;

    wl_signal_emit(&surface->destroy_signal, &surface->resource);

    compositor->surfaces = c_llist_remove(compositor->surfaces, surface);

    cogland_buffer_reference(&surface->buffer_ref, NULL);
    if (surface->texture)
        cg_object_unref(surface->texture);

    if (surface->pending.buffer)
        wl_list_remove(&surface->pending.buffer_destroy_listener.link);

    wl_list_for_each_safe(cb, next, &surface->pending.frame_callback_list, link)
    wl_resource_destroy(cb->resource);

    c_slice_free(struct cogland_surface, surface);

    cogland_queue_redraw(compositor);
}

static void
cogland_surface_resource_destroy_cb(struct wl_resource *resource)
{
    struct cogland_surface *surface = wl_resource_get_user_data(resource);
    cogland_surface_free(surface);
}

static void
surface_handle_pending_buffer_destroy(struct wl_listener *listener,
                                      void *data)
{
    struct cogland_surface *surface =
        wl_container_of(listener, surface, pending.buffer_destroy_listener);

    surface->pending.buffer = NULL;
}

static void
cogland_compositor_create_surface(
    struct wl_client *wayland_client,
    struct wl_resource *wayland_compositor_resource,
    uint32_t id)
{
    struct cogland_compositor *compositor =
        wl_resource_get_user_data(wayland_compositor_resource);
    struct cogland_surface *surface = c_slice_new0(struct cogland_surface);

    surface->compositor = compositor;

    wl_signal_init(&surface->destroy_signal);

    surface->resource = wl_client_add_object(wayland_client,
                                             &wl_surface_interface,
                                             &cogland_surface_interface,
                                             id,
                                             surface);
    wl_resource_set_destructor(surface->resource,
                               cogland_surface_resource_destroy_cb);

    surface->pending.buffer_destroy_listener.notify =
        surface_handle_pending_buffer_destroy;
    wl_list_init(&surface->pending.frame_callback_list);
    region_init(&surface->pending.damage);

    compositor->surfaces = c_llist_prepend(compositor->surfaces, surface);
}

static void
cogland_region_destroy(struct wl_client *client,
                       struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}

static void
cogland_region_add(struct wl_client *client,
                   struct wl_resource *resource,
                   int32_t x,
                   int32_t y,
                   int32_t width,
                   int32_t height)
{
    struct cogland_shared_region *shared_region = wl_resource_get_user_data(resource);

    region_add(&shared_region->region, x, y, width, height);
}

static void
cogland_region_subtract(struct wl_client *client,
                        struct wl_resource *resource,
                        int32_t x,
                        int32_t y,
                        int32_t width,
                        int32_t height)
{
    struct cogland_shared_region *shared_region = wl_resource_get_user_data(resource);

    region_subtract(&shared_region->region, x, y, width, height);
}

const struct wl_region_interface cogland_region_interface = {
    cogland_region_destroy, cogland_region_add, cogland_region_subtract
};

static void
cogland_region_resource_destroy_cb(struct wl_resource *resource)
{
    struct cogland_shared_region *region = wl_resource_get_user_data(resource);

    c_slice_free(struct cogland_shared_region, region);
}

static void
cogland_compositor_create_region(struct wl_client *wayland_client,
                                 struct wl_resource *compositor_resource,
                                 uint32_t id)
{
    struct cogland_shared_region *region = c_slice_new0(struct cogland_shared_region);

    region->resource = wl_client_add_object(wayland_client,
                                            &wl_region_interface,
                                            &cogland_region_interface,
                                            id,
                                            region);
    wl_resource_set_destructor(region->resource,
                               cogland_region_resource_destroy_cb);

    region_init(&region->region);
}

static void
bind_output(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
    struct cogland_output *output = data;
    struct wl_resource *resource =
        wl_client_add_object(client, &wl_output_interface, NULL, id, data);
    c_llist_t *l;

    wl_resource_post_event(resource,
                           WL_OUTPUT_GEOMETRY,
                           output->x,
                           output->y,
                           output->width_mm,
                           output->height_mm,
                           0, /* subpixel: unknown */
                           "unknown", /* make */
                           "unknown"); /* model */

    for (l = output->modes; l; l = l->next) {
        struct cogland_mode *mode = l->data;
        wl_resource_post_event(resource,
                               WL_OUTPUT_MODE,
                               mode->flags,
                               mode->width,
                               mode->height,
                               mode->refresh);
    }
}

static void
dirty_cb(cg_onscreen_t *onscreen,
         const cg_onscreen_dirty_info_t *info,
         void *user_data)
{
    struct cogland_compositor *compositor = user_data;

    cogland_queue_redraw(compositor);
}

static void
cogland_compositor_create_output(
    struct cogland_compositor *compositor, int x, int y, int width_mm, int height_mm)
{
    struct cogland_output *output = c_slice_new0(struct cogland_output);
    cg_framebuffer_t *fb;
    cg_error_t *error = NULL;
    struct cogland_mode *mode;

    output->x = x;
    output->y = y;
    output->width_mm = width_mm;
    output->height_mm = height_mm;

    output->wayland_output.interface = &wl_output_interface;

    wl_display_add_global(
        compositor->wayland_display, &wl_output_interface, output, bind_output);

    output->onscreen =
        cg_onscreen_new(compositor->dev, width_mm, height_mm);
    /* Eventually there will be an implicit allocate on first use so this
     * will become optional... */
    fb = output->onscreen;
    if (!cg_framebuffer_allocate(fb, &error))
        c_error("Failed to allocate framebuffer: %s\n", error->message);

    cg_onscreen_add_dirty_callback(
        output->onscreen, dirty_cb, compositor, NULL /* destroy */);

    cg_onscreen_show(output->onscreen);
    cg_framebuffer_set_viewport(
        fb, -x, -y, compositor->virtual_width, compositor->virtual_height);

    mode = c_slice_new0(struct cogland_mode);
    mode->flags = 0;
    mode->width = width_mm;
    mode->height = height_mm;
    mode->refresh = 60;

    output->modes = c_llist_prepend(output->modes, mode);

    compositor->outputs = c_llist_prepend(compositor->outputs, output);
}

const static struct wl_compositor_interface cogland_compositor_interface = {
    cogland_compositor_create_surface, cogland_compositor_create_region
};

static void
compositor_bind(struct wl_client *client,
                void *data,
                uint32_t version,
                uint32_t id)
{
    struct cogland_compositor *compositor = data;

    wl_client_add_object(client,
                         &wl_compositor_interface,
                         &cogland_compositor_interface,
                         id,
                         compositor);
}

static void
shell_surface_pong(struct wl_client *client,
                   struct wl_resource *resource,
                   uint32_t serial)
{
}

static void
shell_surface_move(struct wl_client *client,
                   struct wl_resource *resource,
                   struct wl_resource *seat,
                   uint32_t serial)
{
}

static void
shell_surface_resize(struct wl_client *client,
                     struct wl_resource *resource,
                     struct wl_resource *seat,
                     uint32_t serial,
                     uint32_t edges)
{
}

static void
shell_surface_set_toplevel(struct wl_client *client,
                           struct wl_resource *resource)
{
}

static void
shell_surface_set_transient(struct wl_client *client,
                            struct wl_resource *resource,
                            struct wl_resource *parent,
                            int32_t x,
                            int32_t y,
                            uint32_t flags)
{
}

static void
shell_surface_set_fullscreen(struct wl_client *client,
                             struct wl_resource *resource,
                             uint32_t method,
                             uint32_t framerate,
                             struct wl_resource *output)
{
}

static void
shell_surface_set_popup(struct wl_client *client,
                        struct wl_resource *resource,
                        struct wl_resource *seat,
                        uint32_t serial,
                        struct wl_resource *parent,
                        int32_t x,
                        int32_t y,
                        uint32_t flags)
{
}

static void
shell_surface_set_maximized(struct wl_client *client,
                            struct wl_resource *resource,
                            struct wl_resource *output)
{
}

static void
shell_surface_set_title(struct wl_client *client,
                        struct wl_resource *resource,
                        const char *title)
{
}

static void
shell_surface_set_class(struct wl_client *client,
                        struct wl_resource *resource,
                        const char *class_)
{
}

static const struct wl_shell_surface_interface cg_shell_surface_interface = {
    shell_surface_pong,          shell_surface_move,
    shell_surface_resize,        shell_surface_set_toplevel,
    shell_surface_set_transient, shell_surface_set_fullscreen,
    shell_surface_set_popup,     shell_surface_set_maximized,
    shell_surface_set_title,     shell_surface_set_class
};

static void
destroy_shell_surface(struct cogland_shell_surface *shell_surface)
{
    /* In case cleaning up a dead client destroys shell_surface first */
    if (shell_surface->surface) {
        wl_list_remove(&shell_surface->surface_destroy_listener.link);
        shell_surface->surface->has_shell_surface = false;
    }

    c_free(shell_surface);
}

static void
destroy_shell_surface_cb(struct wl_resource *resource)
{
    destroy_shell_surface(wl_resource_get_user_data(resource));
}

static void
shell_handle_surface_destroy(struct wl_listener *listener,
                             void *data)
{
    struct cogland_shell_surface *shell_surface =
        wl_container_of(listener, shell_surface, surface_destroy_listener);

    shell_surface->surface->has_shell_surface = false;
    shell_surface->surface = NULL;

    if (shell_surface->resource)
        wl_resource_destroy(shell_surface->resource);
    else
        destroy_shell_surface(shell_surface);
}

static void
get_shell_surface(struct wl_client *client,
                  struct wl_resource *resource,
                  uint32_t id,
                  struct wl_resource *surface_resource)
{
    struct cogland_surface *surface = wl_resource_get_user_data(surface_resource);
    struct cogland_shell_surface *shell_surface;

    if (surface->has_shell_surface) {
        wl_resource_post_error(surface_resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "wl_shell::get_shell_surface already requested");
        return;
    }

    shell_surface = c_new0(struct cogland_shell_surface, 1);

    shell_surface->surface = surface;
    shell_surface->surface_destroy_listener.notify =
        shell_handle_surface_destroy;
    wl_signal_add(&surface->destroy_signal,
                  &shell_surface->surface_destroy_listener);

    surface->has_shell_surface = true;

    shell_surface->resource = wl_client_add_object(client,
                                                   &wl_shell_surface_interface,
                                                   &cg_shell_surface_interface,
                                                   id,
                                                   shell_surface);
    wl_resource_set_destructor(shell_surface->resource,
                               destroy_shell_surface_cb);
}

static const struct wl_shell_interface cogland_shell_interface = {
    get_shell_surface
};

static void
bind_shell(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
    wl_client_add_object(
        client, &wl_shell_interface, &cogland_shell_interface, id, data);
}

static cg_device_t *
create_cg_device(struct cogland_compositor *compositor,
                  bool use_egl_constraint,
                  cg_error_t **error)
{
    cg_renderer_t *renderer = renderer = cg_renderer_new();
    cg_display_t *display;
    cg_device_t *dev;

    if (use_egl_constraint)
        cg_renderer_add_constraint(renderer, CG_RENDERER_CONSTRAINT_USES_EGL);

    if (!cg_renderer_connect(renderer, error)) {
        cg_object_unref(renderer);
        return NULL;
    }

    display = cg_display_new(renderer, NULL);
    cg_wayland_display_set_compositor_display(display,
                                              compositor->wayland_display);

    dev = cg_device_new();
    cg_device_set_display(dev, display);
    cg_device_connect(dev, error);

    cg_object_unref(renderer);
    cg_object_unref(display);

    return dev;
}

static void
cogland_main_prepare_cb(uv_prepare_t *prepare)
{
    struct cogland_compositor *compositor = prepare->data;

    wl_display_flush_clients(compositor->wayland_display);
}

static void
cogland_main_dispatch_cb(uv_poll_t *poll, int status, int events)
{
    struct cogland_compositor *compositor = poll->data;

    wl_event_loop_dispatch(compositor->wayland_loop, 0);
}

int
main(int argc, char **argv)
{
    struct cogland_compositor compositor;
    cg_error_t *error = NULL;
    cg_vertex_p2c4_t triangle_vertices[] = {
        { 0, 0.7, 0xff, 0x00, 0x00, 0xff },
        { -0.7, -0.7, 0x00, 0xff, 0x00, 0xff },
        { 0.7, -0.7, 0x00, 0x00, 0xff, 0xff }
    };
    uv_loop_t *loop = uv_default_loop();

    process_arguments(argc, argv);

    memset(&compositor, 0, sizeof(compositor));

    compositor.wayland_display = wl_display_create();
    if (compositor.wayland_display == NULL)
        c_error("failed to create wayland display");

    wl_list_init(&compositor.frame_callbacks);

    if (!wl_display_add_global(compositor.wayland_display,
                               &wl_compositor_interface,
                               &compositor,
                               compositor_bind))
        c_error("Failed to register wayland compositor object");

    wl_display_init_shm(compositor.wayland_display);

    /* We want Cogl to use an EGL renderer because otherwise it won't
     * set up the wl_drm object and only SHM buffers will work. */
    compositor.dev = create_cg_device(&compositor,
                                            true /* use EGL constraint */,
                                            &error);
    if (!compositor.dev) {
        /* If we couldn't get an EGL context then try any type of
         * context */
        cg_error_free(error);
        error = NULL;

        compositor.dev =
            create_cg_device(&compositor,
                              false, /* don't set EGL constraint */
                              &error);

        if (compositor.dev)
            c_warning("Failed to create context with EGL constraint, "
                      "falling back");
        else
            c_error("Failed to create a Cogl context: %s\n", error->message);

        cg_error_free(error);
        error = NULL;
    }

    compositor.virtual_width = 800;
    compositor.virtual_height = 600;

    if (option_multiple_outputs) {
        int hw = compositor.virtual_width / 2;
        int hh = compositor.virtual_height / 2;
        /* Emulate compositing with multiple monitors... */
        cogland_compositor_create_output(&compositor, 0, 0, hw, hh);
        cogland_compositor_create_output(&compositor, hw, 0, hw, hh);
        cogland_compositor_create_output(&compositor, 0, hh, hw, hh);
        cogland_compositor_create_output(&compositor, hw, hh, hw, hh);
    } else {
        cogland_compositor_create_output(&compositor,
                                         0,
                                         0,
                                         compositor.virtual_width,
                                         compositor.virtual_height);
    }

    if (wl_display_add_global(compositor.wayland_display,
                              &wl_shell_interface,
                              &compositor,
                              bind_shell) == NULL)
        c_error("Failed to register a global shell object");

    if (wl_display_add_socket(compositor.wayland_display, "wayland-0"))
        c_error("Failed to create socket");

    compositor.triangle = cg_primitive_new_p2c4(compositor.dev,
                                                CG_VERTICES_MODE_TRIANGLES,
                                                3,
                                                triangle_vertices);
    compositor.triangle_pipeline = cg_pipeline_new(compositor.dev);

    compositor.wayland_loop =
        wl_display_get_event_loop(compositor.wayland_display);

    uv_prepare_init(loop, &compositor.main_prepare);
    compositor.main_prepare.data = &compositor;
    uv_prepare_start(&compositor.main_prepare, cogland_main_prepare_cb);

    uv_poll_init(loop, &compositor.poll,
                 wl_event_loop_get_fd(compositor.wayland_loop));
    compositor.poll.data = &compositor;
    uv_poll_start(&compositor.poll, UV_READABLE | UV_WRITABLE,
                  cogland_main_dispatch_cb);

    uv_idle_init(loop, &compositor.idle);
    compositor.idle.data = &compositor;

    cg_uv_set_mainloop(compositor.dev, loop);
    uv_run(loop, UV_RUN_DEFAULT);

    return 0;
}
