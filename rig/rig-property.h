#ifndef _RIG_PROPERTY_H_
#define _RIG_PROPERTY_H_

#include <sys/types.h>

#include <cogl/cogl.h>

#include "rig-memory-stack.h"
#include "rig-types.h"
#include "rig-object.h"

typedef struct _RigPropertyContext
{
  RigMemoryStack *prop_update_stack;
} RigPropertyContext;

typedef enum _RigPropertyType
{
  RIG_PROPERTY_TYPE_FLOAT = 1,
  RIG_PROPERTY_TYPE_DOUBLE,
  RIG_PROPERTY_TYPE_INTEGER,
  RIG_PROPERTY_TYPE_ENUM,
  RIG_PROPERTY_TYPE_UINT32,
  RIG_PROPERTY_TYPE_BOOLEAN,
  RIG_PROPERTY_TYPE_TEXT,
  RIG_PROPERTY_TYPE_QUATERNION,
  RIG_PROPERTY_TYPE_VEC3,
  RIG_PROPERTY_TYPE_VEC4,
  RIG_PROPERTY_TYPE_COLOR,
  RIG_PROPERTY_TYPE_OBJECT,
  RIG_PROPERTY_TYPE_POINTER,
} RigPropertyType;

typedef struct _RigProperty RigProperty;

typedef void (*RigPropertyUpdateCallback) (RigProperty *property,
                                           void *user_data);

typedef union _RigPropertyDefault
{
  int integer;
  CoglBool boolean;
  const void *pointer;
} RigPropertyDefault;

typedef struct _RigBoxed
{
  RigPropertyType type;
  union
    {
      float float_val;
      double double_val;
      int integer_val;
      int enum_val;
      uint32_t uint32_val;
      CoglBool boolean_val;
      char *text_val;
      CoglQuaternion quaternion_val;
      float vec3_val[3];
      float vec4_val[4];
      RigColor color_val;
      RigObject *object_val;
      void *pointer_val;
    } d;
} RigBoxed;


typedef struct _RigPropertyValidationInteger
{
  int min;
  int max;
} RigPropertyValidationInteger;

typedef struct _RigPropertyValidationFloat
{
  float min;
  float max;
} RigPropertyValidationFloat;

typedef struct _RigPropertyValidationVec3
{
  float min;
  float max;
} RigPropertyValidationVec3;

typedef struct _RigPropertyValidationVec4
{
  float min;
  float max;
} RigPropertyValidationVec4;

typedef struct _RigPropertyValidationObject
{
  RigType *type;
} RigPropertyValidationObject;

typedef union _RigPropertyValidation
{
  RigPropertyValidationInteger int_range;
  RigPropertyValidationFloat float_range;
  RigPropertyValidationVec3 vec3_range;
  RigPropertyValidationVec4 vec4_range;
  RigPropertyValidationObject object;
  const RigUIEnum *ui_enum;
} RigPropertyValidation;

typedef enum _RigPropertyFlags
{
  RIG_PROPERTY_FLAG_READWRITE = 1<<0,
  RIG_PROPERTY_FLAG_READABLE = 1<<1,
  RIG_PROPERTY_FLAG_VALIDATE = 1<<2,
} RigPropertyFlags;

typedef struct _RigPropertySpec
{
  const char *name;

  /* XXX: this might be too limited since it means we can't have
   * dynamically allocated properties that get associated with an
   * object...
   *
   * I suppose though in such a case it's just required to have
   * associated getter and setter functions which means we won't
   * directly reference the data using the offset anyway.
   */
  size_t data_offset;

  /* Note: these are optional. If the property value doesn't
   * need validation then the setter can be left as NULL
   * and if the value is always up to date the getter can
   * also be left as NULL. */
  void *getter;
  void *setter;

  const char *nick;
  const char *blurb;
  RigPropertyFlags flags;
  RigPropertyDefault default_value;
  RigPropertyValidation validation;

  unsigned int type:16;
  unsigned int is_ui_property:1;
} RigPropertySpec;

typedef void (*RigBindingCallback) (RigProperty *property, void *user_data);

