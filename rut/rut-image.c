/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012,2013 Intel Corporation.
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

#include <cogl/cogl.h>
#include <string.h>
#include <math.h>

#include "rut-interfaces.h"
#include "rut-paintable.h"
#include "rut-image.h"
#include "rut-camera.h"
#include "rut-introspectable.h"

enum {
  RUT_IMAGE_PROP_DRAW_MODE,
  RUT_IMAGE_N_PROPS
};

struct _RutImage
{
  RutObjectBase _base;

  float width, height;
  int tex_width, tex_height;

  /* Cached rectangle to use when the draw mode is
   * SCALE_WITH_ASPECT_RATIO */
  float fit_x1, fit_y1;
  float fit_x2, fit_y2;

  RutContext *context;

  RutPaintableProps paintable;
  RutGraphableProps graphable;

  RutList preferred_size_cb_list;

  RutIntrospectableProps introspectable;
  RutProperty properties[RUT_IMAGE_N_PROPS];

  CoglPipeline *pipeline;


  RutImageDrawMode draw_mode;
};

RutType rut_image_type;

static RutUIEnum
_rut_image_draw_mode_ui_enum =
  {
    .nick = "Draw mode",
    .values =
    {
      {
        RUT_IMAGE_DRAW_MODE_1_TO_1,
        "1 to 1",
        "Show the full image at a 1:1 ratio"
      },
      {
        RUT_IMAGE_DRAW_MODE_REPEAT,
        "Repeat",
        "Fill the widget with repeats of the image"
      },
      {
        RUT_IMAGE_DRAW_MODE_SCALE,
        "Scale",
        "Scale the image to fill the size of the widget"
      },
      {
        RUT_IMAGE_DRAW_MODE_SCALE_WITH_ASPECT_RATIO,
        "Scale with aspect ratio",
        "Scale the image to fill the size of the widget but maintain the aspect"
        "ration"
      },
      { 0 }
    }
  };

static RutPropertySpec
_rut_image_prop_specs[] =
  {
    {
      .name = "draw_mode",
      .type = RUT_PROPERTY_TYPE_ENUM,
      .data_offset = offsetof (RutImage, draw_mode),
      .setter.any_type = rut_image_set_draw_mode,
      .flags = (RUT_PROPERTY_FLAG_READWRITE |
                RUT_PROPERTY_FLAG_VALIDATE),
      .validation = { .ui_enum = &_rut_image_draw_mode_ui_enum }
    },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
  };

static void
_rut_image_free (void *object)
{
  RutImage *image = object;

  rut_closure_list_disconnect_all (&image->preferred_size_cb_list);

  rut_graphable_destroy (image);

  cogl_object_unref (image->pipeline);

  rut_object_free (RutImage, image);
}

static void
_rut_image_paint (RutObject *object,
                  RutPaintContext *paint_ctx)
{
  RutImage *image = object;
  CoglFramebuffer *fb = rut_camera_get_framebuffer (paint_ctx->camera);

  switch (image->draw_mode)
    {
    case RUT_IMAGE_DRAW_MODE_1_TO_1:
      cogl_framebuffer_draw_rectangle (fb,
                                       image->pipeline,
                                       0.0f, 0.0f,
                                       image->tex_width,
                                       image->tex_height);
      break;

    case RUT_IMAGE_DRAW_MODE_SCALE:
      cogl_framebuffer_draw_rectangle (fb,
                                       image->pipeline,
                                       0.0f, 0.0f,
                                       image->width,
                                       image->height);
      break;

    case RUT_IMAGE_DRAW_MODE_REPEAT:
      cogl_framebuffer_draw_textured_rectangle (fb,
                                                image->pipeline,
                                                0.0f, 0.0f,
                                                image->width,
                                                image->height,
                                                0.0f, 0.0f,
                                                image->width /
                                                image->tex_width,
                                                image->height /
                                                image->tex_height);
      break;

    case RUT_IMAGE_DRAW_MODE_SCALE_WITH_ASPECT_RATIO:
      cogl_framebuffer_draw_rectangle (fb,
                                       image->pipeline,
                                       image->fit_x1,
                                       image->fit_y1,
                                       image->fit_x2,
                                       image->fit_y2);
      break;
    }
}

