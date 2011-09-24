#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cogl/cogl.h>
#include <glib.h>
#include <string.h>

#include "rig-planes.h"

typedef struct _Vector4
{
  float x, y, z, w;
} Vector4;

static void
rig_get_eye_planes_for_screen_poly (float *polygon,
                                    int n_vertices,
                                    float *viewport,
                                    const CoglMatrix *projection,
                                    const CoglMatrix *inverse_project,
                                    RigPlane *planes)
{
#if 0
  float Wc;
  Vector4 *tmp_poly;
  RigPlane *plane;
  int i;
  float b[3];
  float c[3];
  int count;

  tmp_poly = g_alloca (sizeof (Vector4) * n_vertices * 2);

#define DEPTH -50

  /* Determine W in clip-space (Wc) for a point (0, 0, DEPTH, 1)
   *
   * Note: the depth could be anything except 0.
   *
   * We will transform the polygon into clip coordinates using this
   * depth and then into eye coordinates. Our clip planes will be
   * defined by triangles that extend between points of the polygon at
   * DEPTH and corresponding points of the same polygon at DEPTH * 2.
   *
   * NB: Wc defines the position of the clip planes in clip
   * coordinates. Given a screen aligned cross section through the
   * frustum; coordinates range from [-Wc,Wc] left to right on the
   * x-axis and [Wc,-Wc] top to bottom on the y-axis.
   */
  Wc = DEPTH * projection->wz + projection->ww;

#define CLIP_X(X) ((((float)X - viewport[0]) * (2.0 / viewport[2])) - 1) * Wc
#define CLIP_Y(Y) ((((float)Y - viewport[1]) * (2.0 / viewport[3])) - 1) * -Wc

  for (i = 0; i < n_vertices; i++)
    {
      tmp_poly[i].x = CLIP_X (polygon[i * 2]);
      tmp_poly[i].y = CLIP_Y (polygon[i * 2 + 1]);
      tmp_poly[i].z = DEPTH;
      tmp_poly[i].w = Wc;
    }

  Wc = DEPTH * 2 * projection->wz + projection->ww;

  /* FIXME: technically we don't need to project all of the points
   * twice, it would be enough project every other point since
   * we can share points in this set to define the plane vectors. */
  for (i = 0; i < n_vertices; i++)
    {
      tmp_poly[n_vertices + i].x = CLIP_X (polygon[i * 2]);
      tmp_poly[n_vertices + i].y = CLIP_Y (polygon[i * 2 + 1]);
      tmp_poly[n_vertices + i].z = DEPTH * 2;
      tmp_poly[n_vertices + i].w = Wc;
    }

#undef CLIP_X
#undef CLIP_Y

  cogl_matrix_project_points (inverse_project,
                              4,
                              sizeof (Vector4),
                              tmp_poly,
                              sizeof (Vector4),
                              tmp_poly,
                              n_vertices * 2);

  /* XXX: It's quite ugly that we end up with these casts between
   * Vector4 types and CoglVector3s, it might be better if the
   * cogl_vector APIs just took pointers to floats.
   */

  count = n_vertices - 1;
  for (i = 0; i < count; i++)
    {
      plane = &planes[i];
      plane->v0 = *(CoglVector3 *)&tmp_poly[i];
      b = *(CoglVector3 *)&tmp_poly[n_vertices + i];
      c = *(CoglVector3 *)&tmp_poly[n_vertices + i + 1];
      cogl_vector3_subtract (&b, &b, &plane->v0);
      cogl_vector3_subtract (&c, &c, &plane->v0);
      cogl_vector3_cross_product (&plane->n, &b, &c);
      cogl_vector3_normalize (&plane->n);
    }

  plane = &planes[n_vertices - 1];
  plane->v0 = *(CoglVector3 *)&tmp_poly[0];
  b = *(CoglVector3 *)&tmp_poly[2 * n_vertices - 1];
  c = *(CoglVector3 *)&tmp_poly[n_vertices];
  cogl_vector3_subtract (&b, &b, &plane->v0);
  cogl_vector3_subtract (&c, &c, &plane->v0);
  cogl_vector3_cross_product (&plane->n, &b, &c);
  cogl_vector3_normalize (&plane->n);
#endif
  float Wc;
  Vector4 *tmp_poly;
  RigPlane *plane;
  int i;
  float b[3];
  float c[3];
  int count;

  tmp_poly = g_alloca (sizeof (Vector4) * n_vertices * 2);

#define DEPTH -50

  /* Determine W in clip-space (Wc) for a point (0, 0, DEPTH, 1)
   *
   * Note: the depth could be anything except 0.
   *
   * We will transform the polygon into clip coordinates using this
   * depth and then into eye coordinates. Our clip planes will be
   * defined by triangles that extend between points of the polygon at
   * DEPTH and corresponding points of the same polygon at DEPTH * 2.
   *
   * NB: Wc defines the position of the clip planes in clip
   * coordinates. Given a screen aligned cross section through the
   * frustum; coordinates range from [-Wc,Wc] left to right on the
   * x-axis and [Wc,-Wc] top to bottom on the y-axis.
   */
  Wc = DEPTH * projection->wz + projection->ww;

#define CLIP_X(X) ((((float)X - viewport[0]) * (2.0 / viewport[2])) - 1) * Wc
#define CLIP_Y(Y) ((((float)Y - viewport[1]) * (2.0 / viewport[3])) - 1) * -Wc

  for (i = 0; i < n_vertices; i++)
    {
      tmp_poly[i].x = CLIP_X (polygon[i * 2]);
      tmp_poly[i].y = CLIP_Y (polygon[i * 2 + 1]);
      tmp_poly[i].z = DEPTH;
      tmp_poly[i].w = Wc;
    }

  Wc = DEPTH * 2 * projection->wz + projection->ww;

  /* FIXME: technically we don't need to project all of the points
   * twice, it would be enough project every other point since
   * we can share points in this set to define the plane vectors. */
  for (i = 0; i < n_vertices; i++)
    {
      tmp_poly[n_vertices + i].x = CLIP_X (polygon[i * 2]);
      tmp_poly[n_vertices + i].y = CLIP_Y (polygon[i * 2 + 1]);
      tmp_poly[n_vertices + i].z = DEPTH * 2;
      tmp_poly[n_vertices + i].w = Wc;
    }

#undef CLIP_X
#undef CLIP_Y

  cogl_matrix_project_points (inverse_project,
                              4,
                              sizeof (Vector4),
                              tmp_poly,
                              sizeof (Vector4),
                              tmp_poly,
                              n_vertices * 2);

  /* XXX: It's quite ugly that we end up with these casts between
   * Vector4 types and CoglVector3s, it might be better if the
   * cogl_vector APIs just took pointers to floats.
   */

  count = n_vertices - 1;
  for (i = 0; i < count; i++)
    {
      plane = &planes[i];
      memcpy (plane->v0, tmp_poly + i, sizeof (float) * 3);
      memcpy (b, tmp_poly + n_vertices + i, sizeof (float) * 3);
      memcpy (c, tmp_poly + n_vertices + i + 1, sizeof (float) * 3);
      cogl_vector3_subtract (b, b, plane->v0);
      cogl_vector3_subtract (c, c, plane->v0);
      cogl_vector3_cross_product (plane->n, b, c);
      cogl_vector3_normalize (plane->n);
    }

  plane = &planes[n_vertices - 1];
  memcpy (plane->v0, tmp_poly + 0, sizeof (float) * 3);
  memcpy (b, tmp_poly + (2 * n_vertices - 1), sizeof (float) * 3);
  memcpy (c, tmp_poly + n_vertices, sizeof (float) * 3);
  cogl_vector3_subtract (b, b, plane->v0);
  cogl_vector3_subtract (c, c, plane->v0);
  cogl_vector3_cross_product (plane->n, b, c);
  cogl_vector3_normalize (plane->n);

}
