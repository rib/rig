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

static RObject *text;
static RObject *text_comp;

void
load_cb(RModule *module)
{
    RObject *shape = r_shape_new(module, 8.5, 8.5);
    RObject *material = r_material_new(module);
    RColor light_ambient = { .2, .2, .2, 1 };
    RColor light_diffuse = { .6, .6, .6, 1 };
    RColor light_specular = { .4, .4, .4, 1 };

    RObject *e = r_entity_new(module, NULL);
    r_set_text_by_name(module, e, "label", "light");
    r_set_vec3_by_name(module, e, "position", (float [3]){0, 0, 500});

    r_entity_rotate_x_axis(module, e, 20);
    r_entity_rotate_y_axis(module, e, -20);

    RObject *light = r_light_new(module);
    //r_set_color_by_name(module, light, "ambient", (RColor *)&{ .2, .2, .2, 1});
    r_set_color_by_name(module, light, "ambient", &light_ambient);
    r_set_color_by_name(module, light, "diffuse", &light_diffuse);
    r_set_color_by_name(module, light, "specular", &light_specular);
    r_add_component(module, e, light);

    RObject *light_frustum = r_camera_new(module);
    r_set_vec4_by_name(module, light_frustum, "ortho",
                       (float [4]){-1000, -1000, 1000, 1000});
    r_set_float_by_name(module, light_frustum, "near", 1.1f);
    r_set_float_by_name(module, light_frustum, "far", 1500.f);
    r_add_component(module, e, light_frustum);

    e = r_entity_new(module, NULL);
    r_set_vec3_by_name(module, e, "position", (float [3]){0, 0, 100});
    r_set_text_by_name(module, e, "label", "play-camera");

    RObject *play_cam = r_camera_new(module);
    /* XXX: it looks like there could be some issue with the
     * sequences associated with operations vs property log entries
     * the add-component operation has a sequence value of 0
     * so we don't switch from applying ops to setting properties.
     */
#warning "these properties aren't being set in the frontend..."
    r_set_enum_by_name(module, play_cam, "mode", R_PROJECTION_PERSPECTIVE);
    r_set_float_by_name(module, play_cam, "fov", 10);
    r_set_float_by_name(module, play_cam, "near", 10);
    r_set_float_by_name(module, play_cam, "far", 10000);
    r_set_boolean_by_name(module, play_cam, "clear", false);

    r_add_component(module, e, play_cam);

    r_open_view(module, e);

    r_set_color_by_name(module, material, "ambient",
                        r_color_str(module, "#ff0000"));
    r_set_color_by_name(module, material, "diffuse",
                        r_color_str(module, "#ff0000"));
    r_set_color_by_name(module, material, "specular",
                        r_color_str(module, "#ff0000"));

    test = r_entity_new(module, NULL);
    r_add_component(module, test, shape);
    r_add_component(module, test, material);

    r_set_vec3_by_name(module, test, "position", (float [3]){0, 0, 0});

    r_set_text_by_name(module, test, "label", "test");

#if 1
    text = r_entity_new(module, NULL);
    text_comp = r_text_new(module);

    r_set_text_by_name(module, text_comp, "text", "Hello World");
    r_add_component(module, text, text_comp);
#endif
    cam = r_find(module, "play-camera");

    c_debug("load cb");
}

void
update_cb(RModule *module, double delta_seconds)
{
    static float n = 0;

    //n -= 0.5;
    //r_entity_translate(module, cam, 0, 0, 1);
    //r_set_vec3_by_name(module, test, "position", (float [3]){0, 0, n--});

    r_entity_rotate_z_axis(module, test, 1.f);
    r_request_animation_frame(module);

    c_debug("update_cb (delta = %f)", delta_seconds);
}

void
input_cb(RModule *module, RInputEvent *event)
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

rig_native_module_t *
native_module_new(rig_engine_t *engine)
{
    rut_property_context_t *prop_ctx = engine->property_ctx;
    rig_native_module_t *component;

    prop_ctx->logging_disabled++;
    component = rig_native_module_new(engine);
    prop_ctx->logging_disabled--;

    rig_engine_op_register_component(engine, component);

    return component;
}

