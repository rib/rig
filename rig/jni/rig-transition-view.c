/*
 * Rig
 *
 * Copyright (C) 2012 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>

#include "rut.h"

#include "rig-transition-view.h"

/* The number of controls to display for each property. Currently
 * there is only the label for the property name but there is an
 * expectation that we will add more controls here so the layout is
 * treated as a grid with the potential for more controls */
#define RIG_TRANSITION_VIEW_N_PROPERTY_CONTROLS 1
/* Same for the number of controls per object */
#define RIG_TRANSITION_VIEW_N_OBJECT_CONTROLS 1

#define RIG_TRANSITION_VIEW_N_COLUMNS                   \
  (MAX (RIG_TRANSITION_VIEW_N_PROPERTY_CONTROLS,        \
        RIG_TRANSITION_VIEW_N_OBJECT_CONTROLS))

#define RIG_TRANSITION_VIEW_PROPERTY_INDENTATION 10

/* Width of the progress marker */
#define RIG_TRANSITION_VIEW_PROGRESS_WIDTH 4.0f

#define RIG_TRANSITION_VIEW_UNSELECTED_COLOR \
  GUINT32_TO_BE (0xff0000ff)
#define RIG_TRANSITION_VIEW_SELECTED_COLOR \
  GUINT32_TO_BE (0xffffffff)

typedef struct
{
  RutObject *transform;
  RutObject *control;
} RigTransitionViewControl;

typedef struct _RigTransitionViewObject RigTransitionViewObject;

typedef struct
{
  RutList list_node;

  /* Pointer back to the parent object */
  RigTransitionViewObject *object;

  RutProperty *property;
  RigPath *path;

  RigTransitionViewControl controls[RIG_TRANSITION_VIEW_N_PROPERTY_CONTROLS];

  RutClosure *path_operation_closure;

  /* True if this property currently has any selected nodes. This is
   * an optimisation so that we can generate the dots buffer slightly
   * faster by only checking in the selected nodes list for paths for
   * properties that have selected nodes */
  CoglBool has_selected_nodes;
} RigTransitionViewProperty;

struct _RigTransitionViewObject
{
  RutList list_node;

  RutObject *object;

  RutList properties;

  RigTransitionViewControl controls[RIG_TRANSITION_VIEW_N_OBJECT_CONTROLS];

  /* Pointer back to the transition view so that we can get back to it
   * if we use the property data as the data for the path operation
   * callback */
  RigTransitionView *view;
};

struct _RigTransitionView
{
  RutObjectProps _parent;

  RutContext *context;
  RigTransition *transition;
  RutTimeline *timeline;

  RutInputRegion *input_region;
  CoglBool have_grab;

  RutObject *graph;
  RutClosure *modified_closure;

  RutPaintableProps paintable;
  RutGraphableProps graphable;

  int controls_width;
  int total_width;
  int row_height;

  RutList objects;

  RutList selected_nodes;

  CoglBool dots_dirty;
  CoglAttributeBuffer *dots_buffer;
  CoglPrimitive *dots_primitive;
  CoglPipeline *dots_pipeline;
  int n_dots;

  CoglPipeline *progress_pipeline;

  RutClosure *animated_closure;

  int ref_count;
};

typedef struct
{
  RutList list_node;

  RigTransitionViewProperty *prop_data;
  float t;
} RigTransitionViewSelectedNode;

typedef CoglVertexP2C4 RigTransitionViewDotVertex;

RutType rig_transition_view_type;

static void
rig_transition_view_property_removed_no_allocation (RigTransitionView *view,
                                                    RutProperty *property);

static RutInputEventStatus
rig_transition_view_grab_input_cb (RutInputEvent *event,
                                   void *user_data);

static void
rig_transition_view_ungrab_input (RigTransitionView *view)
{
  if (view->have_grab)
    {
      rut_shell_ungrab_input (view->context->shell,
                              rig_transition_view_grab_input_cb,
                              view);
      view->have_grab = FALSE;
    }
}

static void
rig_transition_view_clear_selected_nodes (RigTransitionView *view)
{
  RigTransitionViewSelectedNode *selected_node, *t;

  if (rut_list_empty (&view->selected_nodes))
    return;

  rut_list_for_each_safe (selected_node, t, &view->selected_nodes, list_node)
    {
      selected_node->prop_data->has_selected_nodes = FALSE;
      g_slice_free (RigTransitionViewSelectedNode, selected_node);
    }

  rut_list_init (&view->selected_nodes);

  view->dots_dirty = TRUE;
}

