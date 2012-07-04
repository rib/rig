/*
 * Rig
 *
 * Copyright (C) 2012  Intel Corporation
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

#include <glib.h>

#include <cogl/cogl.h>

#include "rig-camera.h"
#include "rig-context.h"
#include "rig-global.h"
#include "rig-camera-private.h"

typedef struct
{
  float x, y, z, w;
  float r, g, b, a;
} RigVertex4C4;

static CoglUserDataKey fb_camera_key;

static RigPropertySpec _rig_camera_prop_specs[] = {
  {
    .name = "mode",
    .nick = "Mode",
    .type = RIG_PROPERTY_TYPE_ENUM,
    .getter = rig_camera_get_projection_mode,
    .setter = rig_camera_set_projection_mode,
    .flags = RIG_PROPERTY_FLAG_READWRITE |
      RIG_PROPERTY_FLAG_VALIDATE,
    .validation = { .ui_enum = &_rig_projection_ui_enum }
  },
  {
    .name = "fov",
    .nick = "Field Of View",
    .type = RIG_PROPERTY_TYPE_INTEGER,
    .getter = rig_camera_get_field_of_view,
    .setter = rig_camera_set_field_of_view,
    .flags = RIG_PROPERTY_FLAG_READWRITE |
      RIG_PROPERTY_FLAG_VALIDATE,
    .validation = { .int_range = { .min = 1, .max = 135}}
  },
  {
    .name = "near",
    .nick = "Near Plane",
    .type = RIG_PROPERTY_TYPE_FLOAT,
    .getter = rig_camera_get_near_plane,
    .setter = rig_camera_set_near_plane,
    .flags = RIG_PROPERTY_FLAG_READWRITE
  },
  {
    .name = "far",
    .nick = "Far Plane",
    .type = RIG_PROPERTY_TYPE_FLOAT,
    .getter = rig_camera_get_far_plane,
    .setter = rig_camera_set_far_plane,
    .flags = RIG_PROPERTY_FLAG_READWRITE
  },
  {
    .name = "background_color",
    .nick = "Background Color",
    .type = RIG_PROPERTY_TYPE_COLOR,
    .getter = rig_camera_get_background_color,
    .setter = rig_camera_set_background_color,
    .flags = RIG_PROPERTY_FLAG_READWRITE
  },
  /* FIXME: Figure out how to expose the orthographic coordinates as
   * properties? */
  { 0 }
};


static void
_rig_camera_free (void *object)
{
  RigCamera *camera = object;
  GList *l;

  cogl_object_unref (camera->fb);

  for (l = camera->input_regions; l; l = l->next)
    rig_ref_countable_unref (l->data);
  g_list_free (camera->input_regions);

  g_slice_free (RigCamera, object);
}

RigRefCountableVTable _rig_camera_ref_countable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_camera_free
};

#if 0
static RigGraphableVTable _rig_camera_graphable_vtable = {
  NULL, /* child remove */
  NULL, /* child add */
  NULL /* parent changed */
};

static RigTransformableVTable _rig_camera_transformable_vtable = {
  rig_camera_get_view_transform
};
#endif