static void
setup_ui_cb(hello_sim_t *sim)
{
    rig_simulator_t *simulator = sim->simulator;
    rig_engine_t *engine = simulator->engine;
    rut_property_context_t *prop_ctx = engine->property_ctx;
    rig_entity_t *root;
    rig_native_module_t *native_module;
    rig_ui_t *ui = rig_ui_new(engine);
    //rig_ui_prepare(ui);

    /* XXX: we need to take care not to log properties
     * during these initial steps, until we call the
     * 'load' callback.
     *
     * We're assuming the property context is in its
     * initial state with logging disabled.
     *
     * It would be better if this were integrated with
     * rig-simulator-impl.c which is also responsible
     * for enabling logging before calling user's
     * 'update' code.
     */
    c_return_if_fail(prop_ctx->logging_disabled == 1);

    rig_engine_set_ui(engine, ui);
    rut_object_unref(ui);

    rig_engine_op_apply_context_set_ui(&simulator->apply_op_ctx, ui);

    root = rig_entity_new(engine);

    rig_engine_op_add_entity(engine, NULL, root);

    native_module = native_module_new(engine);
    rig_native_module_set_resolver(native_module, resolve_cb, NULL);

    rig_engine_op_register_component(engine, native_module);
    rig_engine_op_add_component(engine, root, native_module);

    /* XXX: would be better is this was handled in common code */
    prop_ctx->logging_disabled--;
    rig_ui_code_modules_load(ui);
    prop_ctx->logging_disabled++;

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

static uint64_t
lookup_sim_id_cb(void *object, void *user_data)
{
    rig_frontend_t *frontend = user_data;
    return rig_frontend_lookup_id(frontend, object);
}

/* XXX: would be better if most of this became common code */
static void
rig_hello_redraw(rut_shell_t *shell, void *user_data)
{
    rig_hello_t *hello = user_data;
    rig_engine_t *engine = hello->engine;
    rig_frontend_t *frontend = engine->frontend;
    rig_onscreen_view_t *primary_view =
        frontend->onscreen_views ? frontend->onscreen_views->data : NULL;
    int64_t est_frame_delta_ns = 1000000000 / 60;
    int64_t frontend_target = 0;
    double progress = 0;

    if (primary_view) {
        rut_shell_onscreen_t *onscreen = primary_view->onscreen;

        if (onscreen->presentation_time0 && onscreen->presentation_time1) {
            est_frame_delta_ns = (onscreen->presentation_time1 -
                                  onscreen->presentation_time0);
            frontend_target = onscreen->presentation_time1 + est_frame_delta_ns;

            if (frontend->last_target_time &&
                frontend_target <= frontend->last_target_time)
            {
                c_debug("present time0 = %"PRIi64, onscreen->presentation_time0);
                c_debug("present time1 = %"PRIi64, onscreen->presentation_time1);
                c_debug("est frame delta = %"PRIi64, est_frame_delta_ns);
                c_debug("last frontend target = %"PRIi64, frontend->last_target_time);
                c_debug("frontend target      = %"PRIi64, frontend_target);

                c_warning("Redrawing faster than predicted (duplicating frame to avoid going back in time)");
            } else {
                int64_t delta_ns = frontend_target - frontend->last_target_time;
                progress = delta_ns / 1000000000.0;
            }

            frontend->last_target_time = frontend_target;
        }
    }
    c_debug("frontend target = %"PRIi64, frontend_target);

    rut_shell_start_redraw(shell);

    /* XXX: we only kick off a new frame in the simulator if it's not
     * still busy... */
    if (!frontend->ui_update_pending) {
        rut_input_queue_t *input_queue = rut_shell_get_input_queue(shell);
        Rig__FrameSetup setup = RIG__FRAME_SETUP__INIT;
        double sim_progress = 1.0 / 60.0;
        rig_pb_serializer_t *serializer;
        rut_input_event_t *event;

        if (frontend_target && frontend->last_sim_target_time) {
            int64_t sim_target = frontend_target + est_frame_delta_ns;
            int64_t sim_delta_ns = sim_target - frontend->last_sim_target_time;
            sim_progress = sim_delta_ns / 1000000000.0;
        }

        /* Associate all the events with a scene camera entity which
         * also exists in the simulator... */
        c_list_for_each(event, &input_queue->events, list_node) {
            rig_onscreen_view_t *view =
                rig_frontend_find_view_for_onscreen(frontend, event->onscreen);
            event->camera_entity = view->camera_view->camera;
        }

        serializer = rig_pb_serializer_new(engine);

        rig_pb_serializer_set_object_to_id_callback(serializer,
                                                    lookup_sim_id_cb,
                                                    frontend);
        setup.has_progress = true;
        setup.progress = sim_progress;

        setup.n_events = input_queue->n_events;
        setup.events = rig_pb_serialize_input_events(serializer, input_queue);

        rig_frontend_run_simulator_frame(frontend, serializer, &setup);

        rig_pb_serializer_destroy(serializer);

        rut_input_queue_clear(input_queue);

        rut_memory_stack_rewind(engine->sim_frame_stack);
    }

    rut_shell_progress_timelines(shell, progress);

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
