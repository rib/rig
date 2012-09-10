/*
 * Rig
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

#include <cogl/cogl.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <errno.h>

#include "rig.h"
#include "rig-drop-down.h"
#include "rig-text.h"

#define RIG_DROP_DOWN_EDGE_WIDTH 8
#define RIG_DROP_DOWN_EDGE_HEIGHT 16

#define RIG_DROP_DOWN_FONT_SIZE 10

enum {
  RIG_DROP_DOWN_PROP_VALUE,
  RIG_DROP_DOWN_N_PROPS
};

typedef struct
{
  PangoLayout *layout;
  PangoRectangle logical_rect;
  PangoRectangle ink_rect;
} RigDropDownLayout;

struct _RigDropDown
{
  RigObjectProps _parent;

  RigContext *context;

  RigGraphableProps graphable;
  RigPaintableProps paintable;

  CoglPipeline *bg_pipeline;
  CoglPipeline *highlighted_bg_pipeline;

  int width, height;

  int ref_count;

  /* Index of the selected value */
  int value_index;

  int n_values;
  RigDropDownValue *values;

  RigDropDownLayout *layouts;

  PangoFontDescription *font_description;

  RigInputRegion *input_region;

  RigSimpleIntrospectableProps introspectable;
  RigProperty properties[RIG_DROP_DOWN_N_PROPS];

  /* This is set to TRUE whenever the primary mouse button is clicked
   * on the widget and we have the grab */
  CoglBool button_down;
  /* This is set to TRUE when button_down is TRUE and the pointer is
   * within the button */
  CoglBool highlighted;

  gboolean selector_shown;
  int selector_x;
  int selector_y;
  int selector_width;
  int selector_height;
  int selector_value;
  CoglPath *selector_outline_path;
  CoglPipeline *selector_outline_pipeline;
};

/* Some of the pipelines are cached and attached to the CoglContext so
 * that multiple drop downs created using the same CoglContext will
 * use the same pipelines */
typedef struct
{
  CoglPipeline *bg_pipeline;
  CoglPipeline *highlighted_bg_pipeline;
} RigDropDownContextData;

RigType rig_drop_down_type;

static void
rig_drop_down_hide_selector (RigDropDown *drop);

static RigPropertySpec
_rig_drop_down_prop_specs[] =
  {
    {
      .name = "value",
      .type = RIG_PROPERTY_TYPE_INTEGER,
      .getter = rig_drop_down_get_value,
      .setter = rig_drop_down_set_value
    },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
  };

static RigDropDownContextData *
rig_drop_down_get_context_data (RigContext *context)
{
  static CoglUserDataKey context_data_key;
  RigDropDownContextData *context_data =
    cogl_object_get_user_data (COGL_OBJECT (context->cogl_context),
                               &context_data_key);

  if (context_data == NULL)
    {
      context_data = g_new0 (RigDropDownContextData, 1);
      cogl_object_set_user_data (COGL_OBJECT (context->cogl_context),
                                 &context_data_key,
                                 context_data,
                                 g_free);
    }

  return context_data;
}