static void
draw_frustum (RigCamera *camera,
              CoglFramebuffer *fb)
{
  RigVertex4C4 vertices[8] = {
    /* near plane in projection space */
    {-1, -1, -1, 1, /* position */ .8, .8, .8, .8 /* color */ },
    { 1, -1, -1, 1,                .8, .8, .8, .8             },
    { 1,  1, -1, 1,                .8, .8, .8, .8             },
    {-1,  1, -1, 1,                .8, .8, .8, .8             },
    /* far plane in projection space */
    {-1, -1, 1, 1,  /* position */ .3, .3, .3, .3 /* color */ },
    { 1, -1, 1, 1,                 .3, .3, .3, .3             },
    { 1,  1, 1, 1,                 .3, .3, .3, .3             },
    {-1,  1, 1, 1,                 .3, .3, .3, .3             }
  };
  const CoglMatrix *projection_inv;
  CoglAttributeBuffer *attribute_buffer;
  CoglAttribute *attributes[2];
  CoglPrimitive *primitive;
  CoglPipeline *pipeline;
  CoglIndices *indices;
  unsigned char indices_data[24] = {
      0,1, 1,2, 2,3, 3,0,
      4,5, 5,6, 6,7, 7,4,
      0,4, 1,5, 2,6, 3,7
  };
  CoglDepthState depth_state;
  int i;

  projection_inv = rig_camera_get_inverse_projection (camera);

  for (i = 0; i < 8; i++)
    {
      cogl_matrix_transform_point (projection_inv,
                                   &vertices[i].x,
                                   &vertices[i].y,
                                   &vertices[i].z,
                                   &vertices[i].w);
      vertices[i].x /= vertices[i].w;
      vertices[i].y /= vertices[i].w;
      vertices[i].z /= vertices[i].w;
      vertices[i].w /= 1.0f;
    }

  attribute_buffer = cogl_attribute_buffer_new (rig_cogl_context,
                                                8 * sizeof (RigVertex4C4),
                                                vertices);

  attributes[0] = cogl_attribute_new (attribute_buffer,
                                      "cogl_position_in",
                                      sizeof (RigVertex4C4),
                                      offsetof (RigVertex4C4, x),
                                      3,
                                      COGL_ATTRIBUTE_TYPE_FLOAT);

  attributes[1] = cogl_attribute_new (attribute_buffer,
                                      "cogl_color_in",
                                      sizeof (RigVertex4C4),
                                      offsetof (RigVertex4C4, r),
                                      4,
                                      COGL_ATTRIBUTE_TYPE_FLOAT);

  indices = cogl_indices_new (rig_cogl_context,
                              COGL_INDICES_TYPE_UNSIGNED_BYTE,
                              indices_data,
                              G_N_ELEMENTS (indices_data));

  primitive = cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_LINES,
                                                  8, attributes, 2);

  cogl_primitive_set_indices (primitive, indices, G_N_ELEMENTS(indices_data));

  cogl_object_unref (attribute_buffer);
  cogl_object_unref (attributes[0]);
  cogl_object_unref (attributes[1]);
  cogl_object_unref (indices);

  pipeline = cogl_pipeline_new (rig_cogl_context);

  /* enable depth testing */
  cogl_depth_state_init (&depth_state);
  cogl_depth_state_set_test_enabled (&depth_state, TRUE);
  cogl_pipeline_set_depth_state (pipeline, &depth_state, NULL);

  cogl_framebuffer_draw_primitive (fb, pipeline, primitive);

  cogl_object_unref (primitive);
  cogl_object_unref (pipeline);
}

typedef struct _CameraFlushState
{
  RigCamera *current_camera;
  unsigned int transform_age;
  CoglBool in_frame;
} CameraFlushState;

static void
free_camera_flush_state (void *user_data)
{
  CameraFlushState *state = user_data;
  g_slice_free (CameraFlushState, state);
}

static void
_rig_camera_flush_transforms (RigCamera *camera)
{
  const CoglMatrix *projection;
  CoglFramebuffer *fb = camera->fb;
  CameraFlushState *state =
    cogl_object_get_user_data (COGL_OBJECT (fb), &fb_camera_key);
  if (!state)
    {
      state = g_slice_new (CameraFlushState);
      state->in_frame = FALSE;
      cogl_object_set_user_data (COGL_OBJECT (fb),
                                 &fb_camera_key,
                                 state,
                                 free_camera_flush_state);
    }
  else if (state->current_camera == camera &&
           camera->transform_age == state->transform_age)
    {
      state->in_frame = TRUE;
      return;
    }

  if (state->in_frame)
    {
      g_warning ("Un-balanced rig_camera_flush/_end calls: "
                 "repeat _flush() calls before _end()");
    }

  cogl_framebuffer_set_viewport (fb,
                                 camera->viewport[0],
                                 camera->viewport[1],
                                 camera->viewport[2],
                                 camera->viewport[3]);

  projection = rig_camera_get_projection (camera);
  cogl_framebuffer_set_projection_matrix (fb, projection);

  cogl_framebuffer_set_modelview_matrix (fb, &camera->view);

  state->current_camera = camera;
  state->transform_age = camera->transform_age;
  state->in_frame = TRUE;
}

