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
#include "rut-icon.h"
#include "rut-image.h"
#include "rut-camera.h"

struct _RutIcon
{
  RutObjectBase _base;

  RutContext *context;

  RutImage *image;

  float width, height;

  RutGraphableProps graphable;
};

static void
_rut_icon_free (void *object)
{
  RutIcon *icon = object;

  rut_object_unref (icon->context);

  rut_graphable_destroy (icon);

  rut_object_free (RutIcon, icon);
}

static void
rut_icon_set_size (void *object,
                   float width,
                   float height)
{
  RutIcon *icon = RUT_ICON (object);

  rut_sizable_set_size (icon->image, width, height);
}

static void
rut_icon_get_preferred_width (void *object,
                              float for_height,
                              float *min_width_p,
                              float *natural_width_p)
{
  RutIcon *icon = object;

  rut_sizable_get_preferred_width (icon->image,
                                   for_height,
                                   min_width_p,
                                   natural_width_p);
}

static void
rut_icon_get_preferred_height (void *object,
                               float for_width,
                               float *min_height_p,
                               float *natural_height_p)
{
  RutIcon *icon = object;

  rut_sizable_get_preferred_height (icon->image,
                                    for_width,
                                    min_height_p,
                                    natural_height_p);
}

static void
rut_icon_get_size (void *object,
                   float *width,
                   float *height)
{
  RutIcon *icon = object;

  rut_sizable_get_size (icon->image, width, height);
}

RutType rut_icon_type;

static void
_rut_icon_init_type (void)
{
  static RutGraphableVTable graphable_vtable = {
    NULL, /* child removed */
    NULL, /* child addded */
    NULL /* parent changed */
  };

  static RutSizableVTable sizable_vtable = {
    rut_icon_set_size,
    rut_icon_get_size,
    rut_icon_get_preferred_width,
    rut_icon_get_preferred_height,
    NULL /* add_preferred_size_callback */
  };

  RutType *type = &rut_icon_type;
#define TYPE RutIcon

  rut_type_init (type, G_STRINGIFY (TYPE), _rut_icon_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_GRAPHABLE,
                      offsetof (TYPE, graphable),
                      &graphable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_SIZABLE,
                      0, /* no implied properties */
                      &sizable_vtable);

#undef TYPE
}

RutIcon *
rut_icon_new (RutContext *ctx,
              const char *filename)
{
  RutIcon *icon =
    rut_object_alloc0 (RutIcon, &rut_icon_type, _rut_icon_init_type);
  CoglTexture *texture;
  GError *error = NULL;

  icon->context = rut_object_ref (ctx);


  rut_graphable_init (icon);

  texture = rut_load_texture_from_data_file (ctx, filename, &error);
  if (texture)
    {
      icon->image = rut_image_new (ctx, texture);
      rut_image_set_draw_mode (icon->image, RUT_IMAGE_DRAW_MODE_1_TO_1);
      rut_graphable_add_child (icon, icon->image);
      rut_object_unref (icon->image);
    }
  else
    {
      g_warning ("Failed to load icon %s: %s", filename, error->message);
      g_error_free (error);
      rut_icon_set_size (icon, 100, 100);
    }

  return icon;
}
