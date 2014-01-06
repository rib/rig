/*
 * Rut
 *
 * Copyright (C) 2012,2013  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <cogl/cogl.h>

#include "rut-input-region.h"
#include "rut-util.h"
#include "rut-camera-private.h"
#include "rut-inputable.h"
#include "rut-pickable.h"

#include "components/rut-camera.h"

typedef enum _RutInputShapeType
{
  RUT_INPUT_SHAPE_TYPE_RECTANGLE,
  RUT_INPUT_SHAPE_TYPE_CIRCLE
} RutInputShapeType;

typedef struct _RutInputShapeRectangle
{
  RutInputShapeType type;
  float x0, y0, x1, y1;
} RutInputShapeRectange;

typedef struct _RutInputShapeCircle
{
  RutInputShapeType type;
  float x, y;
  float r;
  float r_squared;
} RutInputShapeCircle;

typedef struct _RutInputShapeAny
{
  RutInputShapeType type;
} RutInputShapeAny;

typedef union _RutInputShape
{
  RutInputShapeAny any;
  RutInputShapeRectange rectangle;
  RutInputShapeCircle circle;
} RutInputShape;

struct _RutInputRegion
{
  RutObjectProps _parent;

  int ref_count;

  RutInputShape shape;

  RutGraphableProps graphable;

  bool hud_mode;

  RutInputRegionCallback callback;
  void *user_data;
};


static void
_rut_input_region_free (void *object)
{
  RutInputRegion *region = object;

  rut_graphable_destroy (region);

  g_slice_free (RutInputRegion, region);
}

static void
poly_init_from_rectangle (float *poly,
                          RutInputShapeRectange *rectangle)
{
  poly[0] = rectangle->x0;
  poly[1] = rectangle->y0;
  poly[2] = 0;
  poly[3] = 1;

  poly[4] = rectangle->x0;
  poly[5] = rectangle->y1;
  poly[6] = 0;
  poly[7] = 1;

  poly[8] = rectangle->x1;
  poly[9] = rectangle->y1;
  poly[10] = 0;
  poly[11] = 1;

  poly[12] = rectangle->x1;
  poly[13] = rectangle->y0;
  poly[14] = 0;
  poly[15] = 1;
}
/* Given an (x0,y0) (x1,y1) rectangle this transforms it into
 * a polygon in window coordinates that can be intersected
 * with input coordinates for picking.
 */
static void
rect_to_screen_polygon (RutInputShapeRectange *rectangle,
                        const CoglMatrix *modelview,
                        const CoglMatrix *projection,
                        const float *viewport,
                        float *poly)
{
  poly_init_from_rectangle (poly, rectangle);

  rut_util_fully_transform_points (modelview,
                                   projection,
                                   viewport,
                                   poly,
                                   4);
}

static bool
_rut_input_region_pick (RutObject *inputable,
                        RutCamera *camera,
                        const CoglMatrix *graphable_modelview,
                        float x,
                        float y)
{
  RutInputRegion *region = inputable;
  CoglMatrix matrix;
  const CoglMatrix *modelview = NULL;
  float poly[16];
  const CoglMatrix *view = rut_camera_get_view_transform (camera);

#warning "todo: simplify RutInputRegion by removing ::hud_mode"
  if (region->hud_mode)
    modelview = &camera->ctx->identity_matrix;
  else
    {
      if (graphable_modelview)
        modelview = graphable_modelview;
      else
        {
          matrix = *view;
          rut_graphable_apply_transform (inputable, &matrix);
          modelview = &matrix;
        }
    }

  switch (region->shape.any.type)
    {
    case RUT_INPUT_SHAPE_TYPE_RECTANGLE:
      {
        if (!region->hud_mode)
          {
            const CoglMatrix *projection =
              rut_camera_get_projection (camera);
            rect_to_screen_polygon (&region->shape.rectangle,
                                    modelview,
                                    projection,
                                    camera->viewport,
                                    poly);
          }
        else
          poly_init_from_rectangle (poly, &region->shape.rectangle);

#if 0
        g_print ("transformed input region\n");
        for (i = 0; i < 4; i++)
          {
            float *p = poly + 4 * i;
            g_print ("poly[].x=%f\n", p[0]);
            g_print ("poly[].y=%f\n", p[1]);
          }
        g_print ("x=%f y=%f\n", x, y);
#endif
        if (rut_util_point_in_screen_poly (x, y, poly, sizeof (float) * 4, 4))
          return TRUE;
        else
          return FALSE;
      }
    case RUT_INPUT_SHAPE_TYPE_CIRCLE:
      {
        RutInputShapeCircle *circle = &region->shape.circle;
        float center_x = circle->x;
        float center_y = circle->y;
        float z = 0;
        float w = 1;
        float a;
        float b;
        float c2;

        /* Note the circle hit regions are billboarded, such that only the
         * center point is transformed but the raius of the circle stays
         * constant. */

        cogl_matrix_transform_point (modelview,
                                     &center_x, &center_y, &z, &w);

        a = x - center_x;
        b = y - center_y;
        c2 = a * a + b * b;

        if (c2 < circle->r_squared)
          return TRUE;
        else
          return FALSE;
      }
    }

  g_warn_if_reached ();
  return FALSE;
}

