/*
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <getopt.h>

#include <clib.h>
#include <rut.h>

#include <rig-c.h>

static RObject *cam;
static RObject *test;

static RObject *text;
static RObject *text_comp;

static RObject *rects[100];

void
hello_load(RModule *module)
{
    RObject *shape = r_shape_new(module, 8.5, 8.5);
    RObject *material = r_material_new(module);
    RColor light_ambient = { .2, .2, .2, 1 };
    RColor light_diffuse = { .6, .6, .6, 1 };
    RColor light_specular = { .4, .4, .4, 1 };
    RObject *view;
    RObject *controller;
    int x, y;

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
    r_set_enum_by_name(module, play_cam, "mode", R_PROJECTION_PERSPECTIVE);
    r_set_float_by_name(module, play_cam, "fov", 10);
    r_set_float_by_name(module, play_cam, "near", 10);
    r_set_float_by_name(module, play_cam, "far", 10000);
    r_set_boolean_by_name(module, play_cam, "clear", false);

    r_add_component(module, e, play_cam);

    view = r_view_new(module);
    r_set_object_by_name(module, view, "camera_entity", e);

    controller = r_controller_new(module, "Controller 0");
    r_controller_bind(module, controller, play_cam, "viewport_width", view, "width");
    r_controller_bind(module, controller, play_cam, "viewport_height", view, "height");
    r_set_boolean_by_name(module, controller, "active", true);

    r_set_color_by_name(module, material, "ambient",
                        r_color_str(module, "#ff0000"));
    r_set_color_by_name(module, material, "diffuse",
                        r_color_str(module, "#ff0000"));
    r_set_color_by_name(module, material, "specular",
                        r_color_str(module, "#ff0000"));

    test = r_entity_new(module, NULL);
    r_add_component(module, test, shape);
    r_add_component(module, test, material);

    RObject *button = r_button_input_new(module);
    r_add_component(module, test, button);

    r_entity_rotate_z_axis(module, test, 45.f);

    r_set_vec3_by_name(module, test, "position", (float [3]){0, 0, 0});

    r_set_text_by_name(module, test, "label", "test");

    for (y = 0; y < 10; y++)
        for (x = 0; x < 10; x++) {
            RObject *rect = r_entity_clone(module, test);
            r_set_float_by_name(module, rect, "scale", 0.1);
            r_set_vec3_by_name(module, rect, "position", (float [3]){-5 + x, -5 + y, 0});

            rects[10 * y + x] = rect;
        }

    //XXX: maybe add an 'enabled' property on entities
    r_set_boolean_by_name(module, material, "visible", false);

#if 1
    text = r_entity_new(module, NULL);
    text_comp = r_text_new(module);

    r_set_text_by_name(module, text_comp, "text", "Hello World");
    r_add_component(module, text, text_comp);
#endif
    cam = r_find(module, "play-camera");

    c_debug("hello_load callback");
}

void
hello_update(RModule *module, double delta_seconds)
{
    //static float n = 0;

    //n -= 0.5;
    //r_entity_translate(module, cam, 0, 0, 1);
    //r_set_vec3_by_name(module, test, "position", (float [3]){0, 0, n--});

    for (int i = 0; i < 100; i++)
        r_entity_rotate_z_axis(module, rects[i], delta_seconds * 90.f);

    r_request_animation_frame(module);

    //c_debug("hello_update callback (delta = %f)", delta_seconds);
}

void
hello_input(RModule *module, RInputEvent *event)
{
    //c_debug("hello_input callback");
}

int
main(int argc, char **argv)
{
    r_engine_t *engine = r_engine_new();

    r_engine_add_self_as_native_component(engine,
                                          R_ABI_LATEST,
                                          "hello_");

    r_engine_run(engine);
}

