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

#include <config.h>

#include <glib.h>

#include <cogl/cogl.h>
#include <math.h>

#include <rut.h>

#include "rig-camera.h"
#include "rig-engine.h"

typedef struct
{
  float x, y, z, w;
} RutVertex4;

static CoglUserDataKey fb_camera_key;

struct _RigCamera
{
  RutObjectBase _base;

  RigEngine *engine;

  RutCameraProps props;

  RutComponentableProps component;

  RutIntrospectableProps introspectable;
  RutProperty properties[RIG_CAMERA_N_PROPS];
};

typedef struct _CameraFlushState
{
  RigCamera *current_camera;
  unsigned int transform_age;
} CameraFlushState;

static void
free_camera_flush_state (void *user_data)
{
  CameraFlushState *state = user_data;
  g_slice_free (CameraFlushState, state);
}

static RutObject *
_rig_camera_copy (RutObject *obj)
{
  RigCamera *camera = obj;
  RigCamera *copy = rig_camera_new (camera->engine,
                                    -1, /* ortho/vp width */
                                    -1, /* ortho/vp height */
                                    camera->props.fb); /* may be NULL */

  copy->props.clear_fb = camera->props.clear_fb;

  copy->props.x1 = camera->props.x1;
  copy->props.y1 = camera->props.y1;
  copy->props.x2 = camera->props.x2;
  copy->props.y2 = camera->props.y2;
  copy->props.orthographic = camera->props.orthographic;

  copy->props.view = camera->props.view;

  /* TODO: copy input regions */

  rut_introspectable_copy_properties (&camera->engine->ctx->property_ctx,
                                      camera, copy);

  return copy;
}

void
rig_camera_set_background_color4f (RutObject *object,
                                   float red,
                                   float green,
                                   float blue,
                                   float alpha)
{
  RigCamera *camera = object;
  cogl_color_init_from_4f (&camera->props.bg_color,
                           red, green, blue, alpha);
  rut_property_dirty (&camera->engine->ctx->property_ctx,
                      &camera->properties[RIG_CAMERA_PROP_BG_COLOR]);
}

void
rig_camera_set_background_color (RutObject *obj,
                                 const CoglColor *color)
{
  RigCamera *camera = obj;

  camera->props.bg_color = *color;
  rut_property_dirty (&camera->engine->ctx->property_ctx,
                      &camera->properties[RIG_CAMERA_PROP_BG_COLOR]);
}

const CoglColor *
rig_camera_get_background_color (RutObject *obj)
{
  RigCamera *camera = obj;

  return &camera->props.bg_color;
}

void
rig_camera_set_clear (RutObject *object,
                      bool clear)
{
  RigCamera *camera = object;
  if (clear)
    camera->props.clear_fb = TRUE;
  else
    camera->props.clear_fb = FALSE;
}

CoglFramebuffer *
rig_camera_get_framebuffer (RutObject *object)
{
  RigCamera *camera = object;
  return camera->props.fb;
}

void
rig_camera_set_framebuffer (RutObject *object,
                            CoglFramebuffer *framebuffer)
{
  RigCamera *camera = object;
  if (camera->props.fb == framebuffer)
    return;

  if (camera->props.fb)
    cogl_object_unref (camera->props.fb);

  camera->props.fb = cogl_object_ref (framebuffer);
}

static void
_rig_camera_set_viewport (RutObject *object,
                          float x,
                          float y,
                          float width,
                          float height)
{
  RigCamera *camera = object;
  if (camera->props.viewport[0] == x &&
      camera->props.viewport[1] == y &&
      camera->props.viewport[2] == width &&
      camera->props.viewport[3] == height)
    return;

  /* If the aspect ratio changes we may need to update the projection
   * matrix... */
  if ((!camera->props.orthographic) &&
      (camera->props.viewport[2] / camera->props.viewport[3]) != (width / height))
    camera->props.projection_age++;

  camera->props.viewport[0] = x;
  camera->props.viewport[1] = y;
  camera->props.viewport[2] = width;
  camera->props.viewport[3] = height;

  camera->props.transform_age++;
}

