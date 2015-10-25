/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012,2013  Intel Corporation.
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

#include <rut-config.h>

#include <clib.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "rut-property.h"
#include "rut-introspectable.h"
#include "rut-color.h"
#include "rut-util.h"

static int dummy_object;

void
rut_property_context_init(rut_property_context_t *context)
{
    context->logging_disabled = 1;
    context->magic_marker = 0;
    context->change_log_stack = rut_memory_stack_new(4096);
}

void
rut_property_context_clear_log(rut_property_context_t *context)
{
    rut_memory_stack_rewind(context->change_log_stack);
    context->log_len = 0;
}

void
rut_property_context_destroy(rut_property_context_t *context)
{
    rut_memory_stack_free(context->change_log_stack);
}

void
rut_property_init(rut_property_t *property,
                  const rut_property_spec_t *spec,
                  void *object,
                  uint8_t id)
{
    /* If the property is readable there should be some way to read it */
    c_warn_if_fail((spec->flags & RUT_PROPERTY_FLAG_READABLE) == 0 ||
                   spec->data_offset != 0 || spec->getter.any_type);
    /* Same for writable properties */
    c_warn_if_fail((spec->flags & RUT_PROPERTY_FLAG_WRITABLE) == 0 ||
                   spec->data_offset != 0 || spec->setter.any_type);

    property->spec = spec;
    property->dependants = NULL;
    property->binding = NULL;
    property->object = object;
    property->queued_count = 0;
    property->magic_marker = 0;
    property->id = id;
}

static void
_rut_property_destroy_binding(rut_property_t *property)
{
    rut_property_binding_t *binding = property->binding;

    if (binding) {
        int i;

        if (binding->destroy_notify)
            binding->destroy_notify(property, binding->user_data);

        for (i = 0; binding->dependencies[i]; i++) {
            rut_property_t *dependency = binding->dependencies[i];
            dependency->dependants =
                c_sllist_remove(dependency->dependants, property);
        }

        c_slice_free1(sizeof(rut_property_binding_t) + sizeof(void *) * (i + 1),
                      binding);

        property->binding = NULL;
    }
}

void
rut_property_destroy(rut_property_t *property)
{
    c_sllist_t *l;

    _rut_property_destroy_binding(property);

    /* XXX: we don't really know if this property was a hard requirement
     * for the bindings associated with dependants so for now we assume
     * it was and we free all bindings associated with them...
     */
    for (l = property->dependants; l; l = l->next) {
        rut_property_t *dependant = l->data;
        _rut_property_destroy_binding(dependant);
    }
}

