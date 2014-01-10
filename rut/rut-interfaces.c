/*
 * Rut
 *
 * Copyright (C) 2012,2013  Intel Corporation
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

#include <config.h>

#include "rut-object.h"
#include "rut-interfaces.h"
#include "rut-transform-private.h"
#include "rut-property.h"
#include "rut-util.h"
#include "components/rut-camera.h"
#include "rut-refcount-debug.h"

const CoglMatrix *
rut_transformable_get_matrix (RutObject *object)
{
  RutTransformableVTable *transformable =
    rut_object_get_vtable (object, RUT_TRAIT_ID_TRANSFORMABLE);

  return transformable->get_matrix (object);
}

void
rut_sizable_set_size (RutObject *object,
                      float width,
                      float height)
{
  RutSizableVTable *sizable =
    rut_object_get_vtable (object, RUT_TRAIT_ID_SIZABLE);

  sizable->set_size (object,
                     width,
                     height);
}

void
rut_sizable_get_size (void *object,
                      float *width,
                      float *height)
{
  RutSizableVTable *sizable =
    rut_object_get_vtable (object, RUT_TRAIT_ID_SIZABLE);

  sizable->get_size (object,
                     width,
                     height);
}

void
rut_sizable_get_preferred_width (RutObject *object,
                                 float for_height,
                                 float *min_width_p,
                                 float *natural_width_p)
{
  RutSizableVTable *sizable =
    rut_object_get_vtable (object, RUT_TRAIT_ID_SIZABLE);

  sizable->get_preferred_width (object,
                                for_height,
                                min_width_p,
                                natural_width_p);
}

void
rut_sizable_get_preferred_height (RutObject *object,
                                  float for_width,
                                  float *min_height_p,
                                  float *natural_height_p)
{
  RutSizableVTable *sizable =
    rut_object_get_vtable (object, RUT_TRAIT_ID_SIZABLE);

  sizable->get_preferred_height (object,
                                 for_width,
                                 min_height_p,
                                 natural_height_p);
}

void
rut_simple_sizable_get_preferred_width (void *object,
                                        float for_height,
                                        float *min_width_p,
                                        float *natural_width_p)
{
  if (min_width_p)
    *min_width_p = 0;
  if (natural_width_p)
    *natural_width_p = 0;
}

void
rut_simple_sizable_get_preferred_height (void *object,
                                         float for_width,
                                         float *min_height_p,
                                         float *natural_height_p)
{
  if (min_height_p)
    *min_height_p = 0;
  if (natural_height_p)
    *natural_height_p = 0;
}

RutClosure *
rut_sizable_add_preferred_size_callback (RutObject *object,
                                         RutSizablePreferredSizeCallback cb,
                                         void *user_data,
                                         RutClosureDestroyCallback destroy_cb)
{
  RutSizableVTable *sizable =
    rut_object_get_vtable (object, RUT_TRAIT_ID_SIZABLE);

  /* If the object has no implementation for the needs layout callback
   * then we'll assume its preferred size never changes. We'll return
   * a dummy closure object that will never be invoked so that the
   * rest of the code doesn't need to handle this specially */
  if (sizable->add_preferred_size_callback == NULL)
    {
      RutList dummy_list;
      RutClosure *closure;

      rut_list_init (&dummy_list);

      closure = rut_closure_list_add (&dummy_list,
                                      cb,
                                      user_data,
                                      destroy_cb);

      rut_list_init (&closure->list_node);

      return closure;
    }
  else
    return sizable->add_preferred_size_callback (object,
                                                 cb,
                                                 user_data,
                                                 destroy_cb);
}

CoglPrimitive *
rut_primable_get_primitive (RutObject *object)
{
  RutPrimableVTable *primable =
    rut_object_get_vtable (object, RUT_TRAIT_ID_PRIMABLE);

  return primable->get_primitive (object);
}

void
rut_selectable_cancel (RutObject *object)
{
  RutSelectableVTable *selectable =
    rut_object_get_vtable (object, RUT_TRAIT_ID_SELECTABLE);

  selectable->cancel (object);
}
