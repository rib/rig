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

#include <rig/rig.h>
#include <rig/rig-particle-engine.h>
#include <rig/rig-inspector.h>

typedef struct
{
  RigShell *shell;
  RigContext *ctx;
  RigObject *root;

  CoglFramebuffer *fb;

  RigCamera *camera;

  RigParticleEngine *engine;
  RigTransform *engine_transform;

  RigInspector *inspector;
  RigTransform *inspector_transform;
} Data;

static RigCamera *
create_camera (RigContext *ctx,
               CoglFramebuffer *framebuffer)
{
  RigCamera *camera = rig_camera_new (ctx, framebuffer);
  CoglMatrix matrix;

  cogl_matrix_init_identity (&matrix);

  rig_camera_set_view_transform (camera, &matrix);

  return camera;
}

static void
allocate (Data *data)
{
  int fb_width = cogl_framebuffer_get_width (data->fb);
  int fb_height = cogl_framebuffer_get_height (data->fb);
  float width;
  float height;

  rig_sizable_get_preferred_width (data->inspector,
                                   -1, /* for_height */
                                   NULL, /* min_width */
                                   &width);

  /* Make sure the particle engine has at least ¼ of the screen */
  if (width > fb_width * 3 / 4)
    width = fb_width * 3 / 4;

  rig_sizable_get_preferred_height (data->inspector,
                                    width, /* for_width */
                                    NULL, /* min_height */
                                    &height);

  if (height > fb_height)
    height = fb_height;

  rig_transform_init_identity (data->inspector_transform);
  rig_transform_translate (data->inspector_transform,
                           fb_width - width,
                           fb_height - height,
                           0.0f);
  rig_sizable_set_size (data->inspector, width, height);

  /* Center the particle engine using all of the remaining space to
   * the left of the settings panel */
  rig_transform_init_identity (data->engine_transform);
  rig_transform_translate (data->engine_transform,
                           (fb_width - width) / 2.0f,
                           fb_height / 2.0f,
                           0.0f);
}

static void
test_onscreen_resize (CoglOnscreen *onscreen,
                      int width,
                      int height,
                      void *user_data)
{
  Data *data = user_data;

  allocate (data);
}

static void
inspector_property_changed_cb (RigProperty *target_property,
                               RigProperty *source_property,
                               void *user_data)
{
  RigBoxed box;
  Data *data = user_data;

  rig_property_box (source_property, &box);
  rig_property_set_boxed (&data->ctx->property_ctx, target_property, &box);
  rig_boxed_destroy (&box);
}

static void
test_init (RigShell *shell,
           void *user_data)
{
  Data *data = user_data;
  CoglOnscreen *onscreen;
  uint8_t color[4];
  static const char *texture_names[] =
    {
      "star.png"
    };
  int i;

  data->ctx = rig_context_new (data->shell);
  rig_context_init (data->ctx);

  onscreen = cogl_onscreen_new (data->ctx->cogl_context, 800, 600);
  data->fb = COGL_FRAMEBUFFER (onscreen);

  cogl_onscreen_set_resizable (onscreen, TRUE);
  cogl_onscreen_add_resize_handler (onscreen, test_onscreen_resize, data);

  cogl_onscreen_show (onscreen);

  data->camera = create_camera (data->ctx, data->fb);

  data->engine = rig_particle_engine_new (data->ctx);

  color[0] = 255;
  color[1] = 0;
  color[2] = 0;
  color[3] = 255;
  rig_particle_engine_add_color (data->engine, color);

  color[0] = 0;
  color[1] = 255;
  color[2] = 0;
  color[3] = 255;
  rig_particle_engine_add_color (data->engine, color);

  color[0] = 0;
  color[1] = 0;
  color[2] = 255;
  color[3] = 255;
  rig_particle_engine_add_color (data->engine, color);

  for (i = 0; i < G_N_ELEMENTS (texture_names); i++)
    {
      GError *error = NULL;
      CoglTexture *texture =
        cogl_texture_new_from_file (texture_names[i],
                                    COGL_TEXTURE_NONE,
                                    COGL_PIXEL_FORMAT_ANY,
                                    &error);

      if (texture)
        {
          rig_particle_engine_set_texture (data->engine, texture);
          cogl_object_unref (texture);
        }
      else
        {
          g_warning ("%s", error->message);
          g_clear_error (&error);
        }
    }

  data->root = rig_graph_new (data->ctx, NULL);

  data->engine_transform = rig_transform_new (data->ctx, NULL);
  rig_graphable_add_child (data->engine_transform, data->engine);
  rig_graphable_add_child (data->root, data->engine_transform);

  data->inspector = rig_inspector_new (data->ctx,
                                       data->engine,
                                       inspector_property_changed_cb,
                                       data);

  data->inspector_transform = rig_transform_new (data->ctx, NULL);
  rig_graphable_add_child (data->inspector_transform, data->inspector);
  rig_graphable_add_child (data->root, data->inspector_transform);

  rig_shell_add_input_camera (shell, data->camera, data->root);

  allocate (data);
}

