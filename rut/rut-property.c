/*
 * Rut
 *
 * Copyright (C) 2012,2013  Intel Corporation.
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
 */

#include <config.h>

#include <glib.h>
#include <stdint.h>

#include "rut-property.h"
#include "rut-interfaces.h"

void
rut_property_context_init (RutPropertyContext *context)
{
  context->prop_update_stack = rut_memory_stack_new (4096);
}

void
rut_property_context_destroy (RutPropertyContext *context)
{
  rut_memory_stack_free (context->prop_update_stack);
}

void
rut_property_init (RutProperty *property,
                   const RutPropertySpec *spec,
                   void *object)
{
  /* Properties that are neither readable nor writable are a bit
   * pointless so something has probably gone wrong */
  g_warn_if_fail ((spec->flags & RUT_PROPERTY_FLAG_READWRITE) != 0);
  /* If the property is readable there should be some way to read it */
  g_warn_if_fail ((spec->flags & RUT_PROPERTY_FLAG_READABLE) == 0 ||
                  spec->data_offset != 0 ||
                  spec->getter.any_type);
  /* Same for writable properties */
  g_warn_if_fail ((spec->flags & RUT_PROPERTY_FLAG_WRITABLE) == 0 ||
                  spec->data_offset != 0 ||
                  spec->setter.any_type);

  property->spec = spec;
  property->dependants = NULL;
  property->binding = NULL;
  property->object = object;
  property->queued_count = 0;
  property->magic_marker = 0;
}

static void
_rut_property_destroy_binding (RutProperty *property)
{
  RutPropertyBinding *binding = property->binding;
  GList *l;

  if (binding)
    {
      if (binding->destroy_notify)
        binding->destroy_notify (property, binding->user_data);

      for (l = binding->dependencies; l; l = l->next)
        {
          RutProperty *dependency = l->data;
          dependency->dependants =
            g_slist_remove (dependency->dependants, property);
        }

      g_slice_free (RutPropertyBinding, binding);

      property->binding = NULL;
    }
}

void
rut_property_destroy (RutProperty *property)
{
  GSList *l;

  _rut_property_destroy_binding (property);

  /* XXX: we don't really know if this property was a hard requirement
   * for the bindings associated with dependants so for now we assume
   * it was and we free all bindings associated with them...
   */
  for (l = property->dependants; l; l = l->next)
    {
      RutProperty *dependant = l->data;
      _rut_property_destroy_binding (dependant);
    }
}

void
rut_property_copy_value (RutPropertyContext *ctx,
                         RutProperty *dest,
                         RutProperty *src)
{
  g_return_if_fail (src->spec->type == dest->spec->type);

  switch ((RutPropertyType) dest->spec->type)
    {
#define COPIER(SUFFIX, CTYPE, TYPE) \
    case RUT_PROPERTY_TYPE_ ## TYPE: \
      rut_property_set_ ## SUFFIX (ctx, dest, \
                                   rut_property_get_ ## SUFFIX (src)); \
      return
#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE) COPIER(SUFFIX, CTYPE, TYPE);
#define POINTER_TYPE(SUFFIX, CTYPE, TYPE) COPIER(SUFFIX, CTYPE, TYPE);
#define COMPOSITE_TYPE(SUFFIX, CTYPE, TYPE) COPIER(SUFFIX, CTYPE, TYPE);
#define ARRAY_TYPE(SUFFIX, CTYPE, TYPE, LEN) COPIER(SUFFIX, CTYPE, TYPE);

    COPIER(text, char *, TEXT);

#include "rut-property-types.h"

#undef ARRAY_TYPE
#undef COMPOSITE_TYPE
#undef POINTER_TYPE
#undef SCALAR_TYPE
#undef COPIER
    }

  g_warn_if_reached ();
}

static void
_rut_property_set_binding_full_valist (RutProperty *property,
                                       RutBindingCallback callback,
                                       void *user_data,
                                       RutBindingDestroyNotify destroy_notify,
                                       va_list ap)
{
  RutPropertyBinding *binding;
  RutProperty *dependency;

  /* XXX: Note: for now we don't allow multiple bindings for the same
   * property. I'm not sure it would make sense, as they would
   * presumably conflict with each other?
   */

  if (property->binding)
    {
      g_return_if_fail (callback == NULL);

      _rut_property_destroy_binding (property);
      return;
    }

  binding = g_slice_new (RutPropertyBinding);
  binding->callback = callback;
  binding->user_data = user_data;
  binding->destroy_notify = destroy_notify;
  binding->dependencies = NULL;

  while ((dependency = va_arg (ap, RutProperty *)))
    {
      binding->dependencies =
        g_list_prepend (binding->dependencies, dependency);
      dependency->dependants =
        g_slist_prepend (dependency->dependants, property);
    }

  property->binding = binding;
}

