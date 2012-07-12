/*
 * Rig
 *
 * A tiny toolkit
 *
 * Copyright (C) 2012 Intel Corporation.
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
 *
 */

#include <cogl/cogl.h>
#include <string.h>
#include <math.h>

#include "rig.h"
#include "rig-pe-settings.h"
#include "rig-number-slider.h"

typedef struct
{
  const char *name;
  float min_value;
  float max_value;
  float step;
  int decimal_places;
  float (* getter) (RigParticleEngine *engine);
  void (* setter) (RigParticleEngine *engine, float value);
} RigPeSettingsProperty;

static const RigPeSettingsProperty
rig_pe_settings_properties[] =
  {
    {
      .name = "Min initial velocity X",
      .min_value = -G_MAXFLOAT, .max_value = G_MAXFLOAT,
      .step = 1.0f,
      .decimal_places = 2,
      .getter = rig_particle_engine_get_min_initial_velocity_x,
      .setter = rig_particle_engine_set_min_initial_velocity_x
    },
    {
      .name = "Max initial velocity X",
      .min_value = -G_MAXFLOAT, .max_value = G_MAXFLOAT,
      .step = 1.0f,
      .decimal_places = 2,
      .getter = rig_particle_engine_get_max_initial_velocity_x,
      .setter = rig_particle_engine_set_max_initial_velocity_x
    },
    {
      .name = "Min initial velocity Y",
      .min_value = -G_MAXFLOAT, .max_value = G_MAXFLOAT,
      .step = 1.0f,
      .decimal_places = 2,
      .getter = rig_particle_engine_get_min_initial_velocity_y,
      .setter = rig_particle_engine_set_min_initial_velocity_y
    },
    {
      .name = "Max initial velocity Y",
      .min_value = -G_MAXFLOAT, .max_value = G_MAXFLOAT,
      .step = 1.0f,
      .decimal_places = 2,
      .getter = rig_particle_engine_get_max_initial_velocity_y,
      .setter = rig_particle_engine_set_max_initial_velocity_y
    },
    {
      .name = "Min initial velocity Z",
      .min_value = -G_MAXFLOAT, .max_value = G_MAXFLOAT,
      .step = 1.0f,
      .decimal_places = 2,
      .getter = rig_particle_engine_get_min_initial_velocity_z,
      .setter = rig_particle_engine_set_min_initial_velocity_z
    },
    {
      .name = "Max initial velocity Z",
      .min_value = -G_MAXFLOAT, .max_value = G_MAXFLOAT,
      .step = 1.0f,
      .decimal_places = 2,
      .getter = rig_particle_engine_get_max_initial_velocity_z,
      .setter = rig_particle_engine_set_max_initial_velocity_z
    },
  };

#define RIG_PE_SETTINGS_N_PROPERTIES G_N_ELEMENTS (rig_pe_settings_properties)

#define RIG_PE_SETTINGS_EDGE_GAP 5
#define RIG_PE_SETTINGS_PROPERTY_GAP 5

#define RIG_PE_SETTINGS_N_COLUMNS 2
#define RIG_PE_SETTINGS_N_ROWS ((RIG_PE_SETTINGS_N_PROPERTIES +         \
                                 RIG_PE_SETTINGS_N_COLUMNS - 1) /       \
                                RIG_PE_SETTINGS_N_COLUMNS)

typedef struct
{
  RigNumberSlider *slider;
  RigTransform *transform;
} RigPeSettingsPropertyData;

struct _RigPeSettings
{
  RigObjectProps _parent;

  RigContext *context;
  RigParticleEngine *engine;
  CoglPipeline *background_pipeline;

  RigPaintableProps paintable;
  RigGraphableProps graphable;

  RigPeSettingsPropertyData prop_data[RIG_PE_SETTINGS_N_PROPERTIES];

  int width;
  int height;

  int ref_count;
};

RigType rig_pe_settings_type;

static void
_rig_pe_settings_free (void *object)
{
  RigPeSettings *settings = object;
  int i;

  rig_ref_countable_unref (settings->context);
  rig_ref_countable_unref (settings->engine);

  cogl_object_unref (settings->background_pipeline);

  for (i = 0; i < RIG_PE_SETTINGS_N_PROPERTIES; i++)
    {
      RigPeSettingsPropertyData *prop_data = settings->prop_data + i;

      rig_graphable_remove_child (prop_data->slider);
      rig_graphable_remove_child (prop_data->transform);
      rig_ref_countable_unref (prop_data->slider);
      rig_ref_countable_unref (prop_data->transform);
    }

  g_slice_free (RigPeSettings, settings);
}

