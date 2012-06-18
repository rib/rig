
#include <glib.h>
#include <cogl/cogl.h>

#include "rig-util.h"

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
rig_util_fully_transform_vertices (const CoglMatrix *modelview,
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
rig_util_print_quaternion (const char           *prefix,
                           const CoglQuaternion *quaternion)
{
  float axis[3], angle;

  cogl_quaternion_get_rotation_axis (quaternion, axis);
  angle = cogl_quaternion_get_rotation_angle (quaternion);

  g_print ("%saxis: (%.2f,%.2f,%.2f) angle: %.2f\n", prefix, axis[0],
           axis[1], axis[2], angle);
}

void
rig_util_create_pick_ray (const float       viewport[4],
                          const CoglMatrix *inverse_projection,
                          const CoglMatrix *camera_transform,
                          float             screen_pos[2],
                          float             ray_position[3],  /* out */
                          float             ray_direction[3]) /* out */
{
  CoglMatrix inverse_transform;
  float view_x, view_y;
  float projected_points[6], unprojected_points[8];

  /* Get the mouse position before the viewport transformation */
  view_x = (screen_pos[0] - viewport[0]) * 2.0f / viewport[2] - 1.0f;
  view_y = ((viewport[3] - 1 + viewport[1] - screen_pos[1]) * 2.0f /
            viewport[3] - 1.0f);

  /* The main drawing code is doing P x C¯¹ (P is the Projection matrix
   * and C is the Camera transform. To inverse that transformation we need
   * to apply C x P¯¹ to the points */
  cogl_matrix_multiply (&inverse_transform,
                        camera_transform, inverse_projection);

  /* unproject the point at both the near plane and the far plane */
  projected_points[0] = view_x;
  projected_points[1] = view_y;
  projected_points[2] = 0.0f;
  projected_points[3] = view_x;
  projected_points[4] = view_y;
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
