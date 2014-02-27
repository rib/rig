/*
 * Rut
 *
 * A tiny toolkit
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

#include <config.h>

#include <cogl/cogl.h>
#include <math.h>

#include "rut-color-button.h"
#include "rut-color-picker.h"
#include "rut-interfaces.h"
#include "rut-paintable.h"
#include "rut-transform.h"
#include "rut-input-region.h"
#include "rut-inputable.h"
#include "rut-pickable.h"
#include "rut-introspectable.h"
#include "rut-camera.h"

enum {
  RUT_COLOR_BUTTON_PROP_COLOR,
  RUT_COLOR_BUTTON_N_PROPS
};

struct _RutColorButton
{
  RutObjectBase _base;

  RutContext *context;

  RutGraphableProps graphable;
  RutPaintableProps paintable;

  RutIntrospectableProps introspectable;
  RutProperty properties[RUT_COLOR_BUTTON_N_PROPS];

  int width, height;

  CoglColor color;

  CoglBool have_button_grab;
  CoglBool depressed;

  RutTransform *picker_transform;
  RutColorPicker *picker;
  RutInputRegion *picker_input_region;

  CoglPipeline *dark_edge_pipeline;
  CoglPipeline *light_edge_pipeline;
  CoglPipeline *padding_pipeline;

  CoglBool color_pipeline_dirty;
  CoglPipeline *color_pipeline;

  RutInputRegion *input_region;

};

RutType rut_color_button_type;

#define RUT_COLOR_BUTTON_WIDTH 32
#define RUT_COLOR_BUTTON_HEIGHT 16
#define RUT_COLOR_BUTTON_PADDING 2
#define RUT_COLOR_BUTTON_EDGE_SIZE 1

static RutPropertySpec
_rut_color_button_prop_specs[] =
  {
    {
      .name = "color",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_COLOR,
      .data_offset = offsetof (RutColorButton, color),
      .setter.color_type = rut_color_button_set_color
    },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
  };

static void
ungrab (RutColorButton *button);

static void
remove_picker (RutColorButton *button);

static void
_rut_color_button_free (void *object)
{
  RutColorButton *button = object;

  ungrab (button);
  remove_picker (button);

  cogl_object_unref (button->dark_edge_pipeline);
  cogl_object_unref (button->light_edge_pipeline);
  cogl_object_unref (button->padding_pipeline);
  cogl_object_unref (button->color_pipeline);

  rut_graphable_remove_child (button->input_region);
  rut_object_unref (button->input_region);

  rut_object_unref (button->context);

  rut_introspectable_destroy (button);
  rut_graphable_destroy (button);

  rut_object_free (RutColorButton, button);
}

static void
_rut_color_button_paint (RutObject *object,
                         RutPaintContext *paint_ctx)
{
  RutColorButton *button = (RutColorButton *) object;
  RutObject *camera = paint_ctx->camera;
  CoglFramebuffer *fb = rut_camera_get_framebuffer (camera);
  CoglPipeline *tl_pipeline;
  CoglPipeline *br_pipeline;
  float x1, y1, x2, y2;

  if (button->color_pipeline_dirty)
    {
      CoglPipeline *pipeline = cogl_pipeline_copy (button->color_pipeline);

      cogl_pipeline_set_color (pipeline, &button->color);
      cogl_object_unref (button->color_pipeline);
      button->color_pipeline = pipeline;
    }

  if (button->depressed)
    {
      tl_pipeline = button->dark_edge_pipeline;
      br_pipeline = button->light_edge_pipeline;
    }
  else
    {
      tl_pipeline = button->light_edge_pipeline;
      br_pipeline = button->dark_edge_pipeline;
    }

  /* Top edge */
  cogl_framebuffer_draw_rectangle (fb,
                                   tl_pipeline,
                                   0.0f, 0.0f,
                                   button->width,
                                   RUT_COLOR_BUTTON_EDGE_SIZE);
  /* Left edge */
  cogl_framebuffer_draw_rectangle (fb,
                                   tl_pipeline,
                                   0.0f, RUT_COLOR_BUTTON_EDGE_SIZE,
                                   RUT_COLOR_BUTTON_EDGE_SIZE,
                                   button->height);
  /* Bottom edge */
  cogl_framebuffer_draw_rectangle (fb,
                                   br_pipeline,
                                   RUT_COLOR_BUTTON_EDGE_SIZE,
                                   button->height - RUT_COLOR_BUTTON_EDGE_SIZE,
                                   button->width,
                                   button->height);
  /* Right edge */
  cogl_framebuffer_draw_rectangle (fb,
                                   br_pipeline,
                                   button->width - RUT_COLOR_BUTTON_EDGE_SIZE,
                                   RUT_COLOR_BUTTON_EDGE_SIZE,
                                   button->width,
                                   button->height - RUT_COLOR_BUTTON_EDGE_SIZE);

  /* Get the dimensions of the inner rectangle */
  x1 = RUT_COLOR_BUTTON_EDGE_SIZE + RUT_COLOR_BUTTON_PADDING;
  y1 = RUT_COLOR_BUTTON_EDGE_SIZE + RUT_COLOR_BUTTON_PADDING;
  x2 = button->width - RUT_COLOR_BUTTON_EDGE_SIZE - RUT_COLOR_BUTTON_PADDING;
  y2 = button->height - RUT_COLOR_BUTTON_EDGE_SIZE - RUT_COLOR_BUTTON_PADDING;

  /* Offset the rectangle slightly if it is depressed to make it look
   * like it has moved */
  if (button->depressed)
    {
      x1 += 1.0f;
      y1 += 1.0f;
      x2 += 1.0f;
      y2 += 1.0f;
    }

  {
    float padding_rects[4 * 4] =
      {
        /* Top */
        RUT_COLOR_BUTTON_EDGE_SIZE,
        RUT_COLOR_BUTTON_EDGE_SIZE,
        button->width - RUT_COLOR_BUTTON_EDGE_SIZE,
        y1,
        /* Bottom */
        RUT_COLOR_BUTTON_EDGE_SIZE,
        y2,
        button->width - RUT_COLOR_BUTTON_EDGE_SIZE,
        button->height - RUT_COLOR_BUTTON_EDGE_SIZE,
        /* Left */
        RUT_COLOR_BUTTON_EDGE_SIZE,
        y1,
        x1,
        y2,
        /* Right */
        x2,
        y1,
        button->width - RUT_COLOR_BUTTON_EDGE_SIZE,
        y2
      };

    cogl_framebuffer_draw_rectangles (fb,
                                      button->padding_pipeline,
                                      padding_rects,
                                      4 /* n_rects */);
  }

  /* Center color rectangle */
  cogl_framebuffer_draw_rectangle (fb,
                                   button->color_pipeline,
                                   x1, y1, x2, y2);
}