typedef void (*RigBindingDestroyNotify) (RigProperty *property,
                                         void *user_data);

/* XXX: make sure bindings get freed if any of of the dependency
 * properties are destroyed. */
typedef struct _RigPropertyBinding
{
  /* When the property this binding is for gets destroyed we need to
   * know the dependencies so we can remove this property from the
   * corresponding list of dependants for each dependency.
   */
  GList *dependencies;
  RigBindingCallback callback;
  RigBindingDestroyNotify destroy_notify;
  void *user_data;
} RigPropertyBinding;

struct _RigProperty
{
  const RigPropertySpec *spec;
  GSList *dependants;
  RigPropertyBinding *binding; /* Maybe make this a list of bindings? */
  void *object;
  uint16_t queued_count;
  uint16_t magic_marker;
};

#if 0
typedef struct _RigUIProperty
{
  RigProperty _parent;

  const char *nick;
  const char *description;
  void *default_value;
} RigUIProperty;

#endif


/* A quick example of using the rig property system in conjunction
 * with the RigSimpleIntrospectable interface.
 */
#if 0

enum {
  FLIBBLE_X_PROP,
  FLIBBLE_N_PROPS
};

typedef struct _Flibble
{
  float x;

  RigSimpleIntrospectableProps introspectable;
  RigProperty properties[RIG_FLIBBLE_N_PROPS];

} Flibble;

static RigPropertySpec flibble_prop_specs[] = {
  {
    .name = "x";
    .type = RIG_PROPERTY_TYPE_FLOAT;
    .data_offset = offsetof (RigSlider, x);
    .getter = flibble_get_x; /* optional: for non-trivial properties */
    .setter = flibble_set_x; /* optional: for non-trivial properties */
  },
  { 0 } /* XXX: Needed for runtime counting of the number of properties
                if you use the RigSimpleIntrospectable interface */
};

static void
flibble_free (void *object)
{
  /* SNIP */

  rig_simple_introspectable_destroy (flibble);

  /* SNIP */
}

/* SNIP */

void
flibble_new (void)
{
  /* SNIP */

  rig_simple_introspectable_init (flibble,
                                  flibble_prop_specs,
                                  flibble->properties);

  /* SNIP */
}

/* Actually for this example you could leave the getter and setter as
 * NULL since rig can handle this case automatically without any
 * function call overhead. */
void
flibble_set_x (Flibble *flibble, float x)
{
  if (flibble->x == x)
    return;

  flibble->x = x;

  rig_property_dirty (flibble->ctx, &flibble->properties[FLIBBLE_PROP_X]);
}

float
flibble_get_x (Flibble *flibble)
{
  return flibble->x;
}
#endif

void
rig_property_context_init (RigPropertyContext *context);

void
rig_property_context_destroy (RigPropertyContext *context);

void
rig_property_destroy (RigProperty *property);

void
rig_property_set_binding (RigProperty *property,
                          RigBindingCallback callback,
                          void *user_data,
                          ...) G_GNUC_NULL_TERMINATED;

void
rig_property_set_binding_full (RigProperty *property,
                               RigBindingCallback callback,
                               void *user_data,
                               RigBindingDestroyNotify destroy_notify,
                               ...) G_GNUC_NULL_TERMINATED;

void
rig_property_set_binding_by_name (RigObject *object,
                                  const char *name,
                                  RigBindingCallback callback,
                                  void *user_data,
                                  ...) G_GNUC_NULL_TERMINATED;

void
rig_property_set_binding_full_by_name (RigObject *object,
                                       const char *name,
                                       RigBindingCallback callback,
                                       void *user_data,
                                       RigBindingDestroyNotify destroy_notify,
                                       ...) G_GNUC_NULL_TERMINATED;

/**
 * rig_property_set_copy_binding:
 * @context: The property context that will be used to set the property.
 * @target_property: The property to set the binding on.
 * @source_property: The depedent property that the value will be taken from.
 *
 * This links the value of @target_property with the value of
 * @source_property so that whenever the value of the source property
 * changes the target property will be updated with a copy of the same
 * value. Note that the binding is only in one direction so that
 * changes in @target_property do not affect @source_property.
 */
