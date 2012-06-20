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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <math.h>

#include <rig/rig.h>

#define USER_ENTITY 2
#define N_ENTITIES  4

typedef struct
{
  RigShell *shell;
  RigContext *ctx;

  CoglFramebuffer *fb;
  float fb_width, fb_height;
  GTimer *timer;

  RigEntity entities[N_ENTITIES];
  RigEntity *main_camera;
  RigEntity *light;

  /* shadow mapping */
  CoglOffscreen *shadow_fb;
  CoglTexture2D *shadow_color;
  CoglTexture   *shadow_map;

  CoglPipeline *shadow_color_tex;
  CoglPipeline *shadow_map_tex;

  /* root materials */
  CoglPipeline *diffuse_specular;

  /* editor state */
  bool button_down;
  RigEntity pivot;
  RigArcball arcball;
  CoglQuaternion saved_rotation;

  /* picking ray */
  CoglPipeline *picking_ray_color;
  CoglPrimitive *picking_ray;

} Data;

/* in micro seconds  */
static int64_t
get_current_time (Data *data)
{
  return (int64_t) (g_timer_elapsed (data->timer, NULL) * 1e6);
}

static CoglPipeline *
create_color_pipeline (float r, float g, float b)
{
  static CoglPipeline *template = NULL, *new_pipeline;

  if (G_UNLIKELY (template == NULL))
    template = cogl_pipeline_new (rig_cogl_context);

  new_pipeline = cogl_pipeline_copy (template);
  cogl_pipeline_set_color4f (new_pipeline, r, g, b, 1.0f);

  return new_pipeline;
}

static CoglPipeline *
create_texture_pipeline (CoglTexture *texture)
{
  static CoglPipeline *template = NULL, *new_pipeline;

  if (G_UNLIKELY (template == NULL))
    {
      template = cogl_pipeline_new (rig_cogl_context);
      cogl_pipeline_set_layer_null_texture (template, 0, COGL_TEXTURE_TYPE_2D);
    }

  new_pipeline = cogl_pipeline_copy (template);
  cogl_pipeline_set_layer_texture (new_pipeline, 0, texture);

  return new_pipeline;
}

static void
compute_light_shadow_matrix (Data       *data,
                             CoglMatrix *light_matrix,
                             CoglMatrix *light_projection,
                             RigEntity  *light)
{
  CoglMatrix *main_camera, *pivot, pivot_inverse, light_transform, light_view;
  /* Move the unit data from [-1,1] to [0,1], column major order */
  float bias[16] = {
    .5f, .0f, .0f, .0f,
    .0f, .5f, .0f, .0f,
    .0f, .0f, .5f, .0f,
    .5f, .5f, .5f, 1.f
  };

  main_camera = rig_entity_get_transform (data->main_camera);
  pivot = rig_entity_get_transform (&data->pivot);
  cogl_matrix_get_inverse (rig_entity_get_transform (&data->pivot),
                           &pivot_inverse);
  cogl_matrix_multiply (&light_transform, pivot,
                        rig_entity_get_transform (light));
  cogl_matrix_get_inverse (&light_transform, &light_view);

  cogl_matrix_init_from_array (light_matrix, bias);
  cogl_matrix_multiply (light_matrix, light_matrix, light_projection);
  cogl_matrix_multiply (light_matrix, light_matrix, &light_view);
  cogl_matrix_multiply (light_matrix, light_matrix, main_camera);
}

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