void
rig_camera_set_viewport (RutObject *object,
                         float x,
                         float y,
                         float width,
                         float height)
{
  RigCamera *camera = object;
  _rig_camera_set_viewport (camera, x, y, width, height);
  rut_property_dirty (&camera->engine->ctx->property_ctx,
                      &camera->properties[RIG_CAMERA_PROP_VIEWPORT_X]);
  rut_property_dirty (&camera->engine->ctx->property_ctx,
                      &camera->properties[RIG_CAMERA_PROP_VIEWPORT_Y]);
  rut_property_dirty (&camera->engine->ctx->property_ctx,
                      &camera->properties[RIG_CAMERA_PROP_VIEWPORT_WIDTH]);
  rut_property_dirty (&camera->engine->ctx->property_ctx,
                      &camera->properties[RIG_CAMERA_PROP_VIEWPORT_HEIGHT]);
}

void
rig_camera_set_viewport_x (RutObject *obj,
                           float x)
{
  RigCamera *camera = obj;

  _rig_camera_set_viewport (camera,
                            x,
                            camera->props.viewport[1],
                            camera->props.viewport[2],
                            camera->props.viewport[3]);
  rut_property_dirty (&camera->engine->ctx->property_ctx,
                      &camera->properties[RIG_CAMERA_PROP_VIEWPORT_X]);
}

void
rig_camera_set_viewport_y (RutObject *obj,
                           float y)
{
  RigCamera *camera = obj;

  _rig_camera_set_viewport (camera,
                            camera->props.viewport[0],
                            y,
                            camera->props.viewport[2],
                            camera->props.viewport[3]);
  rut_property_dirty (&camera->engine->ctx->property_ctx,
                      &camera->properties[RIG_CAMERA_PROP_VIEWPORT_Y]);
}

void
rig_camera_set_viewport_width (RutObject *obj,
                               float width)
{
  RigCamera *camera = obj;

  _rig_camera_set_viewport (camera,
                            camera->props.viewport[0],
                            camera->props.viewport[1],
                            width,
                            camera->props.viewport[3]);
  rut_property_dirty (&camera->engine->ctx->property_ctx,
                      &camera->properties[RIG_CAMERA_PROP_VIEWPORT_WIDTH]);
}

void
rig_camera_set_viewport_height (RutObject *obj,
                                float height)
{
  RigCamera *camera = obj;

  _rig_camera_set_viewport (camera,
                            camera->props.viewport[0],
                            camera->props.viewport[1],
                            camera->props.viewport[2],
                            height);
  rut_property_dirty (&camera->engine->ctx->property_ctx,
                      &camera->properties[RIG_CAMERA_PROP_VIEWPORT_HEIGHT]);
}

const float *
rig_camera_get_viewport (RutObject *object)
{
  RigCamera *camera = object;
  return camera->props.viewport;
}

const CoglMatrix *
rig_camera_get_projection (RutObject *object)
{
  RigCamera *camera = object;
  if (G_UNLIKELY (camera->props.projection_cache_age != camera->props.projection_age))
    {
      cogl_matrix_init_identity (&camera->props.projection);

      if (camera->props.orthographic)
        {
          float x1, x2, y1, y2;

          if (camera->props.zoom != 1)
            {
              float center_x = camera->props.x1 + (camera->props.x2 - camera->props.x1) / 2.0;
              float center_y = camera->props.y1 + (camera->props.y2 - camera->props.y1) / 2.0;
              float inverse_scale = 1.0 / camera->props.zoom;
              float dx = (camera->props.x2 - center_x) * inverse_scale;
              float dy = (camera->props.y2 - center_y) * inverse_scale;

              camera->props.x1 = center_x - dx;
              camera->props.x2 = center_x + dx;
              camera->props.y1 = center_y - dy;
              camera->props.y2 = center_y + dy;
            }
          else
            {
              x1 = camera->props.x1;
              x2 = camera->props.x2;
              y1 = camera->props.y1;
              y2 = camera->props.y2;
            }

          cogl_matrix_orthographic (&camera->props.projection,
                                    x1, y1, x2, y2,
                                    camera->props.near,
                                    camera->props.far);
        }
      else
        {
          float aspect_ratio = camera->props.viewport[2] / camera->props.viewport[3];
          rut_util_matrix_scaled_perspective (&camera->props.projection,
                                              camera->props.fov,
                                              aspect_ratio,
                                              camera->props.near,
                                              camera->props.far,
                                              camera->props.zoom);
#if 0
          g_print ("fov=%f, aspect=%f, near=%f, far=%f, zoom=%f\n",
                   camera->props.fov,
                   aspect_ratio,
                   camera->props.near,
                   camera->props.far,
                   camera->props.zoom);
#endif
        }

      camera->props.projection_cache_age = camera->props.projection_age;
    }

  return &camera->props.projection;
}

