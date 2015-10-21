#include <config.h>

#include <cglib/cglib.h>
#include <cglib/cg-xlib.h>
#include <cglib/winsys/cg-texture-pixmap-x11.h>
#include <clib.h>
#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <X11/extensions/Xcomposite.h>

#define X11_FOREIGN_EVENT_MASK                                                 \
    (KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |     \
     PointerMotionMask)

#define TFP_XWIN_WIDTH 200
#define TFP_XWIN_HEIGHT 200

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

int
main(int argc, char **argv)
{
    Display *xdpy;
    int composite_error = 0, composite_event = 0;
    cg_renderer_t *renderer;
    cg_onscreen_template_t *onscreen_template;
    cg_display_t *display;
    cg_device_t *dev;
    cg_onscreen_t *onscreen;
    cg_framebuffer_t *fb;
    cg_error_t *error = NULL;
    uint32_t visual;
    XVisualInfo template, *xvisinfo;
    int visinfos_count;
    XSetWindowAttributes xattr;
    unsigned long mask;
    Window xwin;
    int screen;
    Window tfp_xwin;
    Pixmap pixmap;
    cg_texture_pixmap_x11_t *tfp;
    GC gc;

    g_print("NB: Don't use this example as a benchmark since there is "
            "no synchonization between X window updates and onscreen "
            "framebuffer updates!\n");

    /* Since we want to test external ownership of the X display,
     * connect to X manually... */
    xdpy = XOpenDisplay(NULL);
    if (!xdpy) {
        fprintf(stderr, "Failed to open X Display\n");
        return 1;
    }

    XSynchronize(xdpy, True);

    if (XCompositeQueryExtension(xdpy, &composite_event, &composite_error)) {
        int major = 0, minor = 0;
        if (XCompositeQueryVersion(xdpy, &major, &minor)) {
            if (major != 0 || minor < 3)
                g_error("Missing XComposite extension >= 0.3");
        }
    }

    /* Choose a means to render... */
    renderer = cg_renderer_new();
    cg_xlib_renderer_set_foreign_display(renderer, xdpy);
    if (!cg_renderer_connect(renderer, &error)) {
        fprintf(
            stderr, "Failed to connect to a renderer: %s\n", error->message);
    }

    /* Request that onscreen framebuffers should have an alpha component */
    onscreen_template = cg_onscreen_template_new();
    cg_onscreen_template_set_has_alpha(onscreen_template, TRUE);

    /* Give Cogl our template for onscreen windows which can influence
     * how the context will be setup */
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

    XCompositeRedirectSubwindows(xdpy, xwin, CompositeRedirectManual);

    screen = DefaultScreen(xdpy);
    tfp_xwin = XCreateSimpleWindow(xdpy,
                                   xwin,
                                   0,
                                   0,
                                   TFP_XWIN_WIDTH,
                                   TFP_XWIN_HEIGHT,
                                   0,
                                   WhitePixel(xdpy, screen),
                                   WhitePixel(xdpy, screen));
    XMapWindow(xdpy, tfp_xwin);

    gc = XCreateGC(xdpy, tfp_xwin, 0, NULL);

    pixmap = XCompositeNameWindowPixmap(xdpy, tfp_xwin);

    tfp = cg_texture_pixmap_x11_new(dev, pixmap, TRUE, &error);
    if (!tfp) {
        fprintf(stderr,
                "Failed to create cg_texture_pixmap_x11_t: %s",
                error->message);
        return 1;
    }

    fb = onscreen;

    for (;; ) {
        unsigned long pixel;
        cg_pipeline_t *pipeline;

        while (XPending(xdpy)) {
            XEvent event;
            KeySym keysym;
            XNextEvent(xdpy, &event);
            switch (event.type) {
            case KeyRelease:
                keysym = XLookupKeysym(&event.xkey, 0);
                if (keysym == XK_q || keysym == XK_Q || keysym == XK_Escape)
                    return 0;
            }
            cg_xlib_renderer_handle_event(renderer, &event);
        }

        pixel = c_random_int32_range(0, 255) << 24 |
                c_random_int32_range(0, 255) << 16 |
                c_random_int32_range(0, 255) << 8;
        c_random_int32_range(0, 255);
        XSetForeground(xdpy, gc, pixel);
        XFillRectangle(
            xdpy, tfp_xwin, gc, 0, 0, TFP_XWIN_WIDTH, TFP_XWIN_HEIGHT);
        XFlush(xdpy);

        cg_framebuffer_clear4f(fb, CG_BUFFER_BIT_COLOR, 0, 0, 0, 1);
        pipeline = cg_pipeline_new(dev);
        cg_pipeline_set_layer_texture(pipeline, 0, tfp);
        cg_framebuffer_draw_rectangle(fb, pipeline, -0.8, 0.8, 0.8, -0.8);
        cg_object_unref(pipeline);
        cg_onscreen_swap_buffers(onscreen);
    }

    return 0;
}
