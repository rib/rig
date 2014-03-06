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

G_BEGIN_DECLS

/**
 * SECTION:clutter-flow-layout
 * @short_description: A reflowing layout manager
 *
 * #RutFlowLayout is a layout manager which implements the following
 * policy:
 *
 * <itemizedlist>
 *   <listitem><para>the preferred natural size depends on the value
 *   of the #RutFlowLayout:packing property; the layout will try
 *   to maintain all its children on a single row or
 *   column;</para></listitem>
 *   <listitem><para>if either the width or the height allocated are
 *   smaller than the preferred ones, the layout will wrap; in this case,
 *   the preferred height or width, respectively, will take into account
 *   the amount of columns and rows;</para></listitem>
 *   <listitem><para>each line (either column or row) in reflowing will
 *   have the size of the biggest cell on that line; if the
 *   #RutFlowLayout:homogeneous property is set to %FALSE the actor
 *   will be allocated within that area, and if set to %TRUE instead the
 *   actor will be given exactly that area;</para></listitem>
 *   <listitem><para>the size of the columns or rows can be controlled
 *   for both minimum and maximum; the spacing can also be controlled
 *   in both columns and rows.</para></listitem>
 * </itemizedlist>
 *
 * <figure id="flow-layout-image">
 *   <title>Horizontal flow layout</title>
 *   <para>The image shows a #RutFlowLayout with the
 *   #RutFlowLayout:packing property set to
 *   %RUT_FLOW_HORIZONTAL.</para>
 *   <graphic fileref="flow-layout.png" format="PNG"/>
 * </figure>
 *
 * <informalexample>
 *  <programlisting>
 * <xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../../examples/flow-layout.c">
 *   <xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback>
 * </xi:include>
 *  </programlisting>
 * </informalexample>
 */


/**
 * RutFlowLayout:
 */
typedef struct _RutFlowLayout RutFlowLayout;
#define RUT_FLOW_LAYOUT(X) ((RutFlowLayout *)X)

extern RutType rut_flow_layout;

/**
 * RutFlowLayoutPacking:
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
 * a #RutFlowLayout
 */
typedef enum { /*< prefix=RUT_FLOW >*/
  RUT_FLOW_LAYOUT_PACKING_LEFT_TO_RIGHT,
  RUT_FLOW_LAYOUT_PACKING_RIGHT_TO_LEFT,
  RUT_FLOW_LAYOUT_PACKING_TOP_TO_BOTTOM,
  RUT_FLOW_LAYOUT_PACKING_BOTTOM_TO_TOP
} RutFlowLayoutPacking;

/**
 * rut_flow_layout_new:
 * @ctx: A #RutContext
 * @packing: the packing direction of the flow layout
 *
 * Creates a new #RutFlowLayout with the given @packing direction
 *
 * Return value: the newly created #RutFlowLayout
 */
RutFlowLayout *
rut_flow_layout_new (RutContext *ctx,
                     RutFlowLayoutPacking packing);

void
rut_flow_layout_add (RutFlowLayout *flow,
                     RutObject *child_widget);

void
rut_flow_layout_remove (RutFlowLayout *flow,
                        RutObject *child_widget);

/**
 * rut_flow_layout_set_packing:
 * @flow: a #RutFlowLayout
 * @packing: the packing direction of the flow layout
 *
 * Sets the packing direction of the flow layout
 *
 * The packing controls the direction used to allocate the children:
 * either horizontally or vertically. The packing also controls the
 * direction of the overflowing
 */
void
rut_flow_layout_set_packing (RutFlowLayout *flow,
                             RutFlowLayoutPacking packing);

/**
 * rut_flow_layout_get_packing:
 * @flow: a #RutFlowLayout
 *
 * Retrieves the packing direction of the @flow
 *
 * Return value: the packing direction of the #RutFlowLayout
 */
RutFlowLayoutPacking
rut_flow_layout_get_packing (RutFlowLayout *flow);

/**
 * rut_flow_layout_set_homogeneous:
 * @flow: a #RutFlowLayout
 * @homogeneous: whether the layout should be homogeneous or not
 *
 * Sets whether the @flow should allocate the same space for
 * each child
 */