static void
_rig_transition_view_free (void *object)
{
  RigTransitionView *view = object;

  rig_transition_view_ungrab_input (view);

  rig_transition_view_clear_selected_nodes (view);

  rut_closure_disconnect (view->animated_closure);

  rut_closure_disconnect (view->modified_closure);
  rut_refable_unref (view->graph);

  while (!rut_list_empty (&view->objects))
    {
      RigTransitionViewObject *object =
        rut_container_of (view->objects.next, object, list_node);
      CoglBool was_last;

      do
        {
          RigTransitionViewProperty *prop_data =
            rut_container_of (object->properties.next, prop_data, list_node);
          RutProperty *property = prop_data->property;

          was_last = prop_data->list_node.next == &object->properties;

          rig_transition_view_property_removed_no_allocation (view,
                                                              property);
        }
      while (!was_last);
    }

  rut_refable_unref (view->context);

  if (view->dots_buffer)
    cogl_object_unref (view->dots_buffer);
  if (view->dots_primitive)
    cogl_object_unref (view->dots_primitive);

  cogl_object_unref (view->dots_pipeline);

  rut_graphable_remove_child (view->input_region);
  rut_refable_unref (view->input_region);

  rut_graphable_destroy (view);

  rut_refable_unref (view->timeline);

  g_slice_free (RigTransitionView, view);
}

RutRefCountableVTable _rig_transition_view_ref_countable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rig_transition_view_free
};

static RutGraphableVTable _rig_transition_view_graphable_vtable = {
  NULL, /* child removed */
  NULL, /* child addded */
  NULL /* parent changed */
};

static CoglAttributeBuffer *
rig_transition_view_create_dots_buffer (RigTransitionView *view)
{
  int n_vertices = MAX (8, view->n_dots);

  return cogl_attribute_buffer_new (view->context->cogl_context,
                                    n_vertices *
                                    sizeof (RigTransitionViewDotVertex),
                                    NULL);
}

static CoglPrimitive *
rig_transition_view_create_dots_primitive (RigTransitionView *view)
{
  CoglAttribute *attributes[2];
  CoglPrimitive *prim;

  attributes[0] = cogl_attribute_new (view->dots_buffer,
                                      "cogl_position_in",
                                      sizeof (RigTransitionViewDotVertex),
                                      offsetof (RigTransitionViewDotVertex, x),
                                      2, /* n_components */
                                      COGL_ATTRIBUTE_TYPE_FLOAT);
  attributes[1] = cogl_attribute_new (view->dots_buffer,
                                      "cogl_color_in",
                                      sizeof (RigTransitionViewDotVertex),
                                      offsetof (RigTransitionViewDotVertex, r),
                                      4, /* n_components */
                                      COGL_ATTRIBUTE_TYPE_UNSIGNED_BYTE);

  prim = cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_POINTS,
                                             view->n_dots,
                                             attributes,
                                             2 /* n_attributes */);

  cogl_object_unref (attributes[0]);
  cogl_object_unref (attributes[1]);

  return prim;
}

typedef struct
{
  RigTransitionView *view;
  RigTransitionViewProperty *prop_data;

  RigTransitionViewDotVertex *v;
  int row_pos;
} RigTransitionViewDotData;

static void
rig_transition_view_add_dot_unselected_cb (void *data,
                                           void *user_data)
{
  RigNode *node = data;
  RigTransitionViewDotData *dot_data = user_data;
  RigTransitionViewDotVertex *v = dot_data->v;

  v->x = node->t;
  v->y = dot_data->row_pos;
  *(uint32_t *) &v->r = RIG_TRANSITION_VIEW_UNSELECTED_COLOR;

  dot_data->v++;
}

static void
rig_transition_view_add_dot_selected_cb (void *data,
                                         void *user_data)
{
  RigNode *node = data;
  RigTransitionViewDotData *dot_data = user_data;
  RigTransitionViewDotVertex *v = dot_data->v;
  uint32_t color = RIG_TRANSITION_VIEW_UNSELECTED_COLOR;
  RigTransitionViewSelectedNode *selected_node;

  rut_list_for_each (selected_node, &dot_data->view->selected_nodes, list_node)
    {
      if (selected_node->prop_data == dot_data->prop_data &&
          selected_node->t == node->t)
        {
          color = RIG_TRANSITION_VIEW_SELECTED_COLOR;
          break;
        }
    }

  v->x = node->t;
  v->y = dot_data->row_pos;
  *(uint32_t *) &v->r = color;

  dot_data->v++;
}