void
rig_camera_set_near_plane (RutObject *obj,
                           float near)
{
  RigCamera *camera = obj;

  if (camera->props.near == near)
    return;

  camera->props.near = near;
  rut_property_dirty (&camera->engine->ctx->property_ctx,
                      &camera->properties[RIG_CAMERA_PROP_NEAR]);
  camera->props.projection_age++;
  camera->props.transform_age++;
}

float
rig_camera_get_near_plane (RutObject *obj)
{
  RigCamera *camera = obj;

  return camera->props.near;
}

void
rig_camera_set_far_plane (RutObject *obj,
                          float far)
{
  RigCamera *camera = obj;

  if (camera->props.far == far)
    return;

  camera->props.far = far;
  rut_property_dirty (&camera->engine->ctx->property_ctx,
                      &camera->properties[RIG_CAMERA_PROP_FAR]);
  camera->props.projection_age++;
  camera->props.transform_age++;
}

float
rig_camera_get_far_plane (RutObject *obj)
{
  RigCamera *camera = obj;

  return camera->props.far;
}

RutProjection
rig_camera_get_projection_mode (RutObject *object)
{
  RigCamera *camera = object;
  if (camera->props.orthographic)
    return RUT_PROJECTION_ORTHOGRAPHIC;
  else
    return RUT_PROJECTION_PERSPECTIVE;
}

void
rig_camera_set_projection_mode (RutObject *object,
                                RutProjection projection)
{
  RigCamera *camera = object;
  bool orthographic;

  if (projection == RUT_PROJECTION_ORTHOGRAPHIC)
    orthographic = TRUE;
  else
    orthographic = FALSE;

  if (orthographic != camera->props.orthographic)
    {
      camera->props.orthographic = orthographic;
      rut_property_dirty (&camera->engine->ctx->property_ctx,
                          &camera->properties[RIG_CAMERA_PROP_MODE]);
      camera->props.projection_age++;
      camera->props.transform_age++;
    }
}

void
rig_camera_set_field_of_view (RutObject *obj,
                              float fov)
{
  RigCamera *camera = obj;

  if (camera->props.fov == fov)
    return;

  camera->props.fov = fov;
  rut_property_dirty (&camera->engine->ctx->property_ctx,
                      &camera->properties[RIG_CAMERA_PROP_FOV]);
  if (!camera->props.orthographic)
    {
      camera->props.projection_age++;
      camera->props.transform_age++;
    }
}

float
rig_camera_get_field_of_view (RutObject *obj)
{
  RigCamera *camera = obj;

  return camera->props.fov;
}

void
rig_camera_set_orthographic_coordinates (RutObject *object,
                                         float x1,
                                         float y1,
                                         float x2,
                                         float y2)
{
  RigCamera *camera = object;
  if (camera->props.x1 == x1 &&
      camera->props.y1 == y1 &&
      camera->props.x2 == x2 &&
      camera->props.y2 == y2)
    return;

  camera->props.x1 = x1;
  camera->props.y1 = y1;
  camera->props.x2 = x2;
  camera->props.y2 = y2;

  if (camera->props.orthographic)
    camera->props.projection_age++;
}

