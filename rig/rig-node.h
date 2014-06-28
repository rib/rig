/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012  Intel Corporation
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

#ifndef _RUT_NODE_H_
#define _RUT_NODE_H_

#include <cogl/cogl.h>
#include <rut.h>

typedef struct
{
  RutList list_node;

  RutBoxed boxed;

  float t;
} RigNode;

void
rig_node_float_lerp (RigNode *a,
                     RigNode *b,
                     float t,
                     float *value);

void
rig_node_vec3_lerp (RigNode *a,
                    RigNode *b,
                    float t,
                    float value[3]);

void
rig_node_quaternion_lerp (RigNode *a,
                          RigNode *b,
                          float t,
                          CoglQuaternion *value);

void
rig_node_double_lerp (RigNode *a,
                      RigNode *b,
                      float t,
                      double *value);

void
rig_node_integer_lerp (RigNode *a,
                       RigNode *b,
                       float t,
                       int *value);

void
rig_node_uint32_lerp (RigNode *a,
                      RigNode *b,
                      float t,
                      uint32_t *value);

void
rig_node_vec4_lerp (RigNode *a,
                    RigNode *b,
                    float t,
                    float value[4]);

void
rig_node_color_lerp (RigNode *a,
                     RigNode *b,
                     float t,
                     CoglColor *value);

void
rig_node_enum_lerp (RigNode *a,
                    RigNode *b,
                    float t,
                    int *value);

void
rig_node_boolean_lerp (RigNode *a,
                       RigNode *b,
                       float t,
                       bool *value);

void
rig_node_text_lerp (RigNode *a,
                    RigNode *b,
                    float t,
                    const char **value);

void
rig_node_asset_lerp (RigNode *a,
                     RigNode *b,
                     float t,
                     RigAsset **value);

void
rig_node_object_lerp (RigNode *a,
                      RigNode *b,
                      float t,
                      RutObject **value);

bool
rig_node_box (RutPropertyType type,
              RigNode *node,
              RutBoxed *value);

void
rig_node_free (RigNode *node);

RigNode *
rig_node_new_for_float (float t, float value);

RigNode *
rig_node_new_for_double (float t, double value);

RigNode *
rig_node_new_for_vec3 (float t, const float value[3]);

RigNode *
rig_node_new_for_vec4 (float t, const float value[4]);

RigNode *
rig_node_new_for_integer (float t, int value);

RigNode *
rig_node_new_for_uint32 (float t, uint32_t value);

RigNode *
rig_node_new_for_quaternion (float t, const CoglQuaternion *value);

RigNode *
rig_node_new_for_color (float t, const CoglColor *value);

RigNode *
rig_node_new_for_boolean (float t, bool value);

RigNode *
rig_node_new_for_enum (float t, int value);

RigNode *
rig_node_new_for_text (float t, const char *value);

RigNode *
rig_node_new_for_asset (float t, RigAsset *value);

RigNode *
rig_node_new_for_object (float t, RutObject *value);

RigNode *
rig_node_copy (RigNode *node);

RigNode *
rig_nodes_find_less_than (RigNode *start, RutList *end, float t);

RigNode *
rig_nodes_find_less_than_equal (RigNode *start, RutList *end, float t);

RigNode *
rig_nodes_find_greater_than (RigNode *start, RutList *end, float t);

RigNode *
rig_nodes_find_greater_than_equal (RigNode *start, RutList *end, float t);

#endif /* _RUT_NODE_H_ */