static void
rig_transition_view_update_dots_buffer (RigTransitionView *view)
{
  RigTransitionViewObject *object;
  CoglBool buffer_is_mapped;
  RigTransitionViewDotVertex *buffer_data;
  RigTransitionViewDotData dot_data;

  if ((buffer_data = cogl_buffer_map (COGL_BUFFER (view->dots_buffer),
                                      COGL_BUFFER_ACCESS_WRITE,
                                      COGL_BUFFER_MAP_HINT_DISCARD)) == NULL)
    {
      buffer_data =
        g_malloc (sizeof (RigTransitionViewDotVertex) * view->n_dots);
      buffer_is_mapped = FALSE;
    }
  else
    buffer_is_mapped = TRUE;

  dot_data.view = view;
  dot_data.v = buffer_data;
  dot_data.row_pos = 0;

  rut_list_for_each (object, &view->objects, list_node)
    {
      dot_data.row_pos++;

      rut_list_for_each (dot_data.prop_data, &object->properties, list_node)
        {
          RigPath *path =
            rig_transition_get_path_for_property (view->transition,
                                                  dot_data.prop_data->property);

          g_queue_foreach (&path->nodes,
                           dot_data.prop_data->has_selected_nodes ?
                           rig_transition_view_add_dot_selected_cb :
                           rig_transition_view_add_dot_unselected_cb,
                           &dot_data);

          dot_data.row_pos++;
        }
    }

  if (buffer_is_mapped)
    cogl_buffer_unmap (COGL_BUFFER (view->dots_buffer));
  else
    {
      cogl_buffer_set_data (COGL_BUFFER (view->dots_buffer),
                            0, /* offset */
                            buffer_data,
                            sizeof (RigTransitionViewDotVertex) * view->n_dots);
      g_free (buffer_data);
    }

  g_assert (dot_data.v - buffer_data == view->n_dots);
}

static void
_rig_transition_view_paint (RutObject *object,
                            RutPaintContext *paint_ctx)
{
  RigTransitionView *view = object;
  CoglFramebuffer *fb = rut_camera_get_framebuffer (paint_ctx->camera);

  if (view->dots_dirty)
    {
      if (view->dots_buffer == NULL)
        view->dots_buffer = rig_transition_view_create_dots_buffer (view);
      else
        {
          int old_n_vertices =
            (cogl_buffer_get_size (COGL_BUFFER (view->dots_buffer)) /
             sizeof (RigTransitionViewDotVertex));

          if (old_n_vertices < view->n_dots)
            {
              cogl_object_unref (view->dots_buffer);
              cogl_object_unref (view->dots_primitive);
              view->dots_primitive = NULL;
              view->dots_buffer = rig_transition_view_create_dots_buffer (view);
            }
        }

      if (view->dots_primitive == NULL)
        view->dots_primitive = rig_transition_view_create_dots_primitive (view);
      else
        cogl_primitive_set_n_vertices (view->dots_primitive, view->n_dots);

      rig_transition_view_update_dots_buffer (view);

      view->dots_dirty = FALSE;
    }

  /* The transform is set up so that 0→1 along the x-axis extends
   * across the whole timeline. Along the y-axis 1 unit represents the
   * height of one row. This is done so that the changing the size of
   * the transition view doesn't require updating the dots buffer. It
   * doesn't matter that the scale isn't uniform because the dots are
   * drawn as points which are always sized in framebuffer pixels
   * regardless of the transformation */

  cogl_framebuffer_push_matrix (fb);
  cogl_framebuffer_push_rectangle_clip (fb,
                                        view->controls_width,
                                        0.0f,
                                        view->total_width,
                                        view->row_height * 10000.0f);

  cogl_framebuffer_translate (fb,
                              view->controls_width,
                              view->row_height * 0.5f,
                              0.0f);
  cogl_framebuffer_scale (fb,
                          view->total_width - view->controls_width,
                          view->row_height,
                          1.0f);

  cogl_framebuffer_draw_primitive (fb,
                                   view->dots_pipeline,
                                   view->dots_primitive);

  cogl_framebuffer_pop_matrix (fb);

  {
    float progress_x =
      view->controls_width +
      rut_timeline_get_progress (view->timeline) *
      (view->total_width - view->controls_width);

    cogl_framebuffer_draw_rectangle (fb,
                                     view->progress_pipeline,
                                     progress_x -
                                     RIG_TRANSITION_VIEW_PROGRESS_WIDTH / 2.0f,
                                     -10000.0f,
                                     progress_x +
                                     RIG_TRANSITION_VIEW_PROGRESS_WIDTH / 2.0f,
                                     10000.0f);
  }

  cogl_framebuffer_pop_clip (fb);
}

RutPaintableVTable _rig_transition_view_paintable_vtable = {
  _rig_transition_view_paint
};