static void
rut_color_button_set_size (RutObject *object,
                           float width,
                           float height)
{
  RutColorButton *button = object;

  rut_shell_queue_redraw (button->context->shell);

  button->width = width;
  button->height = height;

  rut_input_region_set_rectangle (button->input_region,
                                  0.0f, 0.0f, /* x0 / y0 */
                                  width, height /* x1 / y1 */);
}

static void
rut_color_button_get_size (RutObject *object,
                           float *width,
                           float *height)
{
  RutColorButton *button = object;

  *width = button->width;
  *height = button->height;
}

static void
rut_color_button_get_preferred_width (RutObject *object,
                                       float for_height,
                                       float *min_width_p,
                                       float *natural_width_p)
{
  if (min_width_p)
    *min_width_p = RUT_COLOR_BUTTON_WIDTH;
  if (natural_width_p)
    *natural_width_p = RUT_COLOR_BUTTON_WIDTH;
}

static void
rut_color_button_get_preferred_height (RutObject *object,
                                        float for_width,
                                        float *min_height_p,
                                        float *natural_height_p)
{
  if (min_height_p)
    *min_height_p = RUT_COLOR_BUTTON_HEIGHT;
  if (natural_height_p)
    *natural_height_p = RUT_COLOR_BUTTON_HEIGHT;
}

static void
_rut_color_button_init_type (void)
{
  static RutGraphableVTable graphable_vtable = {
      NULL, /* child removed */
      NULL, /* child addded */
      NULL /* parent changed */
  };

  static RutPaintableVTable paintable_vtable = {
      _rut_color_button_paint
  };


  static RutSizableVTable sizable_vtable = {
      rut_color_button_set_size,
      rut_color_button_get_size,
      rut_color_button_get_preferred_width,
      rut_color_button_get_preferred_height,
      NULL /* add_preferred_size_callback */
  };


  RutType *type = &rut_color_button_type;
#define TYPE RutColorButton

  rut_type_init (type, G_STRINGIFY (TYPE), _rut_color_button_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_GRAPHABLE,
                      offsetof (TYPE, graphable),
                      &graphable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_PAINTABLE,
                      offsetof (TYPE, paintable),
                      &paintable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_INTROSPECTABLE,
                      offsetof (TYPE, introspectable),
                      NULL); /* no implied vtable */
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_SIZABLE,
                      0, /* no implied properties */
                      &sizable_vtable);

#undef TYPE
}