static void
draw_entities (Data            *data,
               CoglFramebuffer *fb,
               RigEntity       *camera,
               gboolean         shadow_pass)
{
  CoglMatrix *transform, inverse, *pivot;
  int i;

  transform = rig_entity_get_transform (camera);
  cogl_matrix_get_inverse (transform, &inverse);
  pivot = rig_entity_get_transform (&data->pivot);
  if (shadow_pass)
    {
      cogl_framebuffer_identity_matrix (fb);
      cogl_framebuffer_scale (fb, 1, -1, 1);
      cogl_framebuffer_transform (fb, &inverse);
    }
  else
    {
      cogl_framebuffer_set_modelview_matrix (fb, &inverse);
      cogl_framebuffer_transform (fb, pivot);
    }
  rig_entity_draw (camera, fb);

  for (i = 1; i < N_ENTITIES; i++)
    {
      RigEntity *entity;

      entity = &data->entities[i];

      if (shadow_pass && !entity_cast_shadow (entity))
        continue;

      cogl_framebuffer_push_matrix (fb);

      transform = rig_entity_get_transform (entity);
      cogl_framebuffer_transform (fb, transform);

      rig_entity_draw (entity, fb);

      cogl_framebuffer_pop_matrix (fb);
    }
}

static void
test_init (RigShell *shell, void *user_data)
{
  Data *data = user_data;
  CoglOnscreen *onscreen;
  CoglTexture2D *color_buffer;
  GError *error = NULL;
  RigComponent *component;
  CoglPipeline *root_pipeline, *pipeline;
  CoglSnippet *snippet;
  CoglColor color;
  float vector3[3];

  data->fb_width = 800;
  data->fb_height = 600;
  onscreen = cogl_onscreen_new (data->ctx->cogl_context,
                                data->fb_width,
                                data->fb_height);
  data->fb = COGL_FRAMEBUFFER (onscreen);

  cogl_onscreen_show (onscreen);

  /*
   * Shadow mapping
   */

  /* Setup the shadow map */
  color_buffer = cogl_texture_2d_new_with_size (rig_cogl_context,
                                                512, 512,
                                                COGL_PIXEL_FORMAT_ANY,
                                                &error);
  if (error)
    g_critical ("could not create texture: %s", error->message);

  data->shadow_color = color_buffer;

  /* XXX: Right now there's no way to disable rendering to the the color
   * buffer. */
  data->shadow_fb =
    cogl_offscreen_new_to_texture (COGL_TEXTURE (color_buffer));

  /* retrieve the depth texture */
  cogl_framebuffer_enable_depth_texture (COGL_FRAMEBUFFER (data->shadow_fb),
                                         TRUE);
  data->shadow_map =
    cogl_framebuffer_get_depth_texture (COGL_FRAMEBUFFER (data->shadow_fb));

  if (data->shadow_fb == NULL)
    g_critical ("could not create offscreen buffer");

  /* Hook the shadow sampling */
  root_pipeline = create_diffuse_specular_material ();
  cogl_pipeline_set_layer_texture (root_pipeline, 7, data->shadow_map);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_LOOKUP,
                              /* declarations */
                              "varying vec4 shadow_coords;\n"
                              "vec4 shadow_coords_d;\n",
                              /* post */
                              "");

  cogl_snippet_set_replace (snippet,
                            "cogl_texel = texture2D(cogl_sampler7, shadow_coords_d.st);\n");

  cogl_pipeline_add_layer_snippet (root_pipeline, 7, snippet);
  cogl_object_unref (snippet);

  /*
   * Setup CoglObjects to render our plane and cube
   */

  /* camera */
  data->main_camera = &data->entities[0];
  rig_entity_init (data->main_camera);

  vector3[0] = 0.f;
  vector3[1] = 2.f;
  vector3[2] = 10.f;
  rig_entity_set_position (data->main_camera, vector3);

  component = rig_camcorder_new ();

  rig_camcorder_set_framebuffer (RIG_CAMCORDER (component), data->fb);
  rig_camcorder_set_field_of_view (RIG_CAMCORDER (component), 60.f);
  rig_camcorder_set_near_plane (RIG_CAMCORDER (component), 1.1f);
  rig_camcorder_set_far_plane (RIG_CAMCORDER (component), 100.f);

  rig_entity_add_component (data->main_camera, component);

  /* light */
  data->light = &data->entities[1];
  rig_entity_init (data->light);

  vector3[0] = 1.0f;
  vector3[1] = 8.0f;
  vector3[2] = -2.0f;
  rig_entity_set_position (data->light, vector3);

  rig_entity_rotate_x_axis (data->light, -120);
  rig_entity_rotate_y_axis (data->light, 10);

  component = rig_light_new ();
  cogl_color_init_from_4f (&color, .2f, .2f, .2f, 1.f);
  rig_light_set_ambient (RIG_LIGHT (component), &color);
  cogl_color_init_from_4f (&color, .6f, .6f, .6f, 1.f);
  rig_light_set_diffuse (RIG_LIGHT (component), &color);
  cogl_color_init_from_4f (&color, .4f, .4f, .4f, 1.f);
  rig_light_set_specular (RIG_LIGHT (component), &color);
  rig_light_add_pipeline (RIG_LIGHT (component), root_pipeline);


  rig_entity_add_component (data->light, component);

  component = rig_camcorder_new ();

  cogl_color_init_from_4f (&color, 0.f, .3f, 0.f, 1.f);
  rig_camcorder_set_background_color (RIG_CAMCORDER (component), &color);
  rig_camcorder_set_framebuffer (RIG_CAMCORDER (component),
                             COGL_FRAMEBUFFER (data->shadow_fb));
  rig_camcorder_set_projection (RIG_CAMCORDER (component), RIG_PROJECTION_ORTHOGRAPHIC);
  rig_camcorder_set_size_of_view (RIG_CAMCORDER (component), 5);
  rig_camcorder_set_near_plane (RIG_CAMCORDER (component), 1.1f);
  rig_camcorder_set_far_plane (RIG_CAMCORDER (component), 20.f);

  rig_entity_add_component (data->light, component);


  /* plane */
  rig_entity_init (&data->entities[USER_ENTITY]);
  rig_entity_set_cast_shadow (&data->entities[USER_ENTITY], FALSE);
  rig_entity_set_y (&data->entities[USER_ENTITY], -1.5f);

  component = rig_mesh_renderer_new_from_template ("plane", root_pipeline);

  rig_entity_add_component (&data->entities[USER_ENTITY], component);

  /* a second, more interesting, entity */
  rig_entity_init (&data->entities[USER_ENTITY + 1]);
  rig_entity_set_cast_shadow (&data->entities[USER_ENTITY + 1], TRUE);
  rig_entity_set_y (&data->entities[USER_ENTITY + 1], .5);
  rig_entity_set_z (&data->entities[USER_ENTITY + 1], 1);
  rig_entity_rotate_y_axis (&data->entities[USER_ENTITY + 1], 10);

  pipeline = cogl_pipeline_copy (root_pipeline);
  cogl_pipeline_set_color4f (pipeline, 0.6f, 0.6f, 0.6f, 1.0f);

  component = rig_mesh_renderer_new_from_template ("cube", pipeline);
  cogl_object_unref (pipeline);

  rig_entity_add_component (&data->entities[USER_ENTITY + 1], component);

  /* create the pipelines to display the shadow color and depth textures */
  data->shadow_color_tex =
      create_texture_pipeline (COGL_TEXTURE (data->shadow_color));
  data->shadow_map_tex =
      create_texture_pipeline (COGL_TEXTURE (data->shadow_map));

  cogl_object_unref (root_pipeline);

  /* editor data */
  {
    int w, h;

    w = cogl_framebuffer_get_width (data->fb);
    h = cogl_framebuffer_get_height (data->fb);

    rig_entity_init (&data->pivot);
    rig_arcball_init (&data->arcball, w / 2, h / 2, sqrtf (w * w + h * h) / 2);

    /* picking ray */
    data->picking_ray_color = create_color_pipeline (1.f, 0.f, 0.f);
  }

  /* timer for the world time */
  data->timer = g_timer_new ();
  g_timer_start (data->timer);
}

