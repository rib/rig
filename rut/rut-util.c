
#include <glib.h>
#include <cogl/cogl.h>

#include "rut-global.h"
#include "rut-mesh.h"
#include "rut-util.h"

/* Help macros to scale from OpenGL <-1,1> coordinates system to
 * window coordinates ranging [0,window-size]
 */
#define MTX_GL_SCALE_X(x,w,v1,v2) ((((((x) / (w)) + 1.0f) / 2.0f) * (v1)) + (v2))
#define MTX_GL_SCALE_Y(y,w,v1,v2) ((v1) - (((((y) / (w)) + 1.0f) / 2.0f) * (v1)) + (v2))
#define MTX_GL_SCALE_Z(z,w,v1,v2) (MTX_GL_SCALE_X ((z), (w), (v1), (v2)))

typedef struct _Vertex4
{
  float x;
  float y;
  float z;
  float w;
} Vertex4;

void
rut_util_fully_transform_vertices (const CoglMatrix *modelview,
                                    const CoglMatrix *projection,
                                    const float *viewport,
                                    const float *vertices3_in,
                                    float *vertices3_out,
                                    int n_vertices)
{
  CoglMatrix modelview_projection;
  Vertex4 *vertices_tmp;
  int i;

  vertices_tmp = g_alloca (sizeof (Vertex4) * n_vertices);

  if (n_vertices >= 4)
    {
      /* XXX: we should find a way to cache this per actor */
      cogl_matrix_multiply (&modelview_projection,
                            projection,
                            modelview);
      cogl_matrix_project_points (&modelview_projection,
                                  3,
                                  sizeof (float) * 3,
                                  vertices3_in,
                                  sizeof (Vertex4),
                                  vertices_tmp,
                                  n_vertices);
    }
  else
    {
      cogl_matrix_transform_points (modelview,
                                    3,
                                    sizeof (float) * 3,
                                    vertices3_in,
                                    sizeof (Vertex4),
                                    vertices_tmp,
                                    n_vertices);

      cogl_matrix_project_points (projection,
                                  3,
                                  sizeof (Vertex4),
                                  vertices_tmp,
                                  sizeof (Vertex4),
                                  vertices_tmp,
                                  n_vertices);
    }

  for (i = 0; i < n_vertices; i++)
    {
      Vertex4 vertex_tmp = vertices_tmp[i];
      float *vertex_out = vertices3_out + i * 3;
      /* Finally translate from OpenGL coords to window coords */
      vertex_out[0] = MTX_GL_SCALE_X (vertex_tmp.x, vertex_tmp.w,
                                      viewport[2], viewport[0]);
      vertex_out[1] = MTX_GL_SCALE_Y (vertex_tmp.y, vertex_tmp.w,
                                      viewport[3], viewport[1]);
    }
}

void
rut_util_print_quaternion (const char           *prefix,
                           const CoglQuaternion *quaternion)
{
  float axis[3], angle;

  cogl_quaternion_get_rotation_axis (quaternion, axis);
  angle = cogl_quaternion_get_rotation_angle (quaternion);

  g_print ("%saxis: (%.2f,%.2f,%.2f) angle: %.2f\n", prefix, axis[0],
           axis[1], axis[2], angle);
}