const CoglMatrix *
rig_camera_get_inverse_projection (RutObject *object)
{
  RigCamera *camera = object;
  const CoglMatrix *projection;

  if (camera->props.inverse_projection_age == camera->props.projection_age)
    return &camera->props.inverse_projection;

  projection = rig_camera_get_projection (camera);

  if (!cogl_matrix_get_inverse (projection,
                                &camera->props.inverse_projection))
    return NULL;

  camera->props.inverse_projection_age = camera->props.projection_age;
  return &camera->props.inverse_projection;
}

void
rig_camera_set_view_transform (RutObject *object,
                               const CoglMatrix *view)
{
  RigCamera *camera = object;
  camera->props.view = *view;

  camera->props.view_age++;
  camera->props.transform_age++;

  /* XXX: we have no way to assert that we are at the bottom of the
   * matrix stack at this point, so this might do bad things...
   */
  //cogl_framebuffer_set_modelview_matrix (camera->props.fb,
  //                                       &camera->props.view);
}

const CoglMatrix *
rig_camera_get_view_transform (RutObject *object)
{
  RigCamera *camera = object;
  return &camera->props.view;
}

const CoglMatrix *
rig_camera_get_inverse_view_transform (RutObject *object)
{
  RigCamera *camera = object;
  if (camera->props.inverse_view_age == camera->props.view_age)
    return &camera->props.inverse_view;

  if (!cogl_matrix_get_inverse (&camera->props.view,
                                &camera->props.inverse_view))
    return NULL;

  camera->props.inverse_view_age = camera->props.view_age;
  return &camera->props.inverse_view;
}

void
rig_camera_set_input_transform (RutObject *object,
                                const CoglMatrix *input_transform)
{
  RigCamera *camera = object;
  camera->props.input_transform = *input_transform;
}

void
rig_camera_add_input_region (RutObject *object,
                             RutInputRegion *region)
{
  RigCamera *camera = object;
  if (g_list_find (camera->props.input_regions, region))
    return;

  rut_object_ref (region);
  camera->props.input_regions = g_list_prepend (camera->props.input_regions, region);
}

void
rig_camera_remove_input_region (RutObject *object,
                                RutInputRegion *region)
{
  RigCamera *camera = object;
  GList *link = g_list_find (camera->props.input_regions, region);
  if (link)
    {
      rut_object_unref (region);
      camera->props.input_regions =
        g_list_delete_link (camera->props.input_regions, link);
    }
}

bool
rig_camera_transform_window_coordinate (RutObject *object,
                                        float *x,
                                        float *y)
{
  RigCamera *camera = object;
  float *viewport = camera->props.viewport;
  *x -= viewport[0];
  *y -= viewport[1];

  if (*x < 0 || *x >= viewport[2] || *y < 0 || *y >= viewport[3])
    return FALSE;
  else
    return TRUE;
}