static CoglPipeline *
rig_drop_down_create_bg_pipeline (RigContext *context)
{
  RigDropDownContextData *context_data =
    rig_drop_down_get_context_data (context);

  /* The pipeline is cached so that if multiple drop downs are created
   * they will share a reference to the same pipeline */
  if (context_data->bg_pipeline)
    return cogl_object_ref (context_data->bg_pipeline);
  else
    {
      CoglPipeline *pipeline = cogl_pipeline_new (context->cogl_context);
      static CoglUserDataKey bg_pipeline_destroy_key;
      CoglTexture *bg_texture;
      CoglError *error = NULL;

      bg_texture = rig_load_texture (context,
                                     RIG_DATA_DIR
                                     "drop-down-background.png",
                                     &error);
      if (bg_texture)
        {
          const CoglPipelineWrapMode wrap_mode =
            COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE;

          cogl_pipeline_set_layer_texture (pipeline, 0, bg_texture);
          cogl_pipeline_set_layer_wrap_mode (pipeline,
                                             0, /* layer_index */
                                             wrap_mode);
          cogl_pipeline_set_layer_filters (pipeline,
                                           0, /* layer_index */
                                           COGL_PIPELINE_FILTER_NEAREST,
                                           COGL_PIPELINE_FILTER_NEAREST);
        }
      else
        {
          g_warning ("Failed to load drop-down-background.png: %s",
                     error->message);
          g_clear_error (&error);
        }

      /* When the last drop down is destroyed the pipeline will be
       * destroyed and we'll set context->bg_pipeline to NULL so that
       * it will be recreated for the next drop down */
      cogl_object_set_user_data (COGL_OBJECT (pipeline),
                                 &bg_pipeline_destroy_key,
                                 &context_data->bg_pipeline,
                                 (CoglUserDataDestroyCallback)
                                 g_nullify_pointer);

      context_data->bg_pipeline = pipeline;

      return pipeline;
    }
}

static CoglPipeline *
rig_drop_down_create_highlighted_bg_pipeline (RigContext *context)
{
  RigDropDownContextData *context_data =
    rig_drop_down_get_context_data (context);

  /* The pipeline is cached so that if multiple drop downs are created
   * they will share a reference to the same pipeline */
  if (context_data->highlighted_bg_pipeline)
    return cogl_object_ref (context_data->highlighted_bg_pipeline);
  else
    {
      CoglPipeline *bg_pipeline =
        rig_drop_down_create_bg_pipeline (context);
      CoglPipeline *pipeline = cogl_pipeline_copy (bg_pipeline);
      static CoglUserDataKey pipeline_destroy_key;

      cogl_object_unref (bg_pipeline);

      /* Invert the colours of the texture so that there is some
       * obvious feedback when the button is pressed. */
      /* What we want is 1-colour. However we want this to remain
       * pre-multiplied so what we actually want is alpha×(1-colour) =
       * alpha-alpha×colour. The texture is already premultiplied so
       * the colour values are already alpha×colour and we just need
       * to subtract it from the alpha value. */
      cogl_pipeline_set_layer_combine (pipeline,
                                       1, /* layer_number */
                                       "RGB = SUBTRACT(PREVIOUS[A], PREVIOUS)"
                                       "A = REPLACE(PREVIOUS[A])",
                                       NULL);

      /* When the last drop down is destroyed the pipeline will be
       * destroyed and we'll set context->highlighted_bg_pipeline to NULL
       * so that it will be recreated for the next drop down */
      cogl_object_set_user_data (COGL_OBJECT (pipeline),
                                 &pipeline_destroy_key,
                                 &context_data->highlighted_bg_pipeline,
                                 (CoglUserDataDestroyCallback)
                                 g_nullify_pointer);

      context_data->highlighted_bg_pipeline = pipeline;

      return pipeline;
    }
}

static void
rig_drop_down_clear_layouts (RigDropDown *drop)
{
  if (drop->layouts)
    {
      int i;

      for (i = 0; i < drop->n_values; i++)
        g_object_unref (drop->layouts[i].layout);

      g_free (drop->layouts);
      drop->layouts = NULL;
    }
}

static void
rig_drop_down_free_values (RigDropDown *drop)
{
  if (drop->values)
    {
      int i = 0;

      for (i = 0; i < drop->n_values; i++)
        g_free ((char *) drop->values[i].name);

      g_free (drop->values);
      drop->values = NULL;
    }
}

