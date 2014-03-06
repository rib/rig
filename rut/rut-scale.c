/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013  Intel Corporation.
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

#include <config.h>

#include <math.h>
#include <glib.h>

#include <cogl/cogl.h>

#include "rut-interfaces.h"
#include "rut-paintable.h"
#include "rut-transform.h"
#include "rut-text.h"
#include "rut-scale.h"
#include "rut-input-region.h"

#include "rut-camera.h"

static void
_rut_scale_set_user_scale (RutObject *object, float factor);

typedef struct _Label
{
  RutTransform *transform;
  RutText *text;
} Label;

static void
_rut_scale_free (void *object)
{
  RutScale *scale = object;
  int i;

  rut_closure_list_disconnect_all (&scale->select_cb_list);

  rut_closure_list_disconnect_all (&scale->preferred_size_cb_list);

  for (i = 0; i < scale->labels->len; i++)
    {
      Label *label = &g_array_index (scale->labels, Label, i);

      rut_graphable_remove_child (label->transform);
      rut_object_unref (label->transform);
    }

  g_array_free (scale->labels, TRUE);

  rut_graphable_remove_child (scale->select_transform);
  rut_object_unref (scale->select_transform);

  rut_graphable_destroy (scale);

  rut_introspectable_destroy (scale);

  cogl_object_unref (scale->pipeline);

  rut_object_free (RutScale, object);
}

static float
get_label_step (RutScale *scale,
                int *precision)
{
  float step;
  float scale_10 = 1;
  int i;

  /* We want the labels to correspond to convenient factors... */

  /* for numbers greater than one... */
  float large_factors[] = { 1, 2, 5, 10, 20, 25, 50, 100 };

  /* for numbers less than one... */
  float small_factors[] = { 0.1, 0.2, 0.25, 0.5, 1 };

  /* We don't want labels any closer than MIN_LABEL_PIXEL_STEP pixels */
#define MIN_LABEL_PIXEL_STEP 100.0

  step = MIN_LABEL_PIXEL_STEP / (scale->default_scale * scale->user_scale);

  if (step >= 1)
    {
      *precision = 0;

      /* normalize step into the range [1, 100] with a
       * power of 10 factor */
      while (step > 100)
        {
          step /= 10;
          scale_10 *= 10;
        }

      for (i = 0; i < G_N_ELEMENTS (large_factors); i++)
        {
          if (large_factors[i] >= step)
            {
              step = large_factors[i];
              break;
            }
        }
    }
  else
    {
      *precision = 1;

      /* normalize step into the range [0.1, 1] with a
       * power of 10 factor */
      while (step < 0.1)
        {
          step *= 10;
          scale_10 /= 10;
          (*precision)++;
        }

      for (i = 0; i < G_N_ELEMENTS (small_factors); i++)
        {
          if (small_factors[i] >= step)
            {
              step = small_factors[i];
              if (step == 1)
                (*precision)--;
              break;
            }
        }
    }

  return step * scale_10;
}

