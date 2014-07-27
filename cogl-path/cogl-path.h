/*
 * Cogl
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009 Intel Corporation.
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
 *
 */

#ifndef __CG_PATH_H__
#define __CG_PATH_H__

#ifdef CG_COMPILATION
#include "cogl-device.h"
#else
#include <cogl/cogl.h>
#endif

CG_BEGIN_DECLS

/**
 * SECTION:cogl-paths
 * @short_description: Functions for constructing and drawing 2D paths.
 *
 * There are two levels on which drawing with cogl-paths can be used.
 * The highest level functions construct various simple primitive
 * shapes to be either filled or stroked. Using a lower-level set of
 * functions more complex and arbitrary paths can be constructed by
 * concatenating straight line, bezier curve and arc segments.
 *
 * When constructing arbitrary paths, the current pen location is
 * initialized using the move_to command. The subsequent path segments
 * implicitly use the last pen location as their first vertex and move
 * the pen location to the last vertex they produce at the end. Also
 * there are special versions of functions that allow specifying the
 * vertices of the path segments relative to the last pen location
 * rather then in the absolute coordinates.
 */
typedef struct _cg_path_t cg_path_t;

/**
 * cg_path_new:
 * @dev: A #cg_device_t pointer
 *
 * Creates a new, empty path object. The default fill rule is
 * %CG_PATH_FILL_RULE_EVEN_ODD.
 *
 * Return value: A pointer to a newly allocated #cg_path_t, which can
 * be freed using cg_object_unref().
 *
 */
cg_path_t *cg_path_new(cg_device_t *context);

/**
 * cg_path_copy:
 * @path: A #cg_path_t object
 *
 * Returns a new copy of the path in @path. The new path has a
 * reference count of 1 so you should unref it with
 * cg_object_unref() if you no longer need it.
 *
 * Internally the path will share the data until one of the paths is
 * modified so copying paths should be relatively cheap.
 *
 * Return value: a copy of the path in @path.
 *
 */
cg_path_t *cg_path_copy(cg_path_t *path);

/**
 * cg_is_path:
 * @object: A #cg_object_t
 *
 * Gets whether the given object references an existing path object.
 *
 * Return value: %true if the object references a #cg_path_t,
 *   %false otherwise.
 *
 */
bool cg_is_path(void *object);

/**
 * cg_path_move_to:
 * @path: A #cg_path_t
 * @x: X coordinate of the pen location to move to.
 * @y: Y coordinate of the pen location to move to.
 *
 * Moves the pen to the given location. If there is an existing path
 * this will start a new disjoint subpath.
 *
 */
void cg_path_move_to(cg_path_t *path, float x, float y);

/**
 * cg_path_rel_move_to:
 * @path: A #cg_path_t
 * @x: X offset from the current pen location to move the pen to.
 * @y: Y offset from the current pen location to move the pen to.
 *
 * Moves the pen to the given offset relative to the current pen
 * location. If there is an existing path this will start a new
 * disjoint subpath.
 *
 */
void cg_path_rel_move_to(cg_path_t *path, float x, float y);

/**
 * cg_path_line_to:
 * @path: A #cg_path_t
 * @x: X coordinate of the end line vertex
 * @y: Y coordinate of the end line vertex
 *
 * Adds a straight line segment to the current path that ends at the
 * given coordinates.
 *
 */
void cg_path_line_to(cg_path_t *path, float x, float y);

/**
 * cg_path_rel_line_to:
 * @path: A #cg_path_t
 * @x: X offset from the current pen location of the end line vertex
 * @y: Y offset from the current pen location of the end line vertex
 *
 * Adds a straight line segment to the current path that ends at the
 * given coordinates relative to the current pen location.
 *
 */
void cg_path_rel_line_to(cg_path_t *path, float x, float y);

/**
 * cg_path_arc:
 * @path: A #cg_path_t
 * @center_x: X coordinate of the elliptical arc center
 * @center_y: Y coordinate of the elliptical arc center
 * @radius_x: X radius of the elliptical arc
 * @radius_y: Y radius of the elliptical arc
 * @angle_1: Angle in degrees at which the arc begin
 * @angle_2: Angle in degrees at which the arc ends
 *
 * Adds an elliptical arc segment to the current path. A straight line
 * segment will link the current pen location with the first vertex
 * of the arc. If you perform a move_to to the arcs start just before
 * drawing it you create a free standing arc.
 *
 * The angles are measured in degrees where 0° is in the direction of
 * the positive X axis and 90° is in the direction of the positive Y
 * axis. The angle of the arc begins at @angle_1 and heads towards
 * @angle_2 (so if @angle_2 is less than @angle_1 it will decrease,
 * otherwise it will increase).
 *
 */
void cg_path_arc(cg_path_t *path,
                 float center_x,
                 float center_y,
                 float radius_x,
                 float radius_y,
                 float angle_1,
                 float angle_2);