RigRefCountableVTable _rig_pe_settings_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_pe_settings_free
};

static RigGraphableVTable _rig_pe_settings_graphable_vtable = {
  NULL, /* child removed */
  NULL, /* child addded */
  NULL /* parent changed */
};

static void
_rig_pe_settings_paint (RigObject *object,
                        RigPaintContext *paint_ctx)
{
  RigPeSettings *settings = (RigPeSettings *) object;
  RigCamera *camera = paint_ctx->camera;
  CoglFramebuffer *fb = rig_camera_get_framebuffer (camera);

  cogl_framebuffer_draw_rectangle (fb,
                                   settings->background_pipeline,
                                   0.0f, 0.0f, /* x1/y1 */
                                   settings->width,
                                   settings->height);
}

RigPaintableVTable _rig_pe_settings_paintable_vtable = {
  _rig_pe_settings_paint
};

static void
_rig_pe_settings_init_type (void)
{
  rig_type_init (&rig_pe_settings_type);
  rig_type_add_interface (&rig_pe_settings_type,
                          RIG_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigPeSettings, ref_count),
                          &_rig_pe_settings_ref_countable_vtable);
  rig_type_add_interface (&rig_pe_settings_type,
                          RIG_INTERFACE_ID_PAINTABLE,
                          offsetof (RigPeSettings, paintable),
                          &_rig_pe_settings_paintable_vtable);
  rig_type_add_interface (&rig_pe_settings_type,
                          RIG_INTERFACE_ID_GRAPHABLE,
                          offsetof (RigPeSettings, graphable),
                          &_rig_pe_settings_graphable_vtable);
}

RigPeSettings *
rig_pe_settings_new (RigContext *context,
                     RigParticleEngine *engine)
{
  RigPeSettings *settings = g_slice_new0 (RigPeSettings);
  static CoglBool initialized = FALSE;
  int i;

  if (initialized == FALSE)
    {
      _rig_init ();
      _rig_pe_settings_init_type ();

      initialized = TRUE;
    }

  settings->ref_count = 1;
  settings->context = rig_ref_countable_ref (context);
  settings->engine = rig_ref_countable_ref (engine);

  settings->background_pipeline = cogl_pipeline_new (context->cogl_context);
  cogl_pipeline_set_color4ub (settings->background_pipeline,
                              64, 64, 128, 128);

  rig_object_init (&settings->_parent, &rig_pe_settings_type);

  rig_paintable_init (RIG_OBJECT (settings));
  rig_graphable_init (RIG_OBJECT (settings));

  for (i = 0; i < RIG_PE_SETTINGS_N_PROPERTIES; i++)
    {
      const RigPeSettingsProperty *prop = rig_pe_settings_properties + i;
      RigPeSettingsPropertyData *prop_data = settings->prop_data + i;
      RigNumberSlider *slider = rig_number_slider_new (context);

      rig_number_slider_set_name (slider, prop->name);
      rig_number_slider_set_min_value (slider, prop->min_value);
      rig_number_slider_set_max_value (slider, prop->max_value);
      rig_number_slider_set_value (slider, prop->getter (engine));
      rig_number_slider_set_step (slider, prop->step);
      rig_number_slider_set_decimal_places (slider, prop->decimal_places);

      prop_data->slider = slider;

      prop_data->transform = rig_transform_new (context, NULL);
      rig_graphable_add_child (prop_data->transform, slider);
      rig_graphable_add_child (settings, prop_data->transform);
    }

  rig_pe_settings_set_size (settings, 10, 10);

  return settings;
}

void
rig_pe_settings_set_size (RigPeSettings *settings,
                          int width,
                          int height)
{
  float total_width = width - RIG_PE_SETTINGS_EDGE_GAP * 2;
  float slider_width =
    (total_width - ((RIG_PE_SETTINGS_N_COLUMNS - 1.0f) *
                    RIG_PE_SETTINGS_PROPERTY_GAP)) /
    RIG_PE_SETTINGS_N_COLUMNS;
  float pixel_slider_width = floorf (slider_width);
  float y_pos = RIG_PE_SETTINGS_EDGE_GAP;
  float row_height = 0.0f;
  int i;

  settings->width = width;
  settings->height = height;

  for (i = 0; i < RIG_PE_SETTINGS_N_PROPERTIES; i++)
    {
      RigPeSettingsPropertyData *prop_data = settings->prop_data + i;
      float preferred_height;

      rig_transform_init_identity (prop_data->transform);
      rig_transform_translate (prop_data->transform,
                               RIG_PE_SETTINGS_EDGE_GAP +
                               nearbyintf ((slider_width +
                                            RIG_PE_SETTINGS_PROPERTY_GAP) *
                                           (i % RIG_PE_SETTINGS_N_COLUMNS)),
                               y_pos,
                               0.0f);

      rig_number_slider_get_preferred_height (prop_data->slider,
                                              pixel_slider_width,
                                              NULL,
                                              &preferred_height);

      rig_number_slider_set_size (prop_data->slider,
                                  pixel_slider_width,
                                  preferred_height);

      if (preferred_height > row_height)
        row_height = preferred_height;

      if ((i + 1) % RIG_PE_SETTINGS_N_COLUMNS == 0)
        {
          y_pos += row_height + RIG_PE_SETTINGS_PROPERTY_GAP;
          row_height = 0.0f;
        }
    }
}