static void
_rig_drop_down_free (void *object)
{
  RigDropDown *drop = object;

  rig_ref_countable_unref (drop->context);
  cogl_object_unref (drop->bg_pipeline);
  cogl_object_unref (drop->highlighted_bg_pipeline);

  rig_drop_down_free_values (drop);

  rig_drop_down_clear_layouts (drop);

  rig_graphable_remove_child (drop->input_region);
  rig_ref_countable_unref (drop->input_region);

  rig_simple_introspectable_destroy (drop);

  pango_font_description_free (drop->font_description);

  rig_drop_down_hide_selector (drop);
  if (drop->selector_outline_pipeline)
    cogl_object_unref (drop->selector_outline_pipeline);

  g_slice_free (RigDropDown, drop);
}

RigRefCountableVTable _rig_drop_down_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_drop_down_free
};

typedef struct
{
  float x1, y1, x2, y2;
  float s1, t1, s2, t2;
} RigDropDownRectangle;

static PangoFontDescription *
rig_drop_down_create_font_description (void)
{
  PangoFontDescription *font_description = pango_font_description_new ();

  pango_font_description_set_family (font_description, "Sans");
  pango_font_description_set_absolute_size (font_description,
                                            RIG_DROP_DOWN_FONT_SIZE *
                                            PANGO_SCALE);

  return font_description;
}

static void
rig_drop_down_ensure_layouts (RigDropDown *drop)
{
  if (drop->layouts == NULL)
    {
      int i;

      drop->layouts = g_new (RigDropDownLayout, drop->n_values);

      for (i = 0; i < drop->n_values; i++)
        {
          RigDropDownLayout *layout = drop->layouts + i;

          layout->layout = pango_layout_new (drop->context->pango_context);

          pango_layout_set_text (layout->layout, drop->values[i].name, -1);

          pango_layout_set_font_description (layout->layout,
                                             drop->font_description);

          pango_layout_get_pixel_extents (layout->layout,
                                          &layout->ink_rect,
                                          &layout->logical_rect);

          cogl_pango_ensure_glyph_cache_for_layout (layout->layout);
        }
    }
}

static void
rig_drop_down_paint_selector (RigDropDown *drop,
                              RigPaintContext *paint_ctx)
{
  RigCamera *camera = paint_ctx->camera;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (camera);
  int i, y_pos = drop->selector_y + 3;

  cogl_framebuffer_draw_textured_rectangle (fb,
                                            drop->bg_pipeline,
                                            drop->selector_x,
                                            drop->selector_y,
                                            drop->selector_x +
                                            drop->selector_width,
                                            drop->selector_y +
                                            drop->selector_height,
                                            /* Stretch centre pixel of
                                               bg texture to entire
                                               rectangle */
                                            0.5f, 0.5f, 0.5f, 0.5f);

  cogl_framebuffer_stroke_path (fb,
                                drop->selector_outline_pipeline,
                                drop->selector_outline_path);

  rig_drop_down_ensure_layouts (drop);

  for (i = 0; i < drop->n_values; i++)
    {
      RigDropDownLayout *layout = drop->layouts + i;
      int x_pos = (drop->selector_x +
                   drop->selector_width / 2 -
                   layout->logical_rect.width / 2);
      CoglColor font_color;

      if (i == drop->selector_value)
        {
          CoglPipeline *pipeline = drop->highlighted_bg_pipeline;

          cogl_framebuffer_draw_textured_rectangle (fb,
                                                    pipeline,
                                                    drop->selector_x,
                                                    y_pos,
                                                    drop->selector_x +
                                                    drop->selector_width - 1,
                                                    y_pos +
                                                    layout->logical_rect.height,
                                                    /* Stretch centre pixel of
                                                       bg texture to entire
                                                       rectangle */
                                                    0.5f, 0.5f, 0.5f, 0.5f);
          cogl_color_init_from_4ub (&font_color, 255, 255, 255, 255);
        }
      else
        cogl_color_init_from_4ub (&font_color, 0, 0, 0, 255);

      cogl_pango_show_layout (fb,
                              layout->layout,
                              x_pos, y_pos,
                              &font_color);

      y_pos += layout->logical_rect.height;
    }
}

