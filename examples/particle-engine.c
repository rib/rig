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

typedef struct
{
  RigShell *shell;
  RigContext *ctx;

  CoglFramebuffer *fb;

  RigCamera *camera;

  RigParticleEngine *engine;
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

  onscreen = cogl_onscreen_new (data->ctx->cogl_context, 800, 600);
  data->fb = COGL_FRAMEBUFFER (onscreen);

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
}

static CoglBool
test_paint (RigShell *shell,
            void *user_data)
{
  RigPaintContext paint_context;
  Data *data = user_data;

  rig_camera_flush (data->camera);

  cogl_framebuffer_clear4f (data->fb,
                            COGL_BUFFER_BIT_COLOR,
                            0.0f, 0.0f, 0.0f, 1.0f);

  cogl_framebuffer_push_matrix (data->fb);
  cogl_framebuffer_translate (data->fb,
                              cogl_framebuffer_get_width (data->fb) / 2.0f,
                              cogl_framebuffer_get_height (data->fb) / 2.0f,
                              0.0f);

  rig_particle_engine_set_time (data->engine,
                                g_get_monotonic_time () / 1000);

  paint_context.camera = data->camera;
  rig_paintable_paint (data->engine, &paint_context);

  cogl_framebuffer_pop_matrix (data->fb);

  cogl_onscreen_swap_buffers (COGL_ONSCREEN (data->fb));

  return TRUE;
}

static void
test_fini (RigShell *shell, void *user_data)
{
  Data *data = user_data;

  rig_ref_countable_unref (data->camera);
  rig_ref_countable_unref (data->engine);
  cogl_object_unref (data->fb);
  rig_ref_countable_unref (data->ctx);
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