static void
update_labels (RutScale *scale)
{
  float unit_range;
  int precision;
  float step;
  int n_labels;
  float f;
  float first_label;
  float normalized_offset;
  float offset;
  float start_pixel_offset;
  int i;

  if (scale->initial_view)
    {
      float length = MAX (scale->natural_length, scale->length);
      scale->default_scale = scale->width / length;
      scale->pixel_scale = scale->default_scale * scale->user_scale;
      rut_property_dirty (&scale->ctx->property_ctx,
                          &scale->properties[RUT_SCALE_PROP_PIXEL_SCALE]);
    }

  step = get_label_step (scale, &precision);

  unit_range = scale->width / (scale->default_scale * scale->user_scale);

  if (scale->width > MIN_LABEL_PIXEL_STEP)
    n_labels = ceilf (unit_range / step);
  else
    n_labels = 0;

  if (scale->labels->len < n_labels)
    {
      for (i = scale->labels->len; i < n_labels; i++)
        {
          Label label;

          label.transform = rut_transform_new (scale->ctx);

          label.text = rut_text_new (scale->ctx);
          rut_text_set_editable (label.text, false);
          rut_text_set_selectable (label.text, false);
          rut_graphable_add_child (label.transform, label.text);
          rut_object_unref (label.text);

          g_array_append_val (scale->labels, label);
        }
    }

  if (scale->n_visible_labels != n_labels)
    {
      for (i = 0; i < scale->labels->len; i++)
        {
          Label *label = &g_array_index (scale->labels, Label, i);
          if (i < n_labels)
            rut_graphable_add_child (scale, label->transform);
          else
            rut_graphable_remove_child (label->transform);
        }

      scale->n_visible_labels = n_labels;
    }

  if (!n_labels)
    return;

  f = 1.0 / step;
  normalized_offset = scale->start_offset * f;
  first_label = ceilf (normalized_offset) * step;

  if (scale->current_first_label != first_label ||
      scale->current_range != unit_range)
    {
      for (i = 0, offset = first_label;
           i < n_labels;
           i++, offset += step)
        {
          Label *label = &g_array_index (scale->labels, Label, i);
          char *str = g_strdup_printf ("%.*f", precision, offset);
          float width, height;

          rut_text_set_text (label->text, str);

          rut_sizable_get_preferred_width (label->text,
                                           scale->height,
                                           NULL,
                                           &width);
          rut_sizable_get_preferred_height (label->text,
                                            width,
                                            NULL,
                                            &height);

          rut_sizable_set_size (label->text, width, height);

          g_free (str);
        }
    }

  scale->current_first_label = first_label;
  scale->current_range = unit_range;

  start_pixel_offset =
    scale->start_offset * scale->default_scale * scale->user_scale;

  for (i = 0, offset = first_label; i < n_labels; i++, offset += step)
    {
      Label *label = &g_array_index (scale->labels, Label, i);
      float pixel_offset = offset * scale->pixel_scale - start_pixel_offset;

      rut_transform_init_identity (label->transform);
      rut_transform_translate (label->transform, (int)pixel_offset, 0, 0);
    }
}

static void
_rut_scale_paint (RutObject *object,
                  RutPaintContext *paint_ctx)
{
  RutScale *scale = object;
  float to_pixel = scale->pixel_scale;
  //RutObject *camera = paint_ctx->camera;

  switch (paint_ctx->layer_number)
    {
    case 0:
      if (scale->changed)
        {
          update_labels (scale);

          rut_sizable_set_size (scale->bg,
                                //scale->width -5,
                                scale->length * to_pixel,
                                scale->height);

          scale->changed = false;
        }

      rut_paint_context_queue_paint (paint_ctx, object);
      break;

    case 1:
      {
        float x0 = scale->focus_offset * to_pixel -
          scale->start_offset * to_pixel;
        if (x0 >= 0 && x0 < scale->width)
          {
            CoglFramebuffer *fb =
              rut_camera_get_framebuffer (paint_ctx->camera);
            cogl_framebuffer_draw_rectangle (fb,
                                             scale->pipeline,
                                             x0, 0,
                                             x0 + 1, scale->height);
          }
      }
      break;
    }

}

static void
_rut_scale_set_size (RutObject *self,
                     float width,
                     float height)
{
  RutScale *scale = self;

  if (scale->width == width && scale->height == height)
    return;

  scale->width = width;
  scale->height = height;

  rut_input_region_set_rectangle (scale->input_region,
                                  0, 0, scale->width, scale->height);

  scale->changed = true;
}

static void
_rut_scale_get_size (RutObject *self,
                     float *width,
                     float *height)
{
  RutScale *scale = self;

  *width = scale->width;
  *height = scale->height;
}

static void
_rut_scale_get_preferred_height (void *sizable,
                                 float for_width,
                                 float *min_height_p,
                                 float *natural_height_p)
{
  RutScale *scale = sizable;
  Label *label;
  float text_width, text_height = 10;

  if (scale->labels->len == 0)
    update_labels (scale);

  label = &g_array_index (scale->labels, Label, 0);

  if (label)
    rut_sizable_get_size (label->text, &text_width, &text_height);

  if (min_height_p)
    *min_height_p = text_height;
  if (natural_height_p)
    *natural_height_p = text_height;
}

static RutClosure *
_rut_scale_add_preferred_size_callback (void *object,
                                        RutSizablePreferredSizeCallback cb,
                                        void *user_data,
                                        RutClosureDestroyCallback destroy)
{
  RutScale *scale = object;

  return rut_closure_list_add (&scale->preferred_size_cb_list,
                               cb,
                               user_data,
                               destroy);
}


RutType rut_scale_type;