void
rut_property_set_binding (RutProperty *property,
                          RutBindingCallback callback,
                          void *user_data,
                          ...)
{
  va_list ap;

  va_start (ap, user_data);
  _rut_property_set_binding_full_valist (property,
                                         callback,
                                         user_data,
                                         NULL, /* destroy_notify */
                                         ap);
  va_end (ap);
}

void
rut_property_set_binding_full (RutProperty *property,
                               RutBindingCallback callback,
                               void *user_data,
                               RutBindingDestroyNotify destroy_notify,
                               ...)
{
  va_list ap;

  va_start (ap, destroy_notify);
  _rut_property_set_binding_full_valist (property,
                                         callback,
                                         user_data,
                                         destroy_notify,
                                         ap);
  va_end (ap);
}

void
rut_property_set_binding_by_name (RutObject *object,
                                  const char *name,
                                  RutBindingCallback callback,
                                  void *user_data,
                                  ...)
{
  RutProperty *property = rut_introspectable_lookup_property (object, name);
  va_list ap;

  g_return_if_fail (property);

  va_start (ap, user_data);
  _rut_property_set_binding_full_valist (property,
                                         callback,
                                         user_data,
                                         NULL,
                                         ap);
  va_end (ap);
}

void
rut_property_set_binding_full_by_name (RutObject *object,
                                       const char *name,
                                       RutBindingCallback callback,
                                       void *user_data,
                                       RutBindingDestroyNotify destroy_notify,
                                       ...)
{
  RutProperty *property = rut_introspectable_lookup_property (object, name);
  va_list ap;

  g_return_if_fail (property);

  va_start (ap, destroy_notify);
  _rut_property_set_binding_full_valist (property,
                                         callback,
                                         user_data,
                                         destroy_notify,
                                         ap);
  va_end (ap);
}

static void
_rut_property_copy_binding_cb (RutProperty *target_property,
                               RutProperty *source_property,
                               void *user_data)
{
  RutPropertyContext *context = user_data;

  rut_property_copy_value (context,
                           target_property,
                           source_property);
}

void
rut_property_set_copy_binding (RutPropertyContext *context,
                               RutProperty *target_property,
                               RutProperty *source_property)
{
  rut_property_set_binding (target_property,
                            _rut_property_copy_binding_cb,
                            context,
                            source_property,
                            NULL /* terminator */);
}

void
rut_property_dirty (RutPropertyContext *ctx,
                    RutProperty *property)
{
  GSList *l;

  /* FIXME: The plan is for updates to happen asynchronously by
   * queueing an update with the context but for now we simply
   * trigger the updates synchronously.
   */
  for (l = property->dependants; l; l = l->next)
    {
      RutProperty *dependant = l->data;
      RutPropertyBinding *binding = dependant->binding;
      if (binding)
        binding->callback (dependant, property, binding->user_data);
    }
}

