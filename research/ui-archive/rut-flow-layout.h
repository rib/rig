/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2009,2013  Intel Corporation.
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
 * Authors:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __RUT_FLOW_LAYOUT_H__
#define __RUT_FLOW_LAYOUT_H__

#include "rut-type.h"
#include "rut-object.h"

C_BEGIN_DECLS

/**
 * SECTION:clutter-flow-layout
 * @short_description: A reflowing layout manager
 *
 * #rut_flow_layout_t is a layout manager which implements the following
 * policy:
 *
 * <itemizedlist>
 *   <listitem><para>the preferred natural size depends on the value
 *   of the #rut_flow_layout_t:packing property; the layout will try
 *   to maintain all its children on a single row or
 *   column;</para></listitem>
 *   <listitem><para>if either the width or the height allocated are
 *   smaller than the preferred ones, the layout will wrap; in this case,
 *   the preferred height or width, respectively, will take into account
 *   the amount of columns and rows;</para></listitem>
 *   <listitem><para>each line (either column or row) in reflowing will
 *   have the size of the biggest cell on that line; if the
 *   #rut_flow_layout_t:homogeneous property is set to %false the actor
 *   will be allocated within that area, and if set to %true instead the
 *   actor will be given exactly that area;</para></listitem>
 *   <listitem><para>the size of the columns or rows can be controlled
 *   for both minimum and maximum; the spacing can also be controlled
 *   in both columns and rows.</para></listitem>
 * </itemizedlist>
 *
 * <figure id="flow-layout-image">
 *   <title>Horizontal flow layout</title>
 *   <para>The image shows a #rut_flow_layout_t with the
 *   #rut_flow_layout_t:packing property set to
 *   %RUT_FLOW_HORIZONTAL.</para>
 *   <graphic fileref="flow-layout.png" format="PNG"/>
 * </figure>
 *
 * <informalexample>
 *  <programlisting>
 * <xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text"
 * href="../../../../examples/flow-layout.c">
 *   <xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback>
 * </xi:include>
 *  </programlisting>
 * </informalexample>
 */

/**
 * rut_flow_layout_t:
 */
typedef struct _rut_flow_layout_t rut_flow_layout_t;
#define RUT_FLOW_LAYOUT(X) ((rut_flow_layout_t *)X)

extern rut_type_t rut_flow_layout;

/**
 * rut_flow_layout_packing_t:
 * @RUT_FLOW_LAYOUT_PACKING_LEFT_TO_RIGHT: Arrange the children of the
 *   flow layout horizontally, left to right first
 * @RUT_FLOW_LAYOUT_PACKING_RIGHT_TO_LEFT: Arrange the children of the
 *   flow layout horizontally, right to left first
 * @RUT_FLOW_LAYOUT_PACKING_TOP_TO_BOTTOM: Arrange the children of the
 *   flow layout vertically, top to bottom first
 * @RUT_FLOW_LAYOUT_PACKING_BOTTOM_TO_TOP: Arrange the children of the
 *   flow layout vertically, bottom to top first
 *
 * The direction of the arrangement of the children inside
 * a #rut_flow_layout_t
 */
typedef enum { /*< prefix=RUT_FLOW >*/
    RUT_FLOW_LAYOUT_PACKING_LEFT_TO_RIGHT,
    RUT_FLOW_LAYOUT_PACKING_RIGHT_TO_LEFT,
    RUT_FLOW_LAYOUT_PACKING_TOP_TO_BOTTOM,
    RUT_FLOW_LAYOUT_PACKING_BOTTOM_TO_TOP
} rut_flow_layout_packing_t;

/**
 * rut_flow_layout_new:
 * @shell: A #rut_shell_t
 * @packing: the packing direction of the flow layout
 *
 * Creates a new #rut_flow_layout_t with the given @packing direction
 *
 * Return value: the newly created #rut_flow_layout_t
 */
rut_flow_layout_t *rut_flow_layout_new(rut_shell_t *shell,
                                       rut_flow_layout_packing_t packing);

void rut_flow_layout_add(rut_flow_layout_t *flow, rut_object_t *child_widget);

void rut_flow_layout_remove(rut_flow_layout_t *flow,
                            rut_object_t *child_widget);

/**
 * rut_flow_layout_set_packing:
 * @flow: a #rut_flow_layout_t
 * @packing: the packing direction of the flow layout
 *
 * Sets the packing direction of the flow layout
 *
 * The packing controls the direction used to allocate the children:
 * either horizontally or vertically. The packing also controls the
 * direction of the overflowing
 */
void rut_flow_layout_set_packing(rut_flow_layout_t *flow,
                                 rut_flow_layout_packing_t packing);

