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

#include "rig-material.h"
#include "rig-global.h"

CoglPipeline *
create_diffuse_specular_material (void)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;
  CoglDepthState depth_state;

  pipeline = cogl_pipeline_new (rig_cogl_context);
  cogl_pipeline_set_color4f (pipeline, 0.8f, 0.8f, 0.8f, 1.f);

  /* enable depth testing */
  cogl_depth_state_init (&depth_state);
  cogl_depth_state_set_test_enabled (&depth_state, TRUE);
  cogl_pipeline_set_depth_state (pipeline, &depth_state, NULL);

  /* set up our vertex shader */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX,

      /* definitions */
      "uniform mat4 light_shadow_matrix;\n"
      "uniform mat3 normal_matrix;\n"
      "varying vec3 normal_direction, eye_direction;\n"
      "varying vec4 shadow_coords;\n",

      "normal_direction = normalize(normal_matrix * cogl_normal_in);\n"
      "eye_direction    = -vec3(cogl_modelview_matrix * cogl_position_in);\n"

      "shadow_coords = light_shadow_matrix * cogl_modelview_matrix *\n"
      "                cogl_position_in;\n"
  );

  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  /* and fragment shader */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
      /* definitions */
      "uniform vec4 light0_ambient, light0_diffuse, light0_specular;\n"
      "uniform vec3 light0_direction_norm;\n"
      "varying vec3 normal_direction, eye_direction;\n",

      /* post */
      NULL);

  cogl_snippet_set_replace (snippet,
      "vec4 final_color = light0_ambient * cogl_color_in;\n"

      " vec3 L = light0_direction_norm;\n"
      " vec3 N = normalize(normal_direction);\n"

      "float lambert = dot(N, L);\n"

      "if (lambert > 0.0)\n"
      "{\n"
      "  final_color += cogl_color_in * light0_diffuse * lambert;\n"

      "  vec3 E = normalize(eye_direction);\n"
      "  vec3 R = reflect (-L, N);\n"
      "  float specular = pow (max(dot(R, E), 0.0),\n"
      "                        2.);\n"
      "  final_color += light0_specular * vec4(.6, .6, .6, 1.0) * specular;\n"
      "}\n"

      "shadow_coords_d = shadow_coords / shadow_coords.w;\n"
      "cogl_texel7 =  cogl_texture_lookup7 (cogl_sampler7, cogl_tex_coord_in[0]);\n"
      "float distance_from_light = cogl_texel7.z + 0.0005;\n"
      "float shadow = 1.0;\n"
      "if (shadow_coords.w > 0.0 && distance_from_light < shadow_coords_d.z)\n"
      "    shadow = 0.5;\n"

      "cogl_color_out = shadow * final_color;\n"
  );

  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  return pipeline;
}

RigType rig_material_type;

static RigComponentableVTable _rig_material_componentable_vtable = {
    0
};

void
_rig_material_init_type (void)
{
  rig_type_init (&rig_material_type);
  rig_type_add_interface (&rig_material_type,
                           RIG_INTERFACE_ID_COMPONENTABLE,
                           offsetof (RigMaterial, component),
                           &_rig_material_componentable_vtable);
}

RigMaterial *
rig_material_new_with_pipeline (RigContext *ctx,
                                CoglPipeline *pipeline)
{
  RigMaterial *material = g_slice_new0 (RigMaterial);

  rig_object_init (&material->_parent, &rig_material_type);
  material->component.type = RIG_COMPONENT_TYPE_MATERIAL;
  material->pipeline = cogl_object_ref (pipeline);

  return material;
}

RigMaterial *
rig_material_new (RigContext *ctx)
{
  /* TODO: Hang a template for this pipeline off ctx */
  CoglPipeline *pipeline = create_diffuse_specular_material ();
  RigMaterial *material = rig_material_new_with_pipeline (ctx, pipeline);

  cogl_object_unref (pipeline);
  return material;
}

RigMaterial *
rig_material_new_with_texture (RigContext *ctx,
                               CoglTexture *texture)
{
  CoglPipeline *pipeline = cogl_pipeline_new (ctx->cogl_context);
  RigMaterial *material;

  cogl_pipeline_set_layer_texture (pipeline, 0, texture);
  material = rig_material_new_with_pipeline (ctx, pipeline);

  cogl_object_unref (pipeline);
  return material;
}

CoglPipeline *
rig_material_get_pipeline (RigMaterial *material)
{
  return material->pipeline;
}

void
rig_material_set_pipeline (RigMaterial *material,
                           CoglPipeline *pipeline)
{
  if (material->pipeline == pipeline)
    return;

  if (material->pipeline)
    {
      cogl_object_unref (material->pipeline);
      material->pipeline = NULL;
    }

  if (pipeline)
    material->pipeline = cogl_object_ref (pipeline);
}