void
rig_camera_draw (RigObject *object,
                 CoglFramebuffer *fb)
{
  RigCamera *camera = object;

  if (fb != camera->fb)
    {
      /* When the fb we have to draw to is not the one this camera is
       * responsible for, we can draw its frustum to visualize it */
      draw_frustum (camera, fb);
      return;
    }

  _rig_camera_flush_transforms (camera);

  if (camera->clear_fb)
    {
      cogl_framebuffer_clear4f (fb,
                                COGL_BUFFER_BIT_COLOR |
                                COGL_BUFFER_BIT_DEPTH |
                                COGL_BUFFER_BIT_STENCIL,
                                cogl_color_get_red_float (&camera->bg_color),
                                cogl_color_get_green_float (&camera->bg_color),
                                cogl_color_get_blue_float (&camera->bg_color),
                                cogl_color_get_alpha_float (&camera->bg_color));
    }
}

static RigComponentableVTable _rig_camera_componentable_vtable = {
  .draw = rig_camera_draw
};

static RigIntrospectableVTable _rig_camera_introspectable_vtable = {
  rig_simple_introspectable_lookup_property,
  rig_simple_introspectable_foreach_property
};


RigType rig_camera_type;

void
_rig_camera_init_type (void)
{
  rig_type_init (&rig_camera_type);
  rig_type_add_interface (&rig_camera_type,
                           RIG_INTERFACE_ID_REF_COUNTABLE,
                           offsetof (RigCamera, ref_count),
                           &_rig_camera_ref_countable);
  rig_type_add_interface (&rig_camera_type,
                           RIG_INTERFACE_ID_COMPONENTABLE,
                           offsetof (RigCamera, component),
                           &_rig_camera_componentable_vtable);
  rig_type_add_interface (&rig_camera_type,
                          RIG_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rig_camera_introspectable_vtable);
  rig_type_add_interface (&rig_camera_type,
                          RIG_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RigCamera, introspectable),
                          NULL); /* no implied vtable */

#if 0
  rig_type_add_interface (&rig_camera_type,
                           RIG_INTERFACE_ID_GRAPHABLE,
                           offsetof (RigCamera, graphable),
                           &_rig_camera_graphable_vtable);
  rig_type_add_interface (&rig_camera_type,
                           RIG_INTERFACE_ID_TRANSFORMABLE,
                           0,
                           &_rig_camera_transformable_vtable);
#endif
}

RigCamera *
rig_camera_new (RigContext *ctx, CoglFramebuffer *framebuffer)
{
  RigCamera *camera = g_slice_new0 (RigCamera);
  int width = cogl_framebuffer_get_width (framebuffer);
  int height = cogl_framebuffer_get_height (framebuffer);

  camera->ctx = rig_ref_countable_ref (ctx);

  rig_object_init (&camera->_parent, &rig_camera_type);

  camera->ref_count = 1;

  camera->component.type = RIG_COMPONENT_TYPE_CAMERA;

  rig_camera_set_background_color4f (camera, 0, 0, 0, 1);
  camera->clear_fb = TRUE;

  //rig_graphable_init (RIG_OBJECT (camera));

  camera->orthographic = TRUE;
  camera->x1 = 0;
  camera->y1 = 0;
  camera->x2 = width;
  camera->y2 = height;
  camera->near = -1;
  camera->far = 100;

  camera->projection_cache_age = -1;
  camera->inverse_projection_age = -1;

  cogl_matrix_init_identity (&camera->view);
  camera->inverse_view_age = -1;

  camera->viewport[2] = width;
  camera->viewport[3] = height;

  camera->transform_age = 0;

  cogl_matrix_init_identity (&camera->input_transform);

  camera->fb = cogl_object_ref (framebuffer);

  camera->frame = 0;
  camera->timer = g_timer_new ();
  g_timer_start (camera->timer);

  rig_simple_introspectable_init (camera,
                                  _rig_camera_prop_specs,
                                  camera->properties);

  return camera;
}

