#include <config.h>

#include <cglib/cglib.h>
#include <cglib/cg-xlib.h>
#include <glib.h>
#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define X11_FOREIGN_EVENT_MASK                                                 \
    (KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |     \
     PointerMotionMask)

static void
update_cg_x11_event_mask(cg_onscreen_t *onscreen,
                         uint32_t event_mask,
                         void *user_data)
{
    Display *xdpy = user_data;
    XSetWindowAttributes attrs;
    uint32_t xwin;

    attrs.event_mask = event_mask | X11_FOREIGN_EVENT_MASK;
    xwin = cg_x11_onscreen_get_window_xid(onscreen);

    XChangeWindowAttributes(xdpy, (Window)xwin, CWEventMask, &attrs);
}

static void
resize_handler(cg_onscreen_t *onscreen, int width, int height, void *user_data)
{
    cg_framebuffer_t *fb = user_data;
    cg_framebuffer_set_viewport(
        fb, width / 4, height / 4, width / 2, height / 2);
}

int
main(int argc, char **argv)
{
    Display *xdpy;
    cg_renderer_t *renderer;
    cg_onscreen_template_t *onscreen_template;
    cg_display_t *display;
    cg_device_t *dev;
    cg_onscreen_t *onscreen;
    cg_framebuffer_t *fb;
    cg_pipeline_t *pipeline;
    cg_error_t *error = NULL;
    uint32_t visual;
    XVisualInfo template, *xvisinfo;
    int visinfos_count;
    XSetWindowAttributes xattr;
    unsigned long mask;
    Window xwin;
    cg_vertex_p2c4_t triangle_vertices[] = {
        { 0, 0.7, 0xff, 0x00, 0x00, 0xff },
        { -0.7, -0.7, 0x00, 0xff, 0x00, 0xff },
        { 0.7, -0.7, 0x00, 0x00, 0xff, 0xff }
    };
    cg_primitive_t *triangle;

    /* Since we want to test external ownership of the X display,
     * connect to X manually... */
    xdpy = XOpenDisplay(NULL);
    if (!xdpy) {
        fprintf(stderr, "Failed to open X Display\n");
        return 1;
    }

    /* Choose a means to render... */
    renderer = cg_renderer_new();
    cg_xlib_renderer_set_foreign_display(renderer, xdpy);
    if (!cg_renderer_connect(renderer, &error)) {
        fprintf(
            stderr, "Failed to connect to a renderer: %s\n", error->message);
    }

    /* Create a template for onscreen framebuffers that requests an
     * alpha component */
    onscreen_template = cg_onscreen_template_new();
    cg_onscreen_template_set_has_alpha(onscreen_template, TRUE);

    /* Give Cogl our template for onscreen framebuffers which can
     * influence how the context will be setup */
    display = cg_display_new(renderer, onscreen_template);
    cg_object_unref(renderer);
    if (!cg_display_setup(display, &error)) {
        fprintf(
            stderr, "Failed to setup a display pipeline: %s\n", error->message);
        return 1;
    }

    dev = cg_device_new();
    if (!cg_device_connect(dev, &error)) {
        fprintf(stderr, "Failed to create context: %s\n", error->message);
        return 1;
    }

    onscreen = cg_onscreen_new(dev, 640, 480);

    /* We want to test that Cogl can handle foreign X windows... */

    visual = cg_x11_onscreen_get_visual_xid(onscreen);
    if (!visual) {
        fprintf(stderr,
                "Failed to query an X visual suitable for the "
                "configured cg_onscreen_t framebuffer\n");
        return 1;
    }

    template.visualid = visual;
    xvisinfo = XGetVisualInfo(xdpy, VisualIDMask, &template, &visinfos_count);

    /* window attributes */
    xattr.background_pixel = WhitePixel(xdpy, DefaultScreen(xdpy));
    xattr.border_pixel = 0;
    xattr.colormap = XCreateColormap(
        xdpy, DefaultRootWindow(xdpy), xvisinfo->visual, AllocNone);
    mask = CWBorderPixel | CWColormap;

    xwin = XCreateWindow(xdpy,
                         DefaultRootWindow(xdpy),
                         0,
                         0,
                         800,
                         600,
                         0,
                         xvisinfo->depth,
                         InputOutput,
                         xvisinfo->visual,
                         mask,
                         &xattr);

    XFree(xvisinfo);

    cg_x11_onscreen_set_foreign_window_xid(
        onscreen, xwin, update_cg_x11_event_mask, xdpy);

    XMapWindow(xdpy, xwin);

    fb = onscreen;

    cg_onscreen_set_resizable(onscreen, TRUE);
    cg_onscreen_add_resize_callback(onscreen, resize_handler, onscreen, NULL);

    triangle = cg_primitive_new_p2c4(dev, CG_VERTICES_MODE_TRIANGLES, 3,
                                     triangle_vertices);
    pipeline = cg_pipeline_new(dev);
    for (;; ) {
        cg_poll_fd_t *poll_fds;
        int n_poll_fds;
        int64_t timeout;

        while (XPending(xdpy)) {
            XEvent event;
            XNextEvent(xdpy, &event);
            switch (event.type) {
            case KeyRelease:
            case ButtonRelease:
                return 0;
            }
            cg_xlib_renderer_handle_event(renderer, &event);
        }

        /* After forwarding native events directly to Cogl you should
         * then allow Cogl to dispatch any corresponding event
         * callbacks, such as resize notification callbacks...
         */
        cg_loop_get_info(
            cg_device_get_renderer(dev), &poll_fds, &n_poll_fds, &timeout);
        g_poll((GPollFD *)poll_fds, n_poll_fds, 0);
        cg_loop_dispatch(
            cg_device_get_renderer(dev), poll_fds, n_poll_fds);

        cg_framebuffer_clear4f(fb, CG_BUFFER_BIT_COLOR, 0, 0, 0, 1);
        cg_primitive_draw(triangle, fb, pipeline);
        cg_onscreen_swap_buffers(onscreen);
    }

    return 0;
}