static void
rig_drop_down_paint_button (RigDropDown *drop,
                            RigPaintContext *paint_ctx)
{
  RigCamera *camera = paint_ctx->camera;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (camera);
  RigDropDownRectangle coords[7];
  int translation = drop->width - RIG_DROP_DOWN_EDGE_WIDTH;
  CoglColor font_color;
  RigDropDownLayout *layout;
  int i;

  /* Top left rounded corner */
  coords[0].x1 = 0.0f;
  coords[0].y1 = 0.0f;
  coords[0].x2 = RIG_DROP_DOWN_EDGE_WIDTH;
  coords[0].y2 = RIG_DROP_DOWN_EDGE_HEIGHT / 2;
  coords[0].s1 = 0.0f;
  coords[0].t1 = 0.0f;
  coords[0].s2 = 0.5f;
  coords[0].t2 = 0.5f;

  /* Centre gap */
  coords[1].x1 = 0.0f;
  coords[1].y1 = coords[0].y2;
  coords[1].x2 = RIG_DROP_DOWN_EDGE_WIDTH;
  coords[1].y2 = drop->height - RIG_DROP_DOWN_EDGE_HEIGHT / 2;
  coords[1].s1 = 0.0f;
  coords[1].t1 = 0.5f;
  coords[1].s2 = 0.5f;
  coords[1].t2 = 0.5f;

  /* Bottom left rounded corner */
  coords[2].x1 = 0.0f;
  coords[2].y1 = coords[1].y2;
  coords[2].x2 = RIG_DROP_DOWN_EDGE_WIDTH;
  coords[2].y2 = drop->height;
  coords[2].s1 = 0.0f;
  coords[2].t1 = 0.5f;
  coords[2].s2 = 0.5f;
  coords[2].t2 = 1.0f;

  /* Centre rectangle */
  coords[3].x1 = RIG_DROP_DOWN_EDGE_WIDTH;
  coords[3].y1 = 0.0f;
  coords[3].x2 = drop->width - RIG_DROP_DOWN_EDGE_WIDTH;
  coords[3].y2 = drop->height;
  /* Stetch the centre pixel to cover the entire rectangle */
  coords[3].s1 = 0.5f;
  coords[3].t1 = 0.5f;
  coords[3].s2 = 0.5f;
  coords[3].t2 = 0.5f;

  /* The right hand side rectangles are just translated copies of the
   * left hand side rectangles with the texture coordinates shifted
   * over to the other half */
  for (i = 0; i < 3; i++)
    {
      RigDropDownRectangle *out = coords + i + 4;
      const RigDropDownRectangle *in = coords + i;

      out->x1 = in->x1 + translation;
      out->y1 = in->y1;
      out->x2 = in->x2 + translation;
      out->y2 = in->y2;
      out->s1 = in->s1 + 0.5f;
      out->t1 = in->t1;
      out->s2 = in->s2 + 0.5f;
      out->t2 = in->t2;
    }

  cogl_framebuffer_draw_textured_rectangles (fb,
                                             drop->highlighted ?
                                             drop->highlighted_bg_pipeline :
                                             drop->bg_pipeline,
                                             (float *) coords,
                                             G_N_ELEMENTS (coords));

  layout = drop->layouts + drop->value_index;

  rig_drop_down_ensure_layouts (drop);

  cogl_color_init_from_4ub (&font_color, 0, 0, 0, 255);

  cogl_pango_show_layout (fb,
                          layout->layout,
                          drop->width / 2 -
                          layout->logical_rect.width / 2,
                          drop->height / 2 -
                          layout->logical_rect.height / 2,
                          &font_color);
}

static void
_rig_drop_down_paint (RigObject *object,
                      RigPaintContext *paint_ctx)
{
  RigDropDown *drop = RIG_DROP_DOWN (object);

  switch (paint_ctx->layer_number)
    {
    case 0:
      rig_drop_down_paint_button (drop, paint_ctx);

      /* If the selector is visible then we'll queue it to be painted
       * in the next layer so that it won't appear under the
       * subsequent controls */
      if (drop->selector_shown)
        rig_paint_context_queue_paint (paint_ctx, object);
      break;

    case 1:
      rig_drop_down_paint_selector (drop, paint_ctx);
      break;
    }
}

