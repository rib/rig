/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013,2014  Intel Corporation.
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

#include <rig-config.h>

#include <stdlib.h>
#include <getopt.h>

#include <clib.h>
#include <rut.h>

#include "rig-frontend.h"
#include "rig-engine.h"
#include "rig-pb.h"
#include "rig-logs.h"

#ifdef USE_NCURSES
#include "rig-curses-debug.h"
#endif

#include "rig.pb-c.h"

typedef struct _rig_device_t {
    rut_object_base_t _base;

    rut_shell_t *shell;
    rig_frontend_t *frontend;
    rig_engine_t *engine;

    enum rig_simulator_run_mode simulator_mode;
    char *simulator_address;
    int simulator_port;

    char *ui_filename;

} rig_device_t;

static bool rig_device_fullscreen_option;

static void
rig_device_redraw(rut_shell_t *shell, void *user_data)
{
    rig_device_t *device = user_data;
    rig_engine_t *engine = device->engine;
    rig_frontend_t *frontend = engine->frontend;

    rut_shell_start_redraw(shell);

    /* XXX: we only kick off a new frame in the simulator if it's not
     * still busy... */
    if (!frontend->ui_update_pending) {
        rut_input_queue_t *input_queue = rut_shell_get_input_queue(shell);
        Rig__FrameSetup setup = RIG__FRAME_SETUP__INIT;
        rig_pb_serializer_t *serializer;

        serializer = rig_pb_serializer_new(engine);

        setup.n_events = input_queue->n_events;
        setup.events = rig_pb_serialize_input_events(serializer, input_queue);

        rig_frontend_run_simulator_frame(frontend, serializer, &setup);

        rig_pb_serializer_destroy(serializer);

        rut_input_queue_clear(input_queue);

        rut_memory_stack_rewind(engine->sim_frame_stack);
    }

#warning "update redraw loop"
    rig_engine_progress_timelines(engine, 1.0/60.0);

    rut_shell_run_pre_paint_callbacks(shell);

    rut_shell_run_start_paint_callbacks(shell);

    rig_frontend_paint(frontend);

    rut_shell_run_post_paint_callbacks(shell);

    rig_engine_garbage_collect(engine);

    rut_memory_stack_rewind(engine->frame_stack);

    rut_shell_end_redraw(shell);

    /* FIXME: we should hook into an asynchronous notification of
     * when rendering has finished for determining when a frame is
     * finished. */
    rut_shell_finish_frame(shell);

    if (rig_engine_check_timelines(engine))
        rut_shell_queue_redraw(shell);
}

static void
_rig_device_free(void *object)
{
    rig_device_t *device = object;

    rut_object_unref(device->engine);

    rut_object_unref(device->shell);

    if (device->simulator_address)
        free(device->simulator_address);

    rut_object_free(rig_device_t, device);
}

static rut_type_t rig_device_type;

static void
_rig_device_init_type(void)
{
    rut_type_init(&rig_device_type, "rig_device_t", _rig_device_free);
}

static void
rig_device_init(rut_shell_t *shell, void *user_data)
{
    rig_device_t *device = user_data;
    rig_engine_t *engine;

    device->frontend = rig_frontend_new(device->shell);

    engine = device->frontend->engine;
    device->engine = engine;

    rig_frontend_spawn_simulator(device->frontend,
                                 device->simulator_mode,
                                 device->simulator_address,
                                 device->simulator_port,
                                 NULL, /* local sim init */
                                 NULL, /* local sim init data */
                                 device->ui_filename);

#warning "FIXME: support starting rig-device fullscreen"
#if 0
    if (rig_device_fullscreen_option) {
        rig_view_t *view = ui->views->data;
        rut_shell_onscreen_t *onscreen = view->onscreen;

        rut_shell_onscreen_set_fullscreen(onscreen, true);
    }
#endif

    //rig_frontend_set_simulator_connected_callback(
    //    device->frontend, simulator_connected_cb, device);
}

static rig_device_t *
rig_device_new(enum rig_simulator_run_mode simulator_mode,
               const char *simulator_address,
               int simulator_port,
               const char *ui_filename)
{
    rig_device_t *device = rut_object_alloc0(
        rig_device_t, &rig_device_type, _rig_device_init_type);

    device->simulator_mode = simulator_mode;
    device->simulator_address = simulator_address ? strdup(simulator_address) : NULL;
    device->simulator_port = simulator_port;

    device->ui_filename = c_strdup(ui_filename);

    device->shell = rut_shell_new(NULL, rig_device_redraw, device);

#ifdef USE_NCURSES
    rig_curses_add_to_shell(device->shell);
#endif

    rut_shell_set_on_run_callback(device->shell,
                                  rig_device_init,
                                  device);

    if (ui_filename) {
        char *assets_location = c_path_get_dirname(ui_filename);

        rut_shell_set_assets_location(device->shell, assets_location);

        c_free(assets_location);
    }

    return device;
}