static void
rut_image_set_size (void *object,
                    float width,
                    float height)
{
  RutImage *image = object;

  image->width = width;
  image->height = height;

  if (height == 0.0f)
    {
      image->fit_x1 = 0.0f;
      image->fit_y1 = 0.0f;
      image->fit_x2 = 0.0f;
      image->fit_y2 = 0.0f;
    }
  else
    {
      float widget_aspect = width / height;
      float tex_aspect = image->tex_width / (float) image->tex_height;

      if (tex_aspect > widget_aspect)
        {
          /* Fit the width */
          float draw_height = width / tex_aspect;
          image->fit_x1 = 0.0f;
          image->fit_x2 = width;
          image->fit_y1 = height / 2.0f - draw_height / 2.0f;
          image->fit_y2 = image->fit_y1 + draw_height;
        }
      else
        {
          /* Fit the height */
          float draw_width = height * tex_aspect;
          image->fit_y1 = 0.0f;
          image->fit_y2 = height;
          image->fit_x1 = width / 2.0f - draw_width / 2.0f;
          image->fit_x2 = image->fit_x1 + draw_width;
        }
    }

  rut_shell_queue_redraw (image->context->shell);
}

static void
rut_image_get_preferred_width (void *object,
                               float for_height,
                               float *min_width_p,
                               float *natural_width_p)
{
  RutImage *image = object;

  if (image->draw_mode == RUT_IMAGE_DRAW_MODE_1_TO_1)
    {
      if (min_width_p)
        *min_width_p = image->tex_width;
      if (natural_width_p)
        *natural_width_p = image->tex_width;
    }
  else if (image->draw_mode == RUT_IMAGE_DRAW_MODE_SCALE_WITH_ASPECT_RATIO)
    {
      if (min_width_p)
        *min_width_p = 0.0f;

      if (natural_width_p)
        {
          if (for_height != -1.0f)
            {
              float aspect = (float)image->tex_width / image->tex_height;

              /* Our preferrence is to have just enough space to be
               * able to show the image at 1:1, not to necessarily
               * fill the for_height space...
               */

              if (for_height > image->tex_height)
                *natural_width_p = image->tex_width;
              else
                *natural_width_p = for_height * aspect;
            }
          else
            *natural_width_p = image->tex_width;
        }
    }
  else
    {
      if (min_width_p)
        *min_width_p = 0.0f;

      if (natural_width_p)
        {
          if (for_height != -1.0f)
            {
              float aspect = image->tex_width / image->tex_height;

              *natural_width_p = for_height * aspect;
            }
          else
            *natural_width_p = image->tex_width;
        }
    }
}

static void
rut_image_get_preferred_height (void *object,
                                float for_width,
                                float *min_height_p,
                                float *natural_height_p)
{
  RutImage *image = object;

  if (image->draw_mode == RUT_IMAGE_DRAW_MODE_1_TO_1)
    {
      if (min_height_p)
        *min_height_p = image->tex_height;
      if (natural_height_p)
        *natural_height_p = image->tex_height;
    }
  else if (image->draw_mode == RUT_IMAGE_DRAW_MODE_SCALE_WITH_ASPECT_RATIO)
    {
      if (min_height_p)
        *min_height_p = 0.0f;

      if (natural_height_p)
        {
          if (for_width != -1.0f)
            {
              float aspect = (float)image->tex_height / image->tex_width;

              /* Our preferrence is to have just enough space to be
               * able to show the image at 1:1, not to necessarily
               * fill the for_width space...
               */

              if (for_width > image->tex_width)
                *natural_height_p = image->tex_height;
              else
                *natural_height_p = for_width * aspect;
            }
          else
            *natural_height_p = image->tex_height;
        }
    }
  else
    {
      if (min_height_p)
        *min_height_p = 0.0f;

      if (natural_height_p)
        {
          if (for_width != -1.0f)
            {
              float aspect = image->tex_height / image->tex_width;

              *natural_height_p = for_width * aspect;
            }
          else
            *natural_height_p = image->tex_height;
        }
    }
}

