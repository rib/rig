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
#include "rig-number-slider.h"
#include "rig-text.h"

#define RIG_NUMBER_SLIDER_CORNER_HEIGHT 3
#define RIG_NUMBER_SLIDER_ARROW_WIDTH 8
#define RIG_NUMBER_SLIDER_ARROW_HEIGHT \
  (16 - RIG_NUMBER_SLIDER_CORNER_HEIGHT * 2)
/* The offset to the top of the arrow as a texture coordinate */
#define RIG_NUMBER_SLIDER_CORNER_SIZE           \
  (RIG_NUMBER_SLIDER_CORNER_HEIGHT /            \
   (RIG_NUMBER_SLIDER_ARROW_HEIGHT +            \
    RIG_NUMBER_SLIDER_CORNER_HEIGHT * 2.0f))

#define RIG_NUMBER_SLIDER_FONT_SIZE 10

enum {
  RIG_NUMBER_SLIDER_PROP_VALUE,
  RIG_NUMBER_SLIDER_N_PROPS
};

struct _RigNumberSlider
{
  RigObjectProps _parent;

  RigContext *context;

  RigGraphableProps graphable;
  RigPaintableProps paintable;

  CoglPipeline *bg_pipeline;
  CoglPipeline *selected_bg_pipeline;

  char *name;

  int width, height;

  int decimal_places;

  int ref_count;

  float min_value, max_value;
  float value;
  float step;

  PangoFontDescription *font_description;

  PangoLayout *actual_layout;
  PangoRectangle actual_logical_rect;
  PangoRectangle actual_ink_rect;

  PangoLayout *long_layout;
  PangoRectangle long_logical_rect;
  PangoRectangle long_ink_rect;

  RigInputRegion *input_region;

  RigSimpleIntrospectableProps introspectable;
  RigProperty properties[RIG_NUMBER_SLIDER_N_PROPS];

  /* This is set to true after we get a motion event with the down
   * action regardless of where was clicked */
  CoglBool button_down;
  /* This is set to TRUE if cursor has moved more than a couple of
   * pixels since the button was pressed. Once this happens the press
   * is no longer considered a click but is instead interpreted as a
   * drag to change the value */
  CoglBool button_drag;
  /* Where within the widget the cursor was when the button was
   * originally pressed */
  float button_x, button_y;
  /* The original value when the button was pressed */
  float button_value;

  /* The text control that will be displayed if the user directly
   * clicks on the button. This will be NULL while it is not displayed
   * and it will be immediately destroyed once the editing has
   * finished */
  RigText *text;
  /* The transform for the text. This has the same lifetime as the
   * text control */
  RigTransform *text_transform;
};

/* Some of the pipelines are cached and attached to the CoglContext so
 * that multiple sliders created using the same CoglContext will use
 * the same pipelines */
typedef struct
{
  CoglPipeline *bg_pipeline;
  CoglPipeline *selected_bg_pipeline;
} RigNumberSliderContextData;

RigType rig_number_slider_type;

static void
rig_number_slider_remove_text (RigNumberSlider *slider);

static RigPropertySpec
_rig_number_slider_prop_specs[] =
  {
    {
      .name = "value",
      .type = RIG_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof (RigNumberSlider, value),
      .setter = rig_number_slider_set_value
    },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
  };

static RigNumberSliderContextData *
rig_number_slider_get_context_data (RigContext *context)
{
  static CoglUserDataKey context_data_key;
  RigNumberSliderContextData *context_data =
    cogl_object_get_user_data (COGL_OBJECT (context->cogl_context),
                               &context_data_key);

  if (context_data == NULL)
    {
      context_data = g_new0 (RigNumberSliderContextData, 1);
      cogl_object_set_user_data (COGL_OBJECT (context->cogl_context),
                                 &context_data_key,
                                 context_data,
                                 g_free);
    }

  return context_data;
}

