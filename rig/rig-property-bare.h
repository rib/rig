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

/* Note: This API is shared with Rig's runtime code generation */

#pragma once

typedef enum _rig_property_type_t {
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
    RUT_PROPERTY_TYPE_POINTER,
    RUT_PROPERTY_TYPE_CONTAINER,
} rig_property_type_t;

typedef struct _rut_boxed_t {
    rig_property_type_t type;
    union {
        float float_val;
        double double_val;
        int integer_val;
        int enum_val;
        uint32_t uint32_val;
        bool boolean_val;
        char *text_val;
        c_quaternion_t quaternion_val;
        float vec3_val[3];
        float vec4_val[4];
        cg_color_t color_val;
        rut_object_t *object_val;
        void *pointer_val;
    } d;
} rut_boxed_t;

typedef struct _rig_property_change_t {
    void *object;
    rut_boxed_t boxed;
    int prop_id;
} rig_property_change_t;

typedef struct _rig_property_context_t {
    int logging_disabled;
    int magic_marker;
    rut_memory_stack_t *change_log_stack;
    int log_len;
} rig_property_context_t;

typedef struct _rig_property_t rig_property_t;

typedef void (*rig_property_update_callback_t)(rig_property_t *property,
                                               void *user_data);

typedef union _rig_property_default_t {
    int integer;
    bool boolean;
    const void *pointer;
} rig_property_default_t;

typedef struct _rig_property_validation_integer_t {
    int min;
    int max;
} rig_property_validation_integer_t;

typedef struct _rig_property_validation_float_t {
    float min;
    float max;
} rig_property_validation_float_t;

typedef struct _rig_property_validation_vec3_t {
    float min;
    float max;
} rig_property_validation_vec3_t;

typedef struct _rig_property_validation_vec4_t {
    float min;
    float max;
} rig_property_validation_vec4_t;

typedef struct _rig_property_validation_object_t {
    rut_type_t *type;
} rig_property_validation_object_t;

typedef union _rig_property_validation_t {
    rig_property_validation_integer_t int_range;
    rig_property_validation_float_t float_range;
    rig_property_validation_vec3_t vec3_range;
    rig_property_validation_vec4_t vec4_range;
    rig_property_validation_object_t object;
    const rut_ui_enum_t *ui_enum;
} rig_property_validation_t;

typedef enum _rig_property_flags_t {
    RUT_PROPERTY_FLAG_READABLE = 1 << 0,
    RUT_PROPERTY_FLAG_WRITABLE = 1 << 1,
    RUT_PROPERTY_FLAG_VALIDATE = 1 << 2,
    RUT_PROPERTY_FLAG_READWRITE =
        (RUT_PROPERTY_FLAG_READABLE | RUT_PROPERTY_FLAG_WRITABLE),
    RUT_PROPERTY_FLAG_EXPORT_FRONTEND = 1 << 3, /* Changes affect rendering and
                                                   should be fowarded to
                                                   frontend */
} rig_property_flags_t;