void
rig_camera_set_background_color4f (RigCamera *camera,
                                   float red,
                                   float green,
                                   float blue,
                                   float alpha)
{
  cogl_color_init_from_4f (&camera->bg_color,
                           red, green, blue, alpha);
  rig_property_dirty (&camera->ctx->property_ctx,
                      &camera->properties[RIG_CAMERA_PROP_BG_COLOR]);
}

void
rig_camera_set_background_color (RigCamera *camera,
                                 const CoglColor *color)
{
  camera->bg_color = *color;
  rig_property_dirty (&camera->ctx->property_ctx,
                      &camera->properties[RIG_CAMERA_PROP_BG_COLOR]);
}

void
rig_camera_get_background_color (RigCamera *camera,
                                 CoglColor *color)
{
  *color = camera->bg_color;
}

void
rig_camera_set_clear (RigCamera *camera,
                      CoglBool clear)
{
  if (clear)
    camera->clear_fb = TRUE;
  else
    camera->clear_fb = FALSE;
}

CoglFramebuffer *
rig_camera_get_framebuffer (RigCamera *camera)
{
  return camera->fb;
}

void
rig_camera_set_framebuffer (RigCamera *camera,
                            CoglFramebuffer *framebuffer)
{
  if (camera->fb == framebuffer)
    return;

  cogl_object_unref (camera->fb);
  camera->fb = cogl_object_ref (framebuffer);
}

void
rig_camera_set_viewport (RigCamera *camera,
                         float x,
                         float y,
                         float width,
                         float height)
{
  if (camera->viewport[0] == x &&
      camera->viewport[1] == y &&
      camera->viewport[2] == width &&
      camera->viewport[3] == height)
    return;

  camera->viewport[0] = x;
  camera->viewport[1] = y;
  camera->viewport[2] = width;
  camera->viewport[3] = height;

  camera->transform_age++;
}

const float *
rig_camera_get_viewport (RigCamera *camera)
{
  return camera->viewport;
}

const CoglMatrix *
rig_camera_get_projection (RigCamera *camera)
{
  if (G_UNLIKELY (camera->projection_cache_age != camera->projection_age))
    {
      cogl_matrix_init_identity (&camera->projection);

      if (camera->orthographic)
        {
          cogl_matrix_orthographic (&camera->projection,
                                    camera->x1,
                                    camera->y1,
                                    camera->x2,
                                    camera->y2,
                                    camera->near,
                                    camera->far);
        }
      else
        {
          float aspect_ratio = camera->viewport[2] / camera->viewport[3];
          cogl_matrix_perspective (&camera->projection,
                                   camera->fov,
                                   aspect_ratio,
                                   camera->near,
                                   camera->far);
        }

      camera->projection_cache_age = camera->projection_age;
    }

  return &camera->projection;
}

void
rig_camera_set_near_plane (RigCamera *camera,
                           float near)
{
  if (camera->near == near)
    return;

  camera->near = near;
  rig_property_dirty (&camera->ctx->property_ctx,
                      &camera->properties[RIG_CAMERA_PROP_NEAR]);
  camera->projection_age++;
  camera->transform_age++;
}

float
rig_camera_get_near_plane (RigCamera *camera)
{
  return camera->near;
}

void
rig_camera_set_far_plane (RigCamera *camera,
                          float far)
{
  if (camera->far == far)
    return;

  camera->far = far;
  rig_property_dirty (&camera->ctx->property_ctx,
                      &camera->properties[RIG_CAMERA_PROP_FAR]);
  camera->projection_age++;
  camera->transform_age++;
}

float
rig_camera_get_far_plane (RigCamera *camera)
{
  return camera->far;
}

RigProjection
rig_camera_get_projection_mode (RigCamera *camera)
{
  if (camera->orthographic)
    return RIG_PROJECTION_ORTHOGRAPHIC;
  else
    return RIG_PROJECTION_PERSPECTIVE;
}

