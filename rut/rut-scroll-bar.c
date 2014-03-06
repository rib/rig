/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012 Intel Corporation.
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

#include "rut-context.h"
#include "rut-shell.h"
#include "rut-interfaces.h"
#include "rut-paintable.h"
#include "rut-camera.h"
#include "rut-scroll-bar.h"
#include "rut-color.h"
#include "rut-input-region.h"
#include "rut-introspectable.h"

#define THICKNESS 20
#define HANDLE_THICKNESS 15

enum {
  RUT_SCROLL_BAR_PROP_LENGTH,
  RUT_SCROLL_BAR_PROP_VIRTUAL_LENGTH,
  RUT_SCROLL_BAR_PROP_VIRTUAL_VIEWPORT,
  RUT_SCROLL_BAR_PROP_VIRTUAL_OFFSET,
  RUT_SCROLL_BAR_N_PROPS
};

struct _RutScrollBar
{
  RutObjectBase _base;

  RutContext *ctx;

  CoglColor color;

  int thickness;

  float trough_range;

  CoglPipeline *rounded_pipeline;
  CoglPipeline *rect_pipeline;
  float handle_len;
  float handle_pos;

  RutGraphableProps graphable;
  RutPaintableProps paintable;

  RutInputRegion *input_region;

  RutAxis axis;
  float length;
  float virtual_length;
  float viewport_length;
  float offset;

  float grab_x;
  float grab_y;
  float grab_offset;

  RutIntrospectableProps introspectable;
  RutProperty properties[RUT_SCROLL_BAR_N_PROPS];
};