void
rig_camera_unproject_coord (RutObject *object,
                            const CoglMatrix *modelview,
                            const CoglMatrix *inverse_modelview,
                            float object_coord_z,
                            float *x,
                            float *y)
{
  RigCamera *camera = object;
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

static void
_rig_camera_flush_transforms (RigCamera *camera)
{
  const CoglMatrix *projection;
  CoglFramebuffer *fb = camera->props.fb;
  CameraFlushState *state;

  /* While a camera is in a suspended state then we don't expect to
   * _flush() and use that camera before it is restored. */
  g_return_if_fail (camera->props.suspended == FALSE);

  state = cogl_object_get_user_data (fb, &fb_camera_key);
  if (!state)
    {
      state = g_slice_new (CameraFlushState);
      cogl_object_set_user_data (fb,
                                 &fb_camera_key,
                                 state,
                                 free_camera_flush_state);
    }
  else if (state->current_camera == camera &&
           camera->props.transform_age == state->transform_age)
    goto done;

  if (camera->props.in_frame)
    {
      g_warning ("Un-balanced rig_camera_flush/_end calls: "
                 "repeat _flush() calls before _end()");
    }

  cogl_framebuffer_set_viewport (fb,
                                 camera->props.viewport[0],
                                 camera->props.viewport[1],
                                 camera->props.viewport[2],
                                 camera->props.viewport[3]);

  projection = rig_camera_get_projection (camera);
  cogl_framebuffer_set_projection_matrix (fb, projection);

  cogl_framebuffer_set_modelview_matrix (fb, &camera->props.view);

  state->current_camera = camera;
  state->transform_age = camera->props.transform_age;

done:
  camera->props.in_frame = TRUE;
}

void
rig_camera_flush (RutObject *object)
{
  RigCamera *camera = object;
  _rig_camera_flush_transforms (camera);

  if (camera->props.clear_fb)
    {
      cogl_framebuffer_clear4f (camera->props.fb,
                                COGL_BUFFER_BIT_COLOR |
                                COGL_BUFFER_BIT_DEPTH |
                                COGL_BUFFER_BIT_STENCIL,
                                cogl_color_get_red_float (&camera->props.bg_color),
                                cogl_color_get_green_float (&camera->props.bg_color),
                                cogl_color_get_blue_float (&camera->props.bg_color),
                                cogl_color_get_alpha_float (&camera->props.bg_color));
    }
}

void
rig_camera_end_frame (RutObject *object)
{
  RigCamera *camera = object;
  if (G_UNLIKELY (camera->props.in_frame != TRUE))
    g_warning ("Un-balanced rig_camera_flush/end frame calls. "
               "_end before _flush");
  camera->props.in_frame = FALSE;
}

void
rig_camera_set_focal_distance (RutObject *obj,
                               float focal_distance)
{
  RigCamera *camera = obj;

  if (camera->props.focal_distance == focal_distance)
    return;

  camera->props.focal_distance = focal_distance;

  rut_shell_queue_redraw (camera->engine->shell);

  rut_property_dirty (&camera->engine->ctx->property_ctx,
                      &camera->properties[RIG_CAMERA_PROP_FOCAL_DISTANCE]);
}

float
rig_camera_get_focal_distance (RutObject *obj)
{
  RigCamera *camera = obj;

  return camera->props.focal_distance;
}

void
rig_camera_set_depth_of_field (RutObject *obj,
                               float depth_of_field)
{
  RigCamera *camera = obj;

  if (camera->props.depth_of_field == depth_of_field)
    return;

  camera->props.depth_of_field = depth_of_field;

  rut_shell_queue_redraw (camera->engine->shell);

  rut_property_dirty (&camera->engine->ctx->property_ctx,
                      &camera->properties[RIG_CAMERA_PROP_FOCAL_DISTANCE]);
}

float
rig_camera_get_depth_of_field (RutObject *obj)
{
  RigCamera *camera = obj;

  return camera->props.depth_of_field;
}

void
rig_camera_suspend (RutObject *object)
{
  RigCamera *camera = object;
  CameraFlushState *state;

  /* There's not point suspending a frame that hasn't been flushed */
  g_return_if_fail (camera->props.in_frame == TRUE);

  g_return_if_fail (camera->props.suspended == FALSE);

  state = cogl_object_get_user_data (camera->props.fb, &fb_camera_key);

  /* We only expect to be saving a camera that has been flushed */
  g_return_if_fail (state != NULL);

  /* While the camera is in a suspended state we aren't expecting the
   * camera to be touched but we want to double check that at least
   * the transform hasn't been touched when we come to resume the
   * camera... */
  camera->props.at_suspend_transform_age = camera->props.transform_age;

  /* When we resume the camer we'll need to restore the modelview,
   * projection and viewport transforms. The easiest way for us to
   * handle restoring the modelview is to use the framebuffer's
   * matrix stack... */
  cogl_framebuffer_push_matrix (camera->props.fb);

  camera->props.suspended = TRUE;
  camera->props.in_frame = FALSE;
}

void
rig_camera_resume (RutObject *object)
{
  RigCamera *camera = object;
  CameraFlushState *state;
  CoglFramebuffer *fb = camera->props.fb;

  g_return_if_fail (camera->props.in_frame == FALSE);
  g_return_if_fail (camera->props.suspended == TRUE);

  /* While a camera is in a suspended state we don't expect the camera
   * to be touched so its transforms shouldn't have changed... */
  g_return_if_fail (camera->props.at_suspend_transform_age == camera->props.transform_age);

  state = cogl_object_get_user_data (fb, &fb_camera_key);

  /* We only expect to be restoring a camera that has been flushed
   * before */
  g_return_if_fail (state != NULL);

  cogl_framebuffer_pop_matrix (fb);

  /* If the save turned out to be redundant then we have nothing
   * else to restore... */
  if (state->current_camera == camera)
    goto done;

  cogl_framebuffer_set_viewport (fb,
                                 camera->props.viewport[0],
                                 camera->props.viewport[1],
                                 camera->props.viewport[2],
                                 camera->props.viewport[3]);

  cogl_framebuffer_set_projection_matrix (fb, &camera->props.projection);

  state->current_camera = camera;
  state->transform_age = camera->props.transform_age;

done:
  camera->props.in_frame = TRUE;
  camera->props.suspended = FALSE;
}

void
rig_camera_set_zoom (RutObject *object,
                     float zoom)
{
  RigCamera *camera = object;

  if (camera->props.zoom == zoom)
    return;

  camera->props.zoom = zoom;

  rut_shell_queue_redraw (camera->engine->shell);

  rut_property_dirty (&camera->engine->ctx->property_ctx,
                      &camera->properties[RIG_CAMERA_PROP_ZOOM]);

  camera->props.projection_age++;
  camera->props.transform_age++;
}

float
rig_camera_get_zoom (RutObject *object)
{
  RigCamera *camera = object;

  return camera->props.zoom;
}

RutContext *
rig_camera_get_context (RutObject *object)
{
  RigCamera *camera = object;
  return camera->engine->ctx;
}

CoglPrimitive *
rig_camera_create_frustum_primitive (RutObject *object)
{
  RigCamera *camera = object;
  CoglContext *cogl_context = camera->engine->ctx->cogl_context;
  RutVertex4 vertices[8] = {
    /* near plane in projection space */
    {-1, -1, -1, 1, },
    { 1, -1, -1, 1, },
    { 1,  1, -1, 1, },
    {-1,  1, -1, 1, },
    /* far plane in projection space */
    {-1, -1, 1, 1, },
    { 1, -1, 1, 1, },
    { 1,  1, 1, 1, },
    {-1,  1, 1, 1, }
  };
  const CoglMatrix *projection_inv;
  CoglAttributeBuffer *attribute_buffer;
  CoglAttribute *attributes[1];
  CoglPrimitive *primitive;
  CoglIndices *indices;
  unsigned char indices_data[24] = {
      0,1, 1,2, 2,3, 3,0,
      4,5, 5,6, 6,7, 7,4,
      0,4, 1,5, 2,6, 3,7
  };
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

  attribute_buffer = cogl_attribute_buffer_new (cogl_context,
                                                8 * sizeof (RutVertex4),
                                                vertices);

  attributes[0] = cogl_attribute_new (attribute_buffer,
                                      "cogl_position_in",
                                      sizeof (RutVertex4),
                                      offsetof (RutVertex4, x),
                                      3,
                                      COGL_ATTRIBUTE_TYPE_FLOAT);

  indices = cogl_indices_new (cogl_context,
                              COGL_INDICES_TYPE_UNSIGNED_BYTE,
                              indices_data,
                              G_N_ELEMENTS (indices_data));

  primitive = cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_LINES,
                                                  8, attributes, 1);

  cogl_primitive_set_indices (primitive, indices, G_N_ELEMENTS(indices_data));

  cogl_object_unref (attribute_buffer);
  cogl_object_unref (attributes[0]);
  cogl_object_unref (indices);

  return primitive;
}