static CoglPipeline *
rig_number_slider_create_bg_pipeline (RigContext *context)
{
  RigNumberSliderContextData *context_data =
    rig_number_slider_get_context_data (context);

  /* The pipeline is cached so that if multiple sliders are created
   * they will share a reference to the same pipeline */
  if (context_data->bg_pipeline)
    return cogl_object_ref (context_data->bg_pipeline);
  else
    {
      CoglPipeline *pipeline = cogl_pipeline_new (context->cogl_context);
      static CoglUserDataKey bg_pipeline_destroy_key;
      CoglTexture *bg_texture;
      GError *error = NULL;

      bg_texture = rig_load_texture (context,
                                     RIG_DATA_DIR
                                     "number-slider-background.png",
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
          g_warning ("Failed to load number-slider-background.png: %s",
                     error->message);
          g_clear_error (&error);
        }

      /* When the last slider is destroyed the pipeline will be
       * destroyed and we'll set context->bg_pipeline to NULL
       * so that it will be recreated for the next slider */
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
rig_number_slider_create_selected_bg_pipeline (RigContext *context)
{
  RigNumberSliderContextData *context_data =
    rig_number_slider_get_context_data (context);

  /* The pipeline is cached so that if multiple sliders are created
   * they will share a reference to the same pipeline */
  if (context_data->selected_bg_pipeline)
    return cogl_object_ref (context_data->selected_bg_pipeline);
  else
    {
      CoglPipeline *bg_pipeline =
        rig_number_slider_create_bg_pipeline (context);
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

      /* When the last slider is destroyed the pipeline will be
       * destroyed and we'll set context->selected_bg_pipeline to NULL
       * so that it will be recreated for the next slider */
      cogl_object_set_user_data (COGL_OBJECT (pipeline),
                                 &pipeline_destroy_key,
                                 &context_data->selected_bg_pipeline,
                                 (CoglUserDataDestroyCallback)
                                 g_nullify_pointer);

      context_data->selected_bg_pipeline = pipeline;

      return pipeline;
    }
}

static void
rig_number_slider_clear_layout (RigNumberSlider *slider)
{
  if (slider->actual_layout)
    {
      g_object_unref (slider->actual_layout);
      slider->actual_layout = NULL;
    }

  if (slider->long_layout)
    {
      g_object_unref (slider->long_layout);
      slider->long_layout = NULL;
    }
}

static void
rig_number_slider_commit_text (RigNumberSlider *slider)
{
  if (slider->text)
    {
      const char *text = rig_text_get_text (slider->text);
      double value;

      errno = 0;
      value = strtod (text, NULL);

      if (errno == 0)
        rig_number_slider_set_value (slider, value);

      rig_number_slider_remove_text (slider);
    }
}

static void
_rig_number_slider_free (void *object)
{
  RigNumberSlider *slider = object;

  rig_number_slider_remove_text (slider);

  rig_ref_countable_unref (slider->context);
  cogl_object_unref (slider->bg_pipeline);
  cogl_object_unref (slider->selected_bg_pipeline);

  g_free (slider->name);

  rig_number_slider_clear_layout (slider);

  rig_graphable_remove_child (slider->input_region);
  rig_ref_countable_unref (slider->input_region);

  rig_simple_introspectable_destroy (slider);

  pango_font_description_free (slider->font_description);

  g_slice_free (RigNumberSlider, slider);
}

RigRefCountableVTable _rig_number_slider_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_number_slider_free
};

typedef struct
{
  float x1, y1, x2, y2;
  float s1, t1, s2, t2;
} RigNumberSliderRectangle;

static PangoFontDescription *
rig_number_slider_create_font_description (void)
{
  PangoFontDescription *font_description = pango_font_description_new ();

  pango_font_description_set_family (font_description, "Sans");
  pango_font_description_set_absolute_size (font_description,
                                            RIG_NUMBER_SLIDER_FONT_SIZE *
                                            PANGO_SCALE);

  return font_description;
}

static void
rig_number_slider_setup_layout (RigNumberSlider *slider,
                                PangoLayout *layout)
{
  pango_layout_set_font_description (layout, slider->font_description);
}

static PangoLayout *
rig_number_slider_ensure_actual_layout (RigNumberSlider *slider)
{
  if (slider->actual_layout == NULL)
    {
      PangoLayout *layout;
      char *text;

      layout = pango_layout_new (slider->context->pango_context);

      text = g_strdup_printf ("%s: %.*f",
                              slider->name ? slider->name : "",
                              slider->decimal_places,
                              slider->value);
      pango_layout_set_text (layout, text, -1);

      g_free (text);

      rig_number_slider_setup_layout (slider, layout);

      pango_layout_get_pixel_extents (layout,
                                      &slider->actual_ink_rect,
                                      &slider->actual_logical_rect);

      slider->actual_layout = layout;
    }

  return slider->actual_layout;
}

static PangoLayout *
rig_number_slider_ensure_long_layout (RigNumberSlider *slider)
{
  if (slider->long_layout == NULL)
    {
      PangoLayout *layout;
      char *text;
      float max_value;

      layout = pango_layout_new (slider->context->pango_context);

      /* Use whichever value is likely to have a longer string
       * representation */
      if (fabsf (slider->min_value) > fabsf (slider->max_value))
        max_value = slider->min_value;
      else
        max_value = slider->max_value;

      /* If either of the values are G_MAXFLOAT then there isn't
       * really a range so using the maximum values will make a
       * preferred size that is way too long. Instead we'll just pick
       * a reasonably long number */
      if (max_value >= G_MAXFLOAT)
        max_value = 1000000.0f;

      /* Add a load of decimal places */
      if (max_value < 0)
        max_value = floorf (max_value) - 0.0001f;
      else
        max_value = ceilf (max_value) + 0.9999f;

      text = g_strdup_printf ("%s: %.*f",
                              slider->name ? slider->name : "",
                              slider->decimal_places,
                              max_value);
      pango_layout_set_text (layout, text, -1);

      g_free (text);

      rig_number_slider_setup_layout (slider, layout);

      pango_layout_get_pixel_extents (layout,
                                      &slider->long_ink_rect,
                                      &slider->long_logical_rect);

      slider->long_layout = layout;
    }

  return slider->long_layout;
}

static void
_rig_number_slider_paint (RigObject *object,
                          RigPaintContext *paint_ctx)
{
  RigNumberSlider *slider = (RigNumberSlider *) object;
  RigCamera *camera = paint_ctx->camera;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (camera);
  RigNumberSliderRectangle coords[11];
  int translation = slider->width - RIG_NUMBER_SLIDER_ARROW_WIDTH;
  CoglColor font_color;
  CoglPipeline *pipeline;
  int i;

  /* Top left rounded corner */
  coords[0].x1 = 0.0f;
  coords[0].y1 = 0.0f;
  coords[0].x2 = RIG_NUMBER_SLIDER_ARROW_WIDTH;
  coords[0].y2 = RIG_NUMBER_SLIDER_CORNER_HEIGHT;
  coords[0].s1 = 0.0f;
  coords[0].t1 = 0.0f;
  coords[0].s2 = 1.0f;
  coords[0].t2 = RIG_NUMBER_SLIDER_CORNER_SIZE;

  /* Stretched gap to top of arrow */
  coords[1].x1 = 0.0f;
  coords[1].y1 = RIG_NUMBER_SLIDER_CORNER_HEIGHT;
  coords[1].x2 = RIG_NUMBER_SLIDER_ARROW_WIDTH;
  coords[1].y2 = slider->height / 2 - RIG_NUMBER_SLIDER_ARROW_HEIGHT / 2;
  /* Stetch the rightmost centre pixel to cover the entire rectangle */
  coords[1].s1 = 1.0f;
  coords[1].t1 = 0.5f;
  coords[1].s2 = 1.0f;
  coords[1].t2 = 0.5f;

  /* Centre arrow */
  coords[2].x1 = 0.0f;
  coords[2].y1 = coords[1].y2;
  coords[2].x2 = RIG_NUMBER_SLIDER_ARROW_WIDTH;
  coords[2].y2 = coords[2].y1 + RIG_NUMBER_SLIDER_ARROW_HEIGHT;
  coords[2].s1 = 0.0f;
  coords[2].t1 = RIG_NUMBER_SLIDER_CORNER_SIZE;
  coords[2].s2 = 1.0f;
  coords[2].t2 = 1.0f - RIG_NUMBER_SLIDER_CORNER_SIZE;

  /* Stretched gap to top of rounded corner */
  coords[3].x1 = 0.0f;
  coords[3].y1 = coords[2].y2;
  coords[3].x2 = RIG_NUMBER_SLIDER_ARROW_WIDTH;
  coords[3].y2 = slider->height - RIG_NUMBER_SLIDER_CORNER_HEIGHT;
  /* Stetch the rightmost centre pixel to cover the entire rectangle */
  coords[3].s1 = 1.0f;
  coords[3].t1 = 0.5f;
  coords[3].s2 = 1.0f;
  coords[3].t2 = 0.5f;

  /* Bottom rounded corner */
  coords[4].x1 = 0.0f;
  coords[4].y1 = coords[3].y2;
  coords[4].x2 = RIG_NUMBER_SLIDER_ARROW_WIDTH;
  coords[4].y2 = slider->height;
  coords[4].s1 = 0.0f;
  coords[4].t1 = 1.0f - RIG_NUMBER_SLIDER_CORNER_SIZE;
  coords[4].s2 = 1.0f;
  coords[4].t2 = 1.0f;

  /* Centre rectangle */
  coords[5].x1 = RIG_NUMBER_SLIDER_ARROW_WIDTH;
  coords[5].y1 = 0.0f;
  coords[5].x2 = slider->width - RIG_NUMBER_SLIDER_ARROW_WIDTH;
  coords[5].y2 = slider->height;
  /* Stetch the rightmost centre pixel to cover the entire rectangle */
  coords[5].s1 = 1.0f;
  coords[5].t1 = 0.5f;
  coords[5].s2 = 1.0f;
  coords[5].t2 = 0.5f;

  /* The right hand side rectangles are just translated copies of the
   * left hand side rectangles with flipped texture coordinates */
  for (i = 0; i < 5; i++)
    {
      RigNumberSliderRectangle *out = coords + i + 6;
      const RigNumberSliderRectangle *in = coords + i;

      out->x1 = in->x1 + translation;
      out->y1 = in->y1;
      out->x2 = in->x2 + translation;
      out->y2 = in->y2;
      out->s1 = in->s2;
      out->t1 = in->t1;
      out->s2 = in->s1;
      out->t2 = in->t2;
    }

  if (slider->button_down)
    pipeline = slider->selected_bg_pipeline;
  else
    pipeline = slider->bg_pipeline;

  cogl_framebuffer_draw_textured_rectangles (fb,
                                             pipeline,
                                             (float *) coords,
                                             G_N_ELEMENTS (coords));

  if (slider->text == NULL)
    {
      rig_number_slider_ensure_actual_layout (slider);

      cogl_color_init_from_4ub (&font_color, 0, 0, 0, 255);
      cogl_pango_show_layout (fb,
                              slider->actual_layout,
                              slider->width / 2 -
                              slider->actual_logical_rect.width / 2,
                              slider->height / 2 -
                              slider->actual_logical_rect.height / 2,
                              &font_color);
    }
}

static void
rig_number_slider_update_text_size (RigNumberSlider *slider)
{
  rig_number_slider_ensure_actual_layout (slider);

  rig_transform_init_identity (slider->text_transform);
  rig_transform_translate (slider->text_transform,
                           RIG_NUMBER_SLIDER_ARROW_WIDTH,
                           slider->height / 2 -
                           slider->actual_logical_rect.height / 2,
                           0.0f);

  rig_sizable_set_size (slider->text,
                        slider->width - RIG_NUMBER_SLIDER_ARROW_WIDTH * 2,
                        slider->actual_logical_rect.height);
}

static RigInputEventStatus
rig_number_slider_text_grab_cb (RigInputEvent *event,
                                void *user_data)
{
  RigNumberSlider *slider = user_data;
  float x, y;

  switch (rig_input_event_get_type (event))
    {
    case RIG_INPUT_EVENT_TYPE_MOTION:
      /* Check if this is a click outside of the text control */
      if (rig_motion_event_get_action (event) == RIG_MOTION_EVENT_ACTION_DOWN &&
          (!rig_motion_event_unproject (event,
                                        RIG_OBJECT (slider),
                                        &x, &y) ||
           x < RIG_NUMBER_SLIDER_ARROW_WIDTH ||
           x >= slider->width - RIG_NUMBER_SLIDER_ARROW_WIDTH ||
           y < 0 ||
           y >= slider->height))
        {
          rig_number_slider_commit_text (slider);
          return RIG_INPUT_EVENT_STATUS_HANDLED;
        }
      break;

    case RIG_INPUT_EVENT_TYPE_KEY:
      /* The escape key cancels the text control */
      if (rig_key_event_get_action (event) == RIG_KEY_EVENT_ACTION_DOWN &&
          rig_key_event_get_keysym (event) == RIG_KEY_Escape)
        rig_number_slider_remove_text (slider);
      break;

    default:
      break;
    }

  return RIG_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
rig_number_slider_text_activate_cb (RigText *text,
                                    void *user_data)
{
  RigNumberSlider *slider = user_data;

  rig_number_slider_commit_text (slider);
}

static void
rig_number_slider_handle_click (RigNumberSlider *slider,
                                RigInputEvent *event)
{
  float x, y;

  if (!rig_motion_event_unproject (event,
                                   RIG_OBJECT (slider),
                                   &x, &y))
    return;

  if (x < RIG_NUMBER_SLIDER_ARROW_WIDTH)
    rig_number_slider_set_value (slider, slider->button_value - slider->step);
  else if (x >= slider->width - RIG_NUMBER_SLIDER_ARROW_WIDTH)
    rig_number_slider_set_value (slider, slider->button_value + slider->step);
  else
    {
      int len;
      char *text;

      slider->text_transform = rig_transform_new (slider->context, NULL);
      rig_graphable_add_child (slider, slider->text_transform);

      slider->text = rig_text_new (slider->context);
      rig_text_set_font_description (slider->text, slider->font_description);
      rig_text_set_editable (slider->text, TRUE);
      rig_text_set_activatable (slider->text, TRUE);
      rig_text_add_activate_callback (slider->text,
                                      rig_number_slider_text_activate_cb,
                                      slider,
                                      NULL /* destroy_cb */);

      text = g_strdup_printf ("%.*f",
                              slider->decimal_places,
                              slider->value);
      rig_text_set_text (slider->text, text);
      len = strlen (text);
      g_free (text);

      rig_text_set_cursor_position (slider->text, 0);
      rig_text_set_selection_bound (slider->text, len);

      rig_text_grab_key_focus (slider->text);

      rig_graphable_add_child (slider->text_transform, slider->text);

      rig_number_slider_update_text_size (slider);

      rig_shell_grab_input (slider->context->shell,
                            rig_input_event_get_camera (event),
                            rig_number_slider_text_grab_cb,
                            slider);

      rig_shell_queue_redraw (slider->context->shell);
    }
}

static void
rig_number_slider_remove_text (RigNumberSlider *slider)
{
  if (slider->text)
    {
      rig_graphable_remove_child (slider->text);
      rig_ref_countable_unref (slider->text);

      rig_graphable_remove_child (slider->text_transform);
      rig_ref_countable_unref (slider->text_transform);

      rig_shell_ungrab_input (slider->context->shell,
                              rig_number_slider_text_grab_cb,
                              slider);

      slider->text = NULL;
    }
}

static RigInputEventStatus
rig_number_slider_input_cb (RigInputEvent *event,
                            void *user_data)
{
  RigNumberSlider *slider = user_data;
  float x, y;

  if (rig_input_event_get_type (event) != RIG_INPUT_EVENT_TYPE_MOTION)
    return RIG_INPUT_EVENT_STATUS_UNHANDLED;

  x = rig_motion_event_get_x (event);
  y = rig_motion_event_get_y (event);

  /* If the cursor has at least a pixel since it was clicked then will
   * mark the button as a drag event so that we don't intepret it as a
   * click when the button is released */
  if (fabsf (x - slider->button_x) >= 1.0f ||
      fabsf (y - slider->button_y) >= 1.0f)
    slider->button_drag = TRUE;

  /* Update the value based on the position if we're in a drag */
  if (slider->button_drag)
    rig_number_slider_set_value (slider,
                                 slider->button_value +
                                 (x - slider->button_x) * slider->step);

  if ((rig_motion_event_get_button_state (event) & RIG_BUTTON_STATE_1) == 0)
    {
      slider->button_down = FALSE;
      rig_shell_ungrab_input (slider->context->shell,
                              rig_number_slider_input_cb,
                              user_data);

      /* If we weren't dragging then this must have been an attempt to
       * click somewhere on the widget */
      if (!slider->button_drag)
        rig_number_slider_handle_click (slider, event);

      rig_shell_queue_redraw (slider->context->shell);
    }

  return RIG_INPUT_EVENT_STATUS_HANDLED;
}

static RigInputEventStatus
rig_number_slider_input_region_cb (RigInputRegion *region,
                                   RigInputEvent *event,
                                   void *user_data)
{
  RigNumberSlider *slider = user_data;
  RigCamera *camera;

  if (slider->text == NULL &&
      !slider->button_down &&
      rig_input_event_get_type (event) == RIG_INPUT_EVENT_TYPE_MOTION &&
      rig_motion_event_get_action (event) == RIG_MOTION_EVENT_ACTION_DOWN &&
      (rig_motion_event_get_button_state (event) & RIG_BUTTON_STATE_1) &&
      (camera = rig_input_event_get_camera (event)))
    {
      slider->button_down = TRUE;
      slider->button_drag = FALSE;
      slider->button_value = slider->value;
      slider->button_x = rig_motion_event_get_x (event);
      slider->button_y = rig_motion_event_get_y (event);

      rig_shell_grab_input (slider->context->shell,
                            camera,
                            rig_number_slider_input_cb,
                            slider);

      rig_shell_queue_redraw (slider->context->shell);

      return RIG_INPUT_EVENT_STATUS_HANDLED;
    }

  return RIG_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
rig_number_slider_set_size (RigObject *object,
                            float width,
                            float height)
{
  RigNumberSlider *slider = RIG_NUMBER_SLIDER (object);

  rig_shell_queue_redraw (slider->context->shell);
  slider->width = width;
  slider->height = height;
  rig_input_region_set_rectangle (slider->input_region,
                                  0.0f, 0.0f, /* x0 / y0 */
                                  slider->width, slider->height /* x1 / y1 */);

  if (slider->text)
    rig_number_slider_update_text_size (slider);
}

static void
rig_number_slider_get_size (RigObject *object,
                            float *width,
                            float *height)
{
  RigNumberSlider *slider = RIG_NUMBER_SLIDER (object);

  *width = slider->width;
  *height = slider->height;
}

static void
rig_number_slider_get_preferred_width (RigObject *object,
                                       float for_height,
                                       float *min_width_p,
                                       float *natural_width_p)
{
  RigNumberSlider *slider = RIG_NUMBER_SLIDER (object);
  float min_width;
  int layout_width;

  rig_number_slider_ensure_actual_layout (slider);
  rig_number_slider_ensure_long_layout (slider);

  layout_width = MAX (slider->actual_logical_rect.width,
                      slider->long_logical_rect.width);

  min_width = layout_width + RIG_NUMBER_SLIDER_ARROW_WIDTH * 2;

  if (min_width_p)
    *min_width_p = min_width;
  if (natural_width_p)
    /* Leave two pixels either side of the label */
    *natural_width_p = min_width + 4;
}

static void
rig_number_slider_get_preferred_height (RigObject *object,
                                        float for_width,
                                        float *min_height_p,
                                        float *natural_height_p)
{
  RigNumberSlider *slider = RIG_NUMBER_SLIDER (object);
  float layout_height;

  rig_number_slider_ensure_actual_layout (slider);
  rig_number_slider_ensure_long_layout (slider);

  layout_height = MAX (slider->actual_logical_rect.height,
                       slider->long_logical_rect.height);

  if (min_height_p)
    *min_height_p = MAX (layout_height, RIG_NUMBER_SLIDER_ARROW_HEIGHT);
  if (natural_height_p)
    *natural_height_p = MAX (layout_height + 4,
                             RIG_NUMBER_SLIDER_ARROW_HEIGHT);
}

static RigGraphableVTable _rig_number_slider_graphable_vtable = {
  NULL, /* child removed */
  NULL, /* child addded */
  NULL /* parent changed */
};

static RigPaintableVTable _rig_number_slider_paintable_vtable = {
  _rig_number_slider_paint
};

static RigIntrospectableVTable _rig_number_slider_introspectable_vtable = {
  rig_simple_introspectable_lookup_property,
  rig_simple_introspectable_foreach_property
};

static RigSizableVTable _rig_number_slider_sizable_vtable = {
  rig_number_slider_set_size,
  rig_number_slider_get_size,
  rig_number_slider_get_preferred_width,
  rig_number_slider_get_preferred_height
};

static void
_rig_number_slider_init_type (void)
{
  rig_type_init (&rig_number_slider_type);
  rig_type_add_interface (&rig_number_slider_type,
                          RIG_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigNumberSlider, ref_count),
                          &_rig_number_slider_ref_countable_vtable);
  rig_type_add_interface (&rig_number_slider_type,
                          RIG_INTERFACE_ID_GRAPHABLE,
                          offsetof (RigNumberSlider, graphable),
                          &_rig_number_slider_graphable_vtable);
  rig_type_add_interface (&rig_number_slider_type,
                          RIG_INTERFACE_ID_PAINTABLE,
                          offsetof (RigNumberSlider, paintable),
                          &_rig_number_slider_paintable_vtable);
  rig_type_add_interface (&rig_number_slider_type,
                          RIG_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rig_number_slider_introspectable_vtable);
  rig_type_add_interface (&rig_number_slider_type,
                          RIG_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RigNumberSlider, introspectable),
                          NULL); /* no implied vtable */
  rig_type_add_interface (&rig_number_slider_type,
                          RIG_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &_rig_number_slider_sizable_vtable);
}

RigNumberSlider *
rig_number_slider_new (RigContext *context)
{
  RigNumberSlider *slider = g_slice_new0 (RigNumberSlider);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rig_init ();
      _rig_number_slider_init_type ();

      initialized = TRUE;
    }

  slider->ref_count = 1;
  slider->context = rig_ref_countable_ref (context);
  slider->step = 1.0f;
  slider->decimal_places = 2;

  slider->font_description = rig_number_slider_create_font_description ();

  rig_object_init (&slider->_parent, &rig_number_slider_type);

  rig_paintable_init (RIG_OBJECT (slider));
  rig_graphable_init (RIG_OBJECT (slider));

  rig_simple_introspectable_init (slider,
                                  _rig_number_slider_prop_specs,
                                  slider->properties);

  slider->bg_pipeline = rig_number_slider_create_bg_pipeline (context);
  slider->selected_bg_pipeline =
    rig_number_slider_create_selected_bg_pipeline (context);

  slider->input_region =
    rig_input_region_new_rectangle (0, 0, 0, 0,
                                    rig_number_slider_input_region_cb,
                                    slider);
  rig_graphable_add_child (slider, slider->input_region);

  rig_sizable_set_size (slider, 60, 30);

  return slider;
}