void
rut_property_copy_value(rut_property_context_t *ctx,
                        rut_property_t *dest,
                        rut_property_t *src)
{
    c_return_if_fail(src->spec->type == dest->spec->type);

    switch ((rut_property_type_t)dest->spec->type) {
#define COPIER(SUFFIX, CTYPE, TYPE)                                            \
case RUT_PROPERTY_TYPE_##TYPE:                                             \
    rut_property_set_##SUFFIX(ctx, dest, rut_property_get_##SUFFIX(src));  \
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

    c_warn_if_reached();
}

void
rut_property_cast_scalar_value(rut_property_context_t *ctx,
                               rut_property_t *dest,
                               rut_property_t *src)
{
    double val;

    switch ((rut_property_type_t)src->spec->type) {
#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE)                                       \
case RUT_PROPERTY_TYPE_##TYPE:                                             \
    val = rut_property_get_##SUFFIX(src);                                  \
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
        c_warn_if_reached();
    }

    switch ((rut_property_type_t)dest->spec->type) {
#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE)                                       \
case RUT_PROPERTY_TYPE_##TYPE:                                             \
    rut_property_set_##SUFFIX(ctx, dest, val);                             \
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
        c_warn_if_reached();
    }

    c_warn_if_reached();
}

void
_rut_property_set_binding_full_array(
    rut_property_t *property,
    rut_binding_callback_t callback,
    void *user_data,
    rut_binding_destroy_notify_t destroy_notify,
    rut_property_t **dependencies,
    int n_dependencies)
{
    rut_property_binding_t *binding;
    int i;

    /* XXX: Note: for now we don't allow multiple bindings for the same
     * property. I'm not sure it would make sense, as they would
     * presumably conflict with each other?
     */

    if (property->binding) {
        c_return_if_fail(callback == NULL);

        _rut_property_destroy_binding(property);
        return;
    } else if (callback == NULL)
        return;

    binding = c_slice_alloc(sizeof(rut_property_binding_t) +
                            sizeof(void *) * (n_dependencies + 1));
    binding->callback = callback;
    binding->user_data = user_data;
    binding->destroy_notify = destroy_notify;

    memcpy(
        binding->dependencies, dependencies, sizeof(void *) * n_dependencies);
    binding->dependencies[n_dependencies] = NULL;

    for (i = 0; i < n_dependencies; i++) {
        rut_property_t *dependency = dependencies[i];
        dependency->dependants =
            c_sllist_prepend(dependency->dependants, property);
    }

    property->binding = binding;

    /* A binding with no dependencies will never be triggered in
     * response to anything so we simply trigger it once now... */
    if (n_dependencies == 0)
        callback(property, user_data);
}

static void
_rut_property_set_binding_full_valist(
    rut_property_t *property,
    rut_binding_callback_t callback,
    void *user_data,
    rut_binding_destroy_notify_t destroy_notify,
    va_list ap)
{
    rut_property_t *dependency;
    va_list aq;
    int i;

    va_copy(aq, ap);
    for (i = 0; (dependency = va_arg(aq, rut_property_t *)); i++)
        ;
    va_end(aq);

    if (i) {
        rut_property_t **dependencies;
        rut_property_t *dependency;
        int j = 0;

        dependencies = alloca(sizeof(void *) * i);

        for (j = 0; (dependency = va_arg(ap, rut_property_t *)); j++)
            dependencies[j] = dependency;

        _rut_property_set_binding_full_array(
            property, callback, user_data, destroy_notify, dependencies, i);
    } else
        _rut_property_set_binding_full_array(
            property, callback, user_data, destroy_notify, NULL, 0);
}

void
rut_property_set_binding(rut_property_t *property,
                         rut_binding_callback_t callback,
                         void *user_data,
                         ...)
{
    va_list ap;

    va_start(ap, user_data);
    _rut_property_set_binding_full_valist(property,
                                          callback,
                                          user_data,
                                          NULL, /* destroy_notify */
                                          ap);
    va_end(ap);
}

void
rut_property_set_binding_full(rut_property_t *property,
                              rut_binding_callback_t callback,
                              void *user_data,
                              rut_binding_destroy_notify_t destroy_notify,
                              ...)
{
    va_list ap;

    va_start(ap, destroy_notify);
    _rut_property_set_binding_full_valist(
        property, callback, user_data, destroy_notify, ap);
    va_end(ap);
}

void
rut_property_set_binding_by_name(rut_object_t *object,
                                 const char *name,
                                 rut_binding_callback_t callback,
                                 void *user_data,
                                 ...)
{
    rut_property_t *property = rut_introspectable_lookup_property(object, name);
    va_list ap;

    c_return_if_fail(property);

    va_start(ap, user_data);
    _rut_property_set_binding_full_valist(
        property, callback, user_data, NULL, ap);
    va_end(ap);
}

void
rut_property_set_binding_full_by_name(
    rut_object_t *object,
    const char *name,
    rut_binding_callback_t callback,
    void *user_data,
    rut_binding_destroy_notify_t destroy_notify,
    ...)
{
    rut_property_t *property = rut_introspectable_lookup_property(object, name);
    va_list ap;

    c_return_if_fail(property);

    va_start(ap, destroy_notify);
    _rut_property_set_binding_full_valist(
        property, callback, user_data, destroy_notify, ap);
    va_end(ap);
}

static void
_rut_property_copy_binding_cb(rut_property_t *target_property,
                              void *user_data)
{
    rut_property_context_t *context = user_data;
    rut_property_t *source_property =
        rut_property_get_first_source(target_property);

    rut_property_copy_value(context, target_property, source_property);
}

void
rut_property_set_copy_binding(rut_property_context_t *context,
                              rut_property_t *target_property,
                              rut_property_t *source_property)
{
    rut_property_set_binding(target_property,
                             _rut_property_copy_binding_cb,
                             context,
                             source_property,
                             NULL /* terminator */);
    _rut_property_copy_binding_cb(target_property, context);
}

void
rut_property_set_mirror_bindings(rut_property_context_t *context,
                                 rut_property_t *prop0,
                                 rut_property_t *prop1)
{
    rut_property_set_copy_binding(context, prop0, prop1);
    rut_property_set_copy_binding(context, prop1, prop0);
}

static void
_rut_property_cast_binding_cb(rut_property_t *target_property,
                              void *user_data)
{
    rut_property_context_t *context = user_data;
    rut_property_t *source_property =
        rut_property_get_first_source(target_property);

    rut_property_cast_scalar_value(context, target_property, source_property);
}

void
rut_property_set_cast_scalar_binding(rut_property_context_t *context,
                                     rut_property_t *target_property,
                                     rut_property_t *source_property)
{
    rut_property_set_binding(target_property,
                             _rut_property_cast_binding_cb,
                             context,
                             source_property,
                             NULL /* terminator */);
    _rut_property_cast_binding_cb(target_property, context);
}

void
rut_property_remove_binding(rut_property_t *property)
{
    if (!property->binding)
        return;

    rut_property_set_binding(property,
                             NULL, /* no callback */
                             NULL, /* no user data */
                             NULL); /* null vararg terminator */
}

static rut_property_spec_t dummy_property_spec = {
    .name = "dummy",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = 0,
    .setter.any_type = abort,
    .getter.any_type = abort
};

struct _rut_property_closure_t {
    rut_property_t dummy_prop;
    rut_binding_callback_t callback;
    c_destroy_func_t destroy_notify;
    void *user_data;
};

static void
dummy_property_destroy_notify_cb(rut_property_t *property,
                                 void *user_data)
{
    rut_property_closure_t *closure = user_data;

    if (closure->destroy_notify)
        closure->destroy_notify(closure->user_data);

    c_slice_free(rut_property_closure_t, closure);
}

static void
dummy_property_binding_wrapper_cb(rut_property_t *dummy_property,
                                  void *user_data)
{
    rut_property_closure_t *closure = user_data;
    rut_property_t *property = rut_property_get_first_source(dummy_property);

    closure->callback(property, closure->user_data);
}

rut_property_closure_t *
rut_property_connect_callback_full(rut_property_t *property,
                                   rut_binding_callback_t callback,
                                   void *user_data,
                                   c_destroy_func_t destroy_notify)
{
    rut_property_closure_t *closure;

    c_return_val_if_fail(callback != NULL, NULL);

    closure = c_slice_new(rut_property_closure_t);
    rut_property_init(
        &closure->dummy_prop, &dummy_property_spec, &dummy_object, 0); /* id */
    closure->callback = callback;
    closure->destroy_notify = destroy_notify;
    closure->user_data = user_data;
    rut_property_set_binding_full(&closure->dummy_prop,
                                  dummy_property_binding_wrapper_cb,
                                  closure,
                                  dummy_property_destroy_notify_cb,
                                  property,
                                  NULL); /* null terminator */
    return closure;
}

rut_property_closure_t *
rut_property_connect_callback(
    rut_property_t *property, rut_binding_callback_t callback, void *user_data)
{
    return rut_property_connect_callback_full(
        property, callback, user_data, NULL);
}

void
rut_property_closure_destroy(rut_property_closure_t *closure)
{
    rut_property_remove_binding(&closure->dummy_prop);
}

void
rut_property_dirty(rut_property_context_t *ctx, rut_property_t *property)
{
    c_sllist_t *l;
    c_sllist_t *next;

    if (!ctx->logging_disabled &&
        property->spec->flags & RUT_PROPERTY_FLAG_EXPORT_FRONTEND)
    {
        rut_object_t *object = property->object;
        if (object != &dummy_object) {
            rut_property_change_t *change;

#if 0
            c_debug(
                "Log %d: base=%p, offset=%d: obj = %p(%s), prop id=%d(%s)\n",
                ctx->log_len,
                ctx->change_log_stack->sub_stack->data,
                ctx->change_log_stack->sub_stack->offset,
                object,
                rut_object_get_type_name(object),
                property->id,
                property->spec->name);
#endif

            change = rut_memory_stack_alloc(ctx->change_log_stack,
                                            sizeof(rut_property_change_t));

            change->object = object;
            change->prop_id = property->id;
            rut_property_box(property, &change->boxed);
            ctx->log_len++;
        }
    }

    /* FIXME: The plan is for updates to happen asynchronously by
     * queueing an update with the context but for now we simply
     * trigger the updates synchronously.
     */
    for (l = property->dependants; l; l = next) {
        rut_property_t *dependant = l->data;
        rut_property_binding_t *binding = dependant->binding;

        next = l->next;

        if (binding)
            binding->callback(dependant, binding->user_data);
    }
}

void
rut_property_box(rut_property_t *property, rut_boxed_t *boxed)
{
    boxed->type = property->spec->type;

    switch ((rut_property_type_t)property->spec->type) {
#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE)                                       \
case RUT_PROPERTY_TYPE_##TYPE: {                                           \
    boxed->d.SUFFIX##_val = rut_property_get_##SUFFIX(property);           \
    break;                                                                 \
}
/* Special case the _POINTER types so we can take a reference on
 * objects... */
#define POINTER_TYPE(SUFFIX, CTYPE, TYPE)
    case RUT_PROPERTY_TYPE_OBJECT: {
        rut_object_t *obj = rut_property_get_object(property);
        if (obj)
            boxed->d.object_val = rut_object_ref(obj);
        else
            boxed->d.object_val = NULL;
        break;
    }
    case RUT_PROPERTY_TYPE_ASSET: {
        rut_object_t *obj = rut_property_get_asset(property);
        if (obj)
            boxed->d.asset_val = rut_object_ref(obj);
        else
            boxed->d.asset_val = NULL;
        break;
    }
    case RUT_PROPERTY_TYPE_POINTER: {
        boxed->d.pointer_val = rut_property_get_pointer(property);
        break;
    }
#define COMPOSITE_TYPE(SUFFIX, CTYPE, TYPE)                                    \
case RUT_PROPERTY_TYPE_##TYPE: {                                           \
    memcpy(&boxed->d.SUFFIX##_val,                                         \
           rut_property_get_##SUFFIX(property),                            \
           sizeof(boxed->d.SUFFIX##_val));                                 \
    break;                                                                 \
}
#define ARRAY_TYPE(SUFFIX, CTYPE, TYPE, LEN)                                   \
case RUT_PROPERTY_TYPE_##TYPE: {                                           \
    memcpy(&boxed->d.SUFFIX##_val,                                         \
           rut_property_get_##SUFFIX(property),                            \
           sizeof(CTYPE) * LEN);                                           \
    break;                                                                 \
}
    case RUT_PROPERTY_TYPE_TEXT:
        boxed->d.text_val = c_strdup(rut_property_get_text(property));
        break;

#include "rut-property-types.h"
    }

#undef POINTER_TYPE
#undef ARRAY_TYPE
#undef COMPOSITE_TYPE
#undef SCALAR_TYPE
}