void
rut_property_box (RutProperty *property,
                  RutBoxed *boxed)
{
  boxed->type = property->spec->type;

  switch ((RutPropertyType) property->spec->type)
    {
#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE) \
    case RUT_PROPERTY_TYPE_ ## TYPE: \
      { \
        boxed->d. SUFFIX ## _val = rut_property_get_ ## SUFFIX (property); \
        break; \
      }
    /* Special case the _POINTER types so we can take a reference on
     * objects... */
#define POINTER_TYPE(SUFFIX, CTYPE, TYPE)
    case RUT_PROPERTY_TYPE_OBJECT:
      {
        RutObject *obj = rut_property_get_object (property);
        if (obj)
          boxed->d.object_val = rut_refable_ref (obj);
        else
          boxed->d.object_val = NULL;
        break;
      }
    case RUT_PROPERTY_TYPE_POINTER:
      {
        boxed->d.pointer_val = rut_property_get_pointer (property);
        break;
      }
#define COMPOSITE_TYPE(SUFFIX, CTYPE, TYPE) \
    case RUT_PROPERTY_TYPE_ ## TYPE: \
      { \
        memcpy (&boxed->d. SUFFIX ## _val, \
                rut_property_get_ ## SUFFIX (property), \
                sizeof (boxed->d. SUFFIX ## _val)); \
        break; \
      }
#define ARRAY_TYPE(SUFFIX, CTYPE, TYPE, LEN) \
    case RUT_PROPERTY_TYPE_ ## TYPE: \
      { \
        memcpy (&boxed->d. SUFFIX ## _val, \
                rut_property_get_ ## SUFFIX (property), \
                sizeof (CTYPE) * LEN); \
        break; \
      }
    case RUT_PROPERTY_TYPE_TEXT:
      boxed->d.text_val = g_strdup (rut_property_get_text (property));
      break;

#include "rut-property-types.h"
    }

#undef POINTER_TYPE
#undef ARRAY_TYPE
#undef COMPOSITE_TYPE
#undef SCALAR_TYPE
}

void
rut_boxed_copy (RutBoxed *dst,
                const RutBoxed *src)
{
  dst->type = src->type;

  switch (src->type)
    {
#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE)                      \
    case RUT_PROPERTY_TYPE_ ## TYPE:                          \
      dst->d. SUFFIX ## _val = src->d. SUFFIX ## _val;        \
      return;
#define COMPOSITE_TYPE(SUFFIX, CTYPE, TYPE) \
    SCALAR_TYPE (SUFFIX, CTYPE, TYPE)
    /* Special case the _POINTER types so we can take a reference on
     * objects... */
#define POINTER_TYPE(SUFFIX, CTYPE, TYPE)
    case RUT_PROPERTY_TYPE_OBJECT:
      {
        if (src->d.object_val)
          dst->d.object_val = rut_refable_ref (src->d.object_val);
        else
          dst->d.object_val = NULL;
        return;
      }
    case RUT_PROPERTY_TYPE_POINTER:
      dst->d.pointer_val = src->d.pointer_val;
      return;
#define ARRAY_TYPE(SUFFIX, CTYPE, TYPE, LEN)                    \
    case RUT_PROPERTY_TYPE_ ## TYPE:                          \
      {                                                       \
        memcpy (&dst->d. SUFFIX ## _val,                      \
                &src->d. SUFFIX ## _val,                      \
                sizeof (CTYPE) * LEN);                        \
        return;                                               \
      }
    case RUT_PROPERTY_TYPE_TEXT:
      dst->d.text_val = g_strdup (src->d.text_val);
      return;

#include "rut-property-types.h"
    }

#undef POINTER_TYPE
#undef ARRAY_TYPE
#undef COMPOSITE_TYPE
#undef SCALAR_TYPE

  g_assert_not_reached ();
}

void
rut_boxed_destroy (RutBoxed *boxed)
{
  if (boxed->type == RUT_PROPERTY_TYPE_OBJECT && boxed->d.object_val)
    rut_refable_unref (boxed->d.object_val);
  else if (boxed->type == RUT_PROPERTY_TYPE_TEXT)
    g_free (boxed->d.text_val);
}

static double
boxed_to_double (const RutBoxed *boxed)
{
  switch (boxed->type)
    {
#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE) \
    case RUT_PROPERTY_TYPE_ ## TYPE: \
      return boxed->d.SUFFIX ## _val;
#define POINTER_TYPE(SUFFIX, CTYPE, TYPE)
#define COMPOSITE_TYPE(SUFFIX, CTYPE, TYPE)
#define ARRAY_TYPE(SUFFIX, CTYPE, TYPE, LEN)

#include "rut-property-types.h"

#undef SCALAR_TYPE
#undef POINTER_TYPE
#undef COMPOSITE_TYPE
#undef ARRAY_TYPE

    default:
      g_warn_if_reached ();
      return 0;
    }
}

void
rut_property_set_boxed (RutPropertyContext *ctx,
                        RutProperty *property,
                        const RutBoxed *boxed)
{
  /* Handle basic type conversion for scalar types only... */
  if (property->spec->type != boxed->type)
    {
      double intermediate = boxed_to_double (boxed);

      switch (property->spec->type)
        {
#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE) \
        case RUT_PROPERTY_TYPE_ ## TYPE: \
          rut_property_set_ ## SUFFIX (ctx, property, intermediate); \
          return;
#define POINTER_TYPE(SUFFIX, CTYPE, TYPE)
#define COMPOSITE_TYPE(SUFFIX, CTYPE, TYPE)
#define ARRAY_TYPE(SUFFIX, CTYPE, TYPE, LEN)

#include "rut-property-types.h"

#undef SCALAR_TYPE
#undef POINTER_TYPE
#undef COMPOSITE_TYPE
#undef ARRAY_TYPE

        default:
          g_warn_if_reached ();
          return;
        }
    }

  switch (boxed->type)
    {
#define SET_BY_VAL(SUFFIX, CTYPE, TYPE) \
    case RUT_PROPERTY_TYPE_ ## TYPE: \
      rut_property_set_ ## SUFFIX (ctx, property, boxed->d.SUFFIX ## _val); \
      return
#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE) SET_BY_VAL(SUFFIX, CTYPE, TYPE);
#define POINTER_TYPE(SUFFIX, CTYPE, TYPE) SET_BY_VAL(SUFFIX, CTYPE, TYPE);
#define COMPOSITE_TYPE(SUFFIX, CTYPE, TYPE) \
    case RUT_PROPERTY_TYPE_ ## TYPE: \
      rut_property_set_ ## SUFFIX (ctx, property, &boxed->d.SUFFIX ## _val); \
      return;
#define ARRAY_TYPE(SUFFIX, CTYPE, TYPE, LEN) SET_BY_VAL(SUFFIX, CTYPE, TYPE);
    SET_BY_VAL(text, char *, TEXT);

#include "rut-property-types.h"

#undef ARRAY_TYPE
#undef COMPOSITE_TYPE
#undef POINTER_TYPE
#undef SCALAR_TYPE
#undef SET_FROM_BOXED
    }

  g_warn_if_reached ();
}

