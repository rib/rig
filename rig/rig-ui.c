/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2014  Intel Corporation.
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

#include <config.h>

#include <math.h>

#include <rut.h>

#include "rig-ui.h"
#include "rig-code.h"
#include "rig-entity.h"


static void
_rig_ui_free (void *object)
{
  RigUI *ui = object;
  GList *l;

  for (l = ui->suspended_controllers; l; l = l->next)
    rut_object_unref (l->data);
  g_list_free (ui->suspended_controllers);

  for (l = ui->controllers; l; l = l->next)
    rut_object_unref (l->data);
  g_list_free (ui->controllers);

  for (l = ui->assets; l; l = l->next)
    rut_object_unref (l->data);
  g_list_free (ui->assets);

  /* NB: no extra reference is held on ui->light other than the
   * reference for it being in the ->scene. */

  if (ui->scene)
    rut_object_unref (ui->scene);

  if (ui->play_camera)
    rut_object_unref (ui->play_camera);

  if (ui->play_camera_component)
    rut_object_unref (ui->play_camera_component);

  if (ui->dso_data)
    g_free (ui->dso_data);

  rut_object_free (RigUI, object);
}

static RutTraverseVisitFlags
reap_entity_cb (RutObject *object,
                int depth,
                void *user_data)
{
  RigEngine *engine = user_data;

  /* The root node is a RutGraph that shouldn't be reaped */
  if (rut_object_get_type (object) != &rig_entity_type)
    return RUT_TRAVERSE_VISIT_CONTINUE;

  rig_entity_reap (object, engine);

  rut_graphable_remove_child (object);

  return RUT_TRAVERSE_VISIT_CONTINUE;
}

void
rig_ui_reap (RigUI *ui)
{
  RigEngine *engine = ui->engine;
  GList *l;

  rut_graphable_traverse (ui->scene,
                          RUT_TRAVERSE_DEPTH_FIRST,
                          reap_entity_cb,
                          NULL, /* post paint */
                          ui->engine);

  for (l = ui->controllers; l; l = l->next)
    {
      RigController *controller = l->data;

      rig_controller_reap (controller, engine);

      rut_object_release (controller, ui);
    }

  /* We could potentially leave these to be freed in _free() but it
   * seems a bit ugly to keep the list containing pointers to
   * controllers no longer owned by the ui. */
  g_list_free (ui->controllers);
  ui->controllers = NULL;

  rig_engine_queue_delete (engine, ui);
}

RutType rig_ui_type;

static void
_rig_ui_init_type (void)
{
  rut_type_init (&rig_ui_type, "RigUI", _rig_ui_free);
}

RigUI *
rig_ui_new (RigEngine *engine)
{
  RigUI *ui = rut_object_alloc0 (RigUI, &rig_ui_type,
                                 _rig_ui_init_type);

  ui->engine = engine;

  return ui;
}

void
rig_ui_set_dso_data (RigUI *ui, uint8_t *data, int len)
{
  if (ui->dso_data)
    g_free (ui->dso_data);

  ui->dso_data = g_malloc (len);
  memcpy (ui->dso_data, data, len);
  ui->dso_len = len;
}

typedef struct
{
  const char *label;
  RigEntity *entity;
} FindEntityData;

static RutTraverseVisitFlags
find_entity_cb (RutObject *object,
                int depth,
                void *user_data)
{
  FindEntityData *data = user_data;

  if (rut_object_get_type (object) == &rig_entity_type &&
      !strcmp (data->label, rig_entity_get_label (object)))
    {
      data->entity = object;
      return RUT_TRAVERSE_VISIT_BREAK;
    }

  return RUT_TRAVERSE_VISIT_CONTINUE;
}

RigEntity *
rig_ui_find_entity (RigUI *ui, const char *label)
{
  FindEntityData data = { .label = label, .entity = NULL };

  rut_graphable_traverse (ui->scene,
                          RUT_TRAVERSE_DEPTH_FIRST,
                          find_entity_cb,
                          NULL, /* after_children_cb */
                          &data);

  return data.entity;
}