void
rig_pe_settings_get_preferred_width (RigPeSettings *settings,
                                     float for_height,
                                     float *min_width_p,
                                     float *natural_width_p)
{
  float max_natural_width = 0.0f;
  float max_min_width = 0.0f;
  int i;

  /* Convert the for_height into a height for each row */
  if (for_height >= 0)
    for_height = ((for_height -
                   RIG_PE_SETTINGS_EDGE_GAP * 2 -
                   (RIG_PE_SETTINGS_N_ROWS - 1) *
                   RIG_PE_SETTINGS_PROPERTY_GAP) /
                  RIG_PE_SETTINGS_N_ROWS);

  for (i = 0; i < RIG_PE_SETTINGS_N_PROPERTIES; i++)
    {
      RigPeSettingsPropertyData *prop_data = settings->prop_data + i;
      float min_width;
      float natural_width;

      rig_number_slider_get_preferred_width (prop_data->slider,
                                             for_height,
                                             &min_width,
                                             &natural_width);

      if (min_width > max_min_width)
        max_min_width = min_width;
      if (natural_width > max_natural_width)
        max_natural_width = natural_width;
    }

  if (min_width_p)
    *min_width_p = (max_min_width * RIG_PE_SETTINGS_N_COLUMNS +
                    (RIG_PE_SETTINGS_N_COLUMNS - 1) *
                    RIG_PE_SETTINGS_PROPERTY_GAP +
                    RIG_PE_SETTINGS_EDGE_GAP * 2);
  if (natural_width_p)
    *natural_width_p = (max_natural_width * RIG_PE_SETTINGS_N_COLUMNS +
                        (RIG_PE_SETTINGS_N_COLUMNS - 1) *
                        RIG_PE_SETTINGS_PROPERTY_GAP +
                        RIG_PE_SETTINGS_EDGE_GAP * 2);
}

void
rig_pe_settings_get_preferred_height (RigPeSettings *settings,
                                      float for_width,
                                      float *min_height_p,
                                      float *natural_height_p)
{
  float total_height = 0.0f;
  float row_height = 0.0f;
  int i;

  /* Convert the for_width to the width that each slider will actually
   * get */
  if (for_width >= 0.0f)
    {
      float total_width = for_width - RIG_PE_SETTINGS_EDGE_GAP * 2;
      float slider_width =
        (total_width - ((RIG_PE_SETTINGS_N_COLUMNS - 1.0f) *
                        RIG_PE_SETTINGS_PROPERTY_GAP)) /
        RIG_PE_SETTINGS_N_COLUMNS;

      for_width = floorf (slider_width);
    }

  for (i = 0; i < RIG_PE_SETTINGS_N_PROPERTIES; i++)
    {
      RigPeSettingsPropertyData *prop_data = settings->prop_data + i;
      float natural_height;

      rig_number_slider_get_preferred_height (prop_data->slider,
                                              for_width,
                                              NULL, /* min_height */
                                              &natural_height);

      if (natural_height > row_height)
        row_height = natural_height;

      if ((i + 1) % RIG_PE_SETTINGS_N_COLUMNS == 0 ||
          i == RIG_PE_SETTINGS_N_PROPERTIES - 1)
        {
          total_height += row_height;
          row_height = 0.0f;
        }
    }

  total_height += (RIG_PE_SETTINGS_EDGE_GAP * 2 +
                   RIG_PE_SETTINGS_PROPERTY_GAP *
                   (RIG_PE_SETTINGS_N_ROWS - 1));

  if (min_height_p)
    *min_height_p = total_height;
  if (natural_height_p)
    *natural_height_p = total_height;
}
