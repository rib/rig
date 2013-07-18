/*
 * Rut
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
#include <cogl-path/cogl-path.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <errno.h>

#include "rut.h"
#include "rut-drop-down.h"
#include "rut-text.h"

#define RUT_DROP_DOWN_EDGE_WIDTH 8
#define RUT_DROP_DOWN_EDGE_HEIGHT 16

#define RUT_DROP_DOWN_FONT_SIZE 10

enum {
  RUT_DROP_DOWN_PROP_VALUE,
  RUT_DROP_DOWN_N_PROPS
};

typedef struct
{
  PangoLayout *layout;
  PangoRectangle logical_rect;
  PangoRectangle ink_rect;
} RutDropDownLayout;

struct _RutDropDown
{
  RutObjectProps _parent;

  RutContext *context;

  RutGraphableProps graphable;
  RutPaintableProps paintable;

  CoglPipeline *bg_pipeline;
  CoglPipeline *highlighted_bg_pipeline;

  int width, height;

  int ref_count;

  /* Index of the selected value */
  int value_index;

  int n_values;
  RutDropDownValue *values;

  RutDropDownLayout *layouts;

  PangoFontDescription *font_description;

  RutInputRegion *input_region;

  RutSimpleIntrospectableProps introspectable;
  RutProperty properties[RUT_DROP_DOWN_N_PROPS];

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
} RutDropDownContextData;

RutType rut_drop_down_type;

static void
rut_drop_down_hide_selector (RutDropDown *drop);

static RutPropertySpec
_rut_drop_down_prop_specs[] =
  {
    {
      .name = "value",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .getter.integer_type = rut_drop_down_get_value,
      .setter.integer_type = rut_drop_down_set_value
    },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
  };

static RutDropDownContextData *
rut_drop_down_get_context_data (RutContext *context)
{
  static CoglUserDataKey context_data_key;
  RutDropDownContextData *context_data =
    cogl_object_get_user_data (COGL_OBJECT (context->cogl_context),
                               &context_data_key);

  if (context_data == NULL)
    {
      context_data = g_new0 (RutDropDownContextData, 1);
      cogl_object_set_user_data (COGL_OBJECT (context->cogl_context),
                                 &context_data_key,
                                 context_data,
                                 g_free);
    }

  return context_data;
}