static CoglBool
test_paint (RigShell *shell, void *user_data)
{
  Data *data = user_data;
  int i;
  int64_t time; /* micro seconds */
  CoglFramebuffer *shadow_fb;

  /*
   * update entities
   */

  time = get_current_time (data);

  for (i = 0; i < N_ENTITIES; i++)
    {
      RigEntity *entity = &data->entities[i];

      rig_entity_update (entity, time);
    }

  /*
   * render the shadow map
   */

  shadow_fb = COGL_FRAMEBUFFER (data->shadow_fb);

  /* update the light matrix uniform */
  {
    CoglMatrix light_shadow_matrix, light_projection;
    CoglPipeline *pipeline;

    int location;

    cogl_framebuffer_get_projection_matrix (shadow_fb, &light_projection);
    compute_light_shadow_matrix (data,
                                 &light_shadow_matrix,
                                 &light_projection,
                                 data->light);

    pipeline = rig_entity_get_pipeline (&data->entities[USER_ENTITY]);
    location = cogl_pipeline_get_uniform_location (pipeline,
                                                   "light_shadow_matrix");
    cogl_pipeline_set_uniform_matrix (pipeline,
                                      location,
                                      4, 1,
                                      FALSE,
                                      cogl_matrix_get_array (&light_shadow_matrix));

    pipeline = rig_entity_get_pipeline (&data->entities[USER_ENTITY + 1]);
    location = cogl_pipeline_get_uniform_location (pipeline,
                                                   "light_shadow_matrix");
    cogl_pipeline_set_uniform_matrix (pipeline,
                                      location,
                                      4, 1,
                                      FALSE,
                                      cogl_matrix_get_array (&light_shadow_matrix));
  }

  draw_entities (data, shadow_fb, data->light, TRUE /* shadow pass */);

  /*
   * render the scene
   */

  cogl_framebuffer_push_matrix (data->fb);

  /* draw entities */
  draw_entities (data, data->fb, data->main_camera, FALSE /* shadow pass */);

  if (data->picking_ray)
    {
      cogl_framebuffer_draw_primitive (data->fb,
                                       data->picking_ray_color,
                                       data->picking_ray);
    }

  /* draw the color and depth buffers of the shadow FBO to debug them */
  cogl_framebuffer_draw_rectangle (data->fb, data->shadow_color_tex,
                                   -2, 1, -4, 3);
  cogl_framebuffer_draw_rectangle (data->fb, data->shadow_map_tex,
                                   -2, -1, -4, 1);

  cogl_framebuffer_pop_matrix (data->fb);

  cogl_onscreen_swap_buffers (COGL_ONSCREEN (data->fb));

  return TRUE;
}