static void
rig_transition_view_allocate (RigTransitionView *view)
{
  float column_widths[RIG_TRANSITION_VIEW_N_COLUMNS];
  float row_height = 0.0f;
  RigTransitionViewObject *object;
  int i;
  int row_num;

  memset (column_widths, 0, sizeof (column_widths));

  /* Everything in a single column will be allocated to the same
   * width and everything will be allocated to the same height */

  rut_list_for_each (object, &view->objects, list_node)
    {
      RigTransitionViewProperty *prop_data;

      for (i = 0; i < RIG_TRANSITION_VIEW_N_OBJECT_CONTROLS; i++)
        {
          RigTransitionViewControl *control = object->controls + i;
          float width, height;

          rut_sizable_get_preferred_width (control->control,
                                           -1, /* for_height */
                                           NULL, /* min_width_p */
                                           &width);
          rut_sizable_get_preferred_height (control->control,
                                            width,
                                            NULL, /* min_height_p */
                                            &height);

          if (width > column_widths[i])
            column_widths[i] = width;
          if (height > row_height)
            row_height = height;
        }

      rut_list_for_each (prop_data, &object->properties, list_node)
        {
          for (i = 0; i < RIG_TRANSITION_VIEW_N_PROPERTY_CONTROLS; i++)
            {
              RigTransitionViewControl *control = prop_data->controls + i;
              float width, height;

              rut_sizable_get_preferred_width (control->control,
                                               -1, /* for_height */
                                               NULL, /* min_width_p */
                                               &width);
              rut_sizable_get_preferred_height (control->control,
                                                width,
                                                NULL, /* min_height_p */
                                                &height);

              if (i == 0)
                width += RIG_TRANSITION_VIEW_PROPERTY_INDENTATION;

              if (width > column_widths[i])
                column_widths[i] = width;
              if (height > row_height)
                row_height = height;
            }
        }
    }

  row_num = 0;

  rut_list_for_each (object, &view->objects, list_node)
    {
      RigTransitionViewProperty *prop_data;
      float x = 0.0f;

      for (i = 0; i < RIG_TRANSITION_VIEW_N_OBJECT_CONTROLS; i++)
        {
          RigTransitionViewControl *control = object->controls + i;

          rut_transform_init_identity (control->transform);
          rut_transform_translate (control->transform,
                                   nearbyintf (x),
                                   nearbyintf (row_num * row_height),
                                   0.0f);
          rut_sizable_set_size (control->control,
                                nearbyintf (column_widths[i]),
                                nearbyintf (row_height));

          x += column_widths[i];

          row_num++;
        }

      rut_list_for_each (prop_data, &object->properties, list_node)
        {
          x = 0.0f;

          for (i = 0; i < RIG_TRANSITION_VIEW_N_PROPERTY_CONTROLS; i++)
            {
              RigTransitionViewControl *control = prop_data->controls + i;
              int width = nearbyintf (column_widths[i]);

              if (i == 0)
                {
                  x += RIG_TRANSITION_VIEW_PROPERTY_INDENTATION;
                  width -= RIG_TRANSITION_VIEW_PROPERTY_INDENTATION;
                }

              rut_transform_init_identity (control->transform);
              rut_transform_translate (control->transform,
                                       nearbyintf (x),
                                       nearbyintf (row_num * row_height),
                                       0.0f);
              rut_sizable_set_size (control->control,
                                    width,
                                    nearbyintf (row_height));

              row_num++;
            }
        }
    }

  {
    float controls_width = 0;
    for (i = 0; i < RIG_TRANSITION_VIEW_N_COLUMNS; i++)
      controls_width += column_widths[i];
    view->controls_width = nearbyintf (controls_width);
  }

  rut_input_region_set_rectangle (view->input_region,
                                  view->controls_width, /* x0 */
                                  0.0f, /* y0 */
                                  view->total_width,
                                  row_height * row_num /* y1 */);

  view->row_height = nearbyintf (row_height);

  cogl_pipeline_set_point_size (view->dots_pipeline, row_height);
}

static void
rig_transition_view_set_size (void *object,
                              float total_width,
                              float total_height)
{
  RigTransitionView *view = object;

  /* FIXME: RigTransitionView isn't really sizable currently. The methods are
   * just here as stubs because ideally it would be sizable and its
   * preferred size would change when an object is added or removed.
   * This is only really useful once Rut has some way to do an
   * allocation cycle */

  view->total_width = total_width;

  rig_transition_view_allocate (view);
}

static void
rig_transition_view_get_preferred_width (void *object,
                                         float for_height,
                                         float *min_width_p,
                                         float *natural_width_p)
{
  /* FIXME: RigTransitionView isn't really sizable currently. The methods are
   * just here as stubs because ideally it would be sizable and its
   * preferred size would change when an object is added or removed.
   * This is only really useful once Rut has some way to do an
   * allocation cycle */
  if (min_width_p)
    *min_width_p = 10;
  if (natural_width_p)
    *natural_width_p = 10;
}

static void
rig_transition_view_get_preferred_height (void *object,
                                          float for_width,
                                          float *min_height_p,
                                          float *natural_height_p)
{
  /* FIXME: RigTransitionView isn't really sizable currently. The methods are
   * just here as stubs because ideally it would be sizable and its
   * preferred size would change when an object is added or removed.
   * This is only really useful once Rut has some way to do an
   * allocation cycle */
  if (min_height_p)
    *min_height_p = 10;
  if (natural_height_p)
    *natural_height_p = 10;
}