void
rig_camera_set_projection_mode (RigCamera  *camera,
                                RigProjection projection)
{
  CoglBool orthographic;

  if (projection == RIG_PROJECTION_ORTHOGRAPHIC)
    orthographic = TRUE;
  else
    orthographic = FALSE;

  if (orthographic != camera->orthographic)
    {
      camera->orthographic = orthographic;
      rig_property_dirty (&camera->ctx->property_ctx,
                          &camera->properties[RIG_CAMERA_PROP_MODE]);
      camera->projection_age++;
      camera->transform_age++;
    }
}

void
rig_camera_set_field_of_view (RigCamera *camera,
                              float fov)
{
  if (camera->fov == fov)
    return;

  camera->fov = fov;
  rig_property_dirty (&camera->ctx->property_ctx,
                      &camera->properties[RIG_CAMERA_PROP_FOV]);
  if (!camera->orthographic)
    {
      camera->projection_age++;
      camera->transform_age++;
    }
}

float
rig_camera_get_field_of_view (RigCamera *camera)
{
  return camera->fov;
}

void
rig_camera_set_orthographic_coordinates (RigCamera *camera,
                                         float x1,
                                         float y1,
                                         float x2,
                                         float y2)
{
  if (camera->x1 == x1 &&
      camera->y1 == y1 &&
      camera->x2 == x2 &&
      camera->y2 == y2)
    return;

  camera->x1 = x1;
  camera->y1 = y1;
  camera->x2 = x2;
  camera->y2 = y2;

  if (camera->orthographic)
    camera->projection_age++;
}

const CoglMatrix *
rig_camera_get_inverse_projection (RigCamera *camera)
{
  if (camera->inverse_projection_age == camera->projection_age)
    return &camera->inverse_projection;

  if (!cogl_matrix_get_inverse (&camera->projection,
                                &camera->inverse_projection))
    return NULL;

  camera->inverse_projection_age = camera->projection_age;
  return &camera->inverse_projection;
}

void
rig_camera_set_view_transform (RigCamera *camera,
                               const CoglMatrix *view)
{
  camera->view = *view;

  camera->view_age++;
  camera->transform_age++;

  /* XXX: we have no way to assert that we are at the bottom of the
   * matrix stack at this point, so this might do bad things...
   */
  //cogl_framebuffer_set_modelview_matrix (camera->fb,
  //                                       &camera->view);
}

const CoglMatrix *
rig_camera_get_view_transform (RigCamera *camera)
{
  return &camera->view;
}

const CoglMatrix *
rig_camera_get_inverse_view_transform (RigCamera *camera)
{
  if (camera->inverse_view_age == camera->view_age)
    return &camera->inverse_view;

  if (!cogl_matrix_get_inverse (&camera->view,
                                &camera->inverse_view))
    return NULL;

  camera->inverse_view_age = camera->view_age;
  return &camera->inverse_view;
}

void
rig_camera_set_input_transform (RigCamera *camera,
                                const CoglMatrix *input_transform)
{
  camera->input_transform = *input_transform;
}

void
rig_camera_add_input_region (RigCamera *camera,
                             RigInputRegion *region)
{
  g_print ("add input region %p, %p\n", camera, region);
  camera->input_regions = g_list_prepend (camera->input_regions, region);
}

void
rig_camera_remove_input_region (RigCamera *camera,
                                RigInputRegion *region)
{
  camera->input_regions = g_list_remove (camera->input_regions, region);
}

#if 0
typedef struct _RigCameraInputCallbackState
{
  RigInputCallback callback;
  void *user_data;
} RigCameraInputCallbackState;

RigInputEventStatus
_rig_camera_input_callback_wrapper (RigCameraInputCallbackState *state,
                                    RigInputEvent *event)
{
  RigCamera *camera = state->camera;
  float *viewport = camera->viewport;

  cogl_matrix_translate (event->input_transform,
                         viewport[0], viewport[1], 0);
  cogl_matrix_scale (event->input_transform,
                     cogl_framebuffer_get_width (camera->fb) / viewport[2],
                     cogl_framebuffer_get_height (camera->fb) / viewport[3]);

  return state->callback (event, state->user_data);
}