static CoglPipeline *
rut_drop_down_create_bg_pipeline (RutContext *context)
{
  RutDropDownContextData *context_data =
    rut_drop_down_get_context_data (context);

  /* The pipeline is cached so that if multiple drop downs are created
   * they will share a reference to the same pipeline */
  if (context_data->bg_pipeline)
    return cogl_object_ref (context_data->bg_pipeline);
  else
    {
      CoglPipeline *pipeline = cogl_pipeline_new (context->cogl_context);
      static CoglUserDataKey bg_pipeline_destroy_key;
      CoglTexture *bg_texture;
      GError *error = NULL;

      bg_texture = rut_load_texture_from_data_file (context,
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
          g_error_free (error);
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
rut_drop_down_create_highlighted_bg_pipeline (RutContext *context)
{
  RutDropDownContextData *context_data =
    rut_drop_down_get_context_data (context);

  /* The pipeline is cached so that if multiple drop downs are created
   * they will share a reference to the same pipeline */
  if (context_data->highlighted_bg_pipeline)
    return cogl_object_ref (context_data->highlighted_bg_pipeline);
  else
    {
      CoglPipeline *bg_pipeline =
        rut_drop_down_create_bg_pipeline (context);
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
rut_drop_down_clear_layouts (RutDropDown *drop)
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
rut_drop_down_free_values (RutDropDown *drop)
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
_rut_drop_down_free (void *object)
{
  RutDropDown *drop = object;

  rut_refable_unref (drop->context);
  cogl_object_unref (drop->bg_pipeline);
  cogl_object_unref (drop->highlighted_bg_pipeline);

  rut_drop_down_free_values (drop);

  rut_drop_down_clear_layouts (drop);

  rut_graphable_remove_child (drop->input_region);
  rut_refable_unref (drop->input_region);

  rut_simple_introspectable_destroy (drop);
  rut_graphable_destroy (drop);

  pango_font_description_free (drop->font_description);

  rut_drop_down_hide_selector (drop);
  if (drop->selector_outline_pipeline)
    cogl_object_unref (drop->selector_outline_pipeline);

  g_slice_free (RutDropDown, drop);
}

RutRefableVTable _rut_drop_down_refable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_drop_down_free
};

typedef struct
{
  float x1, y1, x2, y2;
  float s1, t1, s2, t2;
} RutDropDownRectangle;

static PangoFontDescription *
rut_drop_down_create_font_description (void)
{
  PangoFontDescription *font_description = pango_font_description_new ();

  pango_font_description_set_family (font_description, "Sans");
  pango_font_description_set_absolute_size (font_description,
                                            RUT_DROP_DOWN_FONT_SIZE *
                                            PANGO_SCALE);

  return font_description;
}

static void
rut_drop_down_ensure_layouts (RutDropDown *drop)
{
  if (drop->layouts == NULL)
    {
      int i;

      drop->layouts = g_new (RutDropDownLayout, drop->n_values);

      for (i = 0; i < drop->n_values; i++)
        {
          RutDropDownLayout *layout = drop->layouts + i;

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
rut_drop_down_paint_selector (RutDropDown *drop,
                              RutPaintContext *paint_ctx)
{
  RutCamera *camera = paint_ctx->camera;
  CoglFramebuffer *fb = rut_camera_get_framebuffer (camera);
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

  cogl_path_stroke (drop->selector_outline_path,
                    fb,
                    drop->selector_outline_pipeline);

  rut_drop_down_ensure_layouts (drop);

  for (i = 0; i < drop->n_values; i++)
    {
      RutDropDownLayout *layout = drop->layouts + i;
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
rut_drop_down_paint_button (RutDropDown *drop,
                            RutPaintContext *paint_ctx)
{
  RutCamera *camera = paint_ctx->camera;
  CoglFramebuffer *fb = rut_camera_get_framebuffer (camera);
  RutDropDownRectangle coords[7];
  int translation = drop->width - RUT_DROP_DOWN_EDGE_WIDTH;
  CoglColor font_color;
  RutDropDownLayout *layout;
  int i;

  /* Top left rounded corner */
  coords[0].x1 = 0.0f;
  coords[0].y1 = 0.0f;
  coords[0].x2 = RUT_DROP_DOWN_EDGE_WIDTH;
  coords[0].y2 = RUT_DROP_DOWN_EDGE_HEIGHT / 2;
  coords[0].s1 = 0.0f;
  coords[0].t1 = 0.0f;
  coords[0].s2 = 0.5f;
  coords[0].t2 = 0.5f;

  /* Centre gap */
  coords[1].x1 = 0.0f;
  coords[1].y1 = coords[0].y2;
  coords[1].x2 = RUT_DROP_DOWN_EDGE_WIDTH;
  coords[1].y2 = drop->height - RUT_DROP_DOWN_EDGE_HEIGHT / 2;
  coords[1].s1 = 0.0f;
  coords[1].t1 = 0.5f;
  coords[1].s2 = 0.5f;
  coords[1].t2 = 0.5f;

  /* Bottom left rounded corner */
  coords[2].x1 = 0.0f;
  coords[2].y1 = coords[1].y2;
  coords[2].x2 = RUT_DROP_DOWN_EDGE_WIDTH;
  coords[2].y2 = drop->height;
  coords[2].s1 = 0.0f;
  coords[2].t1 = 0.5f;
  coords[2].s2 = 0.5f;
  coords[2].t2 = 1.0f;

  /* Centre rectangle */
  coords[3].x1 = RUT_DROP_DOWN_EDGE_WIDTH;
  coords[3].y1 = 0.0f;
  coords[3].x2 = drop->width - RUT_DROP_DOWN_EDGE_WIDTH;
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
      RutDropDownRectangle *out = coords + i + 4;
      const RutDropDownRectangle *in = coords + i;

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

  rut_drop_down_ensure_layouts (drop);

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
_rut_drop_down_paint (RutObject *object,
                      RutPaintContext *paint_ctx)
{
  RutDropDown *drop = RUT_DROP_DOWN (object);

  switch (paint_ctx->layer_number)
    {
    case 0:
      rut_drop_down_paint_button (drop, paint_ctx);

      /* If the selector is visible then we'll queue it to be painted
       * in the next layer so that it won't appear under the
       * subsequent controls */
      if (drop->selector_shown)
        rut_paint_context_queue_paint (paint_ctx, object);
      break;

    case 1:
      rut_drop_down_paint_selector (drop, paint_ctx);
      break;
    }
}

static int
rut_drop_down_find_value_at_position (RutDropDown *drop,
                                      float x,
                                      float y)
{
  int i, y_pos = drop->selector_y + 3;

  if (x < drop->selector_x || x >= drop->selector_x + drop->selector_width)
    return -1;

  for (i = 0; i < drop->n_values; i++)
    {
      const RutDropDownLayout *layout = drop->layouts + i;

      if (y >= y_pos && y < y_pos + layout->logical_rect.height)
        return i;

      y_pos += layout->logical_rect.height;
    }

  return -1;
}

static RutInputEventStatus
rut_drop_down_selector_grab_cb (RutInputEvent *event,
                                void *user_data)
{
  RutDropDown *drop = user_data;

  switch (rut_input_event_get_type (event))
    {
    case RUT_INPUT_EVENT_TYPE_MOTION:
      {
        float x, y;
        int selector_value;

        if (rut_motion_event_unproject (event,
                                        drop,
                                        &x, &y))
          selector_value = rut_drop_down_find_value_at_position (drop, x, y);
        else
          selector_value = -1;

        if (selector_value != drop->selector_value)
          {
            drop->selector_value = selector_value;
            rut_shell_queue_redraw (drop->context->shell);
          }

        /* If this is a click then commit the chosen value */
        if (rut_motion_event_get_action (event) ==
            RUT_MOTION_EVENT_ACTION_DOWN)
        {
          rut_drop_down_hide_selector (drop);

          if (selector_value != -1)
            rut_drop_down_set_value (drop, selector_value);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
      }
      break;

    case RUT_INPUT_EVENT_TYPE_KEY:
      /* The escape key cancels the selector */
      if (rut_key_event_get_action (event) == RUT_KEY_EVENT_ACTION_DOWN &&
          rut_key_event_get_keysym (event) == RUT_KEY_Escape)
        rut_drop_down_hide_selector (drop);
      break;

    default:
      break;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
rut_drop_down_handle_click (RutDropDown *drop,
                            RutInputEvent *event)
{
  CoglMatrix modelview;
  const CoglMatrix *projection;
  RutCamera *camera = rut_input_event_get_camera (event);
  float top_point[4];
  int i;

  drop->selector_width = MAX (drop->width - 6, 0);
  drop->selector_height = 0;

  /* Calculate the size of the selector */
  for (i = 0; i < drop->n_values; i++)
    {
      RutDropDownLayout *layout = drop->layouts + i;

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
  rut_graphable_get_modelview (RUT_OBJECT (drop), camera, &modelview);
  projection = rut_camera_get_projection (camera);
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

  rut_shell_grab_input (drop->context->shell,
                        rut_input_event_get_camera (event),
                        rut_drop_down_selector_grab_cb,
                        drop);

  drop->selector_shown = TRUE;
  drop->selector_value = -1;

  rut_shell_queue_redraw (drop->context->shell);
}

static RutInputEventStatus
rut_drop_down_input_cb (RutInputEvent *event,
                        void *user_data)
{
  RutDropDown *drop = user_data;
  CoglBool highlighted;
  float x, y;

  if (rut_input_event_get_type (event) != RUT_INPUT_EVENT_TYPE_MOTION)
    return RUT_INPUT_EVENT_STATUS_UNHANDLED;

  x = rut_motion_event_get_x (event);
  y = rut_motion_event_get_y (event);

  if ((rut_motion_event_get_button_state (event) & RUT_BUTTON_STATE_1) == 0)
    {
      drop->button_down = FALSE;
      rut_shell_ungrab_input (drop->context->shell,
                              rut_drop_down_input_cb,
                              user_data);

      /* If we the pointer is still over the widget then treat it as a
       * click */
      if (drop->highlighted)
        rut_drop_down_handle_click (drop, event);

      highlighted = FALSE;
    }
  else
    {
      RutCamera *camera = rut_input_event_get_camera (event);

      highlighted = rut_camera_pick_inputable (camera,
                                               drop->input_region,
                                               x, y);
    }

  if (highlighted != drop->highlighted)
    {
      drop->highlighted = highlighted;
      rut_shell_queue_redraw (drop->context->shell);
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static RutInputEventStatus
rut_drop_down_input_region_cb (RutInputRegion *region,
                               RutInputEvent *event,
                               void *user_data)
{
  RutDropDown *drop = user_data;
  RutCamera *camera;

  if (!drop->button_down &&
      !drop->selector_shown &&
      rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION &&
      rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN &&
      (rut_motion_event_get_button_state (event) & RUT_BUTTON_STATE_1) &&
      (camera = rut_input_event_get_camera (event)))
    {
      drop->button_down = TRUE;
      drop->highlighted = TRUE;

      rut_shell_grab_input (drop->context->shell,
                            camera,
                            rut_drop_down_input_cb,
                            drop);

      rut_shell_queue_redraw (drop->context->shell);

      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
rut_drop_down_hide_selector (RutDropDown *drop)
{
  if (drop->selector_shown)
    {
      cogl_object_unref (drop->selector_outline_path);
      drop->selector_shown = FALSE;
      rut_shell_queue_redraw (drop->context->shell);

      rut_shell_ungrab_input (drop->context->shell,
                              rut_drop_down_selector_grab_cb,
                              drop);
    }
}

static void
rut_drop_down_set_size (RutObject *object,
                        float width,
                        float height)
{
  RutDropDown *drop = RUT_DROP_DOWN (object);

  rut_shell_queue_redraw (drop->context->shell);
  drop->width = width;
  drop->height = height;
  rut_input_region_set_rectangle (drop->input_region,
                                  0.0f, 0.0f, /* x0 / y0 */
                                  drop->width, drop->height /* x1 / y1 */);
}

static void
rut_drop_down_get_size (RutObject *object,
                        float *width,
                        float *height)
{
  RutDropDown *drop = RUT_DROP_DOWN (object);

  *width = drop->width;
  *height = drop->height;
}

static void
rut_drop_down_get_preferred_width (RutObject *object,
                                   float for_height,
                                   float *min_width_p,
                                   float *natural_width_p)
{
  RutDropDown *drop = RUT_DROP_DOWN (object);
  int max_width = 0;
  int i;

  rut_drop_down_ensure_layouts (drop);

  /* Get the narrowest layout */
  for (i = 0; i < drop->n_values; i++)
    {
      RutDropDownLayout *layout = drop->layouts + i;

      if (layout->logical_rect.width > max_width)
        max_width = layout->logical_rect.width;
    }

  /* Add space for the edges */
  max_width += RUT_DROP_DOWN_EDGE_WIDTH * 2;

  if (min_width_p)
    *min_width_p = max_width;
  if (natural_width_p)
    /* Leave two pixels either side of the label */
    *natural_width_p = max_width + 4;
}

static void
rut_drop_down_get_preferred_height (RutObject *object,
                                    float for_width,
                                    float *min_height_p,
                                    float *natural_height_p)
{
  RutDropDown *drop = RUT_DROP_DOWN (object);
  int max_height = G_MININT;
  int i;

  rut_drop_down_ensure_layouts (drop);

  /* Get the tallest layout */
  for (i = 0; i < drop->n_values; i++)
    {
      RutDropDownLayout *layout = drop->layouts + i;

      if (layout->logical_rect.height > max_height)
        max_height = layout->logical_rect.height;
    }

  if (min_height_p)
    *min_height_p = MAX (max_height, RUT_DROP_DOWN_EDGE_HEIGHT);
  if (natural_height_p)
    *natural_height_p = MAX (max_height + 4,
                             RUT_DROP_DOWN_EDGE_HEIGHT);
}

static RutGraphableVTable _rut_drop_down_graphable_vtable = {
  NULL, /* child removed */
  NULL, /* child addded */
  NULL /* parent changed */
};

static RutPaintableVTable _rut_drop_down_paintable_vtable = {
  _rut_drop_down_paint
};

static RutIntrospectableVTable _rut_drop_down_introspectable_vtable = {
  rut_simple_introspectable_lookup_property,
  rut_simple_introspectable_foreach_property
};

static RutSizableVTable _rut_drop_down_sizable_vtable = {
  rut_drop_down_set_size,
  rut_drop_down_get_size,
  rut_drop_down_get_preferred_width,
  rut_drop_down_get_preferred_height,
  NULL /* add_preferred_size_callback */
};

static void
_rut_drop_down_init_type (void)
{
  rut_type_init (&rut_drop_down_type, "RigDropDown");
  rut_type_add_interface (&rut_drop_down_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutDropDown, ref_count),
                          &_rut_drop_down_refable_vtable);
  rut_type_add_interface (&rut_drop_down_type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (RutDropDown, graphable),
                          &_rut_drop_down_graphable_vtable);
  rut_type_add_interface (&rut_drop_down_type,
                          RUT_INTERFACE_ID_PAINTABLE,
                          offsetof (RutDropDown, paintable),
                          &_rut_drop_down_paintable_vtable);
  rut_type_add_interface (&rut_drop_down_type,
                          RUT_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rut_drop_down_introspectable_vtable);
  rut_type_add_interface (&rut_drop_down_type,
                          RUT_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RutDropDown, introspectable),
                          NULL); /* no implied vtable */
  rut_type_add_interface (&rut_drop_down_type,
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &_rut_drop_down_sizable_vtable);
}

RutDropDown *
rut_drop_down_new (RutContext *context)
{
  RutDropDown *drop = g_slice_new0 (RutDropDown);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rut_init ();
      _rut_drop_down_init_type ();

      initialized = TRUE;
    }

  drop->ref_count = 1;
  drop->context = rut_refable_ref (context);

  /* Set a dummy value so we can assume that value_index is always a
   * valid index */
  drop->values = g_new (RutDropDownValue, 1);
  drop->values->name = g_strdup ("");
  drop->values->value = 0;

  drop->font_description = rut_drop_down_create_font_description ();

  rut_object_init (&drop->_parent, &rut_drop_down_type);

  rut_paintable_init (RUT_OBJECT (drop));
  rut_graphable_init (RUT_OBJECT (drop));

  rut_simple_introspectable_init (drop,
                                  _rut_drop_down_prop_specs,
                                  drop->properties);

  drop->bg_pipeline = rut_drop_down_create_bg_pipeline (context);
  drop->highlighted_bg_pipeline =
    rut_drop_down_create_highlighted_bg_pipeline (context);

  drop->input_region =
    rut_input_region_new_rectangle (0, 0, 0, 0,
                                    rut_drop_down_input_region_cb,
                                    drop);
  rut_graphable_add_child (drop, drop->input_region);

  rut_sizable_set_size (drop, 60, 30);

  return drop;
}

void
rut_drop_down_set_value (RutObject *obj,
                         int value)
{
  RutDropDown *drop = RUT_DROP_DOWN (obj);

  int i;

  if (value == drop->values[drop->value_index].value)
    return;

  for (i = 0; i < drop->n_values; i++)
    if (drop->values[i].value == value)
      {
        drop->value_index = i;

        rut_property_dirty (&drop->context->property_ctx,
                            &drop->properties[RUT_DROP_DOWN_PROP_VALUE]);

        rut_shell_queue_redraw (drop->context->shell);
        return;
      }

  g_warn_if_reached ();
}

int
rut_drop_down_get_value (RutObject *obj)
{
  RutDropDown *drop = RUT_DROP_DOWN (obj);

  return drop->values[drop->value_index].value;
}

void
rut_drop_down_set_values (RutDropDown *drop,
                          ...)
{
  va_list ap1, ap2;
  const char *name;
  int i = 0, n_values = 0;
  RutDropDownValue *values;

  va_start (ap1, drop);
  G_VA_COPY (ap2, ap1);

  while ((name = va_arg (ap1, const char *)))
    {
      /* Skip the value */
      va_arg (ap1, int);
      n_values++;
    }

  values = g_alloca (sizeof (RutDropDownValue) * n_values);

  while ((name = va_arg (ap2, const char *)))
    {
      values[i].name = name;
      values[i].value = va_arg (ap2, int);
      i++;
    }

  va_end (ap1);

  rut_drop_down_set_values_array (drop, values, n_values);
}

void
rut_drop_down_set_values_array (RutDropDown *drop,
                                const RutDropDownValue *values,
                                int n_values)
{
  int old_value;
  int old_value_index = 0;
  int i;

  g_return_if_fail (n_values >= 0);

  old_value = rut_drop_down_get_value (drop);

  rut_drop_down_free_values (drop);

  drop->values = g_malloc (sizeof (RutDropDownValue) * n_values);
  for (i = 0; i < n_values; i++)
    {
      drop->values[i].name = g_strdup (values[i].name);
      drop->values[i].value = values[i].value;

      if (values[i].value == old_value)
        old_value_index = i;
    }

  drop->n_values = n_values;

  drop->value_index = old_value_index;

  rut_shell_queue_redraw (drop->context->shell);
  rut_drop_down_clear_layouts (drop);
}
