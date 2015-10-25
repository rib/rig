/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012 Intel Corporation
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

#include <math.h>
#include <time.h>
#include <stdlib.h>

#include <rut.h>

#include "rig-entity.h"
#include "rig-entity-inlines.h"
#include "rig-hair.h"
#include "rig-asset.h"

static rut_property_spec_t _rig_hair_prop_specs[] = {
    { .name = "hair-length",
      .nick = "Length",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .getter.float_type = rig_hair_get_length,
      .setter.float_type = rig_hair_set_length,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_VALIDATE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .validation = { .float_range = { 0, 1000 } },
      .animatable = true },
    { .name = "hair-detail",
      .nick = "Detail",
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .getter.integer_type = rig_hair_get_n_shells,
      .setter.integer_type = rig_hair_set_n_shells,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_VALIDATE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .validation = { .int_range = { -1, INT_MAX } }, },
    { .name = "hair-density",
      .nick = "Density",
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .getter.integer_type = rig_hair_get_density,
      .setter.integer_type = rig_hair_set_density,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_VALIDATE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .validation = { .int_range = { 500, INT_MAX } }, },
    { .name = "hair-thickness",
      .nick = "Thickness",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .getter.float_type = rig_hair_get_thickness,
      .setter.float_type = rig_hair_set_thickness,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_VALIDATE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .validation = { .float_range = { 0.02, 100 } } },
    { NULL }
};

typedef struct _hair_particle_t {
    float lifetime;
    float diameter;
    float color[4];
    float position[3];
    float velocity[3];
    float acceleration[3];
} hair_particle_t;

float
_get_interpolated_value(float fmin, float fmax, float min, float max, float x)
{
    return (x - min) / (max - min) * (fmax - fmin) + fmin;
}

static float
_get_fuzzy_float(c_rand_t *rand, float value, float variance)
{
    float v = variance / 2.0;

    return (float)c_rand_double_range(rand, value - v, value + v);
}

static void
init_hair_particle(hair_particle_t *particle, c_rand_t *rand, float diameter)
{
    float magnitude, speed;
    float follicle_x = c_rand_double_range(rand, -1, 1);
    float follicle_y = 0;
    float follicle_z = c_rand_double_range(rand, -1, 1);

    particle->lifetime = _get_fuzzy_float(rand, 0.75, 0.5);
    particle->diameter = diameter;
    particle->acceleration[0] = particle->acceleration[2] = 0;
    particle->acceleration[1] = (-1.0 * particle->lifetime) * 0.5;
    particle->position[0] = follicle_x;
    particle->position[1] = follicle_y;
    particle->position[2] = follicle_z;
    particle->color[0] = particle->color[1] = particle->color[2] = 0.5;
    particle->color[3] = 1.0;
    particle->velocity[0] = _get_fuzzy_float(rand, 0, 0.2);
    particle->velocity[1] = _get_fuzzy_float(rand, 0.75, 0.5);
    particle->velocity[2] = _get_fuzzy_float(rand, 0, 0.2);

    magnitude =
        sqrt(pow(particle->velocity[0], 2) + pow(particle->velocity[1], 2) +
             pow(particle->velocity[2], 2));

    particle->velocity[0] /= magnitude;
    particle->velocity[1] /= magnitude;
    particle->velocity[2] /= magnitude;

    speed = particle->lifetime * 0.5;
    particle->velocity[0] *= speed;
    particle->velocity[1] *= speed;
    particle->velocity[2] *= speed;
}

static void
_get_updated_particle_color(float *color, hair_particle_t *particle, float time)
{
    float blur = particle->lifetime / 10.f;
    float kernel[4] = { 0.15, 0.12, 0.09, 0.05 };
    int i;

    color[0] = color[1] = color[2] = color[3] = 0;

    for (i = 1; i < 5; i++) {
        color[0] += _get_interpolated_value(
            0.5, 1, 0, particle->lifetime, time - (blur * i)) *
                    kernel[i - 1];
        color[3] += _get_interpolated_value(
            1, 0.5, 0, particle->lifetime, time - (blur * i)) *
                    kernel[i - 1];
    }

    color[0] +=
        _get_interpolated_value(0.5, 1, 0, particle->lifetime, time) * 0.16;
    color[3] +=
        _get_interpolated_value(1, 0.5, 0, particle->lifetime, time) * 0.16;

    for (i = 1; i < 5; i++) {
        color[0] += _get_interpolated_value(
            0.5, 1, 0, particle->lifetime, time + (blur * i)) *
                    kernel[i - 1];
        color[3] += _get_interpolated_value(
            1, 0.5, 0, particle->lifetime, time + (blur * i)) *
                    kernel[i - 1];
    }

    color[1] = color[2] = color[0];
}

