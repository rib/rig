/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
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
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __CG_SHADER_BOILERPLATE_H
#define __CG_SHADER_BOILERPLATE_H

#define _CG_COMMON_SHADER_BOILERPLATE                                          \
    "#define CG_VERSION 100\n"                                                 \
    "\n"                                                                       \
    "uniform mat4 cg_modelview_matrix;\n"                                      \
    "uniform mat4 cg_modelview_projection_matrix;\n"                           \
    "uniform mat4 cg_projection_matrix;\n"

/* This declares all of the variables that we might need. This is
 * working on the assumption that the compiler will optimise them out
 * if they are not actually used. The GLSL spec at least implies that
 * this will happen for varyings but it doesn't explicitly so for
 * attributes */
#define _CG_VERTEX_SHADER_BOILERPLATE                                          \
    _CG_COMMON_SHADER_BOILERPLATE                                              \
    "#define cg_color_out _cg_color\n"                                     \
    "out vec4 _cg_color;\n"                                                \
    "#define cg_position_out gl_Position\n"                                \
    "#define cg_point_size_out gl_PointSize\n"                             \
    "\n"                                                                   \
    "in vec4 cg_color_in;\n"                                               \
    "in vec4 cg_position_in;\n"                                            \
    "#define cg_tex_coord_in cg_tex_coord0_in;\n"                          \
    "in vec3 cg_normal_in;\n"

#define _CG_FRAGMENT_SHADER_BOILERPLATE                                        \
    "#ifdef GL_ES\n"                                                           \
    "precision highp float;\n"                                                 \
    "#endif\n" _CG_COMMON_SHADER_BOILERPLATE "\n"                              \
    "in vec4 _cg_color;\n"                                                     \
    "\n"                                                                       \
    "#define cg_color_in _cg_color\n"                                          \
    "\n"                                                                       \
    "#define cg_front_facing gl_FrontFacing\n"                                 \
    "\n"                                                                       \
    "#define cg_point_coord gl_PointCoord\n"
#if 0
/* GLSL 1.2 has a bottom left origin, though later versions
 * allow use of an origin_upper_left keyword which would be
 * more appropriate for CGlib. */
"#define cgFragCoord   gl_FragCoord\n"
#endif

#endif /* __CG_SHADER_BOILERPLATE_H */
