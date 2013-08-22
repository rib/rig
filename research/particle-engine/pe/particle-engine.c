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

#include "particle-engine.h"

struct particle_engine {
	CoglContext *ctx;
	CoglFramebuffer *fb;
	CoglPipeline *pipeline;
	CoglPrimitive *primitive;
	CoglAttributeBuffer *attribute_buffer;

	struct vertex *vertices;

	/* The number of particles in the engine. */
	int particle_count;

	/* The size (in pixels) of particles. Each particle is represented by a
	 * rectangular point of dimensions particle_size × particle_size. */
	float particle_size;
};

struct vertex {
	float position[3];
	CoglColor color;
};

struct particle_engine *particle_engine_new(CoglContext *ctx,
					    CoglFramebuffer *fb,
					    int particle_count,
					    float particle_size)
{
	struct particle_engine *engine;
	CoglAttribute *attributes[2];
	unsigned int i;

	engine = g_slice_new0(struct particle_engine);

	engine->particle_count = particle_count;
	engine->particle_size = particle_size;

	engine->ctx = cogl_object_ref(ctx);
	engine->fb = cogl_object_ref(fb);

	engine->pipeline = cogl_pipeline_new(engine->ctx);
	engine->vertices = g_new0(struct vertex, engine->particle_count);

	engine->attribute_buffer =
		cogl_attribute_buffer_new(engine->ctx,
					  sizeof(struct vertex) *
					  engine->particle_count, engine->vertices);

	attributes[0] = cogl_attribute_new(engine->attribute_buffer,
					   "cogl_position_in",
					   sizeof(struct vertex),
					   G_STRUCT_OFFSET(struct vertex,
							   position),
					   3, COGL_ATTRIBUTE_TYPE_FLOAT);

	attributes[1] = cogl_attribute_new(engine->attribute_buffer,
					   "cogl_color_in",
					   sizeof(struct vertex),
					   G_STRUCT_OFFSET(struct vertex,
							   color),
					   4, COGL_ATTRIBUTE_TYPE_FLOAT);

	engine->primitive =
		cogl_primitive_new_with_attributes(COGL_VERTICES_MODE_POINTS,
						   engine->particle_count,
						   attributes,
						   G_N_ELEMENTS(attributes));

	cogl_pipeline_set_point_size(engine->pipeline, engine->particle_size);
	cogl_primitive_set_n_vertices(engine->primitive, engine->particle_count);

	for (i = 0; i < G_N_ELEMENTS(attributes); i++)
		cogl_object_unref(attributes[i]);

	return engine;
}

void particle_engine_free(struct particle_engine *engine)
{
	cogl_object_unref(engine->ctx);
	cogl_object_unref(engine->fb);
	cogl_object_unref(engine->pipeline);
	cogl_object_unref(engine->primitive);
	cogl_object_unref(engine->attribute_buffer);

	g_free(engine->vertices);
	g_free(engine);
}

inline void particle_engine_push_buffer(struct particle_engine *engine,
					CoglBufferAccess access,
					CoglBufferMapHint hints)
{
	CoglError *error = NULL;

	engine->vertices = cogl_buffer_map(COGL_BUFFER(engine->attribute_buffer),
					   access, hints, &error);

	if (error != NULL) {
		g_error(G_STRLOC " failed to map buffer: %s", error->message);
		return;
	}
}

inline void particle_engine_pop_buffer(struct particle_engine *engine)
{
	cogl_buffer_unmap(COGL_BUFFER(engine->attribute_buffer));
}

inline float *particle_engine_get_particle_position(struct particle_engine *engine,
						    int index)
{
	return &engine->vertices[index].position[0];
}

inline CoglColor *particle_engine_get_particle_color(struct particle_engine *engine,
						     int index)
{
	return &engine->vertices[index].color;
}

void particle_engine_paint(struct particle_engine *engine)
{
	cogl_primitive_draw(engine->primitive,
			    engine->fb,
			    engine->pipeline);
}