static void
rig_transition_view_get_size (void *object,
                              float *width,
                              float *height)
{
  /* FIXME: RigTransitionView isn't really sizable currently. The methods are
   * just here as stubs because ideally it would be sizable and its
   * preferred size would change when an object is added or removed.
   * This is only really useful once Rut has some way to do an
   * allocation cycle */
  *width = 10;
  *height = 10;
}

static RutSizableVTable _rig_transition_view_sizable_vtable = {
  rig_transition_view_set_size,
  rig_transition_view_get_size,
  rig_transition_view_get_preferred_width,
  rig_transition_view_get_preferred_height
};

static void
_rig_transition_view_init_type (void)
{
  rut_type_init (&rig_transition_view_type);
  rut_type_add_interface (&rig_transition_view_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigTransitionView, ref_count),
                          &_rig_transition_view_ref_countable_vtable);
  rut_type_add_interface (&rig_transition_view_type,
                          RUT_INTERFACE_ID_PAINTABLE,
                          offsetof (RigTransitionView, paintable),
                          &_rig_transition_view_paintable_vtable);
  rut_type_add_interface (&rig_transition_view_type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (RigTransitionView, graphable),
                          &_rig_transition_view_graphable_vtable);
  rut_type_add_interface (&rig_transition_view_type,
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &_rig_transition_view_sizable_vtable);
}

static void
rig_transition_view_create_label_control (RigTransitionView *view,
                                          RigTransitionViewControl *control,
                                          const char *text)
{
  RutText *label = rut_text_new (view->context);

  if (text)
    rut_text_set_text (label, text);

  control->transform = rut_transform_new (view->context, NULL);
  control->control = label;

  rut_graphable_add_child (view, control->transform);
  rut_graphable_add_child (control->transform, label);
}

static CoglBool
rig_transition_view_select_node (RigTransitionView *view,
                                 RigTransitionViewProperty *prop_data,
                                 float t)
{
  RigTransitionViewSelectedNode *selected_node;

  /* Check if the node is already selected */
  if (prop_data->has_selected_nodes)
    {
      rut_list_for_each (selected_node, &view->selected_nodes, list_node)
        {
          if (selected_node->prop_data == prop_data &&
              selected_node->t == t)
            return TRUE;
        }
    }

  selected_node = g_slice_new (RigTransitionViewSelectedNode);
  selected_node->prop_data = prop_data;
  selected_node->t = t;

  prop_data->has_selected_nodes = TRUE;
  view->dots_dirty = TRUE;

  rut_list_insert (view->selected_nodes.prev, &selected_node->list_node);

  return FALSE;
}

static void
rig_transition_view_unselect_node (RigTransitionView *view,
                                   RigTransitionViewProperty *prop_data,
                                   float t)
{
  if (prop_data->has_selected_nodes)
    {
      RigTransitionViewSelectedNode *selected_node, *tmp;
      CoglBool has_nodes = FALSE;

      rut_list_for_each_safe (selected_node,
                              tmp,
                              &view->selected_nodes,
                              list_node)
        {
          if (selected_node->prop_data == prop_data)
            {
              if (selected_node->t == t)
                {
                  rut_list_remove (&selected_node->list_node);
                  g_slice_free (RigTransitionViewSelectedNode, selected_node);
                  view->dots_dirty = TRUE;
                  break;
                }
              else
                has_nodes = TRUE;
            }
        }

      prop_data->has_selected_nodes = has_nodes;
    }
}

static void
rig_transition_view_path_operation_cb (RigPath *path,
                                       RigPathOperation op,
                                       RigNode *node,
                                       void *user_data)
{
  RigTransitionViewProperty *prop_data = user_data;
  RigTransitionViewObject *object_data = prop_data->object;
  RigTransitionView *view = object_data->view;

  switch (op)
    {
    case RIG_PATH_OPERATION_MODIFIED:
      break;

    case RIG_PATH_OPERATION_ADDED:
      view->n_dots++;
      view->dots_dirty = TRUE;
      rut_shell_queue_redraw (view->context->shell);
      break;

    case RIG_PATH_OPERATION_REMOVED:
      rig_transition_view_unselect_node (view, prop_data, node->t);

      view->n_dots--;
      view->dots_dirty = TRUE;
      rut_shell_queue_redraw (view->context->shell);
      break;
    }
}

static void
rig_transition_view_update_label_property (RutProperty *target_property,
                                           RutProperty *source_property,
                                           void *user_data)
{
  RigTransitionView *view = user_data;
  const char *label;

  label = rut_property_get_text (source_property);

  if (label == NULL || *label == 0)
    label = "Object";

  rut_property_set_text (&view->context->property_ctx, target_property, label);

  /* FIXME: this should queue an allocation cycle rather than
   * immediately reallocating. Or even better just let setting the
   * property queue an allocation cycle */
  rig_transition_view_allocate (view);
}