static void
test_fini (RigShell *shell, void *user_data)
{

}

static CoglPrimitive *
create_line_primitive (float a[3], float b[3])
{
  CoglVertexP3 data[2];
  CoglAttributeBuffer *attribute_buffer;
  CoglAttribute *attributes[1];
  CoglPrimitive *primitive;

  data[0].x = a[0];
  data[0].y = a[1];
  data[0].z = a[2];
  data[1].x = b[0];
  data[1].y = b[1];
  data[1].z = b[2];

  attribute_buffer = cogl_attribute_buffer_new (rig_cogl_context,
                                                2 * sizeof (CoglVertexP3),
                                                data);

  attributes[0] = cogl_attribute_new (attribute_buffer,
                                      "cogl_position_in",
                                      sizeof (CoglVertexP3),
                                      offsetof (CoglVertexP3, x),
                                      3,
                                      COGL_ATTRIBUTE_TYPE_FLOAT);

  primitive = cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_LINES,
                                                  2, attributes, 1);

  cogl_object_unref (attribute_buffer);
  cogl_object_unref (attributes[0]);

  return primitive;
}

static void
transform_ray (CoglMatrix *transform,
               bool        inverse_transform,
               float       ray_origin[3],
               float       ray_direction[3])
{
  CoglMatrix inverse, normal_matrix, *m;

  m = transform;
  if (inverse_transform)
    {
      cogl_matrix_get_inverse (transform, &inverse);
      m = &inverse;
    }

  cogl_matrix_transform_points (m,
                                3, /* num components for input */
                                sizeof (float) * 3, /* input stride */
                                ray_origin,
                                sizeof (float) * 3, /* output stride */
                                ray_origin,
                                1 /* n_points */);

  cogl_matrix_get_inverse (m, &normal_matrix);
  cogl_matrix_transpose (&normal_matrix);

  rig_util_transform_normal (&normal_matrix,
                             &ray_direction[0],
                             &ray_direction[1],
                             &ray_direction[2]);
}