void
rut_flow_layout_set_homogeneous (RutFlowLayout *flow,
                                 CoglBool homogeneous);

/**
 * rut_flow_layout_get_homogeneous:
 * @flow: a #RutFlowLayout
 *
 * Retrieves whether the @flow is homogeneous
 *
 * Return value: %TRUE if the #RutFlowLayout is homogeneous
 */
CoglBool
rut_flow_layout_get_homogeneous (RutFlowLayout *flow);

/**
 * rut_flow_layout_set_x_padding:
 * @flow: a #RutFlowLayout
 * @spacing: the horizontal space between children
 *
 * Sets the horizontal space between children, in pixels
 */
void
rut_flow_layout_set_x_padding (RutFlowLayout *flow,
                               int padding);

/**
 * rut_flow_layout_get_x_padding:
 * @flow: a #RutFlowLayout
 *
 * Retrieves the horizontal spacing between children
 *
 * Return value: the horizontal spacing between children of the
 *   #RutFlowLayout, in pixels
 */
int
rut_flow_layout_get_x_padding (RutFlowLayout *flow);

/**
 * rut_flow_layout_set_y_padding:
 * @flow: a #RutFlowLayout
 * @spacing: the vertical space between children
 *
 * Sets the vertical spacing between children, in pixels.
 */
void
rut_flow_layout_set_y_padding (RutFlowLayout *flow,
                               int spacing);

/**
 * rut_flow_layout_get_y_padding:
 * @flow: a #RutFlowLayout
 *
 * Retrieves the vertical spacing between children
 *
 * Return value: the vertical spacing between children of the
 *   #RutFlowLayout, in pixels
 */
int
rut_flow_layout_get_y_padding (RutFlowLayout *flow);

/**
 * rut_flow_layout_set_min_child_width:
 * @flow: a #RutFlowLayout
 * @min_width: minimum width of a child
 *
 * Sets the minimum width that a child can have
 */
void
rut_flow_layout_set_min_child_width (RutFlowLayout *flow,
                                     int min_width);

/**
 * rut_flow_layout_get_min_child_width:
 * @flow: a #RutFlowLayout
 *
 * Return value: Returns the minimum child widths
 */
int
rut_flow_layout_get_min_child_width (RutFlowLayout *flow);

/**
 * rut_flow_layout_set_max_child_width:
 * @flow: a #RutFlowLayout
 * @max_width: maximum width of a child
 *
 * Sets the maximum width that a child can have
 */
void
rut_flow_layout_set_max_child_width (RutFlowLayout *flow,
                                     int max_width);

/**
 * rut_flow_layout_get_max_child_width:
 * @flow: a #RutFlowLayout
 *
 * Return value: Returns the maximum child widths
 */
int
rut_flow_layout_get_max_child_width (RutFlowLayout *flow);

/**
 * rut_flow_layout_set_min_child_height:
 * @flow: a #RutFlowLayout
 * @min_height: minimum height of a child
 *
 * Sets the minimum height that a child can have
 */
void
rut_flow_layout_set_min_child_height (RutFlowLayout *flow,
                                      int min_height);

/**
 * rut_flow_layout_get_min_child_height:
 * @flow: a #RutFlowLayout
 *
 * Return value: Returns the minimum child heights
 */
int
rut_flow_layout_get_min_child_height (RutFlowLayout *flow);

/**
 * rut_flow_layout_set_max_child_height:
 * @flow: a #RutFlowLayout
 * @max_height: maximum height of a child
 *
 * Sets the maximum height that a child can have
 */
void
rut_flow_layout_set_max_child_height (RutFlowLayout *flow,
                                      int max_height);

/**
 * rut_flow_layout_get_max_child_height:
 * @flow: a #RutFlowLayout
 *
 * Return value: Returns the maximum child heights
 */
int
rut_flow_layout_get_max_child_height (RutFlowLayout *flow);

G_END_DECLS

#endif /* __RUT_FLOW_LAYOUT_H__ */