static void
_rig_camera_free (void *object)
{
  RigCamera *camera = object;

#ifdef RIG_ENABLE_DEBUG
  {
    RutComponentableProps *component =
      rut_object_get_properties (object, RUT_TRAIT_ID_COMPONENTABLE);
    g_return_if_fail (component->entity == NULL);
  }
#endif

  if (camera->props.fb)
    cogl_object_unref (camera->props.fb);

  while (camera->props.input_regions)
    rig_camera_remove_input_region (camera, camera->props.input_regions->data);

  rut_introspectable_destroy (camera);

  rut_object_free (RigCamera, object);
}

static RutPropertySpec _rig_camera_prop_specs[] = {
  {
    .name = "mode",
    .nick = "Mode",
    .type = RUT_PROPERTY_TYPE_ENUM,
    .getter.any_type = rig_camera_get_projection_mode,
    .setter.any_type = rig_camera_set_projection_mode,
    .flags = RUT_PROPERTY_FLAG_READWRITE |
      RUT_PROPERTY_FLAG_VALIDATE,
    .validation = { .ui_enum = &_rut_projection_ui_enum }
  },
  {
    .name = "viewport_x",
    .nick = "Viewport X",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigCamera, props.viewport[0]),
    .setter.float_type = rig_camera_set_viewport_x
  },
  {
    .name = "viewport_y",
    .nick = "Viewport Y",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigCamera, props.viewport[1]),
    .setter.float_type = rig_camera_set_viewport_y
  },
  {
    .name = "viewport_width",
    .nick = "Viewport Width",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigCamera, props.viewport[2]),
    .setter.float_type = rig_camera_set_viewport_width
  },
  {
    .name = "viewport_height",
    .nick = "Viewport Height",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigCamera, props.viewport[3]),
    .setter.float_type = rig_camera_set_viewport_height
  },
  {
    .name = "fov",
    .nick = "Field Of View",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .getter.float_type = rig_camera_get_field_of_view,
    .setter.float_type = rig_camera_set_field_of_view,
    .flags = RUT_PROPERTY_FLAG_READWRITE |
      RUT_PROPERTY_FLAG_VALIDATE,
    .validation = { .int_range = { .min = 1, .max = 135}},
    .animatable = TRUE
  },
  {
    .name = "near",
    .nick = "Near Plane",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .getter.float_type = rig_camera_get_near_plane,
    .setter.float_type = rig_camera_set_near_plane,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = TRUE
  },
  {
    .name = "far",
    .nick = "Far Plane",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .getter.float_type = rig_camera_get_far_plane,
    .setter.float_type = rig_camera_set_far_plane,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = TRUE
  },
  {
    .name = "zoom",
    .nick = "Zoom",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RigCamera, props.zoom),
    .setter.float_type = rig_camera_set_zoom
  },
  {
    .name = "background_color",
    .nick = "Background Color",
    .type = RUT_PROPERTY_TYPE_COLOR,
    .getter.color_type = rig_camera_get_background_color,
    .setter.color_type = rig_camera_set_background_color,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = TRUE
  },
  {
    .name = "focal_distance",
    .nick = "Focal Distance",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .setter.float_type = rig_camera_set_focal_distance,
    .data_offset = offsetof (RigCamera, props.focal_distance),
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = TRUE
  },
  {
    .name = "depth_of_field",
    .nick = "Depth Of Field",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .setter.float_type = rig_camera_set_depth_of_field,
    .data_offset = offsetof (RigCamera, props.depth_of_field),
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = TRUE
  },

  /* FIXME: Figure out how to expose the orthographic coordinates as
   * properties? */
  { 0 }
};


