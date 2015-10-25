/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2015  Intel Corporation.
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
 *
 */

#include <rut-config.h>

#include <math.h>

#include <clib.h>

#include <cglib/cglib.h>

#include "rut-gradient.h"

bool
rut_linear_gradient_equal(const void *key_a, const void *key_b)
{
    const rut_linear_gradient_t *a = key_a;
    const rut_linear_gradient_t *b = key_b;

    if (a->n_stops != b->n_stops)
        return false;

    return memcmp(a->stops, b->stops, a->n_stops *
                  sizeof(rut_gradient_stop_t)) == 0;
}

static int
_rut_util_next_p2(int a)
{
  int rval = 1;

  while (rval < a)
    rval <<= 1;

  return rval;
}

static float
get_max_color_component_range(const cg_color_t *color0,
                              const cg_color_t *color1)
{
    float range;
    float max = 0;

    range = fabsf(color0->red - color1->red);
    max = MAX(range, max);
    range = fabsf(color0->green - color1->green);
    max = MAX(range, max);
    range = fabsf(color0->blue - color1->blue);
    max = MAX(range, max);
    range = fabsf(color0->alpha - color1->alpha);
    max = MAX(range, max);

    return max;
}

static int
_rut_linear_gradient_width_for_stops(rut_extend_t extend,
                                     unsigned int n_stops,
                                     const rut_gradient_stop_t *stops)
{
    unsigned int n;
    float max_texels_per_unit_offset = 0;
    float total_offset_range;

    /* Find the stop pair demanding the most precision because we are
     * interpolating the largest color-component range.
     *
     * From that we can define the relative sizes of all the other
     * stop pairs within our texture and thus the overall size.
     *
     * To determine the maximum number of texels for a given gap we
     * look at the range of colors we are expected to interpolate (so
     * long as the stop offsets are not degenerate) and we simply
     * assume we want one texel for each unique color value possible
     * for a one byte-per-component representation.
     * XXX: maybe this is overkill and just allowing 128 levels
     * instead of 256 would be enough and then we'd rely on the
     * bilinear filtering to give the full range.
     *
     * XXX: potentially we could try and map offsets to pixels to come
     * up with a more precise mapping, but we are aiming to cache
     * the gradients so we can't make assumptions about how it will be
     * scaled in the future.
     */
    for (n = 1; n < n_stops; n++) {
	float color_range;
	float offset_range;
	float texels;
	float texels_per_unit_offset;

	/* note: degenerate stops don't need to be represented in the
	 * texture but we want to be sure that solid gaps get at least
	 * one texel and all other gaps get at least 2 texels.
	 */

	if (stops[n].offset == stops[n-1].offset)
	    continue;

	color_range = get_max_color_component_range(&stops[n].color, &stops[n-1].color);
	if (color_range == 0)
	    texels = 1;
	else
	    texels = MAX(2, 256.0f * color_range);

	/* So how many texels would we need to map over the full [0,1]
	 * gradient range so this gap would have enough texels? ... */
	offset_range = stops[n].offset - stops[n - 1].offset;
	texels_per_unit_offset = texels / offset_range;

	if (texels_per_unit_offset > max_texels_per_unit_offset)
	    max_texels_per_unit_offset = texels_per_unit_offset;
    }

    total_offset_range = fabsf(stops[n_stops - 1].offset - stops[0].offset);
    return max_texels_per_unit_offset * total_offset_range;
}

/* Aim to create gradient textures without an alpha component so we can avoid
 * needing to use blending... */
static cg_texture_components_t
components_for_stops(rut_extend_t extend,
                     int n_stops,
                     const rut_gradient_stop_t *stops)
{
    int n;

    /* We have to add extra transparent texels to the end of the gradient to
     * handle RUT_EXTEND_NONE... */
    if (extend == RUT_EXTEND_NONE)
	return CG_TEXTURE_COMPONENTS_RGBA;

    for (n = 1; n < n_stops; n++) {
	if (stops[n].color.alpha != 1.0)
            return CG_TEXTURE_COMPONENTS_RGBA;
    }

    return CG_TEXTURE_COMPONENTS_RGB;
}

static void
color_stop_lerp(const cg_color_t *c0,
		const cg_color_t *c1,
		float factor,
		cg_color_t *dest)
{
    dest->red = c0->red * (1.0f - factor) + c1->red * factor;
    dest->green = c0->green * (1.0f - factor) + c1->green * factor;
    dest->blue = c0->blue * (1.0f - factor) + c1->blue * factor;
    dest->alpha = c0->alpha * (1.0f - factor) + c1->alpha * factor;
}