void
rig_number_slider_set_name (RigNumberSlider *slider,
                            const char *name)
{
  rig_shell_queue_redraw (slider->context->shell);
  g_free (slider->name);
  slider->name = g_strdup (name);
}

void
rig_number_slider_set_min_value (RigNumberSlider *slider,
                                 float min_value)
{
  rig_number_slider_clear_layout (slider);

  slider->min_value = min_value;
}

void
rig_number_slider_set_max_value (RigNumberSlider *slider,
                                 float max_value)
{
  rig_number_slider_clear_layout (slider);

  slider->max_value = max_value;
}

void
rig_number_slider_set_value (RigNumberSlider *slider,
                             float value)
{
  value = CLAMP (value, slider->min_value, slider->max_value);

  if (value == slider->value)
    return;

  slider->value = value;

  rig_property_dirty (&slider->context->property_ctx,
                      &slider->properties[RIG_NUMBER_SLIDER_PROP_VALUE]);

  rig_shell_queue_redraw (slider->context->shell);
  rig_number_slider_clear_layout (slider);
}

float
rig_number_slider_get_value (RigNumberSlider *slider)
{
  return slider->value;
}

void
rig_number_slider_set_step (RigNumberSlider *slider,
                            float step)
{
  slider->step = step;
}

int
rig_number_slider_get_decimal_places (RigNumberSlider *slider)
{
  return slider->decimal_places;
}

void
rig_number_slider_set_decimal_places (RigNumberSlider *slider,
                                      int decimal_places)
{
  rig_shell_queue_redraw (slider->context->shell);
  rig_number_slider_clear_layout (slider);

  slider->decimal_places = decimal_places;
}