RutType rig_camera_type;

void
_rig_camera_init_type (void)
{

  static RutComponentableVTable componentable_vtable = {
    .copy = _rig_camera_copy
  };

  /* TODO: reduce this size of this vtable and go straight to the
   * props for more things...
   */
  static RutCameraVTable camera_vtable = {
      .get_context = rig_camera_get_context,
      .set_background_color4f  = rig_camera_set_background_color4f,
      .set_background_color = rig_camera_set_background_color,
      .set_clear = rig_camera_set_clear,
      .set_framebuffer = rig_camera_set_framebuffer,
      .set_viewport = rig_camera_set_viewport,
      .set_viewport_x = rig_camera_set_viewport_x,
      .set_viewport_y = rig_camera_set_viewport_y,
      .set_viewport_width = rig_camera_set_viewport_width,
      .set_viewport_height = rig_camera_set_viewport_height,
      .get_projection = rig_camera_get_projection,
      .set_near_plane = rig_camera_set_near_plane,
      .set_far_plane = rig_camera_set_far_plane,
      .get_projection_mode = rig_camera_get_projection_mode,
      .set_projection_mode = rig_camera_set_projection_mode,
      .set_field_of_view = rig_camera_set_field_of_view,
      .set_orthographic_coordinates = rig_camera_set_orthographic_coordinates,
      .get_inverse_projection = rig_camera_get_inverse_projection,
      .set_view_transform = rig_camera_set_view_transform,
      .get_inverse_view_transform = rig_camera_get_inverse_view_transform,
      .set_input_transform = rig_camera_set_input_transform,
      .flush = rig_camera_flush,
      .suspend = rig_camera_suspend,
      .resume = rig_camera_resume,
      .end_frame = rig_camera_end_frame,
      .add_input_region = rig_camera_add_input_region,
      .remove_input_region = rig_camera_remove_input_region,
      .transform_window_coordinate = rig_camera_transform_window_coordinate,
      .unproject_coord = rig_camera_unproject_coord,
      .create_frustum_primitive = rig_camera_create_frustum_primitive,
      .set_focal_distance = rig_camera_set_focal_distance,
      .set_depth_of_field = rig_camera_set_depth_of_field,
      .set_zoom = rig_camera_set_zoom,
  };

  RutType *type = &rig_camera_type;
#define TYPE RigCamera

  rut_type_init (type, G_STRINGIFY (TYPE), _rig_camera_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_COMPONENTABLE,
                      offsetof (TYPE, component),
                      &componentable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_CAMERA,
                      offsetof (TYPE, props),
                      &camera_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_INTROSPECTABLE,
                      offsetof (TYPE, introspectable),
                      NULL); /* no implied vtable */

#undef TYPE
}