typedef struct _rig_property_spec_t {
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
    union {
        float (*float_type)(void *object);
        double (*double_type)(void *object);
        int (*integer_type)(void *object);
        int (*enum_type)(void *object);
        uint32_t (*uint32_type)(void *object);
        bool (*boolean_type)(void *object);
        const char *(*text_type)(void *object);
        const c_quaternion_t *(*quaternion_type)(void *object);
        const cg_color_t *(*color_type)(void *object);
        const float *(*vec3_type)(void *object);
        const float *(*vec4_type)(void *object);
        void *(*object_type)(void *object);
        void *(*pointer_type)(void *object);

        /* This is just used to check the pointer against NULL */
        void *any_type;
    } getter;
    union {
        void (*float_type)(void *object, float value);
        void (*double_type)(void *object, double value);
        void (*integer_type)(void *object, int value);
        void (*enum_type)(void *object, int value);
        void (*uint32_type)(void *object, uint32_t value);
        void (*boolean_type)(void *object, bool value);
        void (*text_type)(void *object, const char *value);
        void (*quaternion_type)(void *object,
                                const c_quaternion_t *quaternion);
        void (*color_type)(void *object, const cg_color_t *color);
        void (*vec3_type)(void *object, const float value[3]);
        void (*vec4_type)(void *object, const float value[4]);
        void (*object_type)(void *object, void *value);
        void (*pointer_type)(void *object, void *value);

        /* This is just used to check the pointer against NULL */
        void *any_type;
    } setter;

    struct {
        void (*add)(void *object, void *item);
        void (*remove)(void *object, void *item);
        void (*foreach)(void *object,
                        void (*callback)(void *item, void *user_data),
                        void *user_data);
    } container;

    const char *nick;
    const char *blurb;
    rig_property_flags_t flags;
    rig_property_default_t default_value;
    rig_property_validation_t validation;

    unsigned int type : 16;
    unsigned int is_ui_property : 1;
    /* Whether this property is allowed to be animatable or not */
    unsigned int animatable : 1;
} rig_property_spec_t;

/* Note: we intentionally don't pass a pointer to a "source property"
 * that is the property that has changed because rig_property_t is
 * designed so that we can defer binding callbacks until the mainloop
 * so we can avoid redundant callbacks in cases where multiple
 * dependencies of a property may be changed. */
typedef void (*rut_binding_callback_t)(rig_property_t *target_property,
                                       void *user_data);

typedef void (*rut_binding_destroy_notify_t)(rig_property_t *property,
                                             void *user_data);

/* XXX: make sure bindings get freed if any of of the dependency
 * properties are destroyed. */
typedef struct _rig_property_binding_t {
    rut_binding_callback_t callback;
    rut_binding_destroy_notify_t destroy_notify;
    void *user_data;
    /* When the property this binding is for gets destroyed we need to
     * know the dependencies so we can remove this property from the
     * corresponding list of dependants for each dependency.
     */
    rig_property_t *dependencies[];
} rig_property_binding_t;

struct _rig_property_t {
    const rig_property_spec_t *spec;
    c_sllist_t *dependants;
    rig_property_binding_t *binding; /* Maybe make this a list of bindings? */
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

void rig_property_dirty(rig_property_context_t *ctx, rig_property_t *property);

#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE)                                   \
static inline void rig_property_set_##SUFFIX(                              \
    rig_property_context_t *ctx, rig_property_t *property, CTYPE value)    \
{                                                                          \
                                                                           \
    if (property->spec->setter.any_type) {                                 \
        property->spec->setter.SUFFIX##_type(property->object, value);     \
    } else {                                                               \
        CTYPE *data = (CTYPE *)((uint8_t *)property->object +              \
                                property->spec->data_offset);              \
                                                                           \
        c_return_if_fail(property->spec->data_offset != 0);                \
        c_return_if_fail(property->spec->type ==                           \
                         RUT_PROPERTY_TYPE_##TYPE);                        \
                                                                           \
        if (property->spec->getter.any_type == NULL && *data == value)     \
            return;                                                        \
                                                                           \
        *data = value;                                                     \
        rig_property_dirty(ctx, property);                                 \
    }                                                                      \
}                                                                          \
static inline CTYPE rig_property_get_##SUFFIX(rig_property_t *property)    \
{                                                                          \
    c_return_val_if_fail(property->spec->type == RUT_PROPERTY_TYPE_##TYPE, \
                         0);                                               \
                                                                           \
    if (property->spec->getter.any_type) {                                 \
        return property->spec->getter.SUFFIX##_type(property->object);     \
    } else {                                                               \
        CTYPE *data = (CTYPE *)((uint8_t *)property->object +              \
                                property->spec->data_offset);              \
        return *data;                                                      \
    }                                                                      \
}

#define POINTER_TYPE(SUFFIX, CTYPE, TYPE) SCALAR_TYPE(SUFFIX, CTYPE, TYPE)

#define COMPOSITE_TYPE(SUFFIX, CTYPE, TYPE)                                    \
static inline void rig_property_set_##SUFFIX(rig_property_context_t *ctx,  \
                                             rig_property_t *property,     \
                                             const CTYPE *value)           \
{                                                                          \
    c_return_if_fail(property->spec->type == RUT_PROPERTY_TYPE_##TYPE);    \
                                                                           \
    if (property->spec->setter.any_type) {                                 \
        property->spec->setter.SUFFIX##_type(property->object, value);     \
    } else {                                                               \
        CTYPE *data = (CTYPE *)((uint8_t *)property->object +              \
                                property->spec->data_offset);              \
        *data = *value;                                                    \
        rig_property_dirty(ctx, property);                                 \
    }                                                                      \
}                                                                          \
static inline const CTYPE *rig_property_get_##SUFFIX(                      \
    rig_property_t *property)                                              \
{                                                                          \
    c_return_val_if_fail(property->spec->type == RUT_PROPERTY_TYPE_##TYPE, \
                         0);                                               \
                                                                           \
    if (property->spec->getter.any_type) {                                 \
        return property->spec->getter.SUFFIX##_type(property->object);     \
    } else {                                                               \
        CTYPE *data = (CTYPE *)((uint8_t *)property->object +              \
                                property->spec->data_offset);              \
        return data;                                                       \
    }                                                                      \
}