static int
rig_drop_down_find_value_at_position (RigDropDown *drop,
                                      float x,
                                      float y)
{
  int i, y_pos = drop->selector_y + 3;

  if (x < drop->selector_x || x >= drop->selector_x + drop->selector_width)
    return -1;

  for (i = 0; i < drop->n_values; i++)
    {
      const RigDropDownLayout *layout = drop->layouts + i;

      if (y >= y_pos && y < y_pos + layout->logical_rect.height)
        return i;

      y_pos += layout->logical_rect.height;
    }

  return -1;
}

static RigInputEventStatus
rig_drop_down_selector_grab_cb (RigInputEvent *event,
                                void *user_data)
{
  RigDropDown *drop = user_data;

  switch (rig_input_event_get_type (event))
    {
    case RIG_INPUT_EVENT_TYPE_MOTION:
      {
        float x, y;
        int selector_value;

        if (rig_motion_event_unproject (event,
                                        drop,
                                        &x, &y))
          selector_value = rig_drop_down_find_value_at_position (drop, x, y);
        else
          selector_value = -1;

        if (selector_value != drop->selector_value)
          {
            drop->selector_value = selector_value;
            rig_shell_queue_redraw (drop->context->shell);
          }

        /* If this is a click then commit the chosen value */
        if (rig_motion_event_get_action (event) ==
            RIG_MOTION_EVENT_ACTION_DOWN)
        {
          rig_drop_down_hide_selector (drop);

          if (selector_value != -1)
            rig_drop_down_set_value (drop, selector_value);

          return RIG_INPUT_EVENT_STATUS_HANDLED;
        }
      }
      break;

    case RIG_INPUT_EVENT_TYPE_KEY:
      /* The escape key cancels the selector */
      if (rig_key_event_get_action (event) == RIG_KEY_EVENT_ACTION_DOWN &&
          rig_key_event_get_keysym (event) == RIG_KEY_Escape)
        rig_drop_down_hide_selector (drop);
      break;

    default:
      break;
    }

  return RIG_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
rig_drop_down_handle_click (RigDropDown *drop,
                            RigInputEvent *event)
{
  CoglMatrix modelview;
  const CoglMatrix *projection;
  RigCamera *camera = rig_input_event_get_camera (event);
  float top_point[4];
  int i;

  drop->selector_width = MAX (drop->width - 6, 0);
  drop->selector_height = 0;

  /* Calculate the size of the selector */
  for (i = 0; i < drop->n_values; i++)
    {
      RigDropDownLayout *layout = drop->layouts + i;

      if (layout->logical_rect.width > drop->selector_width)
        drop->selector_width = layout->logical_rect.width;

      drop->selector_height += layout->logical_rect.height;
    }

  /* Add three pixels all sides for a 1-pixel border and a two pixel
   * gap */
  drop->selector_width += 6;
  drop->selector_height += 6;

  drop->selector_x = drop->width / 2 - drop->selector_width / 2;

  /* Check whether putting the selector below the control would make
   * it go off the screen */
  rig_graphable_get_modelview (RIG_OBJECT (drop), camera, &modelview);
  projection = rig_camera_get_projection (camera);
  top_point[0] = drop->selector_x;
  top_point[1] = drop->selector_height + drop->height;
  cogl_matrix_transform_points (&modelview,
                                2, /* n_components */
                                sizeof (float) * 4, /* stride_in */
                                top_point, /* points_in */
                                sizeof (float) * 4, /* stride_out */
                                top_point, /* points_out */
                                1 /* n_points */);
  cogl_matrix_project_points (projection,
                              3, /* n_components */
                              sizeof (float) * 4, /* stride_in */
                              top_point, /* points_in */
                              sizeof (float) * 4, /* stride_out */
                              top_point, /* points_out */
                              1 /* n_points */);
  top_point[1] /= top_point[3];

  if (top_point[1] >= -1.0f)
    drop->selector_y = drop->height;
  else
    drop->selector_y = -drop->selector_height;

  if (drop->selector_outline_pipeline == NULL)
    {
      drop->selector_outline_pipeline =
        cogl_pipeline_new (drop->context->cogl_context);
      cogl_pipeline_set_color4ub (drop->selector_outline_pipeline,
                                  0, 0, 0, 255);
    }

  drop->selector_outline_path = cogl_path_new (drop->context->cogl_context);
  cogl_path_rectangle (drop->selector_outline_path,
                       drop->selector_x,
                       drop->selector_y,
                       drop->selector_x + drop->selector_width,
                       drop->selector_y + drop->selector_height);

  rig_shell_grab_input (drop->context->shell,
                        rig_input_event_get_camera (event),
                        rig_drop_down_selector_grab_cb,
                        drop);

  drop->selector_shown = TRUE;
  drop->selector_value = -1;

  rig_shell_queue_redraw (drop->context->shell);
}

static RigInputEventStatus
rig_drop_down_input_cb (RigInputEvent *event,
                        void *user_data)
{
  RigDropDown *drop = user_data;
  CoglBool highlighted;
  float x, y;

  if (rig_input_event_get_type (event) != RIG_INPUT_EVENT_TYPE_MOTION)
    return RIG_INPUT_EVENT_STATUS_UNHANDLED;

  x = rig_motion_event_get_x (event);
  y = rig_motion_event_get_y (event);

  if ((rig_motion_event_get_button_state (event) & RIG_BUTTON_STATE_1) == 0)
    {
      drop->button_down = FALSE;
      rig_shell_ungrab_input (drop->context->shell,
                              rig_drop_down_input_cb,
                              user_data);

      /* If we the pointer is still over the widget then treat it as a
       * click */
      if (drop->highlighted)
        rig_drop_down_handle_click (drop, event);

      highlighted = FALSE;
    }
  else
    {
      RigCamera *camera = rig_input_event_get_camera (event);

      highlighted = rig_camera_pick_input_region (camera,
                                                  drop->input_region,
                                                  x, y);
    }

  if (highlighted != drop->highlighted)
    {
      drop->highlighted = highlighted;
      rig_shell_queue_redraw (drop->context->shell);
    }

  return RIG_INPUT_EVENT_STATUS_UNHANDLED;
}

static RigInputEventStatus
rig_drop_down_input_region_cb (RigInputRegion *region,
                               RigInputEvent *event,
                               void *user_data)
{
  RigDropDown *drop = user_data;
  RigCamera *camera;

  if (!drop->button_down &&
      !drop->selector_shown &&
      rig_input_event_get_type (event) == RIG_INPUT_EVENT_TYPE_MOTION &&
      rig_motion_event_get_action (event) == RIG_MOTION_EVENT_ACTION_DOWN &&
      (rig_motion_event_get_button_state (event) & RIG_BUTTON_STATE_1) &&
      (camera = rig_input_event_get_camera (event)))
    {
      drop->button_down = TRUE;
      drop->highlighted = TRUE;

      rig_shell_grab_input (drop->context->shell,
                            camera,
                            rig_drop_down_input_cb,
                            drop);

      rig_shell_queue_redraw (drop->context->shell);

      return RIG_INPUT_EVENT_STATUS_HANDLED;
    }

  return RIG_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
rig_drop_down_hide_selector (RigDropDown *drop)
{
  if (drop->selector_shown)
    {
      cogl_object_unref (drop->selector_outline_path);
      drop->selector_shown = FALSE;
      rig_shell_queue_redraw (drop->context->shell);

      rig_shell_ungrab_input (drop->context->shell,
                              rig_drop_down_selector_grab_cb,
                              drop);
    }
}

static void
rig_drop_down_set_size (RigObject *object,
                        float width,
                        float height)
{
  RigDropDown *drop = RIG_DROP_DOWN (object);

  rig_shell_queue_redraw (drop->context->shell);
  drop->width = width;
  drop->height = height;
  rig_input_region_set_rectangle (drop->input_region,
                                  0.0f, 0.0f, /* x0 / y0 */
                                  drop->width, drop->height /* x1 / y1 */);
}

static void
rig_drop_down_get_size (RigObject *object,
                        float *width,
                        float *height)
{
  RigDropDown *drop = RIG_DROP_DOWN (object);

  *width = drop->width;
  *height = drop->height;
}

static void
rig_drop_down_get_preferred_width (RigObject *object,
                                   float for_height,
                                   float *min_width_p,
                                   float *natural_width_p)
{
  RigDropDown *drop = RIG_DROP_DOWN (object);
  int max_width = 0;
  int i;

  rig_drop_down_ensure_layouts (drop);

  /* Get the narrowest layout */
  for (i = 0; i < drop->n_values; i++)
    {
      RigDropDownLayout *layout = drop->layouts + i;

      if (layout->logical_rect.width > max_width)
        max_width = layout->logical_rect.width;
    }

  /* Add space for the edges */
  max_width += RIG_DROP_DOWN_EDGE_WIDTH * 2;

  if (min_width_p)
    *min_width_p = max_width;
  if (natural_width_p)
    /* Leave two pixels either side of the label */
    *natural_width_p = max_width + 4;
}

static void
rig_drop_down_get_preferred_height (RigObject *object,
                                    float for_width,
                                    float *min_height_p,
                                    float *natural_height_p)
{
  RigDropDown *drop = RIG_DROP_DOWN (object);
  int max_height = G_MININT;
  int i;

  rig_drop_down_ensure_layouts (drop);

  /* Get the tallest layout */
  for (i = 0; i < drop->n_values; i++)
    {
      RigDropDownLayout *layout = drop->layouts + i;

      if (layout->logical_rect.height > max_height)
        max_height = layout->logical_rect.height;
    }

  if (min_height_p)
    *min_height_p = MAX (max_height, RIG_DROP_DOWN_EDGE_HEIGHT);
  if (natural_height_p)
    *natural_height_p = MAX (max_height + 4,
                             RIG_DROP_DOWN_EDGE_HEIGHT);
}

static RigGraphableVTable _rig_drop_down_graphable_vtable = {
  NULL, /* child removed */
  NULL, /* child addded */
  NULL /* parent changed */
};

static RigPaintableVTable _rig_drop_down_paintable_vtable = {
  _rig_drop_down_paint
};

static RigIntrospectableVTable _rig_drop_down_introspectable_vtable = {
  rig_simple_introspectable_lookup_property,
  rig_simple_introspectable_foreach_property
};

static RigSizableVTable _rig_drop_down_sizable_vtable = {
  rig_drop_down_set_size,
  rig_drop_down_get_size,
  rig_drop_down_get_preferred_width,
  rig_drop_down_get_preferred_height
};

static void
_rig_drop_down_init_type (void)
{
  rig_type_init (&rig_drop_down_type);
  rig_type_add_interface (&rig_drop_down_type,
                          RIG_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigDropDown, ref_count),
                          &_rig_drop_down_ref_countable_vtable);
  rig_type_add_interface (&rig_drop_down_type,
                          RIG_INTERFACE_ID_GRAPHABLE,
                          offsetof (RigDropDown, graphable),
                          &_rig_drop_down_graphable_vtable);
  rig_type_add_interface (&rig_drop_down_type,
                          RIG_INTERFACE_ID_PAINTABLE,
                          offsetof (RigDropDown, paintable),
                          &_rig_drop_down_paintable_vtable);
  rig_type_add_interface (&rig_drop_down_type,
                          RIG_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rig_drop_down_introspectable_vtable);
  rig_type_add_interface (&rig_drop_down_type,
                          RIG_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RigDropDown, introspectable),
                          NULL); /* no implied vtable */
  rig_type_add_interface (&rig_drop_down_type,
                          RIG_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &_rig_drop_down_sizable_vtable);
}

