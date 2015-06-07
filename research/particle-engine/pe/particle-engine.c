/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"

#include "particle-engine.h"

#include <clib.h>

struct particle_engine {
    cg_device_t *dev;
    cg_framebuffer_t *fb;
    cg_pipeline_t *pipeline;
    cg_primitive_t *primitive;
    cg_attribute_buffer_t *attribute_buffer;

    struct vertex *vertices;

    /* The number of particles in the engine. */
    int particle_count;

    /* The size (in pixels) of particles. Each particle is represented by a
     * rectangular point of dimensions particle_size × particle_size. */
    float particle_size;
};

struct vertex {
    float position[3];
    cg_color_t color;
};

struct particle_engine *particle_engine_new(cg_device_t *dev,
                                            cg_framebuffer_t *fb,
                                            int particle_count,
                                            float particle_size)
{
    struct particle_engine *engine;
    cg_attribute_t *attributes[2];
    unsigned int i;

    engine = c_slice_new0(struct particle_engine);

    engine->particle_count = particle_count;
    engine->particle_size = particle_size;

    engine->dev = cg_object_ref(dev);
    engine->fb = cg_object_ref(fb);

    engine->pipeline = cg_pipeline_new(engine->dev);
    engine->vertices = c_new0(struct vertex, engine->particle_count);

    engine->attribute_buffer =
        cg_attribute_buffer_new(engine->dev,
                                sizeof(struct vertex) *
                                engine->particle_count, engine->vertices);

    attributes[0] = cg_attribute_new(engine->attribute_buffer,
                                     "cg_position_in",
                                     sizeof(struct vertex),
                                     C_STRUCT_OFFSET(struct vertex,
                                                     position),
                                     3, CG_ATTRIBUTE_TYPE_FLOAT);

    attributes[1] = cg_attribute_new(engine->attribute_buffer,
                                     "cg_color_in",
                                     sizeof(struct vertex),
                                     C_STRUCT_OFFSET(struct vertex,
                                                     color),
                                     4, CG_ATTRIBUTE_TYPE_FLOAT);

    engine->primitive =
        cg_primitive_new_with_attributes(CG_VERTICES_MODE_POINTS,
                                         engine->particle_count,
                                         attributes,
                                         C_N_ELEMENTS(attributes));

    cg_pipeline_set_point_size(engine->pipeline, engine->particle_size);
    cg_primitive_set_n_vertices(engine->primitive, engine->particle_count);

    for (i = 0; i < C_N_ELEMENTS(attributes); i++)
        cg_object_unref(attributes[i]);

    return engine;
}

void particle_engine_free(struct particle_engine *engine)
{
    cg_object_unref(engine->dev);
    cg_object_unref(engine->fb);
    cg_object_unref(engine->pipeline);
    cg_object_unref(engine->primitive);
    cg_object_unref(engine->attribute_buffer);

    c_free(engine->vertices);
    c_free(engine);
}

void particle_engine_push_buffer(struct particle_engine *engine,
                                 cg_buffer_access_t access,
                                 cg_buffer_map_hint_t hints)
{
    cg_error_t *error = NULL;

    engine->vertices = cg_buffer_map(engine->attribute_buffer,
                                     access, hints, &error);

    if (error != NULL) {
        c_error(C_STRLOC " failed to map buffer: %s", error->message);
        return;
    }
}

void particle_engine_pop_buffer(struct particle_engine *engine)
{
    cg_buffer_unmap(engine->attribute_buffer);
}

float *particle_engine_get_particle_position(struct particle_engine *engine,
                                                    int index)
{
    return &engine->vertices[index].position[0];
}

cg_color_t *particle_engine_get_particle_color(struct particle_engine *engine,
                                                      int index)
{
    return &engine->vertices[index].color;
}

void particle_engine_paint(struct particle_engine *engine)
{
    cg_primitive_draw(engine->primitive,
                      engine->fb,
                      engine->pipeline);
}