static void
_rut_scale_init_type (void)
{

  static RutGraphableVTable graphable_vtable = {
      NULL, /* child remove */
      NULL, /* child add */
      NULL /* parent changed */
  };

  static RutPaintableVTable paintable_vtable = {
      _rut_scale_paint
  };

  static RutSizableVTable sizable_vtable = {
      _rut_scale_set_size,
      _rut_scale_get_size,
      rut_simple_sizable_get_preferred_width,
      _rut_scale_get_preferred_height,
      _rut_scale_add_preferred_size_callback,
  };


  RutType *type = &rut_scale_type;
#define TYPE RutScale

  rut_type_init (type, G_STRINGIFY (TYPE), _rut_scale_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_GRAPHABLE,
                      offsetof (TYPE, graphable),
                      &graphable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_PAINTABLE,
                      offsetof (TYPE, paintable),
                      &paintable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_SIZABLE,
                      0, /* no implied properties */
                      &sizable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_INTROSPECTABLE,
                      offsetof (TYPE, introspectable),
                      NULL); /* no implied vtable */

#undef TYPE
}

static void
preferred_size_changed (RutScale *scale)
{
  rut_closure_list_invoke (&scale->preferred_size_cb_list,
                           RutSizablePreferredSizeCallback,
                           scale);
}

void
rut_scale_set_length (RutObject *object, float length)
{
  RutScale *scale = object;

  if (scale->length == length)
    return;

  scale->length = length;
  scale->changed = true;

  rut_property_dirty (&scale->ctx->property_ctx,
                      &scale->properties[RUT_SCALE_PROP_LENGTH]);

  preferred_size_changed (scale);
  rut_shell_queue_redraw (scale->ctx->shell);
}

float
rut_scale_get_length (RutScale *scale)
{
  return scale->length;
}

static void
_rut_scale_set_user_scale (RutObject *object, float factor)
{
  RutScale *scale = object;

  if (scale->user_scale == factor)
    return;

  scale->user_scale = factor;
  scale->pixel_scale = scale->default_scale * scale->user_scale;

  scale->changed = true;

  rut_property_dirty (&scale->ctx->property_ctx,
                      &scale->properties[RUT_SCALE_PROP_USER_SCALE]);
  rut_property_dirty (&scale->ctx->property_ctx,
                      &scale->properties[RUT_SCALE_PROP_PIXEL_SCALE]);

  preferred_size_changed (scale);
  rut_shell_queue_redraw (scale->ctx->shell);
}

void
rut_scale_set_offset (RutObject *object, float offset)
{
  RutScale *scale = object;

  if (offset < 0)
    offset = 0;

  if (scale->start_offset == offset)
    return;

  scale->start_offset = offset;
  scale->changed = true;

  rut_property_dirty (&scale->ctx->property_ctx,
                      &scale->properties[RUT_SCALE_PROP_OFFSET]);

  preferred_size_changed (scale);
  rut_shell_queue_redraw (scale->ctx->shell);
}

float
rut_scale_get_offset (RutScale *scale)
{
  return scale->start_offset;
}

void
rut_scale_set_focus (RutObject *object, float offset)
{
  RutScale *scale = object;

  if (offset < 0)
    offset = 0;

  if (scale->focus_offset == offset)
    return;

  scale->focus_offset = offset;

  rut_property_dirty (&scale->ctx->property_ctx,
                      &scale->properties[RUT_SCALE_PROP_FOCUS]);

  rut_shell_queue_redraw (scale->ctx->shell);
}

float
rut_scale_get_focus (RutScale *scale)
{
  return scale->focus_offset;
}

float
rut_scale_get_pixel_scale (RutScale *scale)
{
  return scale->pixel_scale;
}

static RutPropertySpec _rut_scale_prop_specs[] = {
  {
    .name = "length",
    .nick = "Length",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutScale, length),
    .setter.float_type = rut_scale_set_length,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = TRUE
  },
  {
    .name = "user_scale",
    .nick = "User Scale",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutScale, user_scale),
    .setter.float_type = _rut_scale_set_user_scale,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = TRUE
  },
  {
    .name = "offset",
    .nick = "Offset",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutScale, start_offset),
    .setter.float_type = rut_scale_set_offset,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = TRUE
  },
  {
    .name = "focus",
    .nick = "Focus",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutScale, focus_offset),
    .setter.float_type = rut_scale_set_focus,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = TRUE
  },
  {
    .name = "pixel_scale",
    .nick = "Pixel Scale",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutScale, pixel_scale),
    .flags = RUT_PROPERTY_FLAG_READABLE,
    .animatable = TRUE
  },
  { NULL }
};

