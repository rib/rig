/*
 * Rig
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

#ifndef _RIG_UTIL_H_
#define _RIG_UTIL_H_

#include <cogl/cogl.h>

/* This is a replacement for the nearbyint function which always
 * rounds to the nearest integer. nearbyint is apparently a C99
 * function so it might not always be available but also it seems in
 * glibc it is defined as a function call so this macro could end up
 * faster anyway. We can't just add 0.5f because it will break for
 * negative numbers. */
#define RIG_UTIL_NEARBYINT(x) ((int) ((x) < 0.0f ? (x) - 0.5f : (x) + 0.5f))

#ifdef RIG_HAS_GLIB_SUPPORT
#define _RIG_RETURN_IF_FAIL(EXPR) g_return_if_fail(EXPR)
#define _RIG_RETURN_VAL_IF_FAIL(EXPR, VAL) g_return_val_if_fail(EXPR, VAL)
#else
#define _RIG_RETURN_IF_FAIL(EXPR) do {                             \
   if (!(EXPR))                                                     \
     {                                                              \
       fprintf (stderr, "file %s: line %d: assertion `%s' failed",  \
                __FILE__,                                           \
                __LINE__,                                           \
                #EXPR);                                             \
       return;                                                      \
     };                                                             \
  } while(0)
#define _RIG_RETURN_VAL_IF_FAIL(EXPR, VAL) do {                            \
   if (!(EXPR))                                                     \
     {                                                              \
       fprintf (stderr, "file %s: line %d: assertion `%s' failed",  \
                __FILE__,                                           \
                __LINE__,                                           \
                #EXPR);                                             \
       return (VAL);                                                \
     };                                                             \
  } while(0)
#endif /* RIG_HAS_GLIB_SUPPORT */

void
rig_util_fully_transform_vertices (const CoglMatrix *modelview,
                                    const CoglMatrix *projection,
                                    const float *viewport,
                                    const float *vertices3_in,
                                    float *vertices3_out,
                                    int n_vertices);

void
rig_util_print_quaternion (const char           *prefix,
                           const CoglQuaternion *quaternion);

#endif /* _RIG_UTIL_H_ */
