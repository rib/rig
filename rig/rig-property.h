/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013  Intel Corporation.
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
 *
 */

#ifndef _RUT_PROPERTY_H_
#define _RUT_PROPERTY_H_

#include <sys/types.h>
#include <stdbool.h>
#include <string.h>

#include <cglib/cglib.h>

#include "rut-memory-stack.h"
#include "rut-object.h"
#include "rut-types.h"

/* FIXME: instead of supporting rig_asset_t properties we should
 * support declaring type validation information for
 * rut_object_t propertys. You should be able to specify a
 * specific rut_type_t or a mask of interfaces.
 */
#ifndef RIG_ASSET_TYPEDEF
/* Note: we avoid including rig-asset.h to avoid a circular
 * dependency */
typedef struct _rig_asset_t rig_asset_t;
#define RIG_ASSET_TYPEDEF
#endif

/* Note that rig-property-bare.h does not include all necessary
 * headers to define all prerequisite types because it also in support
 * of Rig's ui logic where some of the types are defined separately to
 * keep includes for runtime compilation down to a minimum. */
#include "rig-property-bare.h"

#if 0
typedef struct _rut_ui_property_t
{
    rig_property_t _parent;

    const char *nick;
    const char *description;
    void *default_value;
} rut_ui_property_t;

#endif

/* A quick example of using the rig property system in conjunction
 * with the RutIntrospectable interface.
 */
#if 0

enum {
    FLIBBLE_X_PROP,
    FLIBBLE_N_PROPS
};

typedef struct _flibble_t
{
    float x;

    rut_introspectable_props_t introspectable;
    rig_property_t properties[RUT_FLIBBLE_N_PROPS];

} flibble_t;