void
rut_util_create_pick_ray (const float       viewport[4],
                          const CoglMatrix *inverse_projection,
                          const CoglMatrix *camera_transform,
                          float             viewport_pos[2],
                          float             ray_position[3],  /* out */
                          float             ray_direction[3]) /* out */
{
  CoglMatrix inverse_transform;
  float ndc_x, ndc_y;
  float projected_points[6], unprojected_points[8];

  /* Undo the Viewport transform, putting us in Normalized Device
   * Coords
   *
   * XXX: We are assuming the incomming coordinates are in viewport
   * coordinates not device coordinates so we don't need to apply the
   * viewport offset, we just need to normalized according to the
   * width and height of the viewport.
   */
  ndc_x = viewport_pos[0] * 2.0f / viewport[2] - 1.0f;
  ndc_y = ((viewport[3] - 1.0f - viewport_pos[1]) * 2.0f / viewport[3] - 1.0f);

  /* The main drawing code is doing P x C¯¹ (P is the Projection matrix
   * and C is the Camera transform. To inverse that transformation we need
   * to apply C x P¯¹ to the points */
  cogl_matrix_multiply (&inverse_transform,
                        camera_transform, inverse_projection);

  /* unproject the point at both the near plane and the far plane */
  projected_points[0] = ndc_x;
  projected_points[1] = ndc_y;
  projected_points[2] = 0.0f;
  projected_points[3] = ndc_x;
  projected_points[4] = ndc_y;
  projected_points[5] = 1.0f;
  cogl_matrix_project_points (&inverse_transform,
                              3, /* num components for input */
                              sizeof (float) * 3, /* input stride */
                              projected_points,
                              sizeof (float) * 4, /* output stride */
                              unprojected_points,
                              2 /* n_points */);
  unprojected_points[0] /= unprojected_points[3];
  unprojected_points[1] /= unprojected_points[3];
  unprojected_points[2] /= unprojected_points[3];
  unprojected_points[4] /= unprojected_points[7];
  unprojected_points[5] /= unprojected_points[7];
  unprojected_points[6] /= unprojected_points[7];

  ray_position[0] = unprojected_points[0];
  ray_position[1] = unprojected_points[1];
  ray_position[2] = unprojected_points[2];

  ray_direction[0] = unprojected_points[4] - unprojected_points[0];
  ray_direction[1] = unprojected_points[5] - unprojected_points[1];
  ray_direction[2] = unprojected_points[6] - unprojected_points[2];

  cogl_vector3_normalize (ray_direction);
}

void
rut_util_transform_normal (const CoglMatrix *matrix,
                           float            *x,
                           float            *y,
                           float            *z)
{
  float _x = *x, _y = *y, _z = *z;

  *x = matrix->xx * _x + matrix->xy * _y + matrix->xz * _z;
  *y = matrix->yx * _x + matrix->yy * _y + matrix->yz * _z;
  *z = matrix->zx * _x + matrix->zy * _y + matrix->zz * _z;
}

/*
 * From "Fast, Minimum Storage Ray/Triangle Intersection",
 * http://www.cs.virginia.edu/~gfx/Courses/2003/ImageSynthesis/papers/Acceleration/Fast%20MinimumStorage%20RayTriangle%20Intersection.pdf
 */

#define EPSILON 0.00001

