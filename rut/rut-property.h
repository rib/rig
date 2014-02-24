#ifndef _RUT_PROPERTY_H_
#define _RUT_PROPERTY_H_

#include <sys/types.h>
#include <stdbool.h>
#include <string.h>

#include <cogl/cogl.h>

#include "rut-memory-stack.h"
#include "rut-object.h"

typedef enum _RutPropertyType
{
  RUT_PROPERTY_TYPE_FLOAT = 1,
  RUT_PROPERTY_TYPE_DOUBLE,
  RUT_PROPERTY_TYPE_INTEGER,
  RUT_PROPERTY_TYPE_ENUM,
  RUT_PROPERTY_TYPE_UINT32,
  RUT_PROPERTY_TYPE_BOOLEAN,
  RUT_PROPERTY_TYPE_TEXT,
  RUT_PROPERTY_TYPE_QUATERNION,
  RUT_PROPERTY_TYPE_VEC3,
  RUT_PROPERTY_TYPE_VEC4,
  RUT_PROPERTY_TYPE_COLOR,
  RUT_PROPERTY_TYPE_OBJECT,
  RUT_PROPERTY_TYPE_ASSET,
  RUT_PROPERTY_TYPE_POINTER,
} RutPropertyType;


typedef struct _RutBoxed
{
  RutPropertyType type;
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
      CoglColor color_val;
      RutObject *object_val;
      RutObject *asset_val;
      void *pointer_val;
    } d;
} RutBoxed;


typedef struct _RutPropertyChange
{
  void *object;
  RutBoxed boxed;
  int prop_id;
} RutPropertyChange;


/* We forward declare this since we have a circular header dependency
 * where rut-asset.h indirectly includes rut-context.h which depends
 * on this... */
typedef struct _RutPropertyContext
{
  bool log;
  int magic_marker;
  RutMemoryStack *change_log_stack;
  int log_len;
} RutPropertyContext;

#include "rut-types.h"
#include "rut-asset.h"
#include "rut-closure.h"

typedef struct _RutProperty RutProperty;

typedef void (*RutPropertyUpdateCallback) (RutProperty *property,
                                           void *user_data);

typedef union _RutPropertyDefault
{
  int integer;
  CoglBool boolean;
  const void *pointer;
} RutPropertyDefault;


typedef struct _RutPropertyValidationInteger
{
  int min;
  int max;
} RutPropertyValidationInteger;

typedef struct _RutPropertyValidationFloat
{
  float min;
  float max;
} RutPropertyValidationFloat;

typedef struct _RutPropertyValidationVec3
{
  float min;
  float max;
} RutPropertyValidationVec3;

typedef struct _RutPropertyValidationVec4
{
  float min;
  float max;
} RutPropertyValidationVec4;

typedef struct _RutPropertyValidationObject
{
  RutType *type;
} RutPropertyValidationObject;

typedef struct _RutPropertyValidationAsset
{
  RutAssetType type;
} RutPropertyValidationAsset;

typedef union _RutPropertyValidation
{
  RutPropertyValidationInteger int_range;
  RutPropertyValidationFloat float_range;
  RutPropertyValidationVec3 vec3_range;
  RutPropertyValidationVec4 vec4_range;
  RutPropertyValidationObject object;
  RutPropertyValidationAsset asset;
  const RutUIEnum *ui_enum;
} RutPropertyValidation;

typedef enum _RutPropertyFlags
{
  RUT_PROPERTY_FLAG_READABLE = 1<<0,
  RUT_PROPERTY_FLAG_WRITABLE = 1<<1,
  RUT_PROPERTY_FLAG_VALIDATE = 1<<2,

  RUT_PROPERTY_FLAG_READWRITE = (RUT_PROPERTY_FLAG_READABLE |
                                 RUT_PROPERTY_FLAG_WRITABLE)
} RutPropertyFlags;