/**
 * cg_path_curve_to:
 * @path: A #cg_path_t
 * @x_1: X coordinate of the second bezier control point
 * @y_1: Y coordinate of the second bezier control point
 * @x_2: X coordinate of the third bezier control point
 * @y_2: Y coordinate of the third bezier control point
 * @x_3: X coordinate of the fourth bezier control point
 * @y_3: Y coordinate of the fourth bezier control point
 *
 * Adds a cubic bezier curve segment to the current path with the given
 * second, third and fourth control points and using current pen location
 * as the first control point.
 *
 */
void cg_path_curve_to(cg_path_t *path,
                      float x_1,
                      float y_1,
                      float x_2,
                      float y_2,
                      float x_3,
                      float y_3);

/**
 * cg_path_rel_curve_to:
 * @path: A #cg_path_t
 * @x_1: X coordinate of the second bezier control point
 * @y_1: Y coordinate of the second bezier control point
 * @x_2: X coordinate of the third bezier control point
 * @y_2: Y coordinate of the third bezier control point
 * @x_3: X coordinate of the fourth bezier control point
 * @y_3: Y coordinate of the fourth bezier control point
 *
 * Adds a cubic bezier curve segment to the current path with the given
 * second, third and fourth control points and using current pen location
 * as the first control point. The given coordinates are relative to the
 * current pen location.
 *
 */
void cg_path_rel_curve_to(cg_path_t *path,
                          float x_1,
                          float y_1,
                          float x_2,
                          float y_2,
                          float x_3,
                          float y_3);

/**
 * cg_path_close:
 * @path: A #cg_path_t
 *
 * Closes the path being constructed by adding a straight line segment
 * to it that ends at the first vertex of the path.
 *
 */
void cg_path_close(cg_path_t *path);

/**
 * cg_path_line:
 * @path: A #cg_path_t
 * @x_1: X coordinate of the start line vertex
 * @y_1: Y coordinate of the start line vertex
 * @x_2: X coordinate of the end line vertex
 * @y_2: Y coordinate of the end line vertex
 *
 * Constructs a straight line shape starting and ending at the given
 * coordinates. If there is an existing path this will start a new
 * disjoint sub-path.
 *
 */
void cg_path_line(cg_path_t *path, float x_1, float y_1, float x_2, float y_2);

/**
 * cg_path_polyline:
 * @path: A #cg_path_t
 * @coords: (in) (array) (transfer none): A pointer to the first element of an
 * array of fixed-point values that specify the vertex coordinates.
 * @num_points: The total number of vertices.
 *
 * Constructs a series of straight line segments, starting from the
 * first given vertex coordinate. If there is an existing path this
 * will start a new disjoint sub-path. Each subsequent segment starts
 * where the previous one ended and ends at the next given vertex
 * coordinate.
 *
 * The coords array must contain 2 * num_points values. The first value
 * represents the X coordinate of the first vertex, the second value
 * represents the Y coordinate of the first vertex, continuing in the same
 * fashion for the rest of the vertices. (num_points - 1) segments will
 * be constructed.
 *
 */
void cg_path_polyline(cg_path_t *path, const float *coords, int num_points);

/**
 * cg_path_polygon:
 * @path: A #cg_path_t
 * @coords: (in) (array) (transfer none): A pointer to the first element of
 * an array of fixed-point values that specify the vertex coordinates.
 * @num_points: The total number of vertices.
 *
 * Constructs a polygonal shape of the given number of vertices. If
 * there is an existing path this will start a new disjoint sub-path.
 *
 * The coords array must contain 2 * num_points values. The first value
 * represents the X coordinate of the first vertex, the second value
 * represents the Y coordinate of the first vertex, continuing in the same
 * fashion for the rest of the vertices.
 *
 */
void cg_path_polygon(cg_path_t *path, const float *coords, int num_points);

/**
 * cg_path_rectangle:
 * @path: A #cg_path_t
 * @x_1: X coordinate of the top-left corner.
 * @y_1: Y coordinate of the top-left corner.
 * @x_2: X coordinate of the bottom-right corner.
 * @y_2: Y coordinate of the bottom-right corner.
 *
 * Constructs a rectangular shape at the given coordinates. If there
 * is an existing path this will start a new disjoint sub-path.
 *
 */
void
cg_path_rectangle(cg_path_t *path, float x_1, float y_1, float x_2, float y_2);

/**
 * cg_path_ellipse:
 * @path: A #cg_path_t
 * @center_x: X coordinate of the ellipse center
 * @center_y: Y coordinate of the ellipse center
 * @radius_x: X radius of the ellipse
 * @radius_y: Y radius of the ellipse
 *
 * Constructs an ellipse shape. If there is an existing path this will
 * start a new disjoint sub-path.
 *
 */
void cg_path_ellipse(cg_path_t *path,
                     float center_x,
                     float center_y,
                     float radius_x,
                     float radius_y);

/**
 * cg_path_round_rectangle:
 * @path: A #cg_path_t
 * @x_1: X coordinate of the top-left corner.
 * @y_1: Y coordinate of the top-left corner.
 * @x_2: X coordinate of the bottom-right corner.
 * @y_2: Y coordinate of the bottom-right corner.
 * @radius: Radius of the corner arcs.
 * @arc_step: Angle increment resolution for subdivision of
 * the corner arcs.
 *
 * Constructs a rectangular shape with rounded corners. If there is an
 * existing path this will start a new disjoint sub-path.
 *
 */