void
rut_boxed_copy(rut_boxed_t *dst, const rut_boxed_t *src)
{
    dst->type = src->type;

    switch (src->type) {
#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE)                                       \
case RUT_PROPERTY_TYPE_##TYPE:                                             \
    dst->d.SUFFIX##_val = src->d.SUFFIX##_val;                             \
    return;
#define COMPOSITE_TYPE(SUFFIX, CTYPE, TYPE) SCALAR_TYPE(SUFFIX, CTYPE, TYPE)
/* Special case the _POINTER types so we can take a reference on
 * objects... */
#define POINTER_TYPE(SUFFIX, CTYPE, TYPE)
    case RUT_PROPERTY_TYPE_OBJECT: {
        if (src->d.object_val)
            dst->d.object_val = rut_object_ref(src->d.object_val);
        else
            dst->d.object_val = NULL;
        return;
    }
    case RUT_PROPERTY_TYPE_ASSET: {
        if (src->d.asset_val)
            dst->d.asset_val = rut_object_ref(src->d.asset_val);
        else
            dst->d.asset_val = NULL;
        return;
    }
    case RUT_PROPERTY_TYPE_POINTER:
        dst->d.pointer_val = src->d.pointer_val;
        return;
#define ARRAY_TYPE(SUFFIX, CTYPE, TYPE, LEN)                                   \
case RUT_PROPERTY_TYPE_##TYPE: {                                           \
    memcpy(                                                                \
        &dst->d.SUFFIX##_val, &src->d.SUFFIX##_val, sizeof(CTYPE) * LEN);  \
    return;                                                                \
}
    case RUT_PROPERTY_TYPE_TEXT:
        dst->d.text_val = c_strdup(src->d.text_val);
        return;

#include "rut-property-types.h"
    }

#undef POINTER_TYPE
#undef ARRAY_TYPE
#undef COMPOSITE_TYPE
#undef SCALAR_TYPE

    c_assert_not_reached();
}