static float
_get_updated_particle_diameter(hair_particle_t *particle,
                               float time)
{
    return _get_interpolated_value(
        particle->diameter, 0, 0, particle->lifetime, time);
}

static float
_get_current_particle_time(hair_particle_t *particle,
                           float current_y)
{
    float v[3], t;

    v[1] = pow(particle->velocity[1], 2) +
           2 * particle->acceleration[1] * current_y;

    if (v[1] < 0.0)
        return -1;
    else
        v[1] = sqrt(v[1]);

    t = (v[1] - particle->velocity[1]) / particle->acceleration[1];

    if (t > particle->lifetime)
        return -1;

    return t;
}

static void
_get_updated_particle_velocity(float *v, hair_particle_t *particle, float time)
{
    v[0] = particle->velocity[0] + particle->acceleration[0] * time;
    v[1] = particle->velocity[1] + particle->acceleration[1] * time;
    v[2] = particle->velocity[2] + particle->acceleration[2] * time;
}

static bool
_get_updated_particle_position(float *pos,
                               hair_particle_t *particle,
                               float *velocity,
                               float current_y,
                               float time)
{
    pos[0] = 0.5 * (particle->velocity[0] + velocity[0]) * time;
    pos[1] = 0.5 * (particle->velocity[1] + velocity[1]) * time;
    pos[2] = 0.5 * (particle->velocity[2] + velocity[2]) * time;

    if (pos[1] > current_y + (current_y / 10.0) ||
        pos[1] < current_y - (current_y / 10.0)) {
        return false;
    }

    return true;
}

static bool
calculate_updated_particle(hair_particle_t *updated_particle,
                           hair_particle_t *particle,
                           float current_y)
{
    float time;
    float v[3];
    float pos[3];
    float color[4];
    int i;

    time = _get_current_particle_time(particle, current_y);

    if (time < 0.0)
        return false;

    _get_updated_particle_velocity(v, particle, time);

    if (!_get_updated_particle_position(pos, particle, v, current_y, time))
        return false;

    _get_updated_particle_color(color, particle, time);

    updated_particle->diameter = _get_updated_particle_diameter(particle, time);
    updated_particle->lifetime = particle->lifetime - time;
    for (i = 0; i < 3; i++) {
        updated_particle->velocity[i] = v[i];
        updated_particle->position[i] =
            particle->position[i] + pos[i] + particle->diameter;
        updated_particle->color[i] = color[i];
    }

    updated_particle->color[3] = color[3];

    return true;
}

static cg_texture_t *
_rig_hair_get_fin_texture(rig_hair_t *hair)
{
    rut_shell_t *shell = rig_component_props_get_shell(&hair->component);
    cg_offscreen_t *offscreen;
    cg_pipeline_t *pipeline;
    cg_texture_t *fin_texture;
    float current_y = -1;
    float geometric_y = -0.995;
    float geo_y_iter = 0.01;
    int fin_density = hair->density * 0.01;
    float y_iter = 0.01;
    int i;

    fin_texture = (cg_texture_t *)cg_texture_2d_new_with_size(shell->cg_device,
                                                              1000, 1000);

    pipeline = cg_pipeline_new(shell->cg_device);

    offscreen = cg_offscreen_new_with_texture(fin_texture);

    cg_framebuffer_clear4f(offscreen, CG_BUFFER_BIT_COLOR, 0, 0, 0, 0);

    while (current_y <= 1.f) {
        hair_particle_t *particle;
        float pos = _get_interpolated_value(0, 1, -1, 1, current_y);
        for (i = 0; i < fin_density; i++) {
            hair_particle_t updated_particle;

            particle = &c_array_index(hair->particles, hair_particle_t, i);
            particle->diameter = hair->thickness;

            if (calculate_updated_particle(&updated_particle, particle, pos)) {
                float x = _get_interpolated_value(
                    -1, 1, 0, 1, updated_particle.position[0]);

                cg_pipeline_set_color4f(pipeline,
                                        updated_particle.color[0],
                                        updated_particle.color[1],
                                        updated_particle.color[2],
                                        updated_particle.color[3]);

                cg_framebuffer_draw_rectangle(offscreen,
                                              pipeline,
                                              x - updated_particle.diameter / 2,
                                              geometric_y - geo_y_iter,
                                              x + updated_particle.diameter / 2,
                                              geometric_y + geo_y_iter);
            }
        }

        current_y += y_iter;
        geometric_y += geo_y_iter;
    }

    cg_object_unref(offscreen);
    cg_object_unref(pipeline);

    return fin_texture;
}