static RutClosure *
rut_image_add_preferred_size_callback (void *object,
                                       RutSizablePreferredSizeCallback cb,
                                       void *user_data,
                                       RutClosureDestroyCallback destroy)
{
  RutImage *image = object;

  return rut_closure_list_add (&image->preferred_size_cb_list,
                               cb,
                               user_data,
                               destroy);
}

static void
rut_image_get_size (void *object,
                    float *width,
                    float *height)
{
  RutImage *image = object;

  *width = image->width;
  *height = image->height;
}

static void
_rut_image_init_type (void)
{
  static RutPaintableVTable paintable_vtable = {
      _rut_image_paint
  };
  static RutGraphableVTable graphable_vtable = {
      NULL, /* child removed */
      NULL, /* child addded */
      NULL /* parent changed */
  };
  static RutSizableVTable sizable_vtable = {
      rut_image_set_size,
      rut_image_get_size,
      rut_image_get_preferred_width,
      rut_image_get_preferred_height,
      rut_image_add_preferred_size_callback
  };

  RutType *type = &rut_image_type;
#define TYPE RutImage

  rut_type_init (type, G_STRINGIFY (TYPE), _rut_image_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_PAINTABLE,
                      offsetof (TYPE, paintable),
                      &paintable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_GRAPHABLE,
                      offsetof (TYPE, graphable),
                      &graphable_vtable);
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

RutImage *
rut_image_new (RutContext *ctx,
               CoglTexture *texture)
{
  RutImage *image = rut_object_alloc0 (RutImage,
                                       &rut_image_type,
                                       _rut_image_init_type);

  image->context = ctx;

  rut_list_init (&image->preferred_size_cb_list);

  image->pipeline = cogl_pipeline_new (ctx->cogl_context);
  cogl_pipeline_set_layer_texture (image->pipeline,
                                   0, /* layer_num */
                                   texture);

  image->tex_width = cogl_texture_get_width (texture);
  image->tex_height = cogl_texture_get_height (texture);

  rut_paintable_init (image);
  rut_graphable_init (image);

  rut_introspectable_init (image,
                           _rut_image_prop_specs,
                           image->properties);

  rut_image_set_draw_mode (image, RUT_IMAGE_DRAW_MODE_SCALE_WITH_ASPECT_RATIO);

  rut_image_set_size (image, image->tex_width, image->tex_height);

  return image;
}

static void
preferred_size_changed (RutImage *image)
{
  rut_closure_list_invoke (&image->preferred_size_cb_list,
                           RutSizablePreferredSizeCallback,
                           image);
}

void
rut_image_set_draw_mode (RutImage *image,
                         RutImageDrawMode draw_mode)
{
  if (draw_mode != image->draw_mode)
    {
      CoglPipelineWrapMode wrap_mode;
      CoglPipelineFilter min_filter, mag_filter;

      if (draw_mode == RUT_IMAGE_DRAW_MODE_1_TO_1 ||
          image->draw_mode == RUT_IMAGE_DRAW_MODE_1_TO_1)
        preferred_size_changed (image);

      image->draw_mode = draw_mode;

      switch (draw_mode)
        {
        case RUT_IMAGE_DRAW_MODE_1_TO_1:
        case RUT_IMAGE_DRAW_MODE_REPEAT:
          wrap_mode = COGL_PIPELINE_WRAP_MODE_REPEAT;
          min_filter = COGL_PIPELINE_FILTER_NEAREST;
          mag_filter = COGL_PIPELINE_FILTER_NEAREST;
          break;

        case RUT_IMAGE_DRAW_MODE_SCALE:
        case RUT_IMAGE_DRAW_MODE_SCALE_WITH_ASPECT_RATIO:
          wrap_mode = COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE;
          min_filter = COGL_PIPELINE_FILTER_LINEAR_MIPMAP_NEAREST;
          mag_filter = COGL_PIPELINE_FILTER_LINEAR;
          break;
        }

      cogl_pipeline_set_layer_wrap_mode (image->pipeline,
                                         0, /* layer_num */
                                         wrap_mode);
      cogl_pipeline_set_layer_filters (image->pipeline,
                                       0, /* layer_num */
                                       min_filter,
                                       mag_filter);

      rut_property_dirty (&image->context->property_ctx,
                          &image->properties[RUT_IMAGE_PROP_DRAW_MODE]);
    }
}