typedef struct _RutPropertySpec
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
  union
  {
    float (* float_type) (void *object);
    double (* double_type) (void *object);
    int (* integer_type) (void *object);
    int (* enum_type) (void *object);
    uint32_t (* uint32_type) (void *object);
    bool (* boolean_type) (void *object);
    const char *(* text_type) (void *object);
    const CoglQuaternion *(* quaternion_type) (void *object);
    const CoglColor *(* color_type) (void *object);
    const float *(* vec3_type) (void *object);
    const float *(* vec4_type) (void *object);
    void *(* object_type) (void *object);
    RutAsset *(* asset_type) (RutObject *object);
    void *(* pointer_type) (void *object);

    /* This is just used to check the pointer against NULL */
    void *any_type;
  } getter;
  union
  {
    void (* float_type) (void *object, float value);
    void (* double_type) (void *object, double value);
    void (* integer_type) (void *object, int value);
    void (* enum_type) (void *object, int value);
    void (* uint32_type) (void *object, uint32_t value);
    void (* boolean_type) (void *object, bool value);
    void (* text_type) (void *object, const char *value);
    void (* quaternion_type) (void *object,
                              const CoglQuaternion *quaternion);
    void (* color_type) (void *object,
                         const CoglColor *color);
    void (* vec3_type) (void *object, const float value[3]);
    void (* vec4_type) (void *object, const float value[4]);
    void (* object_type) (void *object, void *value);
    void (* asset_type) (RutObject *object, RutAsset *value);
    void (* pointer_type) (void *object, void *value);

    /* This is just used to check the pointer against NULL */
    void *any_type;
  } setter;

  const char *nick;
  const char *blurb;
  RutPropertyFlags flags;
  RutPropertyDefault default_value;
  RutPropertyValidation validation;

  unsigned int type:16;
  unsigned int is_ui_property:1;
  /* Whether this property is allowed to be animatable or not */
  unsigned int animatable:1;
} RutPropertySpec;

/* Note: we intentionally don't pass a pointer to a "source property"
 * that is the property that has changed because RutProperty is
 * designed so that we can defer binding callbacks until the mainloop
 * so we can avoid redundant callbacks in cases where multiple
 * dependencies of a property may be changed. */
typedef void (*RutBindingCallback) (RutProperty *target_property,
                                    void *user_data);

typedef void (*RutBindingDestroyNotify) (RutProperty *property,
                                         void *user_data);

/* XXX: make sure bindings get freed if any of of the dependency
 * properties are destroyed. */
typedef struct _RutPropertyBinding
{
  RutBindingCallback callback;
  RutBindingDestroyNotify destroy_notify;
  void *user_data;
  /* When the property this binding is for gets destroyed we need to
   * know the dependencies so we can remove this property from the
   * corresponding list of dependants for each dependency.
   */
  RutProperty *dependencies[];
} RutPropertyBinding;

struct _RutProperty
{
  const RutPropertySpec *spec;
  GSList *dependants;
  RutPropertyBinding *binding; /* Maybe make this a list of bindings? */
  void *object;

  uint16_t queued_count;
  uint8_t magic_marker;

  /* Most properties are stored in an array associated with an object
   * with an enum to index the array. This will be an index into the
   * array in that case and serves as a unique identifier for the
   * property for the associated object.
   *
   * XXX: consider moving this into the spec:
   */
  uint8_t id; /* NB: This implies we can have no more
                 than 255 properties per object */
};

#if 0
typedef struct _RutUIProperty
{
  RutProperty _parent;

  const char *nick;
  const char *description;
  void *default_value;
} RutUIProperty;

#endif


/* A quick example of using the rig property system in conjunction
 * with the RutIntrospectable interface.
 */
#if 0

enum {
  FLIBBLE_X_PROP,
  FLIBBLE_N_PROPS
};

typedef struct _Flibble
{
  float x;

  RutIntrospectableProps introspectable;
  RutProperty properties[RUT_FLIBBLE_N_PROPS];

} Flibble;