#define ARRAY_TYPE(SUFFIX, CTYPE, TYPE, LEN)                                   \
static inline void rig_property_set_##SUFFIX(rig_property_context_t *ctx,  \
                                             rig_property_t *property,     \
                                             const CTYPE value[LEN])       \
{                                                                          \
                                                                           \
    c_return_if_fail(property->spec->type == RUT_PROPERTY_TYPE_##TYPE);    \
                                                                           \
    if (property->spec->setter.any_type) {                                 \
        property->spec->setter.SUFFIX##_type(property->object, value);     \
    } else {                                                               \
        CTYPE *data = (CTYPE *)((uint8_t *)property->object +              \
                                property->spec->data_offset);              \
        memcpy(data, value, sizeof(CTYPE) * LEN);                          \
        rig_property_dirty(ctx, property);                                 \
    }                                                                      \
}                                                                          \
static inline const CTYPE *rig_property_get_##SUFFIX(                      \
    rig_property_t *property)                                              \
{                                                                          \
    c_return_val_if_fail(property->spec->type == RUT_PROPERTY_TYPE_##TYPE, \
                         0);                                               \
                                                                           \
    if (property->spec->getter.any_type) {                                 \
        return property->spec->getter.SUFFIX##_type(property->object);     \
    } else {                                                               \
        const CTYPE *data = (const CTYPE *)((uint8_t *)property->object +  \
                                            property->spec->data_offset);  \
        return data;                                                       \
    }                                                                      \
}

#include "rig-property-types.h"

#undef ARRAY_TYPE
#undef POINTER_TYPE
#undef COMPOSITE_TYPE
#undef SCALAR_TYPE

static inline void
rig_property_set_text(rig_property_context_t *ctx,
                      rig_property_t *property,
                      const char *value)
{
    char **data =
        (char **)(uint8_t *)property->object + property->spec->data_offset;

    c_return_if_fail(property->spec->type == RUT_PROPERTY_TYPE_TEXT);

    if (property->spec->setter.any_type) {
        property->spec->setter.text_type(property->object, value);
    } else {
        if (*data)
            c_free(*data);
        *data = c_strdup(value);
        rig_property_dirty(ctx, property);
    }
}

static inline const char *
rig_property_get_text(rig_property_t *property)
{
    c_return_val_if_fail(property->spec->type == RUT_PROPERTY_TYPE_TEXT, 0);

    if (property->spec->getter.any_type) {
        return property->spec->getter.text_type(property->object);
    } else {
        const char **data = (const char **)((uint8_t *)property->object +
                                            property->spec->data_offset);
        return *data;
    }
}