static void
_rig_hair_draw_shell_texture(rig_hair_t *hair,
                             cg_texture_t *shell_texture,
                             int position)
{
    rut_shell_t *shell = rig_component_props_get_shell(&hair->component);
    cg_offscreen_t *offscreen;
    cg_pipeline_t *pipeline;
    int i;

    pipeline = cg_pipeline_new(shell->cg_device);

    offscreen = cg_offscreen_new_with_texture(shell_texture);

    cg_framebuffer_clear4f(offscreen, CG_BUFFER_BIT_COLOR, 0, 0, 0, 0);

    if (position == 0) {
        cg_pipeline_set_color4f(pipeline, 0.75, 0.75, 0.75, 1.0);
        cg_framebuffer_draw_rectangle(offscreen, pipeline, -1, -1, 1, 1);
        return;
    }

    cg_pipeline_set_layer_texture(pipeline, 0, hair->circle);

    for (i = 0; i < hair->density; i++) {
        hair_particle_t *particle;
        hair_particle_t updated_particle;
        float current_y = (float)position / (float)hair->n_shells;

        particle = &c_array_index(hair->particles, hair_particle_t, i);

        particle->diameter = hair->thickness;

        if (calculate_updated_particle(
                &updated_particle, particle, current_y)) {
            cg_pipeline_set_color4f(pipeline,
                                    updated_particle.color[0],
                                    updated_particle.color[1],
                                    updated_particle.color[2],
                                    updated_particle.color[3]);

            cg_framebuffer_draw_rectangle(
                offscreen,
                pipeline,
                updated_particle.position[0] -
                (updated_particle.diameter / 2.0),
                updated_particle.position[2] -
                (updated_particle.diameter / 2.0),
                updated_particle.position[0] +
                (updated_particle.diameter / 2.0),
                updated_particle.position[2] +
                (updated_particle.diameter / 2.0));
        }
    }

    cg_object_unref(offscreen);
    cg_object_unref(pipeline);
}

static void
_rig_hair_generate_shell_textures(rig_hair_t *hair)
{
    c_rand_t *rand = c_rand_new();
    int num_particles = hair->particles->len;
    int num_textures = hair->shell_textures->len;
    int i;

    if (hair->density > num_particles) {
        c_array_set_size(hair->particles, hair->density);

        for (i = num_particles; i < hair->density; i++) {
            hair_particle_t *particle =
                &c_array_index(hair->particles, hair_particle_t, i);
            init_hair_particle(particle, rand, hair->thickness);
        }
    } else if (hair->density < num_particles)
        c_array_set_size(hair->particles, hair->density);

    c_rand_free(rand);

    if (hair->n_shells > num_textures) {
        rut_shell_t *shell =
            rig_component_props_get_shell(&hair->component);

        c_array_set_size(hair->shell_textures, hair->n_shells);

        for (i = num_textures; i < hair->n_shells; i++) {
            cg_texture_t **textures = (void *)hair->shell_textures->data;
            textures[i] = (cg_texture_t *)cg_texture_2d_new_with_size(shell->cg_device,
                                                                      256, 256);
        }
    } else if (hair->n_shells < num_textures) {
        for (i = hair->n_shells; i < num_textures; i++) {
            cg_texture_t *texture =
                c_array_index(hair->shell_textures, cg_texture_t *, i);
            cg_object_unref(texture);
        }

        c_array_set_size(hair->shell_textures, hair->n_shells);
    }

    for (i = 0; i < hair->n_shells; i++) {
        cg_texture_t *texture =
            c_array_index(hair->shell_textures, cg_texture_t *, i);
        _rig_hair_draw_shell_texture(hair, texture, i);
    }

    hair->n_textures = hair->n_shells;
}

