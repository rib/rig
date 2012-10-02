/*
 * Rut
 *
 * A rig of UI prototyping utilities
 *
 * Copyright (C) 2011  Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

#ifndef _RUT_UTIL_H_
#define _RUT_UTIL_H_

#include <stdbool.h>

#include <cogl/cogl.h>

/* This is a replacement for the nearbyint function which always
 * rounds to the nearest integer. nearbyint is apparently a C99
 * function so it might not always be available but also it seems in
 * glibc it is defined as a function call so this macro could end up
 * faster anyway. We can't just add 0.5f because it will break for
 * negative numbers. */
#define RUT_UTIL_NEARBYINT(x) ((int) ((x) < 0.0f ? (x) - 0.5f : (x) + 0.5f))

#ifdef RUT_HAS_GLIB_SUPPORT
#define _RUT_RETURN_IF_FAIL(EXPR) g_return_if_fail(EXPR)
#define _RUT_RETURN_VAL_IF_FAIL(EXPR, VAL) g_return_val_if_fail(EXPR, VAL)
#else
#define _RUT_RETURN_IF_FAIL(EXPR) do {                             \
   if (!(EXPR))                                                     \
     {                                                              \
       fprintf (stderr, "file %s: line %d: assertion `%s' failed",  \
                __FILE__,                                           \
                __LINE__,                                           \
                #EXPR);                                             \
       return;                                                      \
     };                                                             \
  } while(0)
#define _RUT_RETURN_VAL_IF_FAIL(EXPR, VAL) do {                            \
   if (!(EXPR))                                                     \
     {                                                              \
       fprintf (stderr, "file %s: line %d: assertion `%s' failed",  \
                __FILE__,                                           \
                __LINE__,                                           \
                #EXPR);                                             \
       return (VAL);                                                \
     };                                                             \
  } while(0)
#endif /* RUT_HAS_GLIB_SUPPORT */

void
rut_util_fully_transform_vertices (const CoglMatrix *modelview,
                                    const CoglMatrix *projection,
                                    const float *viewport,
                                    const float *vertices3_in,
                                    float *vertices3_out,
                                    int n_vertices);

void
rut_util_create_pick_ray (const float       viewport[4],
                          const CoglMatrix *inverse_projection,
                          const CoglMatrix *camera_transform,
                          float             screen_pos[2],
                          float             ray_position[3],    /* out */
                          float             ray_direction[3]);  /* out */

void
rut_util_print_quaternion (const char           *prefix,
                           const CoglQuaternion *quaternion);

void
rut_util_transform_normal (const CoglMatrix *matrix,
                           float            *x,
                           float            *y,
                           float            *z);

bool
rut_util_intersect_triangle (float v0[3], float v1[3], float v2[3],
                             float ray_origin[3], float ray_direction[3],
                             float *u, float *v, float *t);
bool
rut_util_intersect_model (const void       *vertices,
                          int               n_points,
                          size_t            stride,
                          float             ray_origin[3],
                          float             ray_direction[3],
                          int              *index,
                          float            *t_out);

/* Split Bob Jenkins' One-at-a-Time hash
 *
 * This uses the One-at-a-Time hash algorithm designed by Bob Jenkins
 * but the mixing step is split out so the function can be used in a
 * more incremental fashion.
 */
static inline unsigned int
rut_util_one_at_a_time_hash (unsigned int hash,
                             const void *key,
                             size_t bytes)
{
  const unsigned char *p = key;
  int i;

  for (i = 0; i < bytes; i++)
    {
      hash += p[i];
      hash += (hash << 10);
      hash ^= (hash >> 6);
    }

  return hash;
}

unsigned int
rut_util_one_at_a_time_mix (unsigned int hash);

CoglPipeline *
rut_util_create_texture_pipeline (CoglTexture *texture);

#endif /* _RUT_UTIL_H_ */