static RutPropertySpec _rut_scroll_bar_prop_specs[] = {
  {
    .name = "length",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutScrollBar, length),
    .setter.float_type = rut_scroll_bar_set_length
  },
  {
    .name = "virtual_length",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutScrollBar, virtual_length),
    .setter.float_type = rut_scroll_bar_set_virtual_length
  },
  {
    .name = "virtual_viewport",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutScrollBar, viewport_length),
    .setter.float_type = rut_scroll_bar_set_virtual_viewport
  },
  {
    .name = "virtual_offset",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutScrollBar, offset),
    .setter.float_type = rut_scroll_bar_set_virtual_offset
  },


  { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rut_scroll_bar_free (void *object)
{
  RutScrollBar *scroll_bar = object;

  rut_graphable_destroy (scroll_bar);

  rut_object_free (RutScrollBar, object);
}

static void
_rut_scroll_bar_paint (RutObject *object, RutPaintContext *paint_ctx)
{
  RutScrollBar *scroll_bar = RUT_SCROLL_BAR (object);
  CoglFramebuffer *fb = rut_camera_get_framebuffer (paint_ctx->camera);

  cogl_framebuffer_push_matrix (fb);

  if (scroll_bar->axis == RUT_AXIS_X)
    {
      cogl_framebuffer_translate (fb,
                                  scroll_bar->handle_pos, 0, 0);
    }
  else
    {
      cogl_framebuffer_translate (fb, 0,
                                  scroll_bar->handle_pos, 0);
      cogl_framebuffer_draw_rectangle (fb,
                                       scroll_bar->rounded_pipeline,
                                       0, 0,
                                       HANDLE_THICKNESS,
                                       HANDLE_THICKNESS);

      cogl_framebuffer_draw_rectangle (fb,
                                       scroll_bar->rounded_pipeline,
                                       0,
                                       scroll_bar->handle_len - HANDLE_THICKNESS,
                                       HANDLE_THICKNESS,
                                       scroll_bar->handle_len);

      cogl_framebuffer_draw_textured_rectangle (fb,
                                                scroll_bar->rounded_pipeline,
                                                0,
                                                HANDLE_THICKNESS / 2,
                                                HANDLE_THICKNESS,
                                                scroll_bar->handle_len - HANDLE_THICKNESS / 2,
                                                0, 0.5, 1, 0.5);
    }

  cogl_framebuffer_pop_matrix (fb);
}

RutType rut_scroll_bar_type;

void
_rut_scroll_bar_init_type (void)
{
  static RutGraphableVTable graphable_vtable = {
    NULL, /* child remove */
    NULL, /* child add */
    NULL /* parent changed */
  };

  static RutPaintableVTable paintable_vtable = {
    _rut_scroll_bar_paint
  };


  RutType *type = &rut_scroll_bar_type;
#define TYPE RutScrollBar

  rut_type_init (type, G_STRINGIFY (TYPE), _rut_scroll_bar_free);
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

#undef TYPE
}

static RutInputEventStatus
_rut_scroll_bar_grab_input_cb (RutInputEvent *event,
                               void *user_data)
{
  RutScrollBar *scroll_bar = user_data;

  if(rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      RutShell *shell = scroll_bar->ctx->shell;
      if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_UP)
        {
          rut_shell_ungrab_input (shell,
                                  _rut_scroll_bar_grab_input_cb,
                                  user_data);
          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
      else if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_MOVE)
        {
          float diff;
          float offset;

          if (scroll_bar->axis == RUT_AXIS_X)
            diff = rut_motion_event_get_x (event) - scroll_bar->grab_x;
          else
            diff = rut_motion_event_get_y (event) - scroll_bar->grab_y;

          diff = (diff / scroll_bar->trough_range) *
            (scroll_bar->virtual_length - scroll_bar->viewport_length);

          offset = scroll_bar->grab_offset + diff;

          rut_scroll_bar_set_virtual_offset (scroll_bar, offset);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static RutInputEventStatus
_rut_scroll_bar_input_cb (RutInputRegion *region,
                          RutInputEvent *event,
                          void *user_data)
{
  RutScrollBar *scroll_bar = user_data;
  float x, y;
  float pos;

  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      if (!rut_motion_event_unproject (event, scroll_bar, &x, &y))
        return RUT_INPUT_EVENT_STATUS_UNHANDLED;

      if (scroll_bar->axis == RUT_AXIS_X)
        pos = x;
      else
        pos = y;

      if (pos > scroll_bar->handle_pos &&
          pos < scroll_bar->handle_pos + scroll_bar->handle_len)
        {
          if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN)
            {
              scroll_bar->grab_x = rut_motion_event_get_x (event);
              scroll_bar->grab_y = rut_motion_event_get_y (event);
              scroll_bar->grab_offset = scroll_bar->offset;

              rut_shell_grab_input (scroll_bar->ctx->shell,
                                    rut_input_event_get_camera (event),
                                    _rut_scroll_bar_grab_input_cb,
                                    scroll_bar);
            }
        }
      else if (pos < scroll_bar->handle_pos)
        {
          //g_print ("Scroll Bar: In front of handle\n");
        }
      else
        {
          //g_print ("Scroll Bar: After handle\n");
        }
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
rut_scroll_bar_get_size (RutScrollBar *scroll_bar,
                         float *width,
                         float *height)
{
  if (scroll_bar->axis == RUT_AXIS_X)
    {
      *width = scroll_bar->length;
      *height = scroll_bar->thickness;
    }
  else
    {
      *width = scroll_bar->thickness;
      *height = scroll_bar->length;
    }
}

static void
update_handle_position (RutScrollBar *scroll_bar)
{
  scroll_bar->handle_pos = (scroll_bar->offset /
     (scroll_bar->virtual_length - scroll_bar->viewport_length)) *
    scroll_bar->trough_range;

  rut_shell_queue_redraw (scroll_bar->ctx->shell);
}

static void
update_handle_length (RutScrollBar *scroll_bar)
{
  float handle_len;

  handle_len = (scroll_bar->viewport_length /
                scroll_bar->virtual_length) * scroll_bar->length;
  handle_len = MAX (scroll_bar->thickness, handle_len);

  scroll_bar->handle_len = handle_len;

  /* The trough range is the range of motion for the handle taking into account
   * that the handle size might not reflect the relative size of the viewport
   * if it was clamped to a larger size.
   *
   * The trough_range maps to (virtual_length - viewport_length)
   */
  scroll_bar->trough_range = scroll_bar->length - scroll_bar->handle_len;
}

static void
update_geometry (RutScrollBar *scroll_bar)
{
  float width, height;

  rut_scroll_bar_get_size (scroll_bar, &width, &height);

  rut_input_region_set_rectangle (scroll_bar->input_region,
                                  0, 0, width, height);

  update_handle_length (scroll_bar);

  update_handle_position (scroll_bar);

  rut_shell_queue_redraw (scroll_bar->ctx->shell);
}

RutScrollBar *
rut_scroll_bar_new (RutContext *ctx,
                    RutAxis axis,
                    float length,
                    float virtual_length,
                    float viewport_length)
{
  RutScrollBar *scroll_bar =
    rut_object_alloc0 (RutScrollBar, &rut_scroll_bar_type, _rut_scroll_bar_init_type);



  rut_introspectable_init (scroll_bar,
                           _rut_scroll_bar_prop_specs,
                           scroll_bar->properties);

  scroll_bar->ctx = ctx;

  rut_graphable_init (scroll_bar);
  rut_paintable_init (scroll_bar);

  scroll_bar->axis = axis;
  scroll_bar->length = length;
  scroll_bar->virtual_length = virtual_length;
  scroll_bar->viewport_length = viewport_length;
  scroll_bar->offset = 0;

  rut_color_init_from_uint32 (&scroll_bar->color, 0x919191ff);

  scroll_bar->thickness = THICKNESS;

  scroll_bar->rect_pipeline = cogl_pipeline_new (ctx->cogl_context);
  cogl_pipeline_set_color (scroll_bar->rect_pipeline,
                           &scroll_bar->color);

  scroll_bar->rounded_pipeline = cogl_pipeline_copy (scroll_bar->rect_pipeline);
  cogl_pipeline_set_layer_texture (scroll_bar->rounded_pipeline,
                                   0,
                                   ctx->circle_texture);

  scroll_bar->input_region =
    rut_input_region_new_rectangle (0, 0, 1, 1,
                                    _rut_scroll_bar_input_cb,
                                    scroll_bar);
  rut_graphable_add_child (scroll_bar, scroll_bar->input_region);

  update_geometry (scroll_bar);

  return scroll_bar;
}

void
rut_scroll_bar_set_length (RutObject *obj,
                           float length)
{
  RutScrollBar *scroll_bar = RUT_SCROLL_BAR (obj);

  if (scroll_bar->length == length)
    return;

  scroll_bar->length = length;

  update_geometry (scroll_bar);
}

static float
clamp_offset (RutScrollBar *scroll_bar,
              float offset)
{
  if (offset + scroll_bar->viewport_length > scroll_bar->virtual_length)
    offset = scroll_bar->virtual_length - scroll_bar->viewport_length;

  if (offset < 0.0f)
    offset = 0.0f;

  return offset;
}

static void
reclamp_offset (RutScrollBar *scroll_bar)
{
  float offset = clamp_offset (scroll_bar, scroll_bar->offset);

  if (offset != scroll_bar->offset)
    {
      RutProperty *property =
        &scroll_bar->properties[RUT_SCROLL_BAR_PROP_VIRTUAL_OFFSET];

      scroll_bar->offset = offset;

      rut_property_dirty (&scroll_bar->ctx->property_ctx,
                          property);
    }
}

void
rut_scroll_bar_set_virtual_length (RutObject *obj,
                                   float virtual_length)
{
  RutScrollBar *scroll_bar = RUT_SCROLL_BAR (obj);

  if (scroll_bar->virtual_length == virtual_length)
    return;

  scroll_bar->virtual_length = virtual_length;

  reclamp_offset (scroll_bar);
  update_handle_length (scroll_bar);
  update_handle_position (scroll_bar);
}

void
rut_scroll_bar_set_virtual_viewport (RutObject *obj,
                                     float viewport_length)
{
  RutScrollBar *scroll_bar = RUT_SCROLL_BAR (obj);

  if (scroll_bar->viewport_length == viewport_length)
    return;

  scroll_bar->viewport_length = viewport_length;

  reclamp_offset (scroll_bar);
  update_handle_length (scroll_bar);
  update_handle_position (scroll_bar);
}

void
rut_scroll_bar_set_virtual_offset (RutObject *obj,
                                   float viewport_offset)
{
  RutScrollBar *scroll_bar = RUT_SCROLL_BAR (obj);

  viewport_offset = clamp_offset (scroll_bar, viewport_offset);

  if (scroll_bar->offset == viewport_offset)
    return;

  scroll_bar->offset = viewport_offset;

  update_handle_position (scroll_bar);

  rut_property_dirty (&scroll_bar->ctx->property_ctx,
                      &scroll_bar->properties[RUT_SCROLL_BAR_PROP_VIRTUAL_OFFSET]);
}

float
rut_scroll_bar_get_virtual_offset (RutScrollBar *scroll_bar)
{
  return scroll_bar->offset;
}

float
rut_scroll_bar_get_virtual_viewport (RutScrollBar *scroll_bar)
{
  return scroll_bar->viewport_length;
}

float
rut_scroll_bar_get_thickness (RutScrollBar *scroll_bar)
{
  return scroll_bar->thickness;
}

void
rut_scroll_bar_set_color (RutScrollBar *scroll_bar,
                          const CoglColor *color)
{
  scroll_bar->color = *color;
}
