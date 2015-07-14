/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2015 Intel Corporation.
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

#include <config.h>

#include <stdlib.h>
#include <getopt.h>

#include <clib.h>
#include <rut.h>

#include "rig-frontend.h"
#include "rig-engine.h"
#include "rig-pb.h"
#include "rig-logs.h"
#include "rig-c.h"

#include "components/rig-native-module.h"

#ifdef USE_NCURSES
#include "rig-curses-debug.h"
#endif

#include "rig.pb-c.h"

typedef struct hello_sim
{
    rig_simulator_t *simulator;

    rut_closure_t setup_idle;
} hello_sim_t;

static RObject *cam;
static RObject *test;

void
load_cb(RModule *module)
{
    RObject *shape = r_shape_new(module, 100, 100);
    RObject *material = r_material_new(module);

    r_set_color(module, material,
                RIG_MATERIAL_PROP_AMBIENT,
                c_color_str(module, "#ff0000"));
    r_set_color(module, material,
                RIG_MATERIAL_PROP_DIFFUSE,
                c_color_str(module, "#ff0000"));
    r_set_color(module, material,
                RIG_MATERIAL_PROP_SPECULAR,
                c_color_str(module, "#ff0000"));

    test = r_entity_new(module, NULL);
    r_add_component(module, test, shape);
    r_add_component(module, test, material);

    r_set_text_by_name(module, test, "label", "test");

    cam = r_find(module, "play-camera");

    c_debug("load cb");
}

void
update_cb(RModule *module)
{

    c_debug("update_cb");
}

void
input_cb(RModule *module)
{
    c_debug("input_cb");
}

static void *
resolve_cb(const char *symbol, void *user_data)
{
    if (strcmp(symbol, "load") == 0)
        return load_cb;
    else if (strcmp(symbol, "update") == 0)
        return update_cb;
    else if (strcmp(symbol, "input") == 0)
        return input_cb;
    return NULL;
}

static void
setup_ui_cb(hello_sim_t *sim)
{
    rig_simulator_t *simulator = sim->simulator;
    rut_shell_t *shell = simulator->shell;
    rig_engine_t *engine = simulator->engine;
    rig_entity_t *entity;
    rig_native_module_t *native_module;

    entity = rig_entity_new(shell);
    rig_engine_op_add_entity(engine, engine->ui->scene, entity);

    native_module = rig_native_module_new(engine);
    rig_native_module_set_resolver(native_module, resolve_cb, NULL);

    rig_engine_op_add_component(engine, entity, native_module);

    rut_closure_remove(&sim->setup_idle);

    c_debug("Simulator setup UI");
}

static void
simulator_init(rig_simulator_t *simulator, void *user_data)
{
    hello_sim_t *sim = c_new0(hello_sim_t, 1);

    sim->simulator = simulator;

    rut_closure_init(&sim->setup_idle, setup_ui_cb, sim);
    rut_poll_shell_add_idle(simulator->shell, &sim->setup_idle);

    c_debug("Simulator Init\n");
}

typedef struct _rig_hello_t {
    rut_object_base_t _base;

    rut_shell_t *shell;
    rig_frontend_t *frontend;
    rig_engine_t *engine;

    enum rig_simulator_run_mode simulator_mode;
    char *simulator_address;
    int simulator_port;

} rig_hello_t;

static bool rig_hello_fullscreen_option;

static void
rig_hello_redraw(rut_shell_t *shell, void *user_data)
{
    rig_hello_t *hello = user_data;
    rig_engine_t *engine = hello->engine;
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

        setup.ui_edit = NULL;

        rig_frontend_run_simulator_frame(frontend, serializer, &setup);

        rig_pb_serializer_destroy(serializer);

        rut_input_queue_clear(input_queue);

        rut_memory_stack_rewind(engine->sim_frame_stack);
    }

    rut_shell_update_timelines(shell);

    rut_shell_run_pre_paint_callbacks(shell);

    rut_shell_run_start_paint_callbacks(shell);

    rig_frontend_paint(frontend);

    rig_engine_garbage_collect(engine);

    rut_shell_run_post_paint_callbacks(shell);

    rut_memory_stack_rewind(engine->frame_stack);

    rut_shell_end_redraw(shell);

    /* FIXME: we should hook into an asynchronous notification of
     * when rendering has finished for determining when a frame is
     * finished. */
    rut_shell_finish_frame(shell);

    if (rut_shell_check_timelines(shell))
        rut_shell_queue_redraw(shell);
}