void
rig_property_set_copy_binding (RigPropertyContext *context,
                               RigProperty *target_property,
                               RigProperty *source_property);

/*
 * XXX: Issues
 *
 * Example problems:
 *
 * What about the possibility of deriving the value of a particular
 * property from ONE of 5 other properties that could change; but you
 * would need to specifically know which of the five properties
 * changed to be able to define a binding?
 *
 *  - One possibility is to define a dummy/intermediate property
 *    for each of the 5 properties and create bindings for
 *    those such that any one of the binding callbacks can
 *    instead update the ONE special property.
 *
 *    It would mean there is a break in the true dependency graph
 *    and so the update process would take multiple iterations due
 *    to updates being queued during update processing.
 *
 * What about a bidirectional relationship between two properties
 * e.g. Celsius and Fahrenheit properties.
 *
 *  - The update processing somehow needs to be designed so that
 *    it can detect circular references....
 *
 *    In addition to updating the queued_count associated with a
 *    property each time it is queued, we also compare
 *    property::magic_marker to a number thats updated for each batch
 *    of updates being queued (just incremented for each batch). Since
 *    the range of the marker is small it can have false positives but
 *    if ::magic_marker matches our current batch number its possible there
 *    is a circular reference so we explicitly walk back through the
 *    stack until the point where this batch started and check for an
 *    existing record for the property. If not found then we save the
 *    random number into ::magic_marker and queue the update.
 *
 *
 * XXX: Is it worth considering a design that works on the basis
 * of dirtying dependants instead of assuming they need to be
 * updated otherwise we might do lots of redundant work updating
 * variables that are costly to derive.
 *
 *  - We could say that the update function is optional and add
 *  a dirty flag which gets set by
 *  rig_property_dirty. Also such properties
 *  could at least just set a dirty flag in their update function
 *  and so long as the property has a getter function they
 *  will get control when someone needs to value so it can be derived
 *  lazily.
 *
 *
 * General design goals:
 * - Minimize function call overhead considering having huge numbers
 *   of properties being updated every frame (tens of thousands)
 *
 * - Consider bidirectional relationships so we can link GUI sliders
 *   to properties, and also allow property updates to affect
 *   corresponding UI elements.
 *
 * - Avoid costly type conversions
 *
 * - Avoid redundant notification work when changing a property, esp
 *   if nothing actually cares about that property.
 *
 * - Allow easy derivation of properties from a set of dependency
 *   properties, where the code making the relationship isn't
 *   part of the code that directly owns the property being modified
 *   but just part of some application logic.
 *
 *
 * XXX:
 * What api do we want for actually declaring a dependency
 * relationship between properties?
 *
 * rig_property_bind (update_cb, dest, srcA, srcB, srcC, NULL);
 *
 * rig_property_add_dependant (srcA, my_flibble_property);
 *
 * rig_property_set_binding (property, binding_cb, user_data);
 * rig_property_add_binding_dependant (property, srcA);
 * rig_property_add_binding_dependant (property, srcB);
 * rig_property_remove_binding_dependant (property, srcB);
 *
 *
 */

void
rig_property_init (RigProperty *property,
                   const RigPropertySpec *spec,
                   void *object);

void
rig_property_dirty (RigPropertyContext *ctx,
                    RigProperty *property);

void
rig_property_copy_value (RigPropertyContext *ctx,
                         RigProperty *target_property,
                         RigProperty *source_property);

void
rig_property_box (RigProperty *property,
                  RigBoxed *boxed);

void
rig_property_set_boxed (RigPropertyContext *ctx,
                        RigProperty *property,
                        const RigBoxed *boxed);

void
rig_boxed_destroy (RigBoxed *boxed);