static RigTraverseVisitFlags
pre_paint_cb (RigObject *object,
              int depth,
              void *user_data)
{
  RigPaintContext *paint_ctx = user_data;
  RigCamera *camera = paint_ctx->camera;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (camera);

  if (rig_object_is (object, RIG_INTERFACE_ID_TRANSFORMABLE))
    {
      const CoglMatrix *matrix = rig_transformable_get_matrix (object);
      cogl_framebuffer_push_matrix (fb);
      cogl_framebuffer_transform (fb, matrix);
    }

  if (rig_object_is (object, RIG_INTERFACE_ID_PAINTABLE))
    {
      RigPaintableVTable *vtable =
        rig_object_get_vtable (object, RIG_INTERFACE_ID_PAINTABLE);

      vtable->paint (object, paint_ctx);
    }

  return RIG_TRAVERSE_VISIT_CONTINUE;
}

static RigTraverseVisitFlags
post_paint_cb (RigObject *object,
               int depth,
               void *user_data)
{
  RigPaintContext *paint_ctx = user_data;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (paint_ctx->camera);

  if (rig_object_is (object, RIG_INTERFACE_ID_TRANSFORMABLE))
    cogl_framebuffer_pop_matrix (fb);

  return RIG_TRAVERSE_VISIT_CONTINUE;
}

static CoglBool
test_paint (RigShell *shell,
            void *user_data)
{
  RigPaintContext paint_context;
  Data *data = user_data;

  rig_particle_engine_set_time (data->engine,
                                g_get_monotonic_time () / 1000);

  cogl_framebuffer_clear4f (data->fb,
                            COGL_BUFFER_BIT_COLOR,
                            0.0f, 0.0f, 0.0f, 1.0f);

  rig_camera_flush (data->camera);

  paint_context.camera = data->camera;

  rig_paint_graph_with_layers (data->root,
                               pre_paint_cb,
                               post_paint_cb,
                               &paint_context);

  rig_camera_end_frame (data->camera);

  cogl_onscreen_swap_buffers (COGL_ONSCREEN (data->fb));

  return TRUE;
}

static void
test_fini (RigShell *shell, void *user_data)
{
  Data *data = user_data;

  rig_ref_countable_unref (data->camera);

  rig_graphable_remove_child (data->engine_transform);
  rig_graphable_remove_child (data->engine);
  rig_ref_countable_unref (data->engine_transform);
  rig_ref_countable_unref (data->engine);

  rig_graphable_remove_child (data->inspector_transform);
  rig_graphable_remove_child (data->engine);
  rig_ref_countable_unref (data->inspector_transform);
  rig_ref_countable_unref (data->inspector);

  cogl_object_unref (data->fb);
  rig_ref_countable_unref (data->ctx);
  rig_ref_countable_unref (data->root);
}

int
main (int argc, char **argv)
{
  Data data;

  memset (&data, 0, sizeof (Data));

  data.shell = rig_shell_new (test_init, test_fini, test_paint, &data);

  rig_shell_main (data.shell);

  return 0;
}