static CoglPipeline *
create_color_pipeline (CoglContext *context,
                       uint32_t color)
{
  CoglPipeline *pipeline = cogl_pipeline_new (context);

  cogl_pipeline_set_color4ub (pipeline,
                              color >> 24,
                              (color >> 16) & 0xff,
                              (color >> 8) & 0xff,
                              color & 0xff);

  return pipeline;
}

static RutInputEventStatus
picker_grab_input_cb (RutInputEvent *event,
                      void *user_data)
{
  RutColorButton *button = user_data;

  switch (rut_input_event_get_type (event))
    {
    case RUT_INPUT_EVENT_TYPE_MOTION:
      /* If the user clicks anywhere but in the picker then we'll
       * remove it */
      if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN)
        {
          float x = rut_motion_event_get_x (event);
          float y = rut_motion_event_get_y (event);

          if (!rut_pickable_pick (button->picker_input_region,
                                   rut_input_event_get_camera (event),
                                   NULL, /* pre-computed modelview */
                                   x, y))
            remove_picker (button);
        }
      break;

    case RUT_INPUT_EVENT_TYPE_KEY:
      /* The picker doesn't currently handle key events so if we see
       * one then the user is probably trying to interact with
       * something else and we should remove the picker */
      if (rut_key_event_get_action (event) == RUT_KEY_EVENT_ACTION_DOWN)
        remove_picker (button);
      break;

    default:
      break;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
remove_picker (RutColorButton *button)
{
  if (button->picker)
    {
      RutProperty *button_color_prop =
        &button->properties[RUT_COLOR_BUTTON_PROP_COLOR];

      rut_property_remove_binding (button_color_prop);

      rut_shell_ungrab_input (button->context->shell,
                              picker_grab_input_cb,
                              button);

      rut_graphable_remove_child (button->picker_input_region);
      rut_object_unref (button->picker_input_region);

      rut_graphable_remove_child (button->picker);
      rut_object_unref (button->picker);

      rut_graphable_remove_child (button->picker_transform);
      rut_object_unref (button->picker_transform);

      button->picker = NULL;

      rut_shell_queue_redraw (button->context->shell);
    }
}

static RutInputEventStatus
picker_input_region_cb (RutInputRegion *region,
                        RutInputEvent *event,
                        void *user_data)
{
  /* This input region is only really used to check whether the input
   * is within the picker during the grab callback so we don't
   * actually need this callback */
  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
show_picker (RutColorButton *button,
             RutObject *camera)
{
  RutObject *root, *parent;
  RutProperty *picker_color_prop;
  float picker_width, picker_height;
  CoglMatrix model_transform;
  float button_points[3 * 2];
  float x1, y1, x2, y2;
  float picker_x, picker_y;

  g_return_if_fail (button->picker == NULL);

  button->picker = rut_color_picker_new (button->context);

  rut_color_picker_set_color (button->picker, &button->color);

  /* Find the root of the graph that the color button is in */
  for (root = button;
       (parent = rut_graphable_get_parent (root));
       root = parent);

  picker_color_prop =
    rut_introspectable_lookup_property (button->picker, "color");

  if (picker_color_prop)
    {
      RutProperty *button_color_prop =
        &button->properties[RUT_COLOR_BUTTON_PROP_COLOR];

      rut_property_set_copy_binding (&button->context->property_ctx,
                                     button_color_prop,
                                     picker_color_prop);
    }

  rut_sizable_get_preferred_width (button->picker,
                                   -1, /* for_height */
                                   NULL, /* min_width */
                                   &picker_width);
  rut_sizable_get_preferred_height (button->picker,
                                    picker_width, /* for_width */
                                    NULL, /* min_height */
                                    &picker_height);

  rut_sizable_set_size (button->picker, picker_width, picker_height);

  button->picker_transform = rut_transform_new (button->context);
  rut_graphable_add_child (button->picker_transform, button->picker);

  rut_graphable_get_transform (button, &model_transform);

  button_points[0] = 0.0f;
  button_points[1] = 0.0f;
  button_points[2] = 0.0f;
  button_points[3] = button->width;
  button_points[4] = button->height;
  button_points[5] = 0.0f;
  cogl_matrix_transform_points (&model_transform,
                                2, /* n_components */
                                sizeof (float) * 3, /* stride_in */
                                button_points, /* points_in */
                                sizeof (float) * 3, /* stride_out */
                                button_points, /* points_out */
                                2 /* n_points */);

  x1 = MIN (button_points[0], button_points[3]);
  x2 = MAX (button_points[0], button_points[3]);
  y1 = MIN (button_points[1], button_points[4]);
  y2 = MAX (button_points[1], button_points[4]);

  if (x2 - picker_width < 0.0f)
    picker_x = x1;
  else
    picker_x = x2 - picker_width;

  if (y1 - picker_height < 0.0f)
    picker_y = y2;
  else
    picker_y = y1 - picker_height;

  rut_transform_translate (button->picker_transform,
                           picker_x,
                           picker_y,
                           0.0f /* z */);

  button->picker_input_region =
    rut_input_region_new_rectangle (picker_x,
                                    picker_y,
                                    picker_x + picker_width,
                                    picker_y + picker_height,
                                    picker_input_region_cb,
                                    button);
  rut_graphable_add_child (root, button->picker_input_region);

  rut_graphable_add_child (root, button->picker_transform);

  rut_shell_grab_input (button->context->shell,
                        camera,
                        picker_grab_input_cb,
                        button);
}

static RutInputEventStatus
button_grab_input_cb (RutInputEvent *event,
                      void *user_data)
{
  RutColorButton *button = user_data;
  RutObject *camera = rut_input_event_get_camera (event);
  CoglBool depressed;
  float x, y;

  if (rut_input_event_get_type (event) != RUT_INPUT_EVENT_TYPE_MOTION)
    return RUT_INPUT_EVENT_STATUS_UNHANDLED;

  x = rut_motion_event_get_x (event);
  y = rut_motion_event_get_y (event);

  depressed = rut_pickable_pick (button->input_region,
                                  camera,
                                  NULL, /* pre-computed modelview */
                                  x, y);

  if ((rut_motion_event_get_button_state (event) & RUT_BUTTON_STATE_1) == 0)
    {
      ungrab (button);

      if (depressed)
        {
          show_picker (button, camera);
          depressed = FALSE;
        }
    }

  if (depressed != button->depressed)
    {
      button->depressed = depressed;
      rut_shell_queue_redraw (button->context->shell);
    }

  return RUT_INPUT_EVENT_STATUS_HANDLED;
}

static void
ungrab (RutColorButton *button)
{
  if (button->have_button_grab)
    {
      rut_shell_ungrab_input (button->context->shell,
                              button_grab_input_cb,
                              button);
      button->have_button_grab = FALSE;
    }
}

static RutInputEventStatus
button_input_region_cb (RutInputRegion *region,
                        RutInputEvent *event,
                        void *user_data)
{
  RutColorButton *button = user_data;
  RutObject *camera;

  if (!button->have_button_grab &&
      button->picker == NULL &&
      rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION &&
      rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN &&
      (rut_motion_event_get_button_state (event) & RUT_BUTTON_STATE_1) &&
      (camera = rut_input_event_get_camera (event)))
    {
      button->have_button_grab = TRUE;
      button->depressed = TRUE;

      rut_shell_grab_input (button->context->shell,
                            camera,
                            button_grab_input_cb,
                            button);

      rut_shell_queue_redraw (button->context->shell);

      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

RutColorButton *
rut_color_button_new (RutContext *context)
{
  RutColorButton *button =
    rut_object_alloc0 (RutColorButton, &rut_color_button_type, _rut_color_button_init_type);

  button->context = rut_object_ref (context);

  cogl_color_init_from_4ub (&button->color, 0, 0, 0, 255);


  button->dark_edge_pipeline =
    create_color_pipeline (context->cogl_context, 0x000000ff);
  button->light_edge_pipeline =
    create_color_pipeline (context->cogl_context, 0xdadadaff);
  button->padding_pipeline =
    create_color_pipeline (context->cogl_context, 0x919191ff);
  button->color_pipeline =
    create_color_pipeline (context->cogl_context, 0x000000ff);

  rut_paintable_init (button);
  rut_graphable_init (button);

  rut_introspectable_init (button,
                           _rut_color_button_prop_specs,
                           button->properties);

  button->input_region =
    rut_input_region_new_rectangle (0, 0, 0, 0,
                                    button_input_region_cb,
                                    button);
  rut_graphable_add_child (button, button->input_region);

  rut_sizable_set_size (button,
                        RUT_COLOR_BUTTON_WIDTH,
                        RUT_COLOR_BUTTON_HEIGHT);

  return button;
}

void
rut_color_button_set_color (RutObject *obj,
                            const CoglColor *color)
{
  RutColorButton *button = obj;

  if (memcmp (&button->color, color, sizeof (CoglColor)))
    {
      button->color = *color;

      button->color_pipeline_dirty = TRUE;

      rut_property_dirty (&button->context->property_ctx,
                          &button->properties[RUT_COLOR_BUTTON_PROP_COLOR]);

      rut_shell_queue_redraw (button->context->shell);
    }
}

const CoglColor *
rut_color_button_get_color (RutColorButton *button)
{
  return &button->color;
}