/**
 * rut_flow_layout_get_packing:
 * @flow: a #rut_flow_layout_t
 *
 * Retrieves the packing direction of the @flow
 *
 * Return value: the packing direction of the #rut_flow_layout_t
 */
rut_flow_layout_packing_t rut_flow_layout_get_packing(rut_flow_layout_t *flow);

/**
 * rut_flow_layout_set_homogeneous:
 * @flow: a #rut_flow_layout_t
 * @homogeneous: whether the layout should be homogeneous or not
 *
 * Sets whether the @flow should allocate the same space for
 * each child
 */
void rut_flow_layout_set_homogeneous(rut_flow_layout_t *flow, bool homogeneous);

/**
 * rut_flow_layout_get_homogeneous:
 * @flow: a #rut_flow_layout_t
 *
 * Retrieves whether the @flow is homogeneous
 *
 * Return value: %true if the #rut_flow_layout_t is homogeneous
 */
bool rut_flow_layout_get_homogeneous(rut_flow_layout_t *flow);

/**
 * rut_flow_layout_set_x_padding:
 * @flow: a #rut_flow_layout_t
 * @spacing: the horizontal space between children
 *
 * Sets the horizontal space between children, in pixels
 */
void rut_flow_layout_set_x_padding(rut_flow_layout_t *flow, int padding);

/**
 * rut_flow_layout_get_x_padding:
 * @flow: a #rut_flow_layout_t
 *
 * Retrieves the horizontal spacing between children
 *
 * Return value: the horizontal spacing between children of the
 *   #rut_flow_layout_t, in pixels
 */
int rut_flow_layout_get_x_padding(rut_flow_layout_t *flow);

/**
 * rut_flow_layout_set_y_padding:
 * @flow: a #rut_flow_layout_t
 * @spacing: the vertical space between children
 *
 * Sets the vertical spacing between children, in pixels.
 */
void rut_flow_layout_set_y_padding(rut_flow_layout_t *flow, int spacing);

/**
 * rut_flow_layout_get_y_padding:
 * @flow: a #rut_flow_layout_t
 *
 * Retrieves the vertical spacing between children
 *
 * Return value: the vertical spacing between children of the
 *   #rut_flow_layout_t, in pixels
 */
int rut_flow_layout_get_y_padding(rut_flow_layout_t *flow);

/**
 * rut_flow_layout_set_min_child_width:
 * @flow: a #rut_flow_layout_t
 * @min_width: minimum width of a child
 *
 * Sets the minimum width that a child can have
 */
void rut_flow_layout_set_min_child_width(rut_flow_layout_t *flow,
                                         int min_width);

/**
 * rut_flow_layout_get_min_child_width:
 * @flow: a #rut_flow_layout_t
 *
 * Return value: Returns the minimum child widths
 */
int rut_flow_layout_get_min_child_width(rut_flow_layout_t *flow);

/**
 * rut_flow_layout_set_max_child_width:
 * @flow: a #rut_flow_layout_t
 * @max_width: maximum width of a child
 *
 * Sets the maximum width that a child can have
 */
void rut_flow_layout_set_max_child_width(rut_flow_layout_t *flow,
                                         int max_width);

/**
 * rut_flow_layout_get_max_child_width:
 * @flow: a #rut_flow_layout_t
 *
 * Return value: Returns the maximum child widths
 */
int rut_flow_layout_get_max_child_width(rut_flow_layout_t *flow);

/**
 * rut_flow_layout_set_min_child_height:
 * @flow: a #rut_flow_layout_t
 * @min_height: minimum height of a child
 *
 * Sets the minimum height that a child can have
 */
void rut_flow_layout_set_min_child_height(rut_flow_layout_t *flow,
                                          int min_height);

/**
 * rut_flow_layout_get_min_child_height:
 * @flow: a #rut_flow_layout_t
 *
 * Return value: Returns the minimum child heights
 */
int rut_flow_layout_get_min_child_height(rut_flow_layout_t *flow);

/**
 * rut_flow_layout_set_max_child_height:
 * @flow: a #rut_flow_layout_t
 * @max_height: maximum height of a child
 *
 * Sets the maximum height that a child can have
 */
void rut_flow_layout_set_max_child_height(rut_flow_layout_t *flow,
                                          int max_height);

/**
 * rut_flow_layout_get_max_child_height:
 * @flow: a #rut_flow_layout_t
 *
 * Return value: Returns the maximum child heights
 */
int rut_flow_layout_get_max_child_height(rut_flow_layout_t *flow);

C_END_DECLS

#endif /* __RUT_FLOW_LAYOUT_H__ */
