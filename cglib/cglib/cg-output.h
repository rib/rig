/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Red Hat, Inc.
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
 *
 * Authors:
 *   Owen Taylor <otaylor@redhat.com>
 */
#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_OUTPUT_H
#define __CG_OUTPUT_H

#include <cglib/cg-types.h>

CG_BEGIN_DECLS

/**
 * SECTION:cg-output
 * @short_description: information about an output device
 *
 * The #cg_output_t object holds information about an output device
 * such as a monitor or laptop display. It can be queried to find
 * out the position of the output with respect to the screen
 * coordinate system and other information such as the resolution
 * and refresh rate of the device.
 *
 * There can be any number of outputs which may overlap: the
 * same area of the screen may be displayed by multiple output
 * devices.
 *
 * XXX: though it's possible to query the position of the output
 * with respect to screen coordinates, there is currently no way
 * of finding out the position of a #cg_onscreen_t in screen
 * coordinates, at least without using windowing-system specific
 * API's, so it's not easy to get the output positions relative
 * to the #cg_onscreen_t.
 */

typedef struct _cg_output_t cg_output_t;
#define CG_OUTPUT(X) ((cg_output_t *)(X))

/**
 * cg_subpixel_order_t:
 * @CG_SUBPIXEL_ORDER_UNKNOWN: the layout of subpixel
 *   components for the device is unknown.
 * @CG_SUBPIXEL_ORDER_NONE: the device displays colors
 *   without geometrically-separated subpixel components,
 *   or the positioning or colors of the components do not
 *   match any of the values in the enumeration.
 * @CG_SUBPIXEL_ORDER_HORIZONTAL_RGB: the device has
 *   horizontally arranged components in the order
 *   red-green-blue from left to right.
 * @CG_SUBPIXEL_ORDER_HORIZONTAL_BGR: the device has
 *   horizontally arranged  components in the order
 *   blue-green-red from left to right.
 * @CG_SUBPIXEL_ORDER_VERTICAL_RGB: the device has
 *   vertically arranged components in the order
 *   red-green-blue from top to bottom.
 * @CG_SUBPIXEL_ORDER_VERTICAL_BGR: the device has
 *   vertically arranged components in the order
 *   blue-green-red from top to bottom.
 *
 * Some output devices (such as LCD panels) display colors
 * by making each pixel consist of smaller "subpixels"
 * that each have a particular color. By using knowledge
 * of the layout of this subpixel components, it is possible
 * to create image content with higher resolution than the
 * pixel grid.
 *
 * Stability: unstable
 */
typedef enum {
    CG_SUBPIXEL_ORDER_UNKNOWN,
    CG_SUBPIXEL_ORDER_NONE,
    CG_SUBPIXEL_ORDER_HORIZONTAL_RGB,
    CG_SUBPIXEL_ORDER_HORIZONTAL_BGR,
    CG_SUBPIXEL_ORDER_VERTICAL_RGB,
    CG_SUBPIXEL_ORDER_VERTICAL_BGR
} cg_subpixel_order_t;

/**
 * cg_is_output:
 * @object: A #cg_object_t pointer
 *
 * Gets whether the given object references a #cg_output_t.
 *
 * Return value: %true if the object references a #cg_output_t
 *   and %false otherwise.
 * Stability: unstable
 */
bool cg_is_output(void *object);

/**
 * cg_output_get_x:
 * @output: a #cg_output_t
 *
 * Gets the X position of the output with respect to the coordinate
 * system of the screen.
 *
 * Return value: the X position of the output as a pixel offset
 *  from the left side of the screen coordinate space
 * Stability: unstable
 */
int cg_output_get_x(cg_output_t *output);

/**
 * cg_output_get_y:
 * @output: a #cg_output_t
 *
 * Gets the Y position of the output with respect to the coordinate
 * system of the screen.
 *
 * Return value: the Y position of the output as a pixel offset
 *  from the top side of the screen coordinate space
 * Stability: unstable
 */
int cg_output_get_y(cg_output_t *output);

/**
 * cg_output_get_width:
 * @output: a #cg_output_t
 *
 * Gets the width of the output in pixels.
 *
 * Return value: the width of the output in pixels
 * Stability: unstable
 */
int cg_output_get_width(cg_output_t *output);

/**
 * cg_output_get_height:
 * @output: a #cg_output_t
 *
 * Gets the height of the output in pixels.
 *
 * Return value: the height of the output in pixels
 * Stability: unstable
 */
int cg_output_get_height(cg_output_t *output);

/**
 * cg_output_get_mm_width:
 * @output: a #cg_output_t
 *
 * Gets the physical width of the output. In some cases (such as
 * as a projector), the value returned here might correspond to
 * nominal resolution rather than the actual physical size of the
 * output device.
 *
 * Return value: the height of the output in millimeters. A value
 *  of 0 indicates the width is unknown
 * Stability: unstable
 */
int cg_output_get_mm_width(cg_output_t *output);

/**
 * cg_output_get_mm_height:
 * @output: a #cg_output_t
 *
 * Gets the physical height of the output. In some cases (such as
 * as a projector), the value returned here might correspond to
 * nominal resolution rather than the actual physical size of the
 * output device.
 *
 * Return value: the height of the output in millimeters. A value
 *  of 0 indicates that the height is unknown
 * Stability: unstable
 */
int cg_output_get_mm_height(cg_output_t *output);

/**
 * cg_output_get_subpixel_order:
 * @output: a #cg_output_t
 *
 * For an output device where each pixel is made up of smaller components
 * with different colors, returns the layout of the subpixel
 * components.
 *
 * Return value: the order of subpixel components for the output device
 * Stability: unstable
 */
cg_subpixel_order_t cg_output_get_subpixel_order(cg_output_t *output);

/**
 * cg_output_get_refresh_rate:
 * @output: a #cg_output_t
 *
 * Gets the number of times per second that the output device refreshes
 * the display contents.
 *
 * Return value: the refresh rate of the output device. A value of zero
 *  indicates that the refresh rate is unknown.
 * Stability: unstable
 */
float cg_output_get_refresh_rate(cg_output_t *output);

CG_END_DECLS

#endif /* __CG_OUTPUT_H */