static rig_property_spec_t flibble_prop_specs[] = {
    {
        .name = "x";
        .flags = RUT_PROPERTY_FLAG_READWRITE,
        .type = RUT_PROPERTY_TYPE_FLOAT;
        .data_offset = offsetof (rut_slider_t, x);
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
flibble_set_x (flibble_t *flibble, float x)
{
    if (flibble->x == x)
        return;

    flibble->x = x;

    rig_property_dirty (flibble->shell, &flibble->properties[FLIBBLE_PROP_X]);
}

float
flibble_get_x (flibble_t *flibble)
{
    return flibble->x;
}
#endif

void rig_property_context_init(rig_property_context_t *context);

void rig_property_context_clear_log(rig_property_context_t *context);

void rig_property_context_destroy(rig_property_context_t *context);

void rig_property_destroy(rig_property_t *property);

void rig_property_set_binding(rig_property_t *property,
                              rut_binding_callback_t callback,
                              void *user_data,
                              ...) C_GNUC_NULL_TERMINATED;

void _rig_property_set_binding_full_array(
    rig_property_t *property,
    rut_binding_callback_t callback,
    void *user_data,
    rut_binding_destroy_notify_t destroy_notify,
    rig_property_t **dependencies,
    int n_dependencies);

void rig_property_set_binding_full(rig_property_t *property,
                                   rut_binding_callback_t callback,
                                   void *user_data,
                                   rut_binding_destroy_notify_t destroy_notify,
                                   ...) C_GNUC_NULL_TERMINATED;

void rig_property_set_binding_by_name(rut_object_t *object,
                                      const char *name,
                                      rut_binding_callback_t callback,
                                      void *user_data,
                                      ...) C_GNUC_NULL_TERMINATED;

void rig_property_set_binding_full_by_name(
    rut_object_t *object,
    const char *name,
    rut_binding_callback_t callback,
    void *user_data,
    rut_binding_destroy_notify_t destroy_notify,
    ...) C_GNUC_NULL_TERMINATED;

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
 *
 * An initial copy is triggered when setting the binding
 */
void rig_property_set_copy_binding(rig_property_context_t *context,
                                   rig_property_t *target_property,
                                   rig_property_t *source_property);

/**
 * rig_property_set_cast_scalar_binding:
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
void rig_property_set_cast_scalar_binding(rig_property_context_t *context,
                                          rig_property_t *target_property,
                                          rig_property_t *source_property);

void rig_property_set_mirror_bindings(rig_property_context_t *context,
                                      rig_property_t *prop0,
                                      rig_property_t *prop1);

/**
 * rig_property_remove_binding:
 * @property: The property to remove any binding from
 *
 * This removes any binding callback currently associated with the
 * given @property.
 */
void rig_property_remove_binding(rig_property_t *property);

typedef struct _rig_property_closure_t rig_property_closure_t;

/*
 * rig_property_connect_callback_full:
 * @property: a property you want to be notified of changes to
 * @callback: callback to be called whenever @property changes
 * @user_data: private data to be passed to @callback
 * @destroy_notify: a callback to clean up private data if @property
 *                  is destroyed or rig_property_closure_destroy()
 *                  is called
 *
 * Lets you be notified via a @callback whenever a given @property
 * changes.
 *
 * <note>It is not recommended that this api should be used to
 * generally to bind properties together by using the @callback to
 * then set other properties; instead you should use
 * rig_property_set_binding() so that dependencies can be fully
 * tracked. This mechanism is only intended as a way to trigger logic
 * in response to a property change.</note>
 *
 * Returns: a #rig_property_closure_t that can be explicitly destroyed by
 *          calling rig_property_closure_destroy() or indirectly by
 *          destroying @property.
 */
rig_property_closure_t *
rig_property_connect_callback_full(rig_property_t *property,
                                   rut_binding_callback_t callback,
                                   void *user_data,
                                   c_destroy_func_t destroy_notify);

rig_property_closure_t *rig_property_connect_callback(
    rig_property_t *property, rut_binding_callback_t callback, void *user_data);

void rig_property_closure_destroy(rig_property_closure_t *closure);

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

void rig_property_init(rig_property_t *property,
                       const rig_property_spec_t *spec,
                       void *object,
                       uint8_t id);

void rig_property_copy_value(rig_property_context_t *ctx,
                             rig_property_t *target_property,
                             rig_property_t *source_property);

void rig_property_cast_scalar_value(rig_property_context_t *ctx,
                                    rig_property_t *dest,
                                    rig_property_t *src);

void rig_property_box(rig_property_t *property, rut_boxed_t *boxed);

void rig_property_set_boxed(rig_property_context_t *ctx,
                            rig_property_t *property,
                            const rut_boxed_t *boxed);

char *rut_boxed_to_string(const rut_boxed_t *boxed,
                          const rig_property_spec_t *spec);

static inline rig_property_t *
rig_property_get_first_source(rig_property_t *property)
{
    return property->binding->dependencies[0];
}

void rut_boxed_destroy(rut_boxed_t *boxed);

void rut_boxed_copy(rut_boxed_t *dst, const rut_boxed_t *src);

#if 0

struct _rig_property_t
{
    /* PRIVATE */

    const char *name;

    void *data;

    /* A property can be linked to any number of dependency properties
     * so that it will be automatically prompted for an update if any of
     * those dependencies change.
     */
    rig_property_update_callback_t update_cb;

    /* This is the list of properties that depend on this property and
     * should be prompted for an update whenever this property changes.
     */
    c_sllist_t *dependants;

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
    unsigned int type : 16;
    unsigned int queued_count : 8;

    /* Can this property be cast to a rut_ui_property_t to access additional
     * information, such as a description and default value?
     */
    unsigned int is_ui_property : 1;

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
rig_property_init (rig_property_t *property,
                   const char *name,
                   rig_property_type_t type,
                   void *value_addr,
                   rig_property_update_callback_t update_cb,
                   void *user_data);

void
rig_property_set_float (rig_property_t *property,
                        float value);

float
rig_property_get_float (rig_property_t *property);

void
rig_property_set_double (rig_property_t *property,
                         double value);

double
rig_property_get_double (rig_property_t *property);

#endif

#endif /* _RUT_PROPERTY_H_ */