#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE) \
static inline void \
rig_property_set_ ## SUFFIX (RigPropertyContext *ctx, \
                             RigProperty *property, \
                             CTYPE value) \
{ \
  CTYPE *data = (CTYPE *)((uint8_t *)property->object + \
                          property->spec->data_offset); \
 \
  g_return_if_fail (property->spec->type == RIG_PROPERTY_TYPE_ ## TYPE); \
 \
  if (property->spec->getter == NULL && *data == value) \
    return; \
 \
  if (property->spec->setter) \
    { \
      void (*setter) (RigProperty *, CTYPE) = property->spec->setter; \
      setter (property->object, value); \
    } \
  else \
    { \
      *data = value; \
      if (property->dependants) \
        rig_property_dirty (ctx, property); \
    } \
} \
 \
static inline CTYPE \
rig_property_get_ ## SUFFIX (RigProperty *property) \
{ \
  g_return_val_if_fail (property->spec->type == RIG_PROPERTY_TYPE_ ## TYPE, 0); \
 \
  if (property->spec->getter) \
    { \
      CTYPE (*getter) (RigProperty *property) = property->spec->getter; \
      return getter (property); \
    } \
  else \
    { \
      CTYPE *data = (CTYPE *)((uint8_t *)property->object + \
                              property->spec->data_offset); \
      return *data; \
    } \
}

#define POINTER_TYPE(SUFFIX, CTYPE, TYPE) SCALAR_TYPE(SUFFIX, CTYPE, TYPE)

#define COMPOSITE_TYPE(SUFFIX, CTYPE, TYPE) \
static inline void \
rig_property_set_ ## SUFFIX (RigPropertyContext *ctx, \
                             RigProperty *property, \
                             const CTYPE *value) \
{ \
  CTYPE *data = \
    (CTYPE *)((uint8_t *)property->object + \
                         property->spec->data_offset); \
 \
  g_return_if_fail (property->spec->type == RIG_PROPERTY_TYPE_ ## TYPE); \
 \
  if (property->spec->setter) \
    { \
      void (*setter) (RigProperty *, const CTYPE *) = property->spec->setter; \
      setter (property->object, value); \
    } \
  else \
    { \
      *data = *value; \
      if (property->dependants) \
        rig_property_dirty (ctx, property); \
    } \
} \
 \
static inline const CTYPE * \
rig_property_get_ ## SUFFIX (RigProperty *property) \
{ \
  g_return_val_if_fail (property->spec->type == RIG_PROPERTY_TYPE_ ## TYPE, 0); \
 \
  if (property->spec->getter) \
    { \
      CTYPE *(*getter) (RigProperty *property) = property->spec->getter; \
      return getter (property); \
    } \
  else \
    { \
      CTYPE *data = (CTYPE *)((uint8_t *)property->object + \
                              property->spec->data_offset); \
      return data; \
    } \
}

#define ARRAY_TYPE(SUFFIX, CTYPE, TYPE, LEN) \
static inline void \
rig_property_set_ ## SUFFIX (RigPropertyContext *ctx, \
                             RigProperty *property, \
                             const CTYPE value[LEN]) \
{ \
  float *data = (float *) ((uint8_t *) property->object + \
                           property->spec->data_offset); \
 \
  g_return_if_fail (property->spec->type == RIG_PROPERTY_TYPE_ ## TYPE); \
 \
  if (property->spec->setter) \
    { \
      void (*setter) (RigProperty *, const CTYPE[LEN]) = \
        property->spec->setter; \
      setter (property->object, value); \
    } \
  else \
    { \
      memcpy (data, value, sizeof (CTYPE) * LEN); \
      if (property->dependants) \
        rig_property_dirty (ctx, property); \
    } \
} \
 \
static inline const CTYPE * \
rig_property_get_ ## SUFFIX (RigProperty *property) \
{ \
  g_return_val_if_fail (property->spec->type == RIG_PROPERTY_TYPE_ ## TYPE, 0); \
 \
  if (property->spec->getter) \
    { \
      const CTYPE *(*getter) (RigProperty *property) = property->spec->getter; \
      return getter (property); \
    } \
  else \
    { \
      const CTYPE *data = (const CTYPE *) ((uint8_t *) property->object + \
                                           property->spec->data_offset); \
      return data; \
    } \
}

#include "rig-property-types.h"

#undef ARRAY_TYPE
#undef POINTER_TYPE
#undef COMPOSITE_TYPE
#undef SCALAR_TYPE

static inline void
rig_property_set_text (RigPropertyContext *ctx,
                       RigProperty *property,
                       const char *value)
{
  char **data =
    (char **)(uint8_t *)property->object + property->spec->data_offset;

  g_return_if_fail (property->spec->type == RIG_PROPERTY_TYPE_TEXT);

  if (property->spec->setter)
    {
      void (*setter) (RigProperty *, const char *) = property->spec->setter;
      setter (property->object, value);
    }
  else
    {
      if (*data)
        g_free (*data);
      *data = g_strdup (value);
      if (property->dependants)
        rig_property_dirty (ctx, property);
    }
}

static inline const char *
rig_property_get_text (RigProperty *property)
{
  g_return_val_if_fail (property->spec->type == RIG_PROPERTY_TYPE_TEXT, 0);

  if (property->spec->getter)
    {
      const char *(*getter) (RigProperty *property) = property->spec->getter;
      return getter (property);
    }
  else
    {
      const char **data = (const char **)((uint8_t *)property->object +
                                          property->spec->data_offset);
      return *data;
    }
}

#if 0

struct _RigProperty
{
  /* PRIVATE */

  const char *name;

  void *data;

  /* A property can be linked to any number of dependency properties
   * so that it will be automatically prompted for an update if any of
   * those dependencies change.
   */
  RigPropertyUpdateCallback update_cb;

  /* This is the list of properties that depend on this property and
   * should be prompted for an update whenever this property changes.
   */
  GSList *dependants;

  /* Callbacks typed according to the property::type for setting and
   * getting the property value. These may be NULL if direct access
   * via the ::data pointer is ok.
   *
   * For example, for _TYPE_FLOAT properties these would be typed as:
   *
   *   float (*getter) (void *object);
   *   void (*setter) (void *object, float value);
   *
   * If the type is a struct then the getter would instead be:
   *
   *   void (*getter) (void *object, StructType *out_value);
   */
  void *getter;
  void *setter;

  /* This can be any private data pointer really, but would normally
   * be a pointer to an object instance. This is passed as the second
   * argument to update callback and as the first argument to the
   * getter and setter callbacks. */
  void *object;

  /* FLOAT | INT | BOOLEAN | STRING etc */
  unsigned int type:16;
  unsigned int queued_count:8;

  /* Can this property be cast to a RigUIProperty to access additional
   * information, such as a description and default value?
   */
  unsigned int is_ui_property:1;

  /* When a property is changed, we queue all dependants to be
   * updated. (We don't update them synchronously.)
   *
   * This is so that properties that depend on multiple properties
   * don't get updated redundantly for each dependency change, they
   * get updated once when potentially many dependencies have been
   * updated.
   *
   * Note: It's possible for properties to be queued multiple times
   * in a frame and we track that by incrementing the ->queued_count
   * each time a property is queued to be updated.
   *
   * Note: queued_count is always initialized to 0 when a property
   * is created and we always reset it to 0 when we actually update
   * the property so we can use this to reliably track how many
   * times the property has been queued.
   *
   * The updates are queued by pushing a pointer to a property to a
   * stack. The underlying allocation for the stack is grow only and
   * the stack is rewound after processing updates.
   *
   * To process updates we simply walk the stack of queued updates
   * in the order the were pushed, and update each property that
   * has a ->queued_count == 0. If we find a property with a
   * queued_count > 0 then we decrement it and continue looking at the
   * next property.
   */
};

void
rig_property_init (RigProperty *property,
                   const char *name,
                   RigPropertyType type,
                   void *value_addr,
                   RigPropertyUpdateCallback update_cb,
                   void *user_data);

void
rig_property_set_float (RigProperty *property,
                        float value);

float
rig_property_get_float (RigProperty *property);

void
rig_property_set_double (RigProperty *property,
                         double value);

double
rig_property_get_double (RigProperty *property);

#endif

#endif /* _RIG_PROPERTY_H_ */