static RutPropertySpec flibble_prop_specs[] = {
  {
    .name = "x";
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_FLOAT;
    .data_offset = offsetof (RutSlider, x);
    /* optional: for non-trivial properties */
    .getter.float_type = flibble_get_x;
    /* optional: for non-trivial properties */
    .setter.float_type = flibble_set_x;
  },
  { 0 } /* XXX: Needed for runtime counting of the number of properties
                if you use the RutSimpleIntrospectable interface */
};

static void
flibble_free (void *object)
{
  /* SNIP */

  rut_introspectable_destroy (flibble);

  /* SNIP */
}

/* SNIP */

void
flibble_new (void)
{
  /* SNIP */

  rut_introspectable_init (flibble,
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

  rut_property_dirty (flibble->ctx, &flibble->properties[FLIBBLE_PROP_X]);
}

float
flibble_get_x (Flibble *flibble)
{
  return flibble->x;
}
#endif

void
rut_property_context_init (RutPropertyContext *context);

void
rut_property_context_clear_log (RutPropertyContext *context);

void
rut_property_context_destroy (RutPropertyContext *context);

void
rut_property_destroy (RutProperty *property);

void
rut_property_set_binding (RutProperty *property,
                          RutBindingCallback callback,
                          void *user_data,
                          ...) G_GNUC_NULL_TERMINATED;

void
_rut_property_set_binding_full_array (RutProperty *property,
                                      RutBindingCallback callback,
                                      void *user_data,
                                      RutBindingDestroyNotify destroy_notify,
                                      RutProperty **dependencies,
                                      int n_dependencies);

void
rut_property_set_binding_full (RutProperty *property,
                               RutBindingCallback callback,
                               void *user_data,
                               RutBindingDestroyNotify destroy_notify,
                               ...) G_GNUC_NULL_TERMINATED;

void
rut_property_set_binding_by_name (RutObject *object,
                                  const char *name,
                                  RutBindingCallback callback,
                                  void *user_data,
                                  ...) G_GNUC_NULL_TERMINATED;

void
rut_property_set_binding_full_by_name (RutObject *object,
                                       const char *name,
                                       RutBindingCallback callback,
                                       void *user_data,
                                       RutBindingDestroyNotify destroy_notify,
                                       ...) G_GNUC_NULL_TERMINATED;

/**
 * rut_property_set_copy_binding:
 * @context: The property context that will be used to set the property.
 * @target_property: The property to set the binding on.
 * @source_property: The depedent property that the value will be taken from.
 *
 * This links the value of @target_property with the value of
 * @source_property so that whenever the value of the source property
 * changes the target property will be updated with a copy of the same
 * value. Note that the binding is only in one direction so that
 * changes in @target_property do not affect @source_property.
 *
 * An initial copy is triggered when setting the binding
 */
void
rut_property_set_copy_binding (RutPropertyContext *context,
                               RutProperty *target_property,
                               RutProperty *source_property);

/**
 * rut_property_set_cast_scalar_binding:
 * @context: The property context that will be used to set the property.
 * @target_property: The property to set the binding on.
 * @source_property: The depedent property that the value will be taken from.
 *
 * This links the value of @target_property with the value of
 * @source_property so that whenever the value of the source property
 * changes the target property will be updated with a copy of the same
 * value. Note that the binding is only in one direction so that
 * changes in @target_property do not affect @source_property.
 *
 * An initial cast is triggered when setting the binding
 */
void
rut_property_set_cast_scalar_binding (RutPropertyContext *context,
                                      RutProperty *target_property,
                                      RutProperty *source_property);

void
rut_property_set_mirror_bindings (RutPropertyContext *context,
                                  RutProperty *prop0,
                                  RutProperty *prop1);

/**
 * rut_property_remove_binding:
 * @property: The property to remove any binding from
 *
 * This removes any binding callback currently associated with the
 * given @property.
 */
void
rut_property_remove_binding (RutProperty *property);

typedef struct _RutPropertyClosure RutPropertyClosure;

/*
 * rut_property_connect_callback_full:
 * @property: a property you want to be notified of changes to
 * @callback: callback to be called whenever @property changes
 * @user_data: private data to be passed to @callback
 * @destroy_notify: a callback to clean up private data if @property
 *                  is destroyed or rut_property_closure_destroy()
 *                  is called
 *
 * Lets you be notified via a @callback whenever a given @property
 * changes.
 *
 * <note>It is not recommended that this api should be used to
 * generally to bind properties together by using the @callback to
 * then set other properties; instead you should use
 * rut_property_set_binding() so that dependencies can be fully
 * tracked. This mechanism is only intended as a way to trigger logic
 * in response to a property change.</note>
 *
 * Returns: a #RutPropertyClosure that can be explicitly destroyed by
 *          calling rut_property_closure_destroy() or indirectly by
 *          destroying @property.
 */
RutPropertyClosure *
rut_property_connect_callback_full (RutProperty *property,
                                    RutBindingCallback callback,
                                    void *user_data,
                                    GDestroyNotify destroy_notify);

RutPropertyClosure *
rut_property_connect_callback (RutProperty *property,
                               RutBindingCallback callback,
                               void *user_data);

void
rut_property_closure_destroy (RutPropertyClosure *closure);

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
 *  rut_property_dirty. Also such properties
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
 * rut_property_bind (update_cb, dest, srcA, srcB, srcC, NULL);
 *
 * rut_property_add_dependant (srcA, my_flibble_property);
 *
 * rut_property_set_binding (property, binding_cb, user_data);
 * rut_property_add_binding_dependant (property, srcA);
 * rut_property_add_binding_dependant (property, srcB);
 * rut_property_remove_binding_dependant (property, srcB);
 *
 *
 */

void
rut_property_init (RutProperty *property,
                   const RutPropertySpec *spec,
                   void *object,
                   uint8_t id);

void
rut_property_dirty (RutPropertyContext *ctx,
                    RutProperty *property);

void
rut_property_copy_value (RutPropertyContext *ctx,
                         RutProperty *target_property,
                         RutProperty *source_property);

void
rut_property_cast_scalar_value (RutPropertyContext *ctx,
                                RutProperty *dest,
                                RutProperty *src);

void
rut_property_box (RutProperty *property,
                  RutBoxed *boxed);

void
rut_property_set_boxed (RutPropertyContext *ctx,
                        RutProperty *property,
                        const RutBoxed *boxed);

char *
rut_boxed_to_string (const RutBoxed *boxed,
                     const RutPropertySpec *spec);

static inline RutProperty *
rut_property_get_first_source (RutProperty *property)
{
  return property->binding->dependencies[0];
}

void
rut_boxed_destroy (RutBoxed *boxed);

void
rut_boxed_copy (RutBoxed *dst,
                const RutBoxed *src);

#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE) \
static inline void \
rut_property_set_ ## SUFFIX (RutPropertyContext *ctx, \
                             RutProperty *property, \
                             CTYPE value) \
{ \
  CTYPE *data = (CTYPE *)((uint8_t *)property->object + \
                          property->spec->data_offset); \
 \
  g_return_if_fail (property->spec->type == RUT_PROPERTY_TYPE_ ## TYPE); \
 \
  if (property->spec->getter.any_type == NULL && *data == value) \
    return; \
 \
  if (property->spec->setter.any_type) \
    { \
      property->spec->setter.SUFFIX ## _type (property->object, value); \
    } \
  else \
    { \
      *data = value; \
      if (property->dependants) \
        rut_property_dirty (ctx, property); \
    } \
} \
 \
static inline CTYPE \
rut_property_get_ ## SUFFIX (RutProperty *property) \
{ \
  g_return_val_if_fail (property->spec->type == RUT_PROPERTY_TYPE_ ## TYPE, 0); \
 \
  if (property->spec->getter.any_type) \
    { \
      return property->spec->getter.SUFFIX ## _type (property->object); \
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
rut_property_set_ ## SUFFIX (RutPropertyContext *ctx, \
                             RutProperty *property, \
                             const CTYPE *value) \
{ \
  CTYPE *data = \
    (CTYPE *)((uint8_t *)property->object + \
                         property->spec->data_offset); \
 \
  g_return_if_fail (property->spec->type == RUT_PROPERTY_TYPE_ ## TYPE); \
 \
  if (property->spec->setter.any_type) \
    { \
      property->spec->setter.SUFFIX ## _type (property->object, value); \
    } \
  else \
    { \
      *data = *value; \
      if (property->dependants) \
        rut_property_dirty (ctx, property); \
    } \
} \
 \
static inline const CTYPE * \
rut_property_get_ ## SUFFIX (RutProperty *property) \
{ \
  g_return_val_if_fail (property->spec->type == RUT_PROPERTY_TYPE_ ## TYPE, 0); \
 \
  if (property->spec->getter.any_type) \
    { \
      return property->spec->getter.SUFFIX ## _type (property->object); \
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
rut_property_set_ ## SUFFIX (RutPropertyContext *ctx, \
                             RutProperty *property, \
                             const CTYPE value[LEN]) \
{ \
  CTYPE *data = (CTYPE *) ((uint8_t *) property->object + \
                           property->spec->data_offset); \
 \
  g_return_if_fail (property->spec->type == RUT_PROPERTY_TYPE_ ## TYPE); \
 \
  if (property->spec->setter.any_type) \
    { \
      property->spec->setter.SUFFIX ## _type (property->object, value); \
    } \
  else \
    { \
      memcpy (data, value, sizeof (CTYPE) * LEN); \
      if (property->dependants) \
        rut_property_dirty (ctx, property); \
    } \
} \
 \
static inline const CTYPE * \
rut_property_get_ ## SUFFIX (RutProperty *property) \
{ \
  g_return_val_if_fail (property->spec->type == RUT_PROPERTY_TYPE_ ## TYPE, 0); \
 \
  if (property->spec->getter.any_type) \
    { \
      return property->spec->getter.SUFFIX ## _type (property->object); \
    } \
  else \
    { \
      const CTYPE *data = (const CTYPE *) ((uint8_t *) property->object + \
                                           property->spec->data_offset); \
      return data; \
    } \
}

#include "rut-property-types.h"

#undef ARRAY_TYPE
#undef POINTER_TYPE
#undef COMPOSITE_TYPE
#undef SCALAR_TYPE

static inline void
rut_property_set_text (RutPropertyContext *ctx,
                       RutProperty *property,
                       const char *value)
{
  char **data =
    (char **)(uint8_t *)property->object + property->spec->data_offset;

  g_return_if_fail (property->spec->type == RUT_PROPERTY_TYPE_TEXT);

  if (property->spec->setter.any_type)
    {
      property->spec->setter.text_type (property->object, value);
    }
  else
    {
      if (*data)
        g_free (*data);
      *data = g_strdup (value);
      if (property->dependants)
        rut_property_dirty (ctx, property);
    }
}

static inline const char *
rut_property_get_text (RutProperty *property)
{
  g_return_val_if_fail (property->spec->type == RUT_PROPERTY_TYPE_TEXT, 0);

  if (property->spec->getter.any_type)
    {
      return property->spec->getter.text_type (property->object);
    }
  else
    {
      const char **data = (const char **)((uint8_t *)property->object +
                                          property->spec->data_offset);
      return *data;
    }
}

#if 0

struct _RutProperty
{
  /* PRIVATE */

  const char *name;

  void *data;

  /* A property can be linked to any number of dependency properties
   * so that it will be automatically prompted for an update if any of
   * those dependencies change.
   */
  RutPropertyUpdateCallback update_cb;

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

  /* Can this property be cast to a RutUIProperty to access additional
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
rut_property_init (RutProperty *property,
                   const char *name,
                   RutPropertyType type,
                   void *value_addr,
                   RutPropertyUpdateCallback update_cb,
                   void *user_data);

void
rut_property_set_float (RutProperty *property,
                        float value);

float
rut_property_get_float (RutProperty *property);

void
rut_property_set_double (RutProperty *property,
                         double value);

double
rut_property_get_double (RutProperty *property);

#endif

#endif /* _RUT_PROPERTY_H_ */