#ifdef __EMSCRIPTEN__

int
main(int argc, char **argv)
{
    rig_device_t *device;

    _c_web_console_assert(0, "start");

    device = rig_device_new(RIG_SIMULATOR_RUN_MODE_WEB_SOCKET,
                            NULL, /* address (FIXME)*/
                            -1, /* port */
                            NULL);

    rut_shell_main(device->shell);

    rut_object_unref(device);
}

#else /* __EMSCRIPTEN__ */

static void
usage(void)
{
    fprintf(stderr, "Usage: rig-device [UI.rig] [OPTION]...\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -f,--fullscreen                          Run fullscreen\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\n");

#ifdef RIG_ENABLE_DEBUG
    fprintf(stderr, "  -s,--simulator={tcp:<address>[:port],    Specify how to spawn or connect to simulator\n");
    fprintf(stderr, "                  abstract:<name>,         (Simulator runs in a separate thread by default)\n");
    fprintf(stderr, "                  mainloop,\n");
    fprintf(stderr, "                  thread,\n");
    fprintf(stderr, "                  process}\n");
    fprintf(stderr, "  -l,--listen={tcp:<address>[:port],       Specify how to listen for a simulator connection\n");
    fprintf(stderr, "               abstract:<name>}\n");

    fprintf(stderr, "\n");
    fprintf(stderr, "  -d,--disable-curses                      Disable curses debug console\n");
    fprintf(stderr, "\n");
#endif
    fprintf(stderr, "  -h,--help                                Display this help message\n");
    exit(1);
}

int
main(int argc, char **argv)
{
    rig_device_t *device;
    struct option long_opts[] = {

        { "fullscreen",         no_argument,       NULL, 'f' },

#ifdef RIG_ENABLE_DEBUG
        { "simulator",          required_argument, NULL, 's' },
        { "listen",             required_argument, NULL, 'l' },
        { "disable-curses",     no_argument,       NULL, 'd' },
#endif /* RIG_ENABLE_DEBUG */

        { "help",               no_argument,       NULL, 'h' },
        { 0,                    0,                 NULL,  0  }
    };

#ifdef RIG_ENABLE_DEBUG
    const char *short_opts = "fs:l:dh";
    bool enable_curses_debug = true;
#else
    const char *short_opts = "fh";
#endif

    const char *ui_filename = NULL;
    enum rig_simulator_run_mode mode;
    char *address = NULL;
    int port = -1;
    int c;

    rut_init();

    mode = RIG_SIMULATOR_RUN_MODE_THREADED;

    while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch(c) {
        case 'f':
            rig_device_fullscreen_option = true;
            break;
#ifdef RIG_ENABLE_DEBUG
        case 's':
            rig_simulator_parse_run_mode(optarg,
                                         usage,
                                         0, /* flags */
                                         &mode,
                                         &address,
                                         &port);
            break;
        case 'l':
            rig_simulator_parse_run_mode(optarg,
                                         usage,
                                         RIG_SIMULATOR_LISTEN,
                                         &mode,
                                         &address,
                                         &port);
            break;

        case 'd':
            enable_curses_debug = false;
            break;
#endif /* RIG_ENABLE_DEBUG */

        default:
            usage();
        }
    }

    if (optind <= argc && argv[optind])
        ui_filename = argv[optind];

    /* We need a UI.rig filename if we have to spawn a simulator */
    if (mode == RIG_SIMULATOR_RUN_MODE_THREADED ||
        mode == RIG_SIMULATOR_RUN_MODE_MAINLOOP ||
        mode == RIG_SIMULATOR_RUN_MODE_PROCESS)
    {
        if (!ui_filename) {
            fprintf(stderr, "Needs a UI.rig filename\n\n");
            usage();
        }
    }

#if defined(RIG_ENABLE_DEBUG) && defined(USE_NCURSES)
    if (enable_curses_debug)
        rig_curses_init();
#endif

    device = rig_device_new(mode,
                            address,
                            port,
                            ui_filename);

    rut_shell_main(device->shell);

    rut_object_unref(device);

    return 0;
}

#endif /* __EMSCRIPTEN__ */