RigDropDown *
rig_drop_down_new (RigContext *context)
{
  RigDropDown *drop = g_slice_new0 (RigDropDown);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rig_init ();
      _rig_drop_down_init_type ();

      initialized = TRUE;
    }

  drop->ref_count = 1;
  drop->context = rig_ref_countable_ref (context);

  /* Set a dummy value so we can assume that value_index is always a
   * valid index */
  drop->values = g_new (RigDropDownValue, 1);
  drop->values->name = g_strdup ("");
  drop->values->value = 0;

  drop->font_description = rig_drop_down_create_font_description ();

  rig_object_init (&drop->_parent, &rig_drop_down_type);

  rig_paintable_init (RIG_OBJECT (drop));
  rig_graphable_init (RIG_OBJECT (drop));

  rig_simple_introspectable_init (drop,
                                  _rig_drop_down_prop_specs,
                                  drop->properties);

  drop->bg_pipeline = rig_drop_down_create_bg_pipeline (context);
  drop->highlighted_bg_pipeline =
    rig_drop_down_create_highlighted_bg_pipeline (context);

  drop->input_region =
    rig_input_region_new_rectangle (0, 0, 0, 0,
                                    rig_drop_down_input_region_cb,
                                    drop);
  rig_graphable_add_child (drop, drop->input_region);

  rig_sizable_set_size (drop, 60, 30);

  return drop;
}

