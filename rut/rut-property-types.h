/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013,2014  Intel Corporation.
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
 */

SCALAR_TYPE(float, float, FLOAT)
SCALAR_TYPE(double, double, DOUBLE)
SCALAR_TYPE(integer, int, INTEGER)
SCALAR_TYPE(enum, int, ENUM)
SCALAR_TYPE(uint32, uint32_t, UINT32)
SCALAR_TYPE(boolean, bool, BOOLEAN)

POINTER_TYPE(object, rut_object_t *, OBJECT)
POINTER_TYPE(asset, rig_asset_t *, ASSET)
POINTER_TYPE(pointer, void *, POINTER)

COMPOSITE_TYPE(quaternion, c_quaternion_t, QUATERNION)
COMPOSITE_TYPE(color, cg_color_t, COLOR)

ARRAY_TYPE(vec3, float, VEC3, 3)
ARRAY_TYPE(vec4, float, VEC4, 4)