static void
emit_stop(cg_vertex_p2c4_t **position,
	  float left,
	  float right,
	  const cg_color_t *left_color,
	  const cg_color_t *right_color)
{
    cg_vertex_p2c4_t *p = *position;

    uint8_t lr = left_color->red * 255;
    uint8_t lg = left_color->green * 255;
    uint8_t lb = left_color->blue * 255;
    uint8_t la = left_color->alpha * 255;

    uint8_t rr = right_color->red * 255;
    uint8_t rg = right_color->green * 255;
    uint8_t rb = right_color->blue * 255;
    uint8_t ra = right_color->alpha * 255;

    p[0].x = left;
    p[0].y = 0;
    p[0].r = lr; p[0].g = lg; p[0].b = lb; p[0].a = la;
    p[1].x = left;
    p[1].y = 1;
    p[1].r = lr; p[1].g = lg; p[1].b = lb; p[1].a = la;
    p[2].x = right;
    p[2].y = 1;
    p[2].r = rr; p[2].g = rg; p[2].b = rb; p[2].a = ra;

    p[3].x = left;
    p[3].y = 0;
    p[3].r = lr; p[3].g = lg; p[3].b = lb; p[3].a = la;
    p[4].x = right;
    p[4].y = 1;
    p[4].r = rr; p[4].g = rg; p[4].b = rb; p[4].a = ra;
    p[5].x = right;
    p[5].y = 0;
    p[5].r = rr; p[5].g = rg; p[5].b = rb; p[5].a = ra;

    *position = &p[6];
}

static void
_rut_linear_gradient_free(void *object)
{
    rut_linear_gradient_t *gradient = object;

    cg_object_unref(gradient->texture);
    c_free(gradient->stops);

    rut_object_free(rut_linear_gradient_t, object);
}

rut_type_t rut_linear_gradient_type;

static void
_rut_linear_gradient_init_type(void)
{
    rut_type_init(&rut_linear_gradient_type, "rut_linear_gradient_t",
                  _rut_linear_gradient_free);
}