static void
initialise_play_camera_position (RigEngine *engine,
                                 RigUI *ui)
{
  float fov_y = 10; /* y-axis field of view */
  float aspect = (float) engine->device_width / (float) engine->device_height;
  float z_near = 10; /* distance to near clipping plane */
  float z_2d = 30;
  float position[3];
  float left, right, top;
  float left_2d_plane, right_2d_plane;
  float width_scale;
  float width_2d_start;

  /* Initialise the camera to the center of the device with a z
   * position that will give it pixel aligned coordinates at the
   * origin */
  top = z_near * tanf (fov_y * G_PI / 360.0);
  left = -top * aspect;
  right = top * aspect;

  left_2d_plane = left / z_near * z_2d;
  right_2d_plane = right / z_near * z_2d;

  width_2d_start = right_2d_plane - left_2d_plane;

  width_scale = width_2d_start / engine->device_width;

  position[0] = engine->device_width / 2.0f;
  position[1] = engine->device_height / 2.0f;
  position[2] = z_2d / width_scale;

  rig_entity_set_position (ui->play_camera, position);
}

void
rig_ui_prepare (RigUI *ui)
{
  RigEngine *engine = ui->engine;
  RigController *controller;
  RutObject *light_camera;
  GList *l;

  if (!ui->scene)
    ui->scene = rut_graph_new (engine->ctx);

  if (!ui->light)
    {
      RigLight *light;
      float vector3[3];
      CoglColor color;

      ui->light = rig_entity_new (engine->ctx);
      rig_entity_set_label (ui->light, "light");

      vector3[0] = 0;
      vector3[1] = 0;
      vector3[2] = 500;
      rig_entity_set_position (ui->light, vector3);

      rig_entity_rotate_x_axis (ui->light, 20);
      rig_entity_rotate_y_axis (ui->light, -20);

      light = rig_light_new (engine->ctx);
      cogl_color_init_from_4f (&color, .2f, .2f, .2f, 1.f);
      rig_light_set_ambient (light, &color);
      cogl_color_init_from_4f (&color, .6f, .6f, .6f, 1.f);
      rig_light_set_diffuse (light, &color);
      cogl_color_init_from_4f (&color, .4f, .4f, .4f, 1.f);
      rig_light_set_specular (light, &color);

      rig_entity_add_component (ui->light, light);

      rut_graphable_add_child (ui->scene, ui->light);
    }

  light_camera = rig_entity_get_component (ui->light,
                                           RUT_COMPONENT_TYPE_CAMERA);
  if (!light_camera)
    {
      light_camera = rig_camera_new (engine,
                                     1000, /* ortho/vp width */
                                     1000, /* ortho/vp height */
                                     NULL);

      rut_camera_set_background_color4f (light_camera, 0.f, .3f, 0.f, 1.f);
      rut_camera_set_projection_mode (light_camera,
                                      RUT_PROJECTION_ORTHOGRAPHIC);
      rut_camera_set_orthographic_coordinates (light_camera,
                                               -1000, -1000, 1000, 1000);
      rut_camera_set_near_plane (light_camera, 1.1f);
      rut_camera_set_far_plane (light_camera, 1500.f);

      rig_entity_add_component (ui->light, light_camera);
    }

  if (engine->frontend)
    {
      CoglFramebuffer *fb = engine->shadow_fb;
      int width = cogl_framebuffer_get_width (fb);
      int height = cogl_framebuffer_get_height (fb);
      rut_camera_set_framebuffer (light_camera, fb);
      rut_camera_set_viewport (light_camera, 0, 0, width, height);
    }

  if (!ui->controllers)
    {
      controller = rig_controller_new (engine, "Controller 0");
      rig_controller_set_active (controller, true);
      ui->controllers = g_list_prepend (ui->controllers, controller);
    }

  /* Explcitly transfer ownership of controllers to the UI for
   * improved ref-count debugging.
   *
   * XXX: don't RIG_ENABLE_DEBUG guard this without also
   * updating rig_ui_reap()
   */
  for (l = ui->controllers; l; l = l->next)
    {
      rut_object_claim (l->data, ui);
      rut_object_unref (l->data);
    }

  if (!ui->play_camera)
    {
      /* Check if there is already an entity labelled ‘play-camera’
       * in the scene graph */
      ui->play_camera = rig_ui_find_entity (ui, "play-camera");

      if (ui->play_camera)
        ui->play_camera = rut_object_ref (ui->play_camera);
      else
        {
          ui->play_camera = rig_entity_new (engine->ctx);
          rig_entity_set_label (ui->play_camera, "play-camera");

          initialise_play_camera_position (engine, ui);

          rut_graphable_add_child (ui->scene, ui->play_camera);
        }
    }

  if (!ui->play_camera_component)
    {
      ui->play_camera_component =
        rig_entity_get_component (ui->play_camera, RUT_COMPONENT_TYPE_CAMERA);

      if (ui->play_camera_component)
        rut_object_ref (ui->play_camera_component);
      else
        {
          ui->play_camera_component =
            rig_camera_new (engine,
                            -1, /* ortho/vp width */
                            -1, /* ortho/vp height */
                            engine->onscreen);

          rig_entity_add_component (ui->play_camera, ui->play_camera_component);
        }
    }

  rut_camera_set_clear (ui->play_camera_component, false);

  rig_ui_suspend (ui);
}

