
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