void
rig_camera_add_input_callback (RigCamera *camera,
                               RigInputCallback callback,
                               void *user_data)
{
  RigCameraInputCallbackState *state = g_slice_new (RigCameraInputCallbackState);
  state->camera = camera;
  state->callback = callback;
  state->user_data = user_data;
  camera->input_callbacks = g_list_prepend (camera->input_callbacks, state);
}
#endif

CoglBool
rig_camera_transform_window_coordinate (RigCamera *camera,
                                        float *x,
                                        float *y)
{
  float *viewport = camera->viewport;
  *x -= viewport[0];
  *y -= viewport[1];

  if (*x < 0 || *x >= viewport[2] || *y < 0 || *y >= viewport[3])
    return FALSE;
  else
    return TRUE;
}

void
rig_camera_unproject_coord (RigCamera *camera,
                            const CoglMatrix *modelview,
                            const CoglMatrix *inverse_modelview,
                            float object_coord_z,
                            float *x,
                            float *y)
{
  const CoglMatrix *projection = rig_camera_get_projection (camera);
  const CoglMatrix *inverse_projection =
    rig_camera_get_inverse_projection (camera);
  //float z;
  float ndc_x, ndc_y, ndc_z, ndc_w;
  float eye_x, eye_y, eye_z, eye_w;
  const float *viewport = rig_camera_get_viewport (camera);

  /* Convert item z into NDC z */
  {
    //float x = 0, y = 0, z = 0, w = 1;
    float z = 0, w = 1;
    float tmp_x, tmp_y, tmp_z;
    const CoglMatrix *m = modelview;

    tmp_x = m->xw;
    tmp_y = m->yw;
    tmp_z = m->zw;

    m = projection;
    z = m->zx * tmp_x + m->zy * tmp_y + m->zz * tmp_z + m->zw;
    w = m->wx * tmp_x + m->wy * tmp_y + m->wz * tmp_z + m->ww;

    ndc_z = z / w;
  }

  /* Undo the Viewport transform, putting us in Normalized Device Coords */
  ndc_x = (*x - viewport[0]) * 2.0f / viewport[2] - 1.0f;
  ndc_y = ((viewport[3] - 1 + viewport[1] - *y) * 2.0f / viewport[3] - 1.0f);

  /* Undo the Projection, putting us in Eye Coords. */
  ndc_w = 1;
  cogl_matrix_transform_point (inverse_projection,
                               &ndc_x, &ndc_y, &ndc_z, &ndc_w);
  eye_x = ndc_x / ndc_w;
  eye_y = ndc_y / ndc_w;
  eye_z = ndc_z / ndc_w;
  eye_w = 1;

  /* Undo the Modelview transform, putting us in Object Coords */
  cogl_matrix_transform_point (inverse_modelview,
                               &eye_x,
                               &eye_y,
                               &eye_z,
                               &eye_w);

  *x = eye_x;
  *y = eye_y;
  //*z = eye_z;
}

void
rig_camera_flush (RigCamera *camera)
{
  rig_camera_draw (camera, camera->fb);
}

void
rig_camera_end_frame (RigCamera *camera)
{
  double elapsed;
  CameraFlushState *state =
    cogl_object_get_user_data (COGL_OBJECT (camera->fb), &fb_camera_key);
  if (state)
    {
      if (state->in_frame != TRUE)
        g_warning ("Un-balanced rig_camera_flush/end frame calls. "
                   "_end before _flush");
      state->in_frame = FALSE;
    }
  camera->frame++;

  elapsed = g_timer_elapsed (camera->timer, NULL);
  if (elapsed > 1.0)
    {
      g_print ("fps = %f (camera = %p)\n",
               camera->frame / elapsed,
               camera);
      g_timer_start (camera->timer);
      camera->frame = 0;
    }
}


