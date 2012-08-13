#include <glib.h>
#include <stdint.h>

#include "rig-property.h"
#include "rig-interfaces.h"

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

void
rig_property_copy_value (RigPropertyContext *ctx,
                         RigProperty *dest,
                         RigProperty *src)
{
  g_return_if_fail (src->spec->type == dest->spec->type);

  switch ((RigPropertyType) dest->spec->type)
    {
#define COPIER(SUFFIX, CTYPE, TYPE) \
    case RIG_PROPERTY_TYPE_ ## TYPE: \
      rig_property_set_ ## SUFFIX (ctx, dest, \
                                   rig_property_get_ ## SUFFIX (src)); \
      return
#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE) COPIER(SUFFIX, CTYPE, TYPE);
#define POINTER_TYPE(SUFFIX, CTYPE, TYPE) COPIER(SUFFIX, CTYPE, TYPE);
#define COMPOSITE_TYPE(SUFFIX, CTYPE, TYPE) COPIER(SUFFIX, CTYPE, TYPE);
#define ARRAY_TYPE(SUFFIX, CTYPE, TYPE, LEN) COPIER(SUFFIX, CTYPE, TYPE);

    COPIER(text, char *, TEXT);

#include "rig-property-types.h"

#undef ARRAY_TYPE
#undef COMPOSITE_TYPE
#undef POINTER_TYPE
#undef SCALAR_TYPE
#undef COPIER
    }

  g_warn_if_reached ();
}

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

void
rig_property_box (RigProperty *property,
                  RigBoxed *boxed)
{
  boxed->type = property->spec->type;

  switch (property->spec->type)
    {
#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE) \
    case RIG_PROPERTY_TYPE_ ## TYPE: \
      { \
        boxed->d. SUFFIX ## _val = rig_property_get_ ## SUFFIX (property); \
        break; \
      }
    /* Special case the _POINTER types so we can take a reference on
     * objects... */
    case RIG_PROPERTY_TYPE_OBJECT:
      {
        RigObject *obj = rig_property_get_object (property);
        if (obj)
          boxed->d.object_val = rig_ref_countable_ref (obj);
        else
          boxed->d.object_val = NULL;
        break;
      }
    case RIG_PROPERTY_TYPE_POINTER:
      {
        boxed->d.pointer_val = rig_property_get_pointer (property);
        break;
      }
#define COMPOSITE_TYPE(SUFFIX, CTYPE, TYPE) \
    case RIG_PROPERTY_TYPE_ ## TYPE: \
      { \
        memcpy (&boxed->d. SUFFIX ## _val, \
                rig_property_get_ ## SUFFIX (property), \
                sizeof (boxed->d. SUFFIX ## _val)); \
        break; \
      }
#define ARRAY_TYPE(SUFFIX, CTYPE, TYPE, LEN) \
    case RIG_PROPERTY_TYPE_ ## TYPE: \
      { \
        memcpy (&boxed->d. SUFFIX ## _val, \
                rig_property_get_ ## SUFFIX (property), \
                sizeof (CTYPE) * LEN); \
        break; \
      }
    case RIG_PROPERTY_TYPE_TEXT:
      boxed->d.text_val = g_strdup (rig_property_get_text (property));
      break;
    }

#undef ARRAY_TYPE
#undef COMPOSITE_TYPE
#undef SCALAR_TYPE
}

void
rig_boxed_destroy (RigBoxed *boxed)
{
  if (boxed->type == RIG_PROPERTY_TYPE_OBJECT && boxed->d.object_val)
    rig_ref_countable_unref (boxed->d.object_val);
  else if (boxed->type == RIG_PROPERTY_TYPE_TEXT)
    g_free (boxed->d.text_val);
}

static double
boxed_to_double (const RigBoxed *boxed)
{
  switch (boxed->type)
    {
#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE) \
    case RIG_PROPERTY_TYPE_ ## TYPE: \
      return boxed->d.SUFFIX ## _val;
#define POINTER_TYPE(SUFFIX, CTYPE, TYPE)
#define COMPOSITE_TYPE(SUFFIX, CTYPE, TYPE)
#define ARRAY_TYPE(SUFFIX, CTYPE, TYPE, LEN)

#include "rig-property-types.h"

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
rig_property_set_boxed (RigPropertyContext *ctx,
                        RigProperty *property,
                        const RigBoxed *boxed)
{
  /* Handle basic type conversion for scalar types only... */
  if (property->spec->type != boxed->type)
    {
      double intermediate = boxed_to_double (boxed);

      switch (property->spec->type)
        {
#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE) \
        case RIG_PROPERTY_TYPE_ ## TYPE: \
          rig_property_set_ ## SUFFIX (ctx, property, intermediate); \
          return;
#define POINTER_TYPE(SUFFIX, CTYPE, TYPE)
#define COMPOSITE_TYPE(SUFFIX, CTYPE, TYPE)
#define ARRAY_TYPE(SUFFIX, CTYPE, TYPE, LEN)

#include "rig-property-types.h"

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
    case RIG_PROPERTY_TYPE_ ## TYPE: \
      rig_property_set_ ## SUFFIX (ctx, property, boxed->d.SUFFIX ## _val); \
      return
#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE) SET_BY_VAL(SUFFIX, CTYPE, TYPE);
#define POINTER_TYPE(SUFFIX, CTYPE, TYPE) SET_BY_VAL(SUFFIX, CTYPE, TYPE);
#define COMPOSITE_TYPE(SUFFIX, CTYPE, TYPE) \
    case RIG_PROPERTY_TYPE_ ## TYPE: \
      rig_property_set_ ## SUFFIX (ctx, property, &boxed->d.SUFFIX ## _val); \
      return;
#define ARRAY_TYPE(SUFFIX, CTYPE, TYPE, LEN) SET_BY_VAL(SUFFIX, CTYPE, TYPE);
    SET_BY_VAL(text, char *, TEXT);

#include "rig-property-types.h"

#undef ARRAY_TYPE
#undef COMPOSITE_TYPE
#undef POINTER_TYPE
#undef SCALAR_TYPE
#undef SET_FROM_BOXED
    }

  g_warn_if_reached ();
}