static void
_rig_hello_free(void *object)
{
    rig_hello_t *hello = object;

    rut_object_unref(hello->engine);

    rut_object_unref(hello->shell);

    if (hello->simulator_address)
        free(hello->simulator_address);

    rut_object_free(rig_hello_t, hello);
}

static rut_type_t rig_hello_type;

static void
_rig_hello_init_type(void)
{
    rut_type_init(&rig_hello_type, "rig_hello_t", _rig_hello_free);
}

static void
rig_hello_init(rut_shell_t *shell, void *user_data)
{
    rig_hello_t *hello = user_data;
    rig_engine_t *engine;

    hello->frontend = rig_frontend_new(hello->shell);

    engine = hello->frontend->engine;
    hello->engine = engine;

    rig_frontend_spawn_simulator(hello->frontend,
                                 hello->simulator_mode,
                                 hello->simulator_address,
                                 hello->simulator_port,
                                 simulator_init,
                                 hello, /* local sim init data */
                                 NULL); /* no ui to load */

    if (rig_hello_fullscreen_option) {
        rig_onscreen_view_t *onscreen_view = hello->frontend->onscreen_views->data;
        rut_shell_onscreen_t *onscreen = onscreen_view->onscreen;

        rut_shell_onscreen_set_fullscreen(onscreen, true);
    }

    //rig_frontend_set_simulator_connected_callback(
    //    hello->frontend, simulator_connected_cb, hello);
}

static rig_hello_t *
rig_hello_new(enum rig_simulator_run_mode simulator_mode,
              const char *simulator_address,
              int simulator_port)
{
    rig_hello_t *hello = rut_object_alloc0(
        rig_hello_t, &rig_hello_type, _rig_hello_init_type);

    hello->simulator_mode = simulator_mode;
    hello->simulator_address = simulator_address ? strdup(simulator_address) : NULL;
    hello->simulator_port = simulator_port;

    hello->shell = rut_shell_new(NULL, rig_hello_redraw, hello);

#ifdef USE_NCURSES
    rig_curses_add_to_shell(hello->shell);
#endif

    rut_shell_set_on_run_callback(hello->shell,
                                  rig_hello_init,
                                  hello);

    return hello;
}

#ifdef __EMSCRIPTEN__

int
main(int argc, char **argv)
{
    rig_hello_t *hello;

    _c_web_console_assert(0, "start");

    hello = rig_hello_new(RIG_SIMULATOR_RUN_MODE_WEB_SOCKET,
                            NULL, /* address (FIXME)*/
                            -1, /* port */
                            NULL);

    rut_shell_main(hello->shell);

    rut_object_unref(hello);
}

#else /* __EMSCRIPTEN__ */

static void
usage(void)
{
    fprintf(stderr, "Usage: rig-hello [OPTION]...\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -f,--fullscreen                          Run fullscreen\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -o,--oculus                              Run in Oculus Rift mode\n");
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
    rig_hello_t *hello;
    struct option long_opts[] = {

        { "fullscreen",         no_argument,       NULL, 'f' },
        { "oculus",             no_argument,       NULL, 'o' },

#ifdef RIG_ENABLE_DEBUG
        { "simulator",          required_argument, NULL, 's' },
        { "listen",             required_argument, NULL, 'l' },
        { "disable-curses",     no_argument,       NULL, 'd' },
#endif /* RIG_ENABLE_DEBUG */

        { "help",               no_argument,       NULL, 'h' },
        { 0,                    0,                 NULL,  0  }
    };

#ifdef RIG_ENABLE_DEBUG
    const char *short_opts = "fos:l:dh";
    bool enable_curses_debug = true;
#else
    const char *short_opts = "foh";
#endif

    enum rig_simulator_run_mode mode;
    char *address = NULL;
    int port = -1;
    int c;

    rut_init();

    mode = RIG_SIMULATOR_RUN_MODE_MAINLOOP;

    while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch(c) {
        case 'f':
            rig_hello_fullscreen_option = true;
            break;
        case 'o':
            rig_engine_vr_mode = true;
            rig_hello_fullscreen_option = true;
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

#if defined(RIG_ENABLE_DEBUG) && defined(USE_NCURSES)
    if (enable_curses_debug)
        rig_curses_init();
#endif

    hello = rig_hello_new(mode,
                          address,
                          port);

    rut_shell_main(hello->shell);

    rut_object_unref(hello);

    return 0;
}

#endif /* __EMSCRIPTEN__ */