static CoglPrimitive *
create_picking_ray (Data            *data,
                    CoglFramebuffer *fb,
                    float            ray_position[3],
                    float            ray_direction[3],
                    float            length)
{
  CoglPrimitive *line;
  float points[6];

  points[0] = ray_position[0];
  points[1] = ray_position[1];
  points[2] = ray_position[2];
  points[3] = ray_position[0] + length * ray_direction[0];
  points[4] = ray_position[1] + length * ray_direction[1];
  points[5] = ray_position[2] + length * ray_direction[2];

  line = create_line_primitive (points, points + 3);

  return line;
}

static RigEntity *
pick (Data  *data,
      float  ray_origin[3],
      float  ray_direction[3])
{
  RigEntity *entity, *selected_entity = NULL;
  RigComponent *mesh;
  uint8_t *vertex_data;
  int n_vertices;
  size_t stride;
  int index;
  int i;
  float distance, min_distance = G_MAXFLOAT;
  bool hit;
  float transformed_ray_origin[3];
  float transformed_ray_direction[3];
  static const char *names[2] = { "plane", "cube" }, *name;

  for (i = 0; i < 2; i++)
    {
      entity = &data->entities[USER_ENTITY + i];

      /* transform the ray into the model space */
      memcpy (transformed_ray_origin, ray_origin, 3 * sizeof (float));
      memcpy (transformed_ray_direction, ray_direction, 3 * sizeof (float));
      transform_ray (rig_entity_get_transform (entity),
                     TRUE, /* inverse of the transform */
                     transformed_ray_origin,
                     transformed_ray_direction);

      /* intersect the transformed ray with the mesh data */
      mesh = rig_entity_get_component (entity,
                                       RIG_COMPONENT_TYPE_MESH_RENDERER);
      vertex_data =
        rig_mesh_renderer_get_vertex_data (RIG_MESH_RENDERER (mesh), &stride);
      n_vertices = rig_mesh_renderer_get_n_vertices (RIG_MESH_RENDERER (mesh));

      hit = rig_util_intersect_mesh (vertex_data,
                                     n_vertices,
                                     stride,
                                     transformed_ray_origin,
                                     transformed_ray_direction,
                                     &index,
                                     &distance);

      /* to compare intersection distances we need to re-transform it back
       * to the world space, */
      cogl_vector3_normalize (transformed_ray_direction);
      transformed_ray_direction[0] *= distance;
      transformed_ray_direction[1] *= distance;
      transformed_ray_direction[2] *= distance;

      rig_util_transform_normal (rig_entity_get_transform (entity),
                                 &transformed_ray_direction[0],
                                 &transformed_ray_direction[1],
                                 &transformed_ray_direction[2]);
      distance = cogl_vector3_magnitude (transformed_ray_direction);

      if (hit && distance < min_distance)
        {
          min_distance = distance;
          selected_entity = entity;
          name = names[i];
        }
    }

  if (selected_entity)
    {
      g_message ("Hit the %s, triangle #%d, distance %.2f",
                 name, index, distance);
    }

  return selected_entity;
}