static void
_rig_hair_generate_hair_positions(rig_hair_t *hair)
{
    float *new_positions;
    int i;

    new_positions = c_new(float, hair->n_shells + 1);

    if (hair->shell_positions)
        c_free(hair->shell_positions);

    for (i = 2; i < hair->n_shells + 1; i++)
        new_positions[i] =
            ((float)(i + 1) / (float)hair->n_shells) * hair->length;

    new_positions[0] = new_positions[1] = 0;

    hair->shell_positions = new_positions;
}

static void
_rig_hair_free(void *object)
{
    rig_hair_t *hair = object;
    int i;

#ifdef RIG_ENABLE_DEBUG
    {
        rut_componentable_props_t *component =
            rut_object_get_properties(object, RUT_TRAIT_ID_COMPONENTABLE);
        c_return_if_fail(!component->parented);
    }
#endif

    for (i = 0; i < hair->n_shells; i++) {
        cg_texture_t *texture =
            c_array_index(hair->shell_textures, cg_texture_t *, i);
        cg_object_unref(texture);
    }
    c_array_free(hair->shell_textures, true);

    c_array_free(hair->particles, true);

    rut_introspectable_destroy(hair);

    if (hair->fin_texture)
        cg_object_unref(hair->fin_texture);
    cg_object_unref(hair->circle);
    c_free(hair->shell_positions);
    rut_object_free(rig_hair_t, hair);
}

static rut_object_t *
_rig_hair_copy(rut_object_t *obj)
{
    rig_hair_t *hair = obj;
    rig_engine_t *engine = rig_component_props_get_engine(&hair->component);
    rig_hair_t *copy = rig_hair_new(engine);

    copy->length = hair->length;
    copy->n_shells = hair->n_shells;
    copy->n_textures = hair->n_textures;
    copy->density = hair->density;
    copy->thickness = hair->thickness;

    return copy;
}

rut_type_t rig_hair_type;

void
_rig_hair_init_type(void)
{

    static rut_componentable_vtable_t componentable_vtable = {
        .copy = _rig_hair_copy
    };

    rut_type_t *type = &rig_hair_type;
#define TYPE rig_hair_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_hair_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_COMPONENTABLE,
                       offsetof(TYPE, component),
                       &componentable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */

#undef TYPE
}

rig_hair_t *
rig_hair_new(rig_engine_t *engine)
{
    rig_hair_t *hair =
        rut_object_alloc0(rig_hair_t, &rig_hair_type, _rig_hair_init_type);

    hair->component.type = RUT_COMPONENT_TYPE_HAIR;
    hair->component.parented = false;
    hair->component.engine = engine;

    hair->length = 100;
    hair->n_shells = 50;
    hair->n_textures = 0;
    hair->density = 20000;
    hair->thickness = 0.05;
    hair->shell_textures = c_array_new(false, false, sizeof(cg_texture_t *));
    hair->fin_texture = NULL;
    hair->particles = c_array_new(false, false, sizeof(hair_particle_t));
    hair->shell_positions = NULL;

    if (!engine->shell->headless) {
        hair->circle = (cg_texture_t *)cg_texture_2d_new_from_file(
            engine->shell->cg_device, rut_find_data_file("circle1.png"), NULL);
    }

    rut_introspectable_init(hair, _rig_hair_prop_specs, hair->properties);

    hair->dirty_hair_positions = true;
    hair->dirty_shell_textures = true;
    hair->dirty_fin_texture = true;

    return hair;
}

void
rig_hair_update_state(rig_hair_t *hair)
{
    if (hair->dirty_shell_textures) {
        _rig_hair_generate_shell_textures(hair);
        hair->dirty_shell_textures = false;
    }

    if (hair->dirty_fin_texture) {
        if (hair->fin_texture)
            cg_object_unref(hair->fin_texture);
        hair->fin_texture = _rig_hair_get_fin_texture(hair);
        hair->dirty_fin_texture = false;
    }

    if (hair->dirty_hair_positions) {
        _rig_hair_generate_hair_positions(hair);
        hair->dirty_hair_positions = false;
    }
}