void
rig_ui_suspend (RigUI *ui)
{
  GList *l;

  if (ui->suspended)
    return;

  for (l = ui->controllers; l; l = l->next)
    {
      RigController *controller = l->data;

      rig_controller_set_suspended (controller, true);

      ui->suspended_controllers =
        g_list_prepend (ui->suspended_controllers, controller);

      /* We take a reference on all suspended controllers so we
       * don't need to worry if any of the controllers are deleted
       * while in edit mode. */
      rut_object_ref (controller);
    }

  ui->suspended = true;
}

void
rig_ui_resume (RigUI *ui)
{
  GList *l;

  if (!ui->suspended)
    return;

  for (l = ui->suspended_controllers; l; l = l->next)
    {
      RigController *controller = l->data;

      rig_controller_set_suspended (controller, false);
      rut_object_unref (controller);
    }

  g_list_free (ui->suspended_controllers);
  ui->suspended_controllers = NULL;

  ui->suspended = false;
}

static void
print_component_cb (RutComponent *component,
                    void *user_data)
{
  int depth = *(int *)user_data;
  char *name = rig_engine_get_object_debug_name (component);
  g_print ("%*s%s\n", depth + 2, " ", name);
  g_free (name);
}

static RutTraverseVisitFlags
print_entity_cb (RutObject *object,
                 int depth,
                 void *user_data)
{
  char *name = rig_engine_get_object_debug_name (object);
  g_print ("%*s%s\n", depth, " ", name);

  if (rut_object_get_type (object) == &rig_entity_type)
    {
      rig_entity_foreach_component_safe (object,
                                         print_component_cb,
                                         &depth);
    }

  g_free (name);

  return RUT_TRAVERSE_VISIT_CONTINUE;
}

void
rig_ui_print (RigUI *ui)
{
  GList *l;

  g_print ("Scenegraph:\n");
  rut_graphable_traverse (ui->scene,
                          RUT_TRAVERSE_DEPTH_FIRST,
                          print_entity_cb,
                          NULL, /* post paint */
                          NULL); /* user data */

  g_print ("Controllers:\n");
  for (l = ui->controllers; l; l = l->next)
    {
      char *name = rig_engine_get_object_debug_name (l->data);
      g_print ("  %s\n", name);
      g_free (name);
    }

  g_print ("Assets:\n");
  for (l = ui->assets; l; l = l->next)
    {
      char *name = rig_engine_get_object_debug_name (l->data);
      g_print ("  %s\n", name);
      g_free (name);
    }
}

void
rig_ui_add_controller (RigUI *ui,
                       RigController *controller)
{
  ui->controllers = g_list_prepend (ui->controllers, controller);
  rut_object_ref (controller);

  if (!ui->suspended)
    rig_controller_set_suspended (controller, false);
}

void
rig_ui_remove_controller (RigUI *ui,
                          RigController *controller)
{
  rig_controller_set_suspended (controller, true);

  ui->controllers = g_list_remove (ui->controllers, controller);
  rut_object_unref (controller);
}