RigCamera *
rig_camera_new (RigEngine *engine,
                float width,
                float height,
                CoglFramebuffer *framebuffer)
{
  RigCamera *camera =
    rut_object_alloc0 (RigCamera, &rig_camera_type, _rig_camera_init_type);

  camera->engine = engine;

  rut_introspectable_init (camera,
                           _rig_camera_prop_specs,
                           camera->properties);

  camera->component.type = RUT_COMPONENT_TYPE_CAMERA;

  rig_camera_set_background_color4f (camera, 0, 0, 0, 1);
  camera->props.clear_fb = TRUE;

  //rut_graphable_init (camera);

  camera->props.orthographic = TRUE;
  camera->props.x1 = 0;
  camera->props.y1 = 0;
  camera->props.x2 = width;
  camera->props.y2 = height;

  camera->props.viewport[2] = width;
  camera->props.viewport[3] = height;

  camera->props.near = -1;
  camera->props.far = 100;

  camera->props.zoom = 1;

  camera->props.focal_distance = 30;
  camera->props.depth_of_field = 3;

  camera->props.projection_cache_age = -1;
  camera->props.inverse_projection_age = -1;

  cogl_matrix_init_identity (&camera->props.view);
  camera->props.inverse_view_age = -1;

  camera->props.transform_age = 0;

  cogl_matrix_init_identity (&camera->props.input_transform);

  if (framebuffer)
    {
      int width = cogl_framebuffer_get_width (framebuffer);
      int height = cogl_framebuffer_get_height (framebuffer);
      camera->props.fb = cogl_object_ref (framebuffer);
      camera->props.viewport[2] = width;
      camera->props.viewport[3] = height;
      camera->props.x2 = width;
      camera->props.y2 = height;
    }

  return camera;
}