void
rut_boxed_destroy(rut_boxed_t *boxed)
{
    switch (boxed->type) {
    case RUT_PROPERTY_TYPE_OBJECT:
        if (boxed->d.object_val)
            rut_object_unref(boxed->d.object_val);
        break;
    case RUT_PROPERTY_TYPE_ASSET:
        if (boxed->d.asset_val)
            rut_object_unref(boxed->d.asset_val);
        break;
    case RUT_PROPERTY_TYPE_POINTER:
        c_free(boxed->d.text_val);
        break;
    default:
        break;
    }
}

static double
boxed_to_double(const rut_boxed_t *boxed)
{
    switch (boxed->type) {
#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE)                                       \
case RUT_PROPERTY_TYPE_##TYPE:                                             \
    return boxed->d.SUFFIX##_val;
#define POINTER_TYPE(SUFFIX, CTYPE, TYPE)
#define COMPOSITE_TYPE(SUFFIX, CTYPE, TYPE)
#define ARRAY_TYPE(SUFFIX, CTYPE, TYPE, LEN)

#include "rut-property-types.h"

#undef SCALAR_TYPE
#undef POINTER_TYPE
#undef COMPOSITE_TYPE
#undef ARRAY_TYPE

    default:
        c_warn_if_reached();
        return 0;
    }
}