float
rig_hair_get_length(rut_object_t *obj)
{
    rig_hair_t *hair = obj;

    return hair->length;
}

void
rig_hair_set_length(rut_object_t *obj, float length)
{
    rig_hair_t *hair = obj;
    rut_property_context_t *prop_ctx;

    if (length < 0)
        length = 0;

    if (length == hair->length)
        return;

    hair->length = length;
    hair->dirty_hair_positions = true;

    prop_ctx = rig_component_props_get_property_context(&hair->component);
    rut_property_dirty(prop_ctx, &hair->properties[RIG_HAIR_PROP_LENGTH]);
}

int
rig_hair_get_n_shells(rut_object_t *obj)
{
    rig_hair_t *hair = obj;

    return hair->n_shells;
}

void
rig_hair_set_n_shells(rut_object_t *obj, int n_shells)
{
    rig_hair_t *hair = obj;
    rut_property_context_t *prop_ctx;

    if (n_shells < 0)
        n_shells = 0;

    if (n_shells == hair->n_shells)
        return;

    hair->n_shells = n_shells;

    hair->dirty_hair_positions = true;
    hair->dirty_shell_textures = true;

    prop_ctx = rig_component_props_get_property_context(&hair->component);
    rut_property_dirty(prop_ctx, &hair->properties[RIG_HAIR_PROP_DETAIL]);
}

int
rig_hair_get_density(rut_object_t *obj)
{
    rig_hair_t *hair = obj;
    return hair->density;
}

void
rig_hair_set_density(rut_object_t *obj, int density)
{
    rig_hair_t *hair = obj;
    rut_property_context_t *prop_ctx;

    if (density == hair->density)
        return;

    hair->density = density;

    hair->dirty_shell_textures = true;
    hair->dirty_fin_texture = true;

    prop_ctx = rig_component_props_get_property_context(&hair->component);
    rut_property_dirty(prop_ctx, &hair->properties[RIG_HAIR_PROP_DENSITY]);
}

float
rig_hair_get_thickness(rut_object_t *obj)

{
    rig_hair_t *hair = obj;
    return hair->thickness;
}

void
rig_hair_set_thickness(rut_object_t *obj, float thickness)
{
    rig_hair_t *hair = obj;
    rut_property_context_t *prop_ctx;

    if (thickness < 0)
        thickness = 0;

    if (thickness == hair->thickness)
        return;

    hair->thickness = thickness;

    hair->dirty_shell_textures = true;
    hair->dirty_fin_texture = true;

    prop_ctx = rig_component_props_get_property_context(&hair->component);
    rut_property_dirty(prop_ctx, &hair->properties[RIG_HAIR_PROP_THICKNESS]);
}

float
rig_hair_get_shell_position(rut_object_t *obj, int shell)
{
    rig_hair_t *hair = obj;

    return hair->shell_positions[shell];
}

void
rig_hair_set_uniform_location(rut_object_t *obj,
                              cg_pipeline_t *pln,
                              int uniform)
{
    rig_hair_t *hair = obj;
    char *uniform_name = NULL;
    int location;

    if (uniform == RIG_HAIR_SHELL_POSITION_BLENDED ||
        uniform == RIG_HAIR_SHELL_POSITION_UNBLENDED ||
        uniform == RIG_HAIR_SHELL_POSITION_SHADOW)
        uniform_name = "hair_pos";
    else if (uniform == RIG_HAIR_LENGTH)
        uniform_name = "length";

    location = cg_pipeline_get_uniform_location(pln, uniform_name);

    hair->uniform_locations[uniform] = location;
}

void
rig_hair_set_uniform_float_value(rut_object_t *obj,
                                 cg_pipeline_t *pln,
                                 int uniform,
                                 float value)
{
    rig_hair_t *hair = obj;
    int location;

    location = hair->uniform_locations[uniform];

    cg_pipeline_set_uniform_1f(pln, location, value);
}