void
rig_drop_down_set_value (RigDropDown *drop,
                         int value)
{
  int i;

  value = CLAMP (value, 0, drop->n_values - 1);

  if (value == drop->values[drop->value_index].value)
    return;

  for (i = 0; i < drop->n_values; i++)
    if (drop->values[i].value == value)
      {
        drop->value_index = i;

        rig_property_dirty (&drop->context->property_ctx,
                            &drop->properties[RIG_DROP_DOWN_PROP_VALUE]);

        rig_shell_queue_redraw (drop->context->shell);
      }
}

int
rig_drop_down_get_value (RigDropDown *drop)
{
  return drop->values[drop->value_index].value;
}

void
rig_drop_down_set_values (RigDropDown *drop,
                          ...)
{
  va_list ap1, ap2;
  const char *name;
  int i = 0, n_values = 0;
  RigDropDownValue *values;

  va_start (ap1, drop);
  G_VA_COPY (ap2, ap1);

  while ((name = va_arg (ap1, const char *)))
    {
      /* Skip the value */
      va_arg (ap1, int);
      n_values++;
    }

  values = g_alloca (sizeof (RigDropDownValue) * n_values);

  while ((name = va_arg (ap2, const char *)))
    {
      values[i].name = name;
      values[i].value = va_arg (ap2, int);
      i++;
    }

  va_end (ap1);

  rig_drop_down_set_values_array (drop, values, n_values);
}

void
rig_drop_down_set_values_array (RigDropDown *drop,
                                const RigDropDownValue *values,
                                int n_values)
{
  int old_value;
  int old_value_index = 0;
  int i;

  g_return_if_fail (n_values >= 0);

  old_value = rig_drop_down_get_value (drop);

  rig_drop_down_free_values (drop);

  drop->values = g_malloc (sizeof (RigDropDownValue) * n_values);
  for (i = 0; i < n_values; i++)
    {
      drop->values[i].name = g_strdup (values[i].name);
      drop->values[i].value = values[i].value;

      if (values[i].value == old_value)
        old_value_index = i;
    }

  drop->n_values = n_values;

  drop->value_index = old_value_index;

  rig_shell_queue_redraw (drop->context->shell);
  rig_drop_down_clear_layouts (drop);
}
