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
#include <string.h>
#include <stdlib.h>

#include "rut-property.h"
#include "rut-introspectable.h"
#include "rut-color.h"
#include "rut-util.h"

static int dummy_object = NULL;

void
rut_property_context_init (RutPropertyContext *context)
{
  context->log = false;
  context->magic_marker = 0;
  context->change_log_stack = rut_memory_stack_new (4096);
}

void
rut_property_context_clear_log (RutPropertyContext *context)
{
  rut_memory_stack_rewind (context->change_log_stack);
  context->log_len = 0;
}

void
rut_property_context_destroy (RutPropertyContext *context)
{
  rut_memory_stack_free (context->change_log_stack);
}

void
rut_property_init (RutProperty *property,
                   const RutPropertySpec *spec,
                   void *object,
                   uint8_t id)
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
  property->id = id;
}

static void
_rut_property_destroy_binding (RutProperty *property)
{
  RutPropertyBinding *binding = property->binding;

  if (binding)
    {
      int i;

      if (binding->destroy_notify)
        binding->destroy_notify (property, binding->user_data);

      for (i = 0; binding->dependencies[i]; i++)
        {
          RutProperty *dependency = binding->dependencies[i];
          dependency->dependants =
            g_slist_remove (dependency->dependants, property);
        }

      g_slice_free1 (sizeof (RutPropertyBinding) + sizeof (void *) * (i + 1),
                     binding);

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

void
rut_property_cast_scalar_value (RutPropertyContext *ctx,
                                RutProperty *dest,
                                RutProperty *src)
{
  double val;

  switch ((RutPropertyType) src->spec->type)
    {
#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE) \
    case RUT_PROPERTY_TYPE_ ## TYPE: \
      val = rut_property_get_ ## SUFFIX (src); \
      break;
#define POINTER_TYPE(SUFFIX, CTYPE, TYPE)
#define COMPOSITE_TYPE(SUFFIX, CTYPE, TYPE)
#define ARRAY_TYPE(SUFFIX, CTYPE, TYPE, LEN)

#include "rut-property-types.h"

#undef ARRAY_TYPE
#undef COMPOSITE_TYPE
#undef POINTER_TYPE
#undef SCALAR_TYPE
    default:
      g_warn_if_reached ();
    }

  switch ((RutPropertyType) dest->spec->type)
    {
#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE) \
    case RUT_PROPERTY_TYPE_ ## TYPE: \
      rut_property_set_ ## SUFFIX (ctx, dest, val); \
      return;
#define POINTER_TYPE(SUFFIX, CTYPE, TYPE)
#define COMPOSITE_TYPE(SUFFIX, CTYPE, TYPE)
#define ARRAY_TYPE(SUFFIX, CTYPE, TYPE, LEN)

#include "rut-property-types.h"

#undef ARRAY_TYPE
#undef COMPOSITE_TYPE
#undef POINTER_TYPE
#undef SCALAR_TYPE
    default:
      g_warn_if_reached ();
    }

  g_warn_if_reached ();
}


void
_rut_property_set_binding_full_array (RutProperty *property,
                                      RutBindingCallback callback,
                                      void *user_data,
                                      RutBindingDestroyNotify destroy_notify,
                                      RutProperty **dependencies,
                                      int n_dependencies)
{
  RutPropertyBinding *binding;
  int i;

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
  else if (callback == NULL)
    return;

  binding = g_slice_alloc (sizeof (RutPropertyBinding) +
                           sizeof (void *) * (n_dependencies + 1));
  binding->callback = callback;
  binding->user_data = user_data;
  binding->destroy_notify = destroy_notify;

  memcpy (binding->dependencies, dependencies,
          sizeof (void *) * n_dependencies);
  binding->dependencies[n_dependencies] = NULL;

  for (i = 0; i < n_dependencies; i++)
    {
      RutProperty *dependency = dependencies[i];
      dependency->dependants =
        g_slist_prepend (dependency->dependants, property);
    }

  property->binding = binding;
}

static void
_rut_property_set_binding_full_valist (RutProperty *property,
                                       RutBindingCallback callback,
                                       void *user_data,
                                       RutBindingDestroyNotify destroy_notify,
                                       va_list ap)
{
  RutProperty *dependency;
  va_list aq;
  int i;

  va_copy (aq, ap);
  for (i = 0; (dependency = va_arg (aq, RutProperty *)); i++)
    ;
  va_end (aq);

  if (i)
    {
      RutProperty **dependencies;
      RutProperty *dependency;
      int j = 0;

      dependencies = alloca (sizeof (void *) * i);

      for (j = 0; (dependency = va_arg (ap, RutProperty *)); j++)
        dependencies[j] = dependency;

      _rut_property_set_binding_full_array (property,
                                            callback,
                                            user_data,
                                            destroy_notify,
                                            dependencies,
                                            i);
    }
  else
    _rut_property_set_binding_full_array (property,
                                          callback,
                                          user_data,
                                          destroy_notify,
                                          NULL,
                                          0);
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
                               void *user_data)
{
  RutPropertyContext *context = user_data;
  RutProperty *source_property =
    rut_property_get_first_source (target_property);

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
  _rut_property_copy_binding_cb (target_property, context);
}

void
rut_property_set_mirror_bindings (RutPropertyContext *context,
                                  RutProperty *prop0,
                                  RutProperty *prop1)
{
  rut_property_set_copy_binding (context, prop0, prop1);
  rut_property_set_copy_binding (context, prop1, prop0);
}

static void
_rut_property_cast_binding_cb (RutProperty *target_property,
                               void *user_data)
{
  RutPropertyContext *context = user_data;
  RutProperty *source_property =
    rut_property_get_first_source (target_property);

  rut_property_cast_scalar_value (context,
                                  target_property,
                                  source_property);
}

void
rut_property_set_cast_scalar_binding (RutPropertyContext *context,
                                      RutProperty *target_property,
                                      RutProperty *source_property)
{
  rut_property_set_binding (target_property,
                            _rut_property_cast_binding_cb,
                            context,
                            source_property,
                            NULL /* terminator */);
  _rut_property_cast_binding_cb (target_property, context);
}

void
rut_property_remove_binding (RutProperty *property)
{
  if (!property->binding)
    return;

  rut_property_set_binding (property,
                            NULL, /* no callback */
                            NULL, /* no user data */
                            NULL); /* null vararg terminator */
}

static RutPropertySpec
dummy_property_spec =
  {
    .name = "dummy",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = 0,
    .setter.any_type = abort,
    .getter.any_type = abort
  };


struct _RutPropertyClosure
{
  RutProperty dummy_prop;
  RutBindingCallback callback;
  GDestroyNotify destroy_notify;
  void *user_data;
};

static void
dummy_property_destroy_notify_cb (RutProperty *property,
                                  void *user_data)
{
  RutPropertyClosure *closure = user_data;

  if (closure->destroy_notify)
    closure->destroy_notify (closure->user_data);

  g_slice_free (RutPropertyClosure, closure);
}

static void
dummy_property_binding_wrapper_cb (RutProperty *dummy_property,
                                   void *user_data)
{
  RutPropertyClosure *closure = user_data;
  RutProperty *property = rut_property_get_first_source (dummy_property);

  closure->callback (property, closure->user_data);
}

RutPropertyClosure *
rut_property_connect_callback_full (RutProperty *property,
                                    RutBindingCallback callback,
                                    void *user_data,
                                    GDestroyNotify destroy_notify)
{
  RutPropertyClosure *closure;

  g_return_val_if_fail (callback != NULL, NULL);

  closure = g_slice_new (RutPropertyClosure);
  rut_property_init (&closure->dummy_prop,
                     &dummy_property_spec,
                     &dummy_object,
                     0); /* id */
  closure->callback = callback;
  closure->destroy_notify = destroy_notify;
  closure->user_data = user_data;
  rut_property_set_binding_full (&closure->dummy_prop,
                                 dummy_property_binding_wrapper_cb,
                                 closure,
                                 dummy_property_destroy_notify_cb,
                                 property,
                                 NULL); /* null terminator */
  return closure;
}

RutPropertyClosure *
rut_property_connect_callback (RutProperty *property,
                               RutBindingCallback callback,
                               void *user_data)
{
  return rut_property_connect_callback_full (property, callback,
                                             user_data, NULL);
}

void
rut_property_closure_destroy (RutPropertyClosure *closure)
{
  rut_property_remove_binding (&closure->dummy_prop);
}

void
rut_property_dirty (RutPropertyContext *ctx,
                    RutProperty *property)
{
  GSList *l;

  if (ctx->log)
    {
      RutObject *object = property->object;
      if (object != &dummy_object)
        {
          RutPropertyChange *change;

          g_print ("Log %d: base=%p, offset=%d: obj = %p(%s), prop id=%d(%s)\n",
                   ctx->log_len,
                   ctx->change_log_stack->sub_stack->data,
                   ctx->change_log_stack->sub_stack->offset,
                   object,
                   rut_object_get_type_name (object),
                   property->id,
                   property->spec->name);

          change = rut_memory_stack_alloc (ctx->change_log_stack,
                                           sizeof (RutPropertyChange));

          change->object = object;
          change->prop_id = property->id;
          rut_property_box (property, &change->boxed);
          ctx->log_len++;
        }
    }

  /* FIXME: The plan is for updates to happen asynchronously by
   * queueing an update with the context but for now we simply
   * trigger the updates synchronously.
   */
  for (l = property->dependants; l; l = l->next)
    {
      RutProperty *dependant = l->data;
      RutPropertyBinding *binding = dependant->binding;
      if (binding)
        binding->callback (dependant, binding->user_data);
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
          boxed->d.object_val = rut_object_ref (obj);
        else
          boxed->d.object_val = NULL;
        break;
      }
    case RUT_PROPERTY_TYPE_ASSET:
      {
        RutObject *obj = rut_property_get_asset (property);
        if (obj)
          boxed->d.asset_val = rut_object_ref (obj);
        else
          boxed->d.asset_val = NULL;
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
          dst->d.object_val = rut_object_ref (src->d.object_val);
        else
          dst->d.object_val = NULL;
        return;
      }
    case RUT_PROPERTY_TYPE_ASSET:
      {
        if (src->d.asset_val)
          dst->d.asset_val = rut_object_ref (src->d.asset_val);
        else
          dst->d.asset_val = NULL;
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
  switch (boxed->type)
    {
    case RUT_PROPERTY_TYPE_OBJECT:
      if (boxed->d.object_val)
        rut_object_unref (boxed->d.object_val);
      break;
    case RUT_PROPERTY_TYPE_ASSET:
      if (boxed->d.asset_val)
        rut_object_unref (boxed->d.asset_val);
      break;
    case RUT_PROPERTY_TYPE_POINTER:
      g_free (boxed->d.text_val);
      break;
    default:
      break;
    }
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

char *
rut_boxed_to_string (const RutBoxed *boxed,
                     const RutPropertySpec *spec)
{
  switch (boxed->type)
    {
    case RUT_PROPERTY_TYPE_FLOAT:
      return g_strdup_printf ("%f", boxed->d.float_val);
    case RUT_PROPERTY_TYPE_DOUBLE:
      return g_strdup_printf ("%f", boxed->d.double_val);
    case RUT_PROPERTY_TYPE_INTEGER:
      return g_strdup_printf ("%d", boxed->d.integer_val);
    case RUT_PROPERTY_TYPE_ENUM:
      {
        if (spec && spec->validation.ui_enum)
          {
            const RutUIEnum *ui_enum = spec->validation.ui_enum;
            int i;
            for (i = 0; ui_enum->values[i].nick; i++)
              {
                const RutUIEnumValue *enum_value = &ui_enum->values[i];
                if (enum_value->value == boxed->d.enum_val)
                  return g_strdup_printf ("<%d:%s>",
                                          boxed->d.enum_val,
                                          enum_value->nick);
              }
          }

        return g_strdup_printf ("<%d:Enum>", boxed->d.enum_val);
      }
    case RUT_PROPERTY_TYPE_UINT32:
      return g_strdup_printf ("%u", boxed->d.uint32_val);
    case RUT_PROPERTY_TYPE_BOOLEAN:
      {
        const char *bool_strings[] = { "true", "false" };
        return g_strdup (bool_strings[!!boxed->d.boolean_val]);
      }
    case RUT_PROPERTY_TYPE_TEXT:
      return g_strdup_printf ("%s", boxed->d.text_val);
    case RUT_PROPERTY_TYPE_QUATERNION:
      {
        const CoglQuaternion *quaternion = &boxed->d.quaternion_val;
        float axis[3], angle;

        cogl_quaternion_get_rotation_axis (quaternion, axis);
        angle = cogl_quaternion_get_rotation_angle (quaternion);

        return g_strdup_printf ("axis: (%.2f,%.2f,%.2f) angle: %.2f\n",
                                axis[0], axis[1], axis[2], angle);
      }
    case RUT_PROPERTY_TYPE_VEC3:
      return g_strdup_printf ("(%.1f, %.1f, %.1f)",
                              boxed->d.vec3_val[0],
                              boxed->d.vec3_val[1],
                              boxed->d.vec3_val[2]);
    case RUT_PROPERTY_TYPE_VEC4:
      return g_strdup_printf ("(%.1f, %.1f, %.1f, %.1f)",
                              boxed->d.vec3_val[0],
                              boxed->d.vec3_val[1],
                              boxed->d.vec3_val[2],
                              boxed->d.vec3_val[3]);
    case RUT_PROPERTY_TYPE_COLOR:
      return rut_color_to_string (&boxed->d.color_val);
    case RUT_PROPERTY_TYPE_OBJECT:
      return g_strdup_printf ("<%p:%s>",
                              boxed->d.object_val,
                              rut_object_get_type_name (boxed->d.object_val));
    case RUT_PROPERTY_TYPE_ASSET:
      return g_strdup_printf ("<%p:Asset>", boxed->d.asset_val);
    case RUT_PROPERTY_TYPE_POINTER:
      return g_strdup_printf ("%p", boxed->d.pointer_val);
    default:
      return g_strdup ("<rut_boxed_dump:unknown property type>");
    }
}