typedef struct _GrabState
{
  RutObject *camera;
  RutScale *scale;
  CoglMatrix transform;
  CoglMatrix inverse_transform;
  bool is_pan;
  bool is_select;
  float grab_offset;
  float grab_x;
  float grab_y;
} GrabState;

static RutInputEventStatus
_rut_scale_grab_input_cb (RutInputEvent *event,
                          void *user_data)
{
  GrabState *state = user_data;
  RutScale *scale = state->scale;

  if(rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      RutShell *shell = scale->ctx->shell;

      if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_UP)
        {
          rut_graphable_remove_child (scale->select_transform);
          rut_shell_queue_redraw (scale->ctx->shell);

          rut_shell_ungrab_input (shell, _rut_scale_grab_input_cb, user_data);
          g_slice_free (GrabState, state);
          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
      else if (rut_motion_event_get_action (event) ==
               RUT_MOTION_EVENT_ACTION_MOVE)
        {
          float x = rut_motion_event_get_x (event);
          float y = rut_motion_event_get_y (event);
          RutObject *camera = state->camera;

          rut_camera_unproject_coord (camera,
                                      &state->transform,
                                      &state->inverse_transform,
                                      0,
                                      &x,
                                      &y);

          if (state->is_pan)
            {
              float dx = x - state->grab_x;
              dx /= (scale->default_scale * scale->user_scale);
              rut_scale_set_offset (scale, state->grab_offset - dx);
            }
          else if (state->is_select)
            {
              float start_x = state->grab_x;
              float end_x = x;
              float start_t, end_t;
              float width, height;

              if (start_x > end_x)
                {
                  float tmp = end_x;
                  end_x = start_x;
                  start_x = tmp;
                }

              start_t = rut_scale_pixel_to_offset (scale, start_x);
              end_t = rut_scale_pixel_to_offset (scale, end_x);

              rut_transform_init_identity (scale->select_transform);
              rut_transform_translate (scale->select_transform, start_x, 0, 0);

              rut_sizable_get_size (scale, &width, &height);
              rut_sizable_set_size (scale->select_rect, end_x - start_x, height);

              rut_shell_queue_redraw (scale->ctx->shell);

              rut_closure_list_invoke (&scale->select_cb_list,
                                       RutScaleSelectCallback,
                                       scale,
                                       start_t,
                                       end_t);
            }
          else
            {
              float focus_offset = scale->start_offset +
                (x / (scale->default_scale * scale->user_scale));
              rut_scale_set_focus (scale, focus_offset);
            }

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static float
offset_to_pixel (RutScale *scale, float offset)
{
  float to_pixel = scale->pixel_scale;

  return offset * to_pixel - scale->start_offset * to_pixel;
}

static void
change_scale (RutScale *scale, float scale_factor)
{
  float focus_offset_px =
    offset_to_pixel (scale, scale->focus_offset);
  float new_focus_offset_px;
  float dx;

  _rut_scale_set_user_scale (scale, scale->user_scale * scale_factor);

  new_focus_offset_px =
    offset_to_pixel (scale, scale->focus_offset);

  dx = new_focus_offset_px - focus_offset_px;
  dx /= (scale->default_scale * scale->user_scale);

  rut_scale_set_offset (scale, scale->start_offset + dx);
}

static RutInputEventStatus
_rut_scale_input_cb (RutInputRegion *region,
                     RutInputEvent *event,
                     void *user_data)
{
  RutScale *scale = user_data;

  if(rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION &&
     rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN)
    {
      GrabState *state = g_slice_new0 (GrabState);
      const CoglMatrix *view;
      float x = rut_motion_event_get_x (event);
      float y = rut_motion_event_get_y (event);

      state->scale = scale;
      state->camera = rut_input_event_get_camera (event);
      view = rut_camera_get_view_transform (state->camera);
      state->transform = *view;
      rut_graphable_apply_transform (scale, &state->transform);
      if (!cogl_matrix_get_inverse (&state->transform,
                                    &state->inverse_transform))
        {
          g_warning ("Failed to calculate inverse of widget transform\n");
          g_slice_free (GrabState, state);
          return RUT_INPUT_EVENT_STATUS_UNHANDLED;
        }

      scale->initial_view = false;

      state->grab_x = x;
      state->grab_y = y;
      rut_camera_unproject_coord (state->camera,
                                  &state->transform,
                                  &state->inverse_transform,
                                  0,
                                  &state->grab_x,
                                  &state->grab_y);

      if (rut_motion_event_get_button_state (event) == RUT_BUTTON_STATE_2 &&
          rut_motion_event_get_modifier_state (event) &
          RUT_MODIFIER_SHIFT_ON)
        {
          state->grab_offset = scale->start_offset;
          state->is_pan = true;
        }
      else if (rut_motion_event_get_button_state (event) == RUT_BUTTON_STATE_1 &&
               rut_motion_event_get_modifier_state (event) &
               RUT_MODIFIER_SHIFT_ON)
        {
          state->grab_offset = rut_scale_pixel_to_offset (scale, state->grab_x);
          state->is_select = true;
          rut_graphable_add_child (scale, scale->select_transform);
        }
      else
        state->grab_offset = scale->focus_offset;

      rut_shell_grab_input (scale->ctx->shell,
                            state->camera,
                            _rut_scale_grab_input_cb,
                            state);
    }
  else if(rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_KEY &&
          rut_key_event_get_action (event) == RUT_KEY_EVENT_ACTION_DOWN)
    {
      switch (rut_key_event_get_keysym (event))
        {
        case RUT_KEY_equal:
          rut_scale_user_zoom_in (scale);
          break;
        case RUT_KEY_minus:
          rut_scale_user_zoom_out (scale);
          break;
        case RUT_KEY_0:
          rut_scale_user_zoom_reset (scale);
          break;
        default:
          break;
        }
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

RutScale *
rut_scale_new (RutContext *ctx,
               float length,
               float natural_length)
{
  RutScale *scale = rut_object_alloc0 (RutScale,
                                       &rut_scale_type,
                                       _rut_scale_init_type);


  scale->ctx = ctx;

  rut_graphable_init (scale);
  rut_paintable_init (scale);

  rut_list_init (&scale->preferred_size_cb_list);
  rut_list_init (&scale->select_cb_list);

  rut_introspectable_init (scale, _rut_scale_prop_specs,
                           scale->properties);

  scale->width = 1;
  scale->height = 1;

  scale->length = length;
  scale->natural_length = natural_length;
  scale->default_scale = 1;
  scale->user_scale = 1;
  scale->pixel_scale = 1;
  scale->initial_view = true;

  scale->labels = g_array_new (FALSE, FALSE, sizeof (Label));

  scale->bg = rut_rectangle_new4f (ctx, 1, 1, 0.8, 0.8, 0.8, 1);
  rut_graphable_add_child (scale, scale->bg);
  rut_object_unref (scale->bg);

  scale->select_transform = rut_transform_new (ctx);

  scale->select_rect = rut_rectangle_new4f (ctx, 1, 1, 0.9, 0.9, 0.8, 1);
  rut_graphable_add_child (scale->select_transform, scale->select_rect);
  rut_object_unref (scale->select_rect);

  scale->pipeline = cogl_pipeline_new (ctx->cogl_context);
  cogl_pipeline_set_color4f (scale->pipeline, 1, 0, 0, 1);

  scale->input_region =
    rut_input_region_new_rectangle (0, 0, 1, 1,
                                    _rut_scale_input_cb,
                                    scale);
  rut_graphable_add_child (scale, scale->input_region);
  rut_object_unref (scale->input_region);

  return scale;
}

float
rut_scale_pixel_to_offset (RutScale *scale, float pixel)
{
  return scale->start_offset + (pixel / (scale->pixel_scale));
}

RutClosure *
rut_scale_add_select_callback (RutScale *scale,
                               RutScaleSelectCallback callback,
                               void *user_data,
                               RutClosureDestroyCallback destroy_cb)
{
  g_return_val_if_fail (callback != NULL, NULL);

  return rut_closure_list_add (&scale->select_cb_list,
                               callback,
                               user_data,
                               destroy_cb);
}

void
rut_scale_user_zoom_in (RutScale *scale)
{
  scale->initial_view = false;
  change_scale (scale, 1.25);
}

void
rut_scale_user_zoom_out (RutScale *scale)
{
  scale->initial_view = false;
  change_scale (scale, 1.0 / 1.25);
}

void
rut_scale_user_zoom_reset (RutScale *scale)
{
  scale->initial_view = false;
  rut_scale_set_offset (scale, 0);
  _rut_scale_set_user_scale (scale, 1);
}

void
rut_scale_set_natural_length (RutScale *scale,
                              float natural_length)
{
  if (scale->natural_length == natural_length)
    return;

  scale->natural_length = natural_length;

  preferred_size_changed (scale);
  rut_shell_queue_redraw (scale->ctx->shell);
}