static RigTransitionViewObject *
rig_transition_view_create_object_data (RigTransitionView *view,
                                        RutObject *object)
{
  RigTransitionViewObject *object_data = g_slice_new (RigTransitionViewObject);
  RutProperty *label_property;
  RutProperty *text_property;
  RutTextBuffer *buffer;

  object_data->object = object;
  object_data->view = view;

  rig_transition_view_create_label_control (view,
                                            object_data->controls + 0,
                                            NULL);

  label_property = rut_introspectable_lookup_property (object, "label");
  buffer = rut_text_get_buffer (object_data->controls[0].control);
  text_property = rut_introspectable_lookup_property (buffer, "text");

  if (label_property && text_property)
    {
      rig_transition_view_update_label_property (text_property,
                                                 label_property,
                                                 view);
      rut_property_set_binding (text_property,
                                rig_transition_view_update_label_property,
                                view,
                                label_property,
                                NULL);
    }

  rut_list_init (&object_data->properties);

  rut_list_insert (view->objects.prev, &object_data->list_node);

  return object_data;
}

static void
rig_transition_view_property_added_no_allocation (RigTransitionView *view,
                                                  RutProperty *property)
{
  RigTransitionViewProperty *prop_data;
  RigTransitionViewObject *object_data;
  const RutPropertySpec *spec = property->spec;
  RigPath *path;

  /* Check if we already have this object */
  rut_list_for_each (object_data, &view->objects, list_node)
    if (object_data->object == property->object)
      goto have_object;

  object_data = rig_transition_view_create_object_data (view, property->object);

 have_object:

  prop_data = g_slice_new (RigTransitionViewProperty);

  prop_data->object = object_data;
  prop_data->property = property;

  rig_transition_view_create_label_control (view,
                                            prop_data->controls + 0,
                                            spec->nick ?
                                            spec->nick :
                                            spec->name);

  path = rig_transition_get_path_for_property (view->transition,
                                               property);

  prop_data->path_operation_closure =
    rig_path_add_operation_callback (path,
                                     rig_transition_view_path_operation_cb,
                                     prop_data,
                                     NULL /* destroy_cb */);

  view->n_dots += path->nodes.length;

  prop_data->path = path;

  rut_list_insert (&object_data->properties, &prop_data->list_node);
}

static void
rig_transition_view_property_added (RigTransitionView *view,
                                    RutProperty *property)
{
  rig_transition_view_property_added_no_allocation (view, property);
  rig_transition_view_allocate (view);
}

static void
rig_transition_view_property_removed (RigTransitionView *view,
                                      RutProperty *property)
{
  rig_transition_view_property_removed_no_allocation (view, property);
  rig_transition_view_allocate (view);
}

static void
rig_transition_view_destroy_control (RigTransitionViewControl *control)
{
  rut_graphable_remove_child (control->control);
  rut_refable_unref (control->control);
  rut_graphable_remove_child (control->transform);
  rut_refable_unref (control->transform);
}

static RigTransitionViewProperty *
rig_transition_view_find_property (RigTransitionView *view,
                                   RutProperty *property)
{
  RigTransitionViewObject *object_data;

  rut_list_for_each (object_data, &view->objects, list_node)
    if (object_data->object == property->object)
      {
        RigTransitionViewProperty *prop_data;

        rut_list_for_each (prop_data, &object_data->properties, list_node)
          if (prop_data->property == property)
            return prop_data;
      }

  return NULL;
}

static void
rig_transition_view_property_removed_no_allocation (RigTransitionView *view,
                                                    RutProperty *property)
{
  RigTransitionViewProperty *prop_data =
    rig_transition_view_find_property (view, property);
  RigTransitionViewObject *object_data;
  int i;

  if (prop_data == NULL)
    return;

  if (prop_data->has_selected_nodes)
    {
      RigTransitionViewSelectedNode *selected_node, *t;

      rut_list_for_each_safe (selected_node,
                              t,
                              &view->selected_nodes,
                              list_node)
        {
          if (selected_node->prop_data == prop_data)
            {
              rut_list_remove (&selected_node->list_node);
              g_slice_free (RigTransitionViewSelectedNode, selected_node);
            }
        }
    }

  object_data = prop_data->object;

  rut_closure_disconnect (prop_data->path_operation_closure);

  for (i = 0; i < RIG_TRANSITION_VIEW_N_PROPERTY_CONTROLS; i++)
    rig_transition_view_destroy_control (prop_data->controls + i);

  rut_list_remove (&prop_data->list_node);

  /* If that was the last property on the object then we'll also
   * destroy the object */
  if (rut_list_empty (&object_data->properties))
    {
      for (i = 0; i < RIG_TRANSITION_VIEW_N_OBJECT_CONTROLS; i++)
        rig_transition_view_destroy_control (object_data->controls + i);

      rut_list_remove (&object_data->list_node);

      g_slice_free (RigTransitionViewObject, object_data);
    }

  rut_shell_queue_redraw (view->context->shell);

  view->dots_dirty = TRUE;
  view->n_dots -= prop_data->path->nodes.length;

  g_slice_free (RigTransitionViewProperty, prop_data);

  /* FIXME: This needs to queue an allocation cycle because its
   * preferred size will have changed */
}