void cg_path_round_rectangle(cg_path_t *path,
                             float x_1,
                             float y_1,
                             float x_2,
                             float y_2,
                             float radius,
                             float arc_step);

/**
 * cg_path_fill_rule_t:
 * @CG_PATH_FILL_RULE_NON_ZERO: Each time the line crosses an edge of
 * the path from left to right one is added to a counter and each time
 * it crosses from right to left the counter is decremented. If the
 * counter is non-zero then the point will be filled. See <xref
 * linkend="fill-rule-non-zero"/>.
 * @CG_PATH_FILL_RULE_EVEN_ODD: If the line crosses an edge of the
 * path an odd number of times then the point will filled, otherwise
 * it won't. See <xref linkend="fill-rule-even-odd"/>.
 *
 * #cg_path_fill_rule_t is used to determine how a path is filled. There
 * are two options - 'non-zero' and 'even-odd'. To work out whether any
 * point will be filled imagine drawing an infinetely long line in any
 * direction from that point. The number of times and the direction
 * that the edges of the path crosses this line determines whether the
 * line is filled as described below. Any open sub paths are treated
 * as if there was an extra line joining the first point and the last
 * point.
 *
 * The default fill rule when creating a path is %CG_PATH_FILL_RULE_EVEN_ODD.
 *
 * <figure id="fill-rule-non-zero">
 *   <title>Example of filling various paths using the non-zero rule</title>
 *   <graphic fileref="fill-rule-non-zero.png" format="PNG"/>
 * </figure>
 *
 * <figure id="fill-rule-even-odd">
 *   <title>Example of filling various paths using the even-odd rule</title>
 *   <graphic fileref="fill-rule-even-odd.png" format="PNG"/>
 * </figure>
 *
 */
typedef enum {
    CG_PATH_FILL_RULE_NON_ZERO,
    CG_PATH_FILL_RULE_EVEN_ODD
} cg_path_fill_rule_t;

/**
 * cg_path_set_fill_rule:
 * @path: A #cg_path_t
 * @fill_rule: The new fill rule.
 *
 * Sets the fill rule of the current path to @fill_rule. This will
 * affect how the path is filled when cg_framebuffer_fill_path() is
 * later called.
 *
 */
void cg_path_set_fill_rule(cg_path_t *path, cg_path_fill_rule_t fill_rule);

/**
 * cg_path_get_fill_rule:
 * @path: A #cg_path_t
 *
 * Retrieves the fill rule set using cg_path_set_fill_rule().
 *
 * Return value: the fill rule that is used for the current path.
 *
 */
cg_path_fill_rule_t cg_path_get_fill_rule(cg_path_t *path);

/**
 * cg_framebuffer_fill_path:
 * @path: The #cg_path_t to fill
 * @framebuffer: A #cg_framebuffer_t
 * @pipeline: A #cg_pipeline_t to render with
 *
 * Draws a triangle tesselation of the given @path using the specified
 * GPU @pipeline to the given @framebuffer.
 *
 * The tesselated interior of the path is determined using the fill
 * rule of the path. See %cg_path_fill_rule_t for details.
 *
 * <note>The result of referencing sliced textures in your current
 * pipeline when filling a path are undefined. You should pass
 * the %CG_TEXTURE_NO_SLICING flag when loading any texture you will
 * use while filling a path.</note>
 *
 */
void cg_path_fill(cg_path_t *path,
                  cg_framebuffer_t *framebuffer,
                  cg_pipeline_t *pipeline);

/**
 * cg_path_stroke:
 * @path: The #cg_path_t to stroke
 * @framebuffer: A #cg_framebuffer_t
 * @pipeline: A #cg_pipeline_t to render with
 *
 * Draws a list of line primitives corresponding to the edge of the
 * given @path using the specified GPU @pipeline to the given
 * @framebuffer.
 *
 * <note>Cogl does not provide any sophisticated path stroking
 * features for things like stroke width, dashes or caps. The stroke
 * line will have a width of 1 pixel regardless of the current
 * transformation matrix.</note>
 *
 */
void cg_path_stroke(cg_path_t *path,
                    cg_framebuffer_t *framebuffer,
                    cg_pipeline_t *pipeline);

/**
 * cg_framebuffer_push_path_clip:
 * @framebuffer: A #cg_framebuffer_t pointer
 * @path: The path to clip with.
 *
 * Sets a new clipping area using the silhouette of the specified,
 * filled @path.  The clipping area is intersected with the previous
 * clipping area. To restore the previous clipping area, call
 * cg_framebuffer_pop_clip().
 *
 * Stability: unstable
 */
void cg_framebuffer_push_path_clip(cg_framebuffer_t *framebuffer,
                                   cg_path_t *path);

CG_END_DECLS

#endif /* __CG_PATH_H__ */