rut_linear_gradient_t *
rut_linear_gradient_new(rut_shell_t *shell,
                        rut_extend_t extend_mode,
                        int n_stops,
                        const rut_gradient_stop_t *stops)
{
    rut_linear_gradient_t *gradient =
        rut_object_alloc(rut_linear_gradient_t, &rut_linear_gradient_type,
                         _rut_linear_gradient_init_type);
    rut_gradient_stop_t *internal_stops;
    int stop_offset;
    int n_internal_stops = n_stops;
    int n;
    int width;
    int left_padding = 0;
    cg_color_t left_padding_color;
    int right_padding = 0;
    cg_color_t right_padding_color;
    cg_texture_2d_t *tex;
    cg_texture_components_t components;
    cg_error_t *catch = NULL;
    int un_padded_width;
    cg_framebuffer_t *offscreen;
    int n_quads;
    int n_vertices;
    float prev;
    float right;
    cg_vertex_p2c4_t *vertices;
    cg_vertex_p2c4_t *p;
    cg_primitive_t *prim;
    cg_pipeline_t *pipeline;

    gradient->extend_mode = extend_mode;

    stop_offset = 0;

    /* We really need stops covering the full [0,1] range for repeat/reflect
     * if we want to use sampler REPEAT/MIRROR wrap modes so we may need
     * to add some extra stops... */
    if (extend_mode == RUT_EXTEND_REPEAT ||
        extend_mode == RUT_EXTEND_REFLECT)
    {
	if (stops[0].offset != 0) {
	    n_internal_stops++;
	    stop_offset++;
	}

	if (stops[n_stops - 1].offset != 1)
	    n_internal_stops++;
    }

    internal_stops = c_malloc(sizeof(rut_gradient_stop_t) * n_internal_stops);

    gradient->n_stops = n_internal_stops;
    gradient->stops = internal_stops;

    /* Input rut_gradient_stop_t colors are all unpremultiplied but we
     * need to interpolate premultiplied colors so we premultiply as
     * we copy the stops internally.
     *
     * Another thing to note is that by premultiplying the colors
     * early we'll also reduce the range of colors to interpolate
     * which can result in smaller gradient textures.
     */
    for (n = 0; n < n_stops; n++) {
        internal_stops[n + stop_offset] = stops[n];
        cg_color_premultiply(&internal_stops[n + stop_offset].color);
    }

    if (n_internal_stops != n_stops) {
	if (extend_mode == RUT_EXTEND_REPEAT) {
	    if (stops[0].offset != 0) {
		/* what's the wrap-around distance between the user's end-stops? */
		double dx = (1.0 - stops[n_stops - 1].offset) + stops[0].offset;
		internal_stops[0].offset = 0;
		color_stop_lerp(&stops[0].color,
				&stops[n_stops - 1].color,
				stops[0].offset / dx,
				&internal_stops[0].color);
	    }
	    if (stops[n_stops - 1].offset != 1) {
		internal_stops[n_internal_stops - 1].offset = 1;
		internal_stops[n_internal_stops - 1].color = internal_stops[0].color;
	    }
	} else if (extend_mode == RUT_EXTEND_REFLECT) {
	    if (stops[0].offset != 0) {
		internal_stops[0].offset = 0;
		internal_stops[0].color = stops[n_stops - 1].color;
	    }
	    if (stops[n_stops - 1].offset != 1) {
		internal_stops[n_internal_stops - 1].offset = 1;
		internal_stops[n_internal_stops - 1].color = stops[0].color;
	    }
	}
    }

    stops = internal_stops;
    n_stops = n_internal_stops;

    width = _rut_linear_gradient_width_for_stops(extend_mode, n_stops, stops);

    if (extend_mode == RUT_EXTEND_PAD) {

	/* Here we need to guarantee that the edge texels of our
	 * texture correspond to the desired padding color so we
	 * can use CLAMP_TO_EDGE.
	 *
	 * For short stop-gaps and especially for degenerate stops
	 * it's possible that without special consideration the
	 * user's end stop colors would not be present in our final
	 * texture.
	 *
	 * To handle this we forcibly add two extra padding texels
	 * at the edges which extend beyond the [0,1] range of the
	 * gradient itself and we will later report a translate and
	 * scale transform to compensate for this.
	 */

	/* XXX: If we consider generating a mipmap for our 1d texture
	 * at some point then we also need to consider how much
	 * padding to add to be sure lower mipmap levels still have
	 * the desired edge color (as opposed to a linear blend with
	 * other colors of the gradient).
	 */

	left_padding = 1;
	left_padding_color = stops[0].color;
	right_padding = 1;
	right_padding_color = stops[n_stops - 1].color;
    } else if (extend_mode == RUT_EXTEND_NONE) {
	/* We handle EXTEND_NONE by adding two extra, transparent, texels at
	 * the ends of the texture and use CLAMP_TO_EDGE.
	 *
	 * We add a scale and translate transform so to account for our texels
	 * extending beyond the [0,1] range. */

	left_padding = 1;
	left_padding_color.red = 0;
	left_padding_color.green = 0;
	left_padding_color.blue = 0;
	left_padding_color.alpha = 0;
	right_padding = 1;
	right_padding_color = left_padding_color;
    }

    /* If we still have stops that don't cover the full [0,1] range
     * then we need to define a texture-coordinate scale + translate
     * transform to account for that... */
    if (stops[n_stops - 1].offset - stops[0].offset < 1) {
	float range = stops[n_stops - 1].offset - stops[0].offset;
	gradient->scale_x = 1.0 / range;
	gradient->translate_x = -(stops[0].offset * gradient->scale_x);
    }

    width += left_padding + right_padding;

    width = _rut_util_next_p2(width);
    width = MIN(4096, width); /* lets not go too stupidly big! */
    components = components_for_stops(extend_mode, n_stops, stops);

    do {
	tex = cg_texture_2d_new_with_size(shell->cg_device,
					  width,
                                          1);
        cg_texture_set_premultiplied(tex, true);
        cg_texture_set_components(tex, components);
        cg_texture_allocate(tex, &catch);

	if (!tex)
	    cg_error_free(catch);
    } while (tex == NULL && width >> 1);

    c_return_val_if_fail(tex, NULL);

    gradient->texture = tex;

    un_padded_width = width - left_padding - right_padding;

    /* XXX: only when we know the final texture width can we calculate the
     * scale and translate factors needed to account for padding... */
    if (un_padded_width != width)
	gradient->scale_x *= (float)un_padded_width / (float)width;
    if (left_padding)
	gradient->translate_x +=
            (gradient->scale_x / (float)un_padded_width) * (float)left_padding;

    offscreen = cg_offscreen_new_with_texture(tex);

    cg_framebuffer_orthographic(offscreen, 0, 0, width, 1, -1, 100);

    cg_framebuffer_clear4f(offscreen,
                           CG_BUFFER_BIT_COLOR,
                           0, 0, 0, 0);

    n_quads = n_stops - 1 + !!left_padding + !!right_padding;
    n_vertices = 6 * n_quads;
    vertices = alloca(sizeof(cg_vertex_p2c4_t) * n_vertices);
    p = vertices;
    if (left_padding)
	emit_stop(&p, 0, left_padding, &left_padding_color, &left_padding_color);
    prev = (float)left_padding;
    for (n = 1; n < n_stops; n++) {
	right = (float)left_padding + (float)un_padded_width * stops[n].offset;
	emit_stop(&p, prev, right, &stops[n-1].color, &stops[n].color);
	prev = right;
    }
    if (right_padding)
	emit_stop(&p, prev, width, &right_padding_color, &right_padding_color);

    pipeline = cg_pipeline_new(shell->cg_device);
    prim = cg_primitive_new_p2c4(shell->cg_device,
                                 CG_VERTICES_MODE_TRIANGLES,
                                 n_vertices,
                                 vertices);
    cg_primitive_draw(prim, offscreen, pipeline);
    cg_object_unref(prim);
    cg_object_unref(pipeline);

    cg_object_unref(offscreen);

    return gradient;
}