static RutInputEventStatus
_rut_input_region_handle_event (RutObject *inputable,
                                RutInputEvent *event)
{
  RutInputRegion *region = inputable;

  return region->callback (region, event, region->user_data);
}

static void
_rut_input_region_set_size (RutObject *self,
                            float width,
                            float height)
{
  RutInputRegion *region = self;

  switch (region->shape.any.type)
    {
    case RUT_INPUT_SHAPE_TYPE_RECTANGLE:
      region->shape.rectangle.x1 = region->shape.rectangle.x0 + width;
      region->shape.rectangle.y1 = region->shape.rectangle.y0 + height;
      break;
    case RUT_INPUT_SHAPE_TYPE_CIRCLE:
      region->shape.circle.r = MAX (width, height) / 2.0f;
      region->shape.circle.r_squared =
        region->shape.circle.r * region->shape.circle.r;
      break;
    }
}

static void
_rut_input_region_get_size (RutObject *self,
                            float *width,
                            float *height)
{
  RutInputRegion *region = self;

  switch (region->shape.any.type)
    {
    case RUT_INPUT_SHAPE_TYPE_RECTANGLE:
      *width = region->shape.rectangle.x1 - region->shape.rectangle.x0;
      *height = region->shape.rectangle.y1 - region->shape.rectangle.y0;
      break;
    case RUT_INPUT_SHAPE_TYPE_CIRCLE:
      *width = *height = region->shape.circle.r * 2;
      break;
    }
}

RutType rut_input_region_type;

static void
_rut_input_region_init_type (void)
{
  static RutGraphableVTable graphable_vtable = {
      NULL, /* child remove */
      NULL, /* child add */
      NULL /* parent changed */
  };

  static RutSizableVTable sizable_vtable = {
      _rut_input_region_set_size,
      _rut_input_region_get_size,
      rut_simple_sizable_get_preferred_width,
      rut_simple_sizable_get_preferred_height,
      NULL /* add_preferred_size_callback */
  };

  static RutPickableVTable pickable_vtable = {
      .pick = _rut_input_region_pick,
  };

  static RutInputableVTable inputable_vtable = {
      .handle_event = _rut_input_region_handle_event
  };

  RutType *type = &rut_input_region_type;
#define TYPE RutInputRegion

  rut_type_init (type, G_STRINGIFY (TYPE));
  rut_type_add_refable (type, ref_count, _rut_input_region_free);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (TYPE, graphable),
                          &graphable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &sizable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_PICKABLE,
                          0, /* no implied properties */
                          &pickable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_INPUTABLE,
                          0, /* no implied properties */
                          &inputable_vtable);

#undef TYPE
}

static RutInputRegion *
_rut_input_region_new_common (RutInputRegionCallback callback,
                              void *user_data)
{
  RutInputRegion *region = rut_object_alloc0 (RutInputRegion,
                                              &rut_input_region_type,
                                              _rut_input_region_init_type);

  region->ref_count = 1;

  rut_graphable_init (region);

  region->callback = callback;
  region->user_data = user_data;

  return region;
}

RutInputRegion *
rut_input_region_new_rectangle (float x0,
                                float y0,
                                float x1,
                                float y1,
                                RutInputRegionCallback callback,
                                void *user_data)
{
  RutInputRegion *region = _rut_input_region_new_common (callback, user_data);

  rut_input_region_set_rectangle (region, x0, y0, x1, y1);

  return region;
}

RutInputRegion *
rut_input_region_new_circle (float x0,
                             float y0,
                             float radius,
                             RutInputRegionCallback callback,
                             void *user_data)
{
  RutInputRegion *region = _rut_input_region_new_common (callback, user_data);

  rut_input_region_set_circle (region, x0, y0, radius);

  return region;
}

void
rut_input_region_set_rectangle (RutInputRegion *region,
                                float x0,
                                float y0,
                                float x1,
                                float y1)
{
  region->shape.any.type = RUT_INPUT_SHAPE_TYPE_RECTANGLE;
  region->shape.rectangle.x0 = x0;
  region->shape.rectangle.y0 = y0;
  region->shape.rectangle.x1 = x1;
  region->shape.rectangle.y1 = y1;
}

void
rut_input_region_set_circle (RutInputRegion *region,
                             float x,
                             float y,
                             float radius)
{
  region->shape.any.type = RUT_INPUT_SHAPE_TYPE_CIRCLE;
  region->shape.circle.x = x;
  region->shape.circle.y = y;
  region->shape.circle.r = radius;
  region->shape.circle.r_squared = radius * radius;
}

void
rut_input_region_set_hud_mode (RutInputRegion *region,
                               bool hud_mode)
{
  region->hud_mode = hud_mode;
}