static void
cross (float dest[3], float v1[3], float v2[3])
{
  dest[0] = v1[1] * v2[2] - v1[2] * v2[1];
  dest[1] = v1[2] * v2[0] - v1[0] * v2[2];
  dest[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

static float
dot (float v1[3], float v2[3])
{
  return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

static void
sub (float dest[3], float v1[3], float v2[3])
{
  dest[0] = v1[0] - v2[0];
  dest[1] = v1[1] - v2[1];
  dest[2] = v1[2] - v2[2];
}

bool
rut_util_intersect_triangle (float v0[3], float v1[3], float v2[3],
                             float ray_origin[3], float ray_direction[3],
                             float *u, float *v, float *t)
{
  float edge1[3], edge2[3], tvec[3], pvec[3], qvec[3];
  float det, inv_det;

  /* find vectors for the two edges sharing v0 */
  sub (edge1, v1, v0);
  sub (edge2, v2, v0);

  /* begin calculating determinant, also used to calculate u */
  cross (pvec, ray_direction, edge2);

  /* if determinant is near zero, ray lies in the triangle's plane */
  det = dot (edge1, pvec);

  if (det > -EPSILON && det < EPSILON)
    return FALSE;

  inv_det = 1.0f / det;

  /* calculate the distance from v0, to ray_origin */
  sub (tvec, ray_origin, v0);

  /* calculate U and test bounds */
  *u = dot (tvec, pvec) * inv_det;
  if (*u < 0.f || *u > 1.f)
    return FALSE;

  /* prepare to test V */
  cross (qvec, tvec, edge1);

  /* calculate V and test bounds */
  *v = dot (ray_direction, qvec) * inv_det;
  if (*v < 0.f || *u + *v > 1.f)
    return FALSE;

  /* calculate t, ray intersects triangle */
  *t = dot (edge2, qvec) * inv_det;

  return TRUE;
}

typedef struct _IntersectState
{
  float *ray_origin;
  float *ray_direction;
  float min_t;
  int index;
  int hit_index;
  bool found;
} IntersectState;

static CoglBool
intersect_triangle_cb (void **attributes_v0,
                       void **attributes_v1,
                       void **attributes_v2,
                       int index_v0,
                       int index_v1,
                       int index_v2,
                       void *user_data)
{
  IntersectState *state = user_data;
  float *pos_v0 = attributes_v0[0];
  float *pos_v1 = attributes_v1[0];
  float *pos_v2 = attributes_v2[0];
  float u, v, t;
  bool hit;

  hit = rut_util_intersect_triangle (pos_v0, pos_v1, pos_v2,
                                     state->ray_origin,
                                     state->ray_direction,
                                     &u, &v, &t);

  /* found a closer triangle. t > 0 means that we don't want results
   * behind the ray origin */
  if (hit && t > 0 && t < state->min_t)
    {
      state->min_t = t;
      state->found = TRUE;
      state->hit_index = state->index;
    }

  state->index++;

  return TRUE;
}

bool
rut_util_intersect_mesh (RutMesh *mesh,
                         float ray_origin[3],
                         float ray_direction[3],
                         int *index,
                         float *t_out)
{
  IntersectState state;

  state.ray_origin = ray_origin;
  state.ray_direction = ray_direction;
  state.min_t = G_MAXFLOAT;
  state.found = FALSE;
  state.index = 0;

  rut_mesh_foreach_triangle (mesh,
                             intersect_triangle_cb,
                             &state,
                             "cogl_position_in",
                             NULL);

  if (state.found)
    {
      if (t_out)
        *t_out = state.min_t;

      if (index)
        *index = state.hit_index;

      return TRUE;
    }

  return FALSE;
}

unsigned int
rut_util_one_at_a_time_mix (unsigned int hash)
{
  hash += ( hash << 3 );
  hash ^= ( hash >> 11 );
  hash += ( hash << 15 );

  return hash;
}

CoglPipeline *
rut_util_create_texture_pipeline (CoglTexture *texture)
{
  static CoglPipeline *template = NULL, *new_pipeline;

  if (G_UNLIKELY (template == NULL))
    {
      template = cogl_pipeline_new (rut_cogl_context);
      cogl_pipeline_set_layer_null_texture (template,
					    0,
					    COGL_TEXTURE_TYPE_2D);
    }

  new_pipeline = cogl_pipeline_copy (template);
  cogl_pipeline_set_layer_texture (new_pipeline, 0, texture);

  return new_pipeline;
}

static const float jitter_offsets[32] =
{
  0.375f, 0.4375f,
  0.625f, 0.0625f,
  0.875f, 0.1875f,
  0.125f, 0.0625f,

  0.375f, 0.6875f,
  0.875f, 0.4375f,
  0.625f, 0.5625f,
  0.375f, 0.9375f,

  0.625f, 0.3125f,
  0.125f, 0.5625f,
  0.125f, 0.8125f,
  0.375f, 0.1875f,

  0.875f, 0.9375f,
  0.875f, 0.6875f,
  0.125f, 0.3125f,
  0.625f, 0.8125f
};

/* XXX: This assumes that the primitive is being drawn in pixel coordinates,
 * since we jitter the modelview not the projection.
 */
void
rut_util_draw_jittered_primitive3f (CoglFramebuffer *fb,
                                    CoglPrimitive *prim,
                                    float red,
                                    float green,
                                    float blue)
{
  CoglContext *cogl_ctx = cogl_framebuffer_get_context (fb);
  CoglPipeline *pipeline = cogl_pipeline_new (cogl_ctx);
  float viewport[4];
  CoglMatrix projection;
  float pixel_dx, pixel_dy;
  int i;


  cogl_pipeline_set_color4f (pipeline,
                             red / 16.0f,
                             green / 16.0f,
                             blue / 16.0f,
                             1.0f / 16.0f);

  cogl_framebuffer_get_viewport4fv (fb, viewport);
  cogl_framebuffer_get_projection_matrix (fb, &projection);

  pixel_dx = 2.0 / viewport[2];
  pixel_dy = 2.0 / viewport[3];

  for (i = 0; i < 16; i++)
    {
      CoglMatrix tmp = projection;
      CoglMatrix jitter;
      CoglMatrix jittered_projection;

      const float *offset = jitter_offsets + 2 * i;

      cogl_matrix_init_identity (&jitter);
      cogl_matrix_translate (&jitter,
                             offset[0] * pixel_dx,
                             offset[1] * pixel_dy,
                             0);
      cogl_matrix_multiply (&jittered_projection, &jitter, &tmp);
      cogl_framebuffer_set_projection_matrix (fb, &jittered_projection);
      cogl_framebuffer_draw_primitive (fb, pipeline, prim);
    }

  cogl_framebuffer_set_projection_matrix (fb, &projection);

  cogl_object_unref (pipeline);
}