void
rut_property_set_boxed(rut_property_context_t *ctx,
                       rut_property_t *property,
                       const rut_boxed_t *boxed)
{
    /* Handle basic type conversion for scalar types only... */
    if (property->spec->type != boxed->type) {
        double intermediate = boxed_to_double(boxed);

        switch (property->spec->type) {
#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE)                                       \
case RUT_PROPERTY_TYPE_##TYPE:                                             \
    rut_property_set_##SUFFIX(ctx, property, intermediate);                \
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
            c_warn_if_reached();
            return;
        }
    }

    switch (boxed->type) {
#define SET_BY_VAL(SUFFIX, CTYPE, TYPE)                                        \
case RUT_PROPERTY_TYPE_##TYPE:                                             \
    rut_property_set_##SUFFIX(ctx, property, boxed->d.SUFFIX##_val);       \
    return
#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE) SET_BY_VAL(SUFFIX, CTYPE, TYPE);
#define POINTER_TYPE(SUFFIX, CTYPE, TYPE) SET_BY_VAL(SUFFIX, CTYPE, TYPE);
#define COMPOSITE_TYPE(SUFFIX, CTYPE, TYPE)                                    \
case RUT_PROPERTY_TYPE_##TYPE:                                             \
    rut_property_set_##SUFFIX(ctx, property, &boxed->d.SUFFIX##_val);      \
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

    c_warn_if_reached();
}

char *
rut_boxed_to_string(const rut_boxed_t *boxed,
                    const rut_property_spec_t *spec)
{
    switch (boxed->type) {
    case RUT_PROPERTY_TYPE_FLOAT:
        return c_strdup_printf("%f", boxed->d.float_val);
    case RUT_PROPERTY_TYPE_DOUBLE:
        return c_strdup_printf("%f", boxed->d.double_val);
    case RUT_PROPERTY_TYPE_INTEGER:
        return c_strdup_printf("%d", boxed->d.integer_val);
    case RUT_PROPERTY_TYPE_ENUM: {
        if (spec && spec->validation.ui_enum) {
            const rut_ui_enum_t *ui_enum = spec->validation.ui_enum;
            int i;
            for (i = 0; ui_enum->values[i].nick; i++) {
                const rut_ui_enum_value_t *enum_value = &ui_enum->values[i];
                if (enum_value->value == boxed->d.enum_val)
                    return c_strdup_printf(
                        "<%d:%s>", boxed->d.enum_val, enum_value->nick);
            }
        }

        return c_strdup_printf("<%d:Enum>", boxed->d.enum_val);
    }
    case RUT_PROPERTY_TYPE_UINT32:
        return c_strdup_printf("%u", boxed->d.uint32_val);
    case RUT_PROPERTY_TYPE_BOOLEAN: {
        const char *bool_strings[] = { "true", "false" };
        return c_strdup(bool_strings[!!boxed->d.boolean_val]);
    }
    case RUT_PROPERTY_TYPE_TEXT:
        return c_strdup_printf("%s", boxed->d.text_val);
    case RUT_PROPERTY_TYPE_QUATERNION: {
        const c_quaternion_t *quaternion = &boxed->d.quaternion_val;
        float axis[3], angle;

        c_quaternion_get_rotation_axis(quaternion, axis);
        angle = c_quaternion_get_rotation_angle(quaternion);

        return c_strdup_printf("axis: (%.2f,%.2f,%.2f) angle: %.2f\n",
                               axis[0],
                               axis[1],
                               axis[2],
                               angle);
    }
    case RUT_PROPERTY_TYPE_VEC3:
        return c_strdup_printf("(%.1f, %.1f, %.1f)",
                               boxed->d.vec3_val[0],
                               boxed->d.vec3_val[1],
                               boxed->d.vec3_val[2]);
    case RUT_PROPERTY_TYPE_VEC4:
        return c_strdup_printf("(%.1f, %.1f, %.1f, %.1f)",
                               boxed->d.vec4_val[0],
                               boxed->d.vec4_val[1],
                               boxed->d.vec4_val[2],
                               boxed->d.vec4_val[3]);
    case RUT_PROPERTY_TYPE_COLOR:
        return rut_color_to_string(&boxed->d.color_val);
    case RUT_PROPERTY_TYPE_OBJECT:
        return c_strdup_printf("<%p:%s>",
                               boxed->d.object_val,
                               rut_object_get_type_name(boxed->d.object_val));
    case RUT_PROPERTY_TYPE_ASSET:
        return c_strdup_printf("<%p:Asset>", boxed->d.asset_val);
    case RUT_PROPERTY_TYPE_POINTER:
        return c_strdup_printf("%p", boxed->d.pointer_val);
    default:
        return c_strdup("<rut_boxed_dump:unknown property type>");
    }
}