static CoglPipeline *
rig_transition_view_create_dots_pipeline (RigTransitionView *view)
{
  CoglPipeline *pipeline = cogl_pipeline_new (view->context->cogl_context);
  char *dot_filename;
  CoglBitmap *bitmap;
  CoglError *error = NULL;

  dot_filename = g_build_filename (RIG_SHARE_DIR, "dot.png", NULL);

  bitmap = cogl_bitmap_new_from_file (view->context->cogl_context,
                                      dot_filename,
                                      &error);

  if (bitmap == NULL)
    {
      g_warning ("Error loading dot.png: %s", error->message);
      cogl_error_free (error);
    }
  else
    {
      CoglTexture2D *texture =
        cogl_texture_2d_new_from_bitmap (bitmap,
                                         COGL_PIXEL_FORMAT_ANY,
                                         &error);

      if (texture == NULL)
        {
          g_warning ("Error loading dot.png: %s", error->message);
          cogl_error_free (error);
        }
      else
        {
          if (!cogl_pipeline_set_layer_point_sprite_coords_enabled (pipeline,
                                                                    0,
                                                                    TRUE,
                                                                    &error))
            {
              g_warning ("Error enabling point sprite coords: %s",
                         error->message);
              cogl_error_free (error);
              cogl_pipeline_remove_layer (pipeline, 0);
            }
          else
            {
              cogl_pipeline_set_layer_texture (pipeline,
                                               0, /* layer_num */
                                               COGL_TEXTURE (texture));
              cogl_pipeline_set_layer_filters
                (pipeline,
                 0, /* layer_num */
                 COGL_PIPELINE_FILTER_LINEAR_MIPMAP_NEAREST,
                 COGL_PIPELINE_FILTER_LINEAR);
              cogl_pipeline_set_layer_wrap_mode
                (pipeline,
                 0, /* layer_num */
                 COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
            }

          cogl_object_unref (texture);
        }

      cogl_object_unref (bitmap);
    }

  g_free (dot_filename);

  return pipeline;
}

static CoglPipeline *
rig_transition_view_create_progress_pipeline (RigTransitionView *view)
{
  CoglPipeline *pipeline = cogl_pipeline_new (view->context->cogl_context);

  cogl_pipeline_set_color4ub (pipeline,
                              128,
                              0,
                              0,
                              128);

  return pipeline;
}

static void
rig_transition_view_update_timeline_progress (RigTransitionView *view,
                                              RutInputEvent *event)
{
  float x = rut_motion_event_get_x (event);
  float y = rut_motion_event_get_y (event);
  float progress;

  if (!rut_motion_event_unproject (event, view, &x, &y))
    g_error ("Failed to get inverse transform");

  progress = ((x - view->controls_width) /
              (view->total_width - view->controls_width));

  rut_timeline_set_progress (view->timeline, progress);
  rut_shell_queue_redraw (view->context->shell);
}

static RutInputEventStatus
rig_transition_view_grab_input_cb (RutInputEvent *event,
                                   void *user_data)
{
  RigTransitionView *view = user_data;

  if (rut_input_event_get_type (event) != RUT_INPUT_EVENT_TYPE_MOTION)
    return RUT_INPUT_EVENT_STATUS_UNHANDLED;

  if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_MOVE)
    {
      RutButtonState state = rut_motion_event_get_button_state (event);

      if ((state & RUT_BUTTON_STATE_1) &&
          rut_list_empty (&view->selected_nodes))
        {
          rig_transition_view_update_timeline_progress (view, event);
          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }
  else if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_UP)
    {
      rig_transition_view_ungrab_input (view);
      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static CoglBool
rig_transition_view_find_node_in_path (RigTransitionView *view,
                                       RigPath *path,
                                       float min_progress,
                                       float max_progress,
                                       float *t_out)
{
  GList *l;

  for (l = path->nodes.head; l; l = l->next)
    {
      RigNode *node = l->data;

      if (node->t >= min_progress && node->t <= max_progress)
        {
          *t_out = node->t;
          return TRUE;
        }
    }

  return FALSE;
}

static CoglBool
rig_transition_view_find_node (RigTransitionView *view,
                               RutInputEvent *event,
                               RigTransitionViewProperty **prop_data_out,
                               float *t_out)
{
  float x = rut_motion_event_get_x (event);
  float y = rut_motion_event_get_y (event);
  float progress;
  RigTransitionViewObject *object_data;
  int row_num = 0;

  if (!rut_motion_event_unproject (event, view, &x, &y))
    {
      g_error ("Failed to get inverse transform");
      return FALSE;
    }

  progress = ((x - view->controls_width) /
              (view->total_width - view->controls_width));

  if (progress < 0.0f || progress > 1.0f)
    return FALSE;

  rut_list_for_each (object_data, &view->objects, list_node)
    {
      RigTransitionViewProperty *prop_data;

      row_num++;

      rut_list_for_each (prop_data, &object_data->properties, list_node)
        {
          if (row_num == (int) (y / view->row_height))
            {
              float scaled_dot_size =
                view->row_height /
                (float) (view->total_width - view->controls_width);

              if (rig_transition_view_find_node_in_path (view,
                                                         prop_data->path,
                                                         progress -
                                                         scaled_dot_size,
                                                         progress +
                                                         scaled_dot_size,
                                                         t_out))
                {
                  *prop_data_out = prop_data;
                  return TRUE;
                }

              return FALSE;
            }

          row_num++;
        }
    }

  return FALSE;
}

static RutInputEventStatus
rig_transition_view_input_region_cb (RutInputRegion *region,
                                     RutInputEvent *event,
                                     void *user_data)
{
  RigTransitionView *view = user_data;

  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION &&
      rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN &&
      (rut_motion_event_get_button_state (event) & RUT_BUTTON_STATE_1) &&
      !view->have_grab)
    {
      RigTransitionViewProperty *prop_data;
      float t;

      if (rig_transition_view_find_node (view, event, &prop_data, &t))
        {
          if ((rut_motion_event_get_modifier_state (event) &
               (RUT_MODIFIER_LEFT_SHIFT_ON |
                RUT_MODIFIER_RIGHT_SHIFT_ON)) == 0)
            rig_transition_view_clear_selected_nodes (view);

          /* If shift is down then we actually want to toggle the
           * node. If the node is already selected then trying to
           * select it again will return TRUE so we know to remove it.
           * If shift wasn't down then it definetely won't be selected
           * because we'll have just cleared the selection above so it
           * doesn't matter if we toggle it */
          if (rig_transition_view_select_node (view, prop_data, t))
            rig_transition_view_unselect_node (view, prop_data, t);

          rut_shell_queue_redraw (view->context->shell);
        }
      else
        {
          rig_transition_view_clear_selected_nodes (view);
          rig_transition_view_update_timeline_progress (view, event);
        }

      view->have_grab = TRUE;
      rut_shell_grab_input (view->context->shell,
                            rut_input_event_get_camera (event),
                            rig_transition_view_grab_input_cb,
                            view);

      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
rig_transition_view_animated_cb (RutProperty *property,
                                 void *user_data)
{
  RigTransitionView *view = user_data;

  if (property->animated)
    rig_transition_view_property_added (view, property);
  else
    rig_transition_view_property_removed (view, property);
}

static void
rig_transition_view_add_property_cb (RutProperty *property,
                                     RigPath *path,
                                     const RutBoxed *constant_value,
                                     void *user_data)
{
  RigTransitionView *view = user_data;

  if (property->animated)
    rig_transition_view_property_added_no_allocation (view, property);
}

RigTransitionView *
rig_transition_view_new (RutContext *ctx,
                         RutObject *graph,
                         RigTransition *transition,
                         RutTimeline *timeline)
{
  RigTransitionView *view = g_slice_new0 (RigTransitionView);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rig_transition_view_init_type ();

      initialized = TRUE;
    }

  view->ref_count = 1;
  view->context = rut_refable_ref (ctx);
  view->graph = rut_refable_ref (graph);
  view->transition = transition;
  view->timeline = rut_refable_ref (timeline);

  view->dots_dirty = TRUE;

  view->dots_pipeline = rig_transition_view_create_dots_pipeline (view);

  view->progress_pipeline = rig_transition_view_create_progress_pipeline (view);

  rut_object_init (&view->_parent, &rig_transition_view_type);

  rut_paintable_init (RUT_OBJECT (view));
  rut_graphable_init (RUT_OBJECT (view));

  view->input_region =
    rut_input_region_new_rectangle (0.0, 0.0, /* x0/y0 */
                                    0.0, 0.0, /* x1/y1 */
                                    rig_transition_view_input_region_cb,
                                    view);
  rut_graphable_add_child (view, view->input_region);

  rut_list_init (&view->selected_nodes);
  rut_list_init (&view->objects);

  /* Add all of the existing animated properties from the transition */
  rig_transition_foreach_property (transition,
                                   rig_transition_view_add_property_cb,
                                   view);

  /* Listen for properties that become animated or not so we can
   * update the list */
  view->animated_closure =
    rut_property_context_add_animated_callback (&ctx->property_ctx,
                                                rig_transition_view_animated_cb,
                                                view,
                                                NULL /* destroy */);

  rig_transition_view_allocate (view);

  return view;
}