static RigInputEventStatus
test_input_handler (RigInputEvent *event, void *user_data)
{
  Data *data = user_data;
  RigMotionEventAction action;
  RigButtonState state;
  float x, y;

  switch (rig_input_event_get_type (event))
    {
      case RIG_INPUT_EVENT_TYPE_MOTION:
      {
        action = rig_motion_event_get_action (event);
        state = rig_motion_event_get_button_state (event);
        x = rig_motion_event_get_x (event);
        y = rig_motion_event_get_y (event);

        if (action == RIG_MOTION_EVENT_ACTION_DOWN &&
            state == RIG_BUTTON_STATE_2)
          {
            data->saved_rotation = *rig_entity_get_rotation (&data->pivot);
            cogl_quaternion_init_identity (&data->arcball.q_drag);

            rig_arcball_mouse_down (&data->arcball, x, data->fb_height - y);

            data->button_down = TRUE;

            return RIG_INPUT_EVENT_STATUS_HANDLED;
          }
        else if (action == RIG_MOTION_EVENT_ACTION_DOWN &&
                 state == RIG_BUTTON_STATE_1)
          {
            /* pick */
            RigComponent *camera;
            float *viewport, ray_position[3], ray_direction[3], screen_pos[2],
                  z_far, z_near;
            CoglMatrix *camera_transform, projection, inverse_projection;

            camera = rig_entity_get_component (data->main_camera,
                                               RIG_COMPONENT_TYPE_CAMCORDER);
            viewport = rig_camcorder_get_viewport (RIG_CAMCORDER (camera));
            z_near = rig_camcorder_get_near_plane (RIG_CAMCORDER (camera));
            z_far = rig_camcorder_get_far_plane (RIG_CAMCORDER (camera));

            cogl_framebuffer_get_projection_matrix (data->fb,
                                                    &projection);
            cogl_matrix_get_inverse (&projection, &inverse_projection);

            camera_transform = rig_entity_get_transform (data->main_camera);

            screen_pos[0] = x;
            screen_pos[1] = y;

            rig_util_create_pick_ray (viewport,
                                      &inverse_projection,
                                      camera_transform,
                                      screen_pos,
                                      ray_position,
                                      ray_direction);

            /* nullify the effect of the pivot */
            transform_ray (rig_entity_get_transform (&data->pivot),
                           TRUE, /* inverse the transform */
                           ray_position,
                           ray_direction);

            if (data->picking_ray)
              cogl_object_unref (data->picking_ray);
            data->picking_ray = create_picking_ray (data,
                                                    data->fb,
                                                    ray_position,
                                                    ray_direction,
                                                    z_far - z_near);

            pick (data, ray_position, ray_direction);
          }
        else if (action == RIG_MOTION_EVENT_ACTION_UP)
          {
            data->button_down = FALSE;
          }
        else if (action == RIG_MOTION_EVENT_ACTION_MOVE &&
                 state == RIG_BUTTON_STATE_2)
          {
            CoglQuaternion new_rotation;

            if (!data->button_down)
              break;

            rig_arcball_mouse_motion (&data->arcball, x, data->fb_height - y);

            cogl_quaternion_multiply (&new_rotation,
                                      &data->arcball.q_drag,
                                      &data->saved_rotation);

            rig_entity_set_rotation (&data->pivot, &new_rotation);

            return RIG_INPUT_EVENT_STATUS_HANDLED;
         }


      }
    break;

      default:
        break;
    }

  return RIG_INPUT_EVENT_STATUS_UNHANDLED;
}

int
main (int argc, char **argv)
{
  Data data;

  memset (&data, 0, sizeof (Data));

  data.shell = rig_shell_new (test_init, test_fini, test_paint, &data);

  data.ctx = rig_context_new (data.shell);

  rig_context_init (data.ctx);

  rig_shell_set_input_callback (data.shell, test_input_handler, &data);

  rig_shell_main (data.shell);

  return 0;
}
