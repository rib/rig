#include <glib.h>
#include <stdint.h>

#include "rig-property.h"

void
rig_property_context_init (RigPropertyContext *context)
{
  context->prop_update_stack = rig_memory_stack_new (4096);
}

void
rig_property_context_destroy (RigPropertyContext *context)
{
  rig_memory_stack_free (context->prop_update_stack);
}

void
rig_property_init (RigProperty *property,
                   const RigPropertySpec *spec,
                   void *object)
{
  property->spec = spec;
  property->dependants = NULL;
  property->binding = NULL;
  property->object = object;
  property->queued_count = 0;
  property->magic_marker = 0;
}

static void
_rig_property_destroy_binding (RigProperty *property)
{
  RigPropertyBinding *binding = property->binding;
  GList *l;

  if (binding)
    {
      if (binding->destroy_notify)
        binding->destroy_notify (property, binding->user_data);

      for (l = binding->dependencies; l; l = l->next)
        {
          RigProperty *dependency = l->data;
          dependency->dependants =
            g_slist_remove (dependency->dependants, property);
        }

      g_slice_free (RigPropertyBinding, binding);

      property->binding = NULL;
    }
}

void
rig_property_destroy (RigProperty *property)
{
  GSList *l;

  _rig_property_destroy_binding (property);

  /* XXX: we don't really know if this property was a hard requirement
   * for the bindings associated with dependants so for now we assume
   * it was and we free all bindings associated with them...
   */
  for (l = property->dependants; l; l = l->next)
    {
      RigProperty *dependant = l->data;
      _rig_property_destroy_binding (dependant);
    }
}

#define DECLARE_STANDARD_COPIER(SUFFIX, CTYPE, TYPE)                    \
  case RIG_PROPERTY_TYPE_ ## TYPE:                                      \
  {                                                                     \
    CTYPE value = rig_property_get_ ## SUFFIX (source_property);        \
    rig_property_set_ ## SUFFIX (ctx, target_property, value);          \
  }                                                                     \
  return

void
rig_property_copy_value (RigPropertyContext *ctx,
                         RigProperty *target_property,
                         RigProperty *source_property)
{
  g_return_if_fail (source_property->spec->type ==
                    target_property->spec->type);

  switch ((RigPropertyType) target_property->spec->type)
    {
      DECLARE_STANDARD_COPIER (float, float, FLOAT);
      DECLARE_STANDARD_COPIER (double, double, DOUBLE);
      DECLARE_STANDARD_COPIER (integer, int, INTEGER);
      DECLARE_STANDARD_COPIER (enum, int, ENUM);
      DECLARE_STANDARD_COPIER (uint32, uint32_t, UINT32);
      DECLARE_STANDARD_COPIER (boolean, CoglBool, BOOLEAN);
      DECLARE_STANDARD_COPIER (object, RigObject *, OBJECT);
      DECLARE_STANDARD_COPIER (pointer, void *, POINTER);
      DECLARE_STANDARD_COPIER (text, const char *, TEXT);
      DECLARE_STANDARD_COPIER (quaternion, const CoglQuaternion *, QUATERNION);
      DECLARE_STANDARD_COPIER (vec3, const float *, VEC3);
      DECLARE_STANDARD_COPIER (color, const RigColor *, COLOR);

    case RIG_PROPERTY_TYPE_UNKNOWN:
      return;
    }

  g_warn_if_reached ();
}

#undef DECLARE_STANDARD_COPIER

static void
_rig_property_set_binding_full_valist (RigProperty *property,
                                       RigBindingCallback callback,
                                       void *user_data,
                                       RigBindingDestroyNotify destroy_notify,
                                       va_list ap)
{
  RigPropertyBinding *binding;
  RigProperty *dependency;

  /* XXX: Note: for now we don't allow multiple bindings for the same
   * property. I'm not sure it would make sense, as they would
   * presumably conflict with each other?
   */

  if (property->binding)
    {
      g_return_if_fail (callback == NULL);

      _rig_property_destroy_binding (property);
      return;
    }

  binding = g_slice_new (RigPropertyBinding);
  binding->callback = callback;
  binding->user_data = user_data;
  binding->destroy_notify = destroy_notify;
  binding->dependencies = NULL;

  while ((dependency = va_arg (ap, RigProperty *)))
    {
      binding->dependencies =
        g_list_prepend (binding->dependencies, dependency);
      dependency->dependants =
        g_slist_prepend (dependency->dependants, property);
    }

  property->binding = binding;
}

void
rig_property_set_binding (RigProperty *property,
                          RigBindingCallback callback,
                          void *user_data,
                          ...)
{
  va_list ap;

  va_start (ap, user_data);
  _rig_property_set_binding_full_valist (property,
                                         callback,
                                         user_data,
                                         NULL, /* destroy_notify */
                                         ap);
  va_end (ap);
}

void
rig_property_set_binding_full (RigProperty *property,
                               RigBindingCallback callback,
                               void *user_data,
                               RigBindingDestroyNotify destroy_notify,
                               ...)
{
  va_list ap;

  va_start (ap, destroy_notify);
  _rig_property_set_binding_full_valist (property,
                                         callback,
                                         user_data,
                                         destroy_notify,
                                         ap);
  va_end (ap);
}

typedef struct
{
  RigPropertyContext *context;
  RigProperty *source_property;
} RigPropertyCopyBindingData;

static void
_rig_property_copy_binding_cb (RigProperty *target_property,
                               void *user_data)
{
  RigPropertyCopyBindingData *data = user_data;

  rig_property_copy_value (data->context,
                           target_property,
                           data->source_property);
}

static void
_rig_property_copy_binding_destroy_notify (RigProperty *property,
                                           void *user_data)
{
  RigPropertyCopyBindingData *data = user_data;

  g_slice_free (RigPropertyCopyBindingData, data);
}

void
rig_property_set_copy_binding (RigPropertyContext *context,
                               RigProperty *target_property,
                               RigProperty *source_property)
{
  RigPropertyCopyBindingData *data = g_slice_new (RigPropertyCopyBindingData);

  data->context = context;
  data->source_property = source_property;

  rig_property_set_binding_full (target_property,
                                 _rig_property_copy_binding_cb,
                                 data,
                                 _rig_property_copy_binding_destroy_notify,
                                 source_property,
                                 NULL /* terminator */);
}

void
rig_property_dirty (RigPropertyContext *ctx,
                    RigProperty *property)
{
  GSList *l;

  /* FIXME: The plan is for updates to happen asynchronously by
   * queueing an update with the context but for now we simply
   * trigger the updates synchronously.
   */
  for (l = property->dependants; l; l = l->next)
    {
      RigProperty *dependant = l->data;
      RigPropertyBinding *binding = dependant->binding;
      if (binding)
        binding->callback (dependant, binding->user_data);
    }
}

