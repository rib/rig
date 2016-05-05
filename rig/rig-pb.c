/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013  Intel Corporation
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

#include <rig-config.h>

#include <math.h>

#include <rut.h>

#include "rig.pb-c.h"
#include "rig-pb.h"
#include "rig-engine.h"
#include "rig-entity.h"

#include "components/rig-button-input.h"
#include "components/rig-camera.h"
#include "components/rig-light.h"
#include "components/rig-material.h"
#include "components/rig-mesh.h"
#include "components/rig-nine-slice.h"
#include "components/rig-text.h"

#ifdef USE_UV
#include "components/rig-native-module.h"
#endif

struct unserializer_error {
    char *msg;
    c_backtrace_t *backtrace;
};


const char *
rig_pb_strdup(rig_pb_serializer_t *serializer, const char *string)
{
    int len = strlen(string);
    void *mem = rut_memory_stack_memalign(serializer->stack, len + 1, 1);
    memcpy(mem, string, len + 1);
    return mem;
}

static Rig__Color *
pb_color_new(rig_pb_serializer_t *serializer,
             const cg_color_t *color)
{
    Rig__Color *pb_color = rig_pb_new(serializer, Rig__Color, rig__color__init);
    pb_color->hex =
        rut_memory_stack_alloc(serializer->stack, 10); /* #rrggbbaa */
    snprintf(pb_color->hex,
             10,
             "#%02x%02x%02x%02x",
             cg_color_get_red_byte(color),
             cg_color_get_green_byte(color),
             cg_color_get_blue_byte(color),
             cg_color_get_alpha_byte(color));

    return pb_color;
}

static Rig__Rotation *
pb_rotation_new(rig_pb_serializer_t *serializer,
                const c_quaternion_t *quaternion)
{
    Rig__Rotation *pb_rotation =
        rig_pb_new(serializer, Rig__Rotation, rig__rotation__init);
    float angle = c_quaternion_get_rotation_angle(quaternion);
    float axis[3];

    c_quaternion_get_rotation_axis(quaternion, axis);

    pb_rotation->angle = angle;
    pb_rotation->x = axis[0];
    pb_rotation->y = axis[1];
    pb_rotation->z = axis[2];

    return pb_rotation;
}

static Rig__Vec3 *
pb_vec3_new(rig_pb_serializer_t *serializer, float x, float y, float z)
{
    Rig__Vec3 *pb_vec3 = rig_pb_new(serializer, Rig__Vec3, rig__vec3__init);

    pb_vec3->x = x;
    pb_vec3->y = y;
    pb_vec3->z = z;

    return pb_vec3;
}

static Rig__Vec4 *
pb_vec4_new(rig_pb_serializer_t *serializer, float x, float y, float z, float w)
{
    Rig__Vec4 *pb_vec4 = rig_pb_new(serializer, Rig__Vec4, rig__vec4__init);

    pb_vec4->x = x;
    pb_vec4->y = y;
    pb_vec4->z = z;
    pb_vec4->w = w;

    return pb_vec4;
}

static Rig__Path *
pb_path_new(rig_pb_serializer_t *serializer, rig_path_t *path)
{
    Rig__Path *pb_path = rig_pb_new(serializer, Rig__Path, rig__path__init);
    rig_node_t *node;
    int i;

    if (!path->length)
        return pb_path;

    pb_path->nodes = rut_memory_stack_memalign(serializer->stack,
                                               sizeof(void *) * path->length,
                                               C_ALIGNOF(void *));
    pb_path->n_nodes = path->length;

    i = 0;
    c_list_for_each(node, &path->nodes, list_node)
    {
        Rig__Node *pb_node = rig_pb_new(serializer, Rig__Node, rig__node__init);

        pb_path->nodes[i] = pb_node;

        pb_node->has_t = true;
        pb_node->t = node->t;

        pb_node->value = rig_pb_new(
            serializer, Rig__PropertyValue, rig__property_value__init);

        switch (path->type) {
        case RUT_PROPERTY_TYPE_FLOAT: {
            pb_node->value->has_float_value = true;
            pb_node->value->float_value = node->boxed.d.float_val;
            break;
        }
        case RUT_PROPERTY_TYPE_DOUBLE: {
            pb_node->value->has_double_value = true;
            pb_node->value->double_value = node->boxed.d.double_val;
            break;
        }
        case RUT_PROPERTY_TYPE_VEC3: {
            float *vec3 = node->boxed.d.vec3_val;
            pb_node->value->vec3_value =
                pb_vec3_new(serializer, vec3[0], vec3[1], vec3[2]);
            break;
        }
        case RUT_PROPERTY_TYPE_VEC4: {
            float *vec4 = node->boxed.d.vec4_val;
            pb_node->value->vec4_value =
                pb_vec4_new(serializer, vec4[0], vec4[1], vec4[2], vec4[3]);
            break;
        }
        case RUT_PROPERTY_TYPE_COLOR: {
            pb_node->value->color_value =
                pb_color_new(serializer, &node->boxed.d.color_val);
            break;
        }
        case RUT_PROPERTY_TYPE_QUATERNION: {
            pb_node->value->quaternion_value =
                pb_rotation_new(serializer, &node->boxed.d.quaternion_val);
            break;
        }
        case RUT_PROPERTY_TYPE_INTEGER: {
            pb_node->value->has_integer_value = true;
            pb_node->value->integer_value = node->boxed.d.integer_val;
            continue;
            break;
        }
        case RUT_PROPERTY_TYPE_UINT32: {
            pb_node->value->has_uint32_value = true;
            pb_node->value->uint32_value = node->boxed.d.uint32_val;
            break;
        }

        /* These types of properties can't be interoplated so they
         * probably shouldn't end up in a path */
        case RUT_PROPERTY_TYPE_ENUM:
        case RUT_PROPERTY_TYPE_BOOLEAN:
        case RUT_PROPERTY_TYPE_TEXT:
        case RUT_PROPERTY_TYPE_ASSET:
        case RUT_PROPERTY_TYPE_OBJECT:
        case RUT_PROPERTY_TYPE_POINTER:
            c_warn_if_reached();
            break;
        }

        i++;
    }

    return pb_path;
}

void
rig_pb_property_value_init(rig_pb_serializer_t *serializer,
                           Rig__PropertyValue *pb_value,
                           const rut_boxed_t *value)
{
    switch (value->type) {
    case RUT_PROPERTY_TYPE_FLOAT:
        pb_value->has_float_value = true;
        pb_value->float_value = value->d.float_val;
        break;

    case RUT_PROPERTY_TYPE_DOUBLE:
        pb_value->has_double_value = true;
        pb_value->double_value = value->d.double_val;
        break;

    case RUT_PROPERTY_TYPE_INTEGER:
        pb_value->has_integer_value = true;
        pb_value->integer_value = value->d.integer_val;
        break;

    case RUT_PROPERTY_TYPE_UINT32:
        pb_value->has_uint32_value = true;
        pb_value->uint32_value = value->d.uint32_val;
        break;

    case RUT_PROPERTY_TYPE_BOOLEAN:
        pb_value->has_boolean_value = true;
        pb_value->boolean_value = value->d.boolean_val;
        break;

    case RUT_PROPERTY_TYPE_TEXT:
        pb_value->text_value = value->d.text_val;
        break;

    case RUT_PROPERTY_TYPE_QUATERNION:
        pb_value->quaternion_value =
            pb_rotation_new(serializer, &value->d.quaternion_val);
        break;

    case RUT_PROPERTY_TYPE_VEC3:
        pb_value->vec3_value = pb_vec3_new(serializer,
                                           value->d.vec3_val[0],
                                           value->d.vec3_val[1],
                                           value->d.vec3_val[2]);
        break;

    case RUT_PROPERTY_TYPE_VEC4:
        pb_value->vec4_value = pb_vec4_new(serializer,
                                           value->d.vec4_val[0],
                                           value->d.vec4_val[1],
                                           value->d.vec4_val[2],
                                           value->d.vec4_val[3]);
        break;

    case RUT_PROPERTY_TYPE_COLOR:
        pb_value->color_value = pb_color_new(serializer, &value->d.color_val);
        break;

    case RUT_PROPERTY_TYPE_ENUM:
        /* XXX: this should possibly save the string names rather than
         * the integer value? */
        pb_value->has_enum_value = true;
        pb_value->enum_value = value->d.enum_val;
        break;

    case RUT_PROPERTY_TYPE_ASSET:
        pb_value->has_asset_value = true;

        if (value->d.asset_val) {
            uint64_t id = rig_pb_serializer_lookup_object_id(
                serializer, value->d.asset_val);

            c_warn_if_fail(id != 0);

            pb_value->asset_value = id;
        } else
            pb_value->asset_value = 0;

        break;

    case RUT_PROPERTY_TYPE_OBJECT:
        pb_value->has_object_value = true;

        if (value->d.object_val) {
            uint64_t id = rig_pb_serializer_lookup_object_id(
                serializer, value->d.object_val);

            c_warn_if_fail(id != 0);

            pb_value->object_value = id;
        } else
            pb_value->object_value = 0;

        break;

    case RUT_PROPERTY_TYPE_POINTER:
        c_warn_if_reached();
        break;
    }
}

Rig__PropertyValue *
pb_property_value_new(rig_pb_serializer_t *serializer,
                      const rut_boxed_t *value)
{
    Rig__PropertyValue *pb_value =
        rig_pb_new(serializer, Rig__PropertyValue, rig__property_value__init);

    rig_pb_property_value_init(serializer, pb_value, value);

    return pb_value;
}

Rig__PropertyType
rig_property_type_to_pb_type(rig_property_type_t type)
{
    switch (type) {
    case RUT_PROPERTY_TYPE_FLOAT:
        return RIG__PROPERTY_TYPE__FLOAT;
    case RUT_PROPERTY_TYPE_DOUBLE:
        return RIG__PROPERTY_TYPE__DOUBLE;
    case RUT_PROPERTY_TYPE_INTEGER:
        return RIG__PROPERTY_TYPE__INTEGER;
    case RUT_PROPERTY_TYPE_ENUM:
        return RIG__PROPERTY_TYPE__ENUM;
    case RUT_PROPERTY_TYPE_UINT32:
        return RIG__PROPERTY_TYPE__UINT32;
    case RUT_PROPERTY_TYPE_BOOLEAN:
        return RIG__PROPERTY_TYPE__BOOLEAN;
    case RUT_PROPERTY_TYPE_TEXT:
        return RIG__PROPERTY_TYPE__TEXT;
    case RUT_PROPERTY_TYPE_QUATERNION:
        return RIG__PROPERTY_TYPE__QUATERNION;
    case RUT_PROPERTY_TYPE_VEC3:
        return RIG__PROPERTY_TYPE__VEC3;
    case RUT_PROPERTY_TYPE_VEC4:
        return RIG__PROPERTY_TYPE__VEC4;
    case RUT_PROPERTY_TYPE_COLOR:
        return RIG__PROPERTY_TYPE__COLOR;
    case RUT_PROPERTY_TYPE_OBJECT:
        return RIG__PROPERTY_TYPE__OBJECT;
    case RUT_PROPERTY_TYPE_ASSET:
        return RIG__PROPERTY_TYPE__ASSET;

    case RUT_PROPERTY_TYPE_POINTER:
        c_warn_if_reached();
        return RIG__PROPERTY_TYPE__OBJECT;
    }

    c_warn_if_reached();
    return 0;
}

Rig__Boxed *
pb_boxed_new(rig_pb_serializer_t *serializer,
             const char *name,
             const rut_boxed_t *boxed)
{
    Rig__Boxed *pb_boxed = rig_pb_new(serializer, Rig__Boxed, rig__boxed__init);

    pb_boxed->name = (char *)name;
    pb_boxed->has_type = true;
    pb_boxed->type = rig_property_type_to_pb_type(boxed->type);
    pb_boxed->value = pb_property_value_new(serializer, boxed);

    return pb_boxed;
}

static void
count_instrospectables_cb(rig_property_t *property, void *user_data)
{
    if (!(property->spec->flags & RUT_PROPERTY_FLAG_WRITABLE))
        return;
    else {
        rig_pb_serializer_t *serializer = user_data;
        serializer->n_properties++;
    }
}

static void
serialize_instrospectables_cb(rig_property_t *property,
                              void *user_data)
{
    rig_pb_serializer_t *serializer = user_data;
    void **properties_out = serializer->properties_out;
    rut_boxed_t boxed;

    if (!(property->spec->flags & RUT_PROPERTY_FLAG_WRITABLE))
        return;

    rig_property_box(property, &boxed);

    properties_out[serializer->n_properties++] =
        pb_boxed_new(serializer, property->spec->name, &boxed);

    rut_boxed_destroy(&boxed);
}

static void
serialize_instrospectable_properties(rut_object_t *object,
                                     size_t *n_properties_out,
                                     void **properties_out,
                                     rig_pb_serializer_t *serializer)
{
    serializer->n_properties = 0;
    rig_introspectable_foreach_property(
        object, count_instrospectables_cb, serializer);
    *n_properties_out = serializer->n_properties;

    serializer->properties_out = *properties_out =
                                     rut_memory_stack_memalign(serializer->stack,
                                                               sizeof(void *) * serializer->n_properties,
                                                               C_ALIGNOF(void *));

    serializer->n_properties = 0;
    rig_introspectable_foreach_property(
        object, serialize_instrospectables_cb, serializer);
}

Rig__Buffer *
rig_pb_serialize_buffer(rig_pb_serializer_t *serializer,
                        rut_buffer_t *buffer,
                        bool include_data)
{
    Rig__Buffer *pb_buffer =
        rig_pb_new(serializer, Rig__Buffer, rig__buffer__init);

    pb_buffer->has_id = true;
    pb_buffer->id = rig_pb_serializer_register_object(serializer, buffer);

    pb_buffer->size = buffer->size;

    if (include_data) {
        pb_buffer->has_data = true;
        pb_buffer->data.data = buffer->data;
        pb_buffer->data.len = buffer->size;
        /* XXX: we probably can't always assume this it's safe to not
         * copy the buffer data here. */
#warning "transient pb_buffer->data.data points to a rut_buffer_t's data which mustn't change"
    } else
        pb_buffer->has_data = false;

    return pb_buffer;
}

/* For some set of attributes they may point to buffers that haven't
 * been registered yet and so they need to be serialized too.
 *
 * Any unregistered buffers that need to be serialized are returned
 * via @new_buffers and @n_new_buffers.
 */
Rig__Attribute **
rig_pb_serialize_attributes(rig_pb_serializer_t *serializer,
                            rut_attribute_t **attributes,
                            int n_attributes,
                            Rig__Buffer **new_buffers_out,
                            int *n_new_buffers_out)
{
    Rig__Attribute **pb_attributes = NULL;
    int64_t *attribute_buffers_id_map = c_alloca(sizeof(int64_t) * n_attributes);
    int n_new_buffers = 0;

    for (int i = 0; i < n_attributes; i++) {
        int64_t buffer_id;

        if (!attributes[i]->is_buffered)
            continue;

        buffer_id =
            rig_pb_serializer_lookup_object_id(serializer,
                                               attributes[i]->buffered.buffer);
        if (buffer_id)
            attribute_buffers_id_map[i] = buffer_id;
        else {
            Rig__Buffer *pb_buffer =
                rig_pb_serialize_buffer(serializer,
                                        attributes[i]->buffered.buffer,
                                        true);

            attribute_buffers_id_map[i] = pb_buffer->id;

            new_buffers_out[n_new_buffers++] = pb_buffer;
        }
    }

    pb_attributes = rut_memory_stack_memalign(serializer->stack,
                                              sizeof(void *) * n_attributes,
                                              C_ALIGNOF(void *));
    for (int i = 0; i < n_attributes; i++) {
        Rig__Attribute *pb_attribute =
            rig_pb_new(serializer, Rig__Attribute, rig__attribute__init);
        Rig__Attribute__Type type;

        pb_attribute->name = (char *)attributes[i]->name;
        pb_attribute->has_normalized = true;
        pb_attribute->normalized = attributes[i]->normalized;

        if (attributes[i]->instance_stride) {
            pb_attribute->has_instance_stride = true;
            pb_attribute->instance_stride = attributes[i]->instance_stride;
        }

        if (attributes[i]->is_buffered) {
            pb_attribute->has_buffer_id = true;
            pb_attribute->buffer_id = attribute_buffers_id_map[i];

            pb_attribute->has_stride = true;
            pb_attribute->stride = attributes[i]->buffered.stride;
            pb_attribute->has_offset = true;
            pb_attribute->offset = attributes[i]->buffered.offset;
            pb_attribute->has_n_components = true;
            pb_attribute->n_components = attributes[i]->buffered.n_components;

            switch (attributes[i]->buffered.type) {
                case RUT_ATTRIBUTE_TYPE_BYTE:
                    type = RIG__ATTRIBUTE__TYPE__BYTE;
                    break;
                case RUT_ATTRIBUTE_TYPE_UNSIGNED_BYTE:
                    type = RIG__ATTRIBUTE__TYPE__UNSIGNED_BYTE;
                    break;
                case RUT_ATTRIBUTE_TYPE_SHORT:
                    type = RIG__ATTRIBUTE__TYPE__SHORT;
                    break;
                case RUT_ATTRIBUTE_TYPE_UNSIGNED_SHORT:
                    type = RIG__ATTRIBUTE__TYPE__UNSIGNED_SHORT;
                    break;
                case RUT_ATTRIBUTE_TYPE_FLOAT:
                    type = RIG__ATTRIBUTE__TYPE__FLOAT;
                    break;
            }
            pb_attribute->has_type = true;
            pb_attribute->type = type;
        } else {
            pb_attribute->has_n_components = true;
            pb_attribute->n_components = attributes[i]->constant.n_components;
            pb_attribute->has_n_columns = true;
            pb_attribute->n_columns = attributes[i]->constant.n_columns;
            pb_attribute->has_transpose = true;
            pb_attribute->transpose = attributes[i]->constant.transpose;

            pb_attribute->n_floats = pb_attribute->n_components * pb_attribute->n_columns;
            /* XXX: we assume the mesh will stay valid/constant and
             * avoid copying the data... */
            pb_attribute->floats = attributes[i]->constant.value;
        }

        pb_attributes[i] = pb_attribute;
    }

    *n_new_buffers_out = n_new_buffers;

    return pb_attributes;
}

static Rig__Mesh *
rig_pb_serialize_mesh(rig_pb_serializer_t *serializer,
                      rut_mesh_t *mesh)
{
    Rig__Attribute **pb_attributes;
    int64_t indices_buffer_id;
    Rig__Buffer **new_buffers;
    int n_new_buffers = 0;
    Rig__Mesh *pb_mesh;

    /* The maximum number of pb_buffers we may need = n_attributes plus 1 in
     * case there is an index buffer... */
    new_buffers =
        rut_memory_stack_memalign(serializer->stack,
                                  sizeof(void *) * (mesh->n_attributes + 1),
                                  C_ALIGNOF(void *));

    /* NB:
     * attributes may refer to shared (already registered) buffers 
     * so we need to first figure out how many unique buffers the mesh
     * refers too...
     */

    pb_attributes = rig_pb_serialize_attributes(serializer,
                                                mesh->attributes,
                                                mesh->n_attributes,
                                                new_buffers,
                                                &n_new_buffers);
    if (!pb_attributes)
        return NULL;

    if (mesh->indices_buffer) {
        indices_buffer_id =
            rig_pb_serializer_lookup_object_id(serializer,
                                               mesh->indices_buffer);

        if (!indices_buffer_id) {
            Rig__Buffer *pb_buffer =
                rig_pb_serialize_buffer(serializer,
                                        mesh->indices_buffer,
                                        true);

            new_buffers[n_new_buffers++] = pb_buffer;
            indices_buffer_id = pb_buffer->id;
        }
    }

    pb_mesh = rig_pb_new(serializer, Rig__Mesh, rig__mesh__init);

    pb_mesh->has_mode = true;
    switch (mesh->mode) {
    case CG_VERTICES_MODE_POINTS:
        pb_mesh->mode = RIG__MESH__MODE__POINTS;
        break;
    case CG_VERTICES_MODE_LINES:
        pb_mesh->mode = RIG__MESH__MODE__LINES;
        break;
    case CG_VERTICES_MODE_LINE_LOOP:
        pb_mesh->mode = RIG__MESH__MODE__LINE_LOOP;
        break;
    case CG_VERTICES_MODE_LINE_STRIP:
        pb_mesh->mode = RIG__MESH__MODE__LINE_STRIP;
        break;
    case CG_VERTICES_MODE_TRIANGLES:
        pb_mesh->mode = RIG__MESH__MODE__TRIANGLES;
        break;
    case CG_VERTICES_MODE_TRIANGLE_STRIP:
        pb_mesh->mode = RIG__MESH__MODE__TRIANGLE_STRIP;
        break;
    case CG_VERTICES_MODE_TRIANGLE_FAN:
        pb_mesh->mode = RIG__MESH__MODE__TRIANGLE_FAN;
        break;
    }

    pb_mesh->n_buffers = n_new_buffers;
    pb_mesh->buffers = new_buffers;

    pb_mesh->attributes = pb_attributes;
    pb_mesh->n_attributes = mesh->n_attributes;

    pb_mesh->has_n_vertices = true;
    pb_mesh->n_vertices = mesh->n_vertices;

    if (mesh->indices_buffer) {
        pb_mesh->has_indices_type = true;
        switch (mesh->indices_type) {
        case CG_INDICES_TYPE_UNSIGNED_BYTE:
            pb_mesh->indices_type = RIG__MESH__INDICES_TYPE__UNSIGNED_BYTE;
            break;
        case CG_INDICES_TYPE_UNSIGNED_SHORT:
            pb_mesh->indices_type = RIG__MESH__INDICES_TYPE__UNSIGNED_SHORT;
            break;
        case CG_INDICES_TYPE_UNSIGNED_INT:
            pb_mesh->indices_type = RIG__MESH__INDICES_TYPE__UNSIGNED_INT;
            break;
        }

        pb_mesh->has_n_indices = true;
        pb_mesh->n_indices = mesh->n_indices;

        pb_mesh->has_indices_buffer_id = true;
        pb_mesh->indices_buffer_id = indices_buffer_id;
    }

    return pb_mesh;
}

Rig__Entity__Component *
rig_pb_serialize_component(rig_pb_serializer_t *serializer,
                           rut_component_t *component)
{
    const rut_type_t *type = rut_object_get_type(component);
    Rig__Entity__Component *pb_component;
    int component_id;

    pb_component = rig_pb_new(
        serializer, Rig__Entity__Component, rig__entity__component__init);

    component_id = rig_pb_serializer_register_object(serializer, component);

    pb_component->has_id = true;
    pb_component->id = component_id;

    pb_component->has_type = true;

    if (type == &rig_light_type) {
        pb_component->type = RIG__ENTITY__COMPONENT__TYPE__LIGHT;
        serialize_instrospectable_properties(component,
                                             &pb_component->n_properties,
                                             (void **)&pb_component->properties,
                                             serializer);
    } else if (type == &rig_material_type) {
        pb_component->type = RIG__ENTITY__COMPONENT__TYPE__MATERIAL;
        serialize_instrospectable_properties(component,
                                             &pb_component->n_properties,
                                             (void **)&pb_component->properties,
                                             serializer);
    } else if (type == &rig_source_type) {
        pb_component->type = RIG__ENTITY__COMPONENT__TYPE__SOURCE;
        serialize_instrospectable_properties(component,
                                             &pb_component->n_properties,
                                             (void **)&pb_component->properties,
                                             serializer);
    } else if (type == &rig_mesh_type) {
        rig_mesh_t *mesh = (rig_mesh_t *)component;

        pb_component->type = RIG__ENTITY__COMPONENT__TYPE__MESH;

        pb_component->mesh = rig_pb_serialize_mesh(serializer, mesh->rut_mesh);

        serialize_instrospectable_properties(component,
                                             &pb_component->n_properties,
                                             (void **)&pb_component->properties,
                                             serializer);
    } else if (type == &rig_text_type) {
        pb_component->type = RIG__ENTITY__COMPONENT__TYPE__TEXT;
        serialize_instrospectable_properties(component,
                                             &pb_component->n_properties,
                                             (void **)&pb_component->properties,
                                             serializer);
    } else if (type == &rig_camera_type) {
        pb_component->type = RIG__ENTITY__COMPONENT__TYPE__CAMERA;
        serialize_instrospectable_properties(component,
                                             &pb_component->n_properties,
                                             (void **)&pb_component->properties,
                                             serializer);
    } else if (type == &rig_nine_slice_type) {
        pb_component->type = RIG__ENTITY__COMPONENT__TYPE__NINE_SLICE;
        serialize_instrospectable_properties(component,
                                             &pb_component->n_properties,
                                             (void **)&pb_component->properties,
                                             serializer);
    } else if (type == &rig_button_input_type) {
        pb_component->type = RIG__ENTITY__COMPONENT__TYPE__BUTTON_INPUT;
        serialize_instrospectable_properties(component,
                                             &pb_component->n_properties,
                                             (void **)&pb_component->properties,
                                             serializer);
    }
#ifdef USE_UV
    else if (type == &rig_native_module_type) {
        pb_component->type = RIG__ENTITY__COMPONENT__TYPE__NATIVE_MODULE;
        serialize_instrospectable_properties(component,
                                             &pb_component->n_properties,
                                             (void **)&pb_component->properties,
                                             serializer);
    }
#endif

    return pb_component;
}

static bool
serialize_component_cb(rut_object_t *component, void *user_data)
{
    rig_pb_serializer_t *serializer = user_data;
    Rig__Entity__Component *pb_component =
        rig_pb_serialize_component(serializer, component);

    serializer->n_pb_components++;
    serializer->pb_components =
        c_llist_prepend(serializer->pb_components, pb_component);

    return true; /* continue */
}

Rig__Entity *
rig_pb_serialize_entity(rig_pb_serializer_t *serializer,
                        rig_entity_t *parent,
                        rig_entity_t *entity)
{
    Rig__Entity *pb_entity =
        rig_pb_new(serializer, Rig__Entity, rig__entity__init);
    const c_quaternion_t *q;
    const char *label;
    Rig__Vec3 *position;
    float scale;
    c_llist_t *l;
    int i;

    pb_entity->has_id = true;
    pb_entity->id = rig_pb_serializer_register_object(serializer, entity);

    if (parent && rut_object_get_type(parent) == &rig_entity_type) {
        uint64_t id = rig_pb_serializer_lookup_object_id(serializer, parent);
        if (id) {
            pb_entity->has_parent_id = true;
            pb_entity->parent_id = id;
        } else
            c_warning("Failed to find id of parent entity\n");
    }

    label = rig_entity_get_label(entity);
    if (label && *label) {
        pb_entity->label = (char *)label;
    }

    q = rig_entity_get_rotation(entity);

    position = rig_pb_new(serializer, Rig__Vec3, rig__vec3__init);
    position->x = rig_entity_get_x(entity);
    position->y = rig_entity_get_y(entity);
    position->z = rig_entity_get_z(entity);
    pb_entity->position = position;

    scale = rig_entity_get_scale(entity);
    if (scale != 1) {
        pb_entity->has_scale = true;
        pb_entity->scale = scale;
    }

    pb_entity->rotation = pb_rotation_new(serializer, q);

    serializer->n_pb_components = 0;
    serializer->pb_components = NULL;
    rig_entity_foreach_component(entity, serialize_component_cb, serializer);

    pb_entity->n_components = serializer->n_pb_components;
    pb_entity->components =
        rut_memory_stack_memalign(serializer->stack,
                                  sizeof(void *) * pb_entity->n_components,
                                  C_ALIGNOF(void *));

    for (i = 0, l = serializer->pb_components; l; i++, l = l->next)
        pb_entity->components[i] = l->data;
    c_llist_free(serializer->pb_components);
    serializer->pb_components = NULL;
    serializer->n_pb_components = 0;

    return pb_entity;
}

static rut_traverse_visit_flags_t
_rig_entitygraph_pre_serialize_cb(
    rut_object_t *object, int depth, void *user_data)
{
    rig_pb_serializer_t *serializer = user_data;
    const rut_type_t *type = rut_object_get_type(object);
    rut_object_t *parent = rut_graphable_get_parent(object);
    rig_entity_t *entity;
    Rig__Entity *pb_entity;
    const char *label;

    if (type != &rig_entity_type) {
        c_warning("Can't save non-entity graphables\n");
        return RUT_TRAVERSE_VISIT_CONTINUE;
    }

    entity = object;

    /* NB: labels with a "rig:" prefix imply that this is an internal
     * entity that shouldn't be saved (such as the editing camera
     * entities) */
    label = rig_entity_get_label(entity);
    if (label && strncmp("rig:", label, 4) == 0)
        return RUT_TRAVERSE_VISIT_CONTINUE;

    pb_entity = rig_pb_serialize_entity(serializer, parent, entity);

    serializer->n_pb_entities++;
    serializer->pb_entities =
        c_llist_prepend(serializer->pb_entities, pb_entity);

    return RUT_TRAVERSE_VISIT_CONTINUE;
}

Rig__SimpleObject *
rig_pb_serialize_simple_object(rig_pb_serializer_t *serializer,
                               rut_object_t *object)
{
    Rig__SimpleObject *pb_object =
        rig_pb_new(serializer, Rig__SimpleObject, rig__simple_object__init);

    pb_object->has_id = true;
    pb_object->id = rig_pb_serializer_register_object(serializer, object);

    serialize_instrospectable_properties(
        object,
        &pb_object->n_properties,
        (void **)&pb_object->properties,
        serializer);

    return pb_object;
}

typedef struct _deps_state_t {
    int i;
    rig_pb_serializer_t *serializer;
    Rig__Controller__Property__Dependency *pb_dependencies;
} deps_state_t;

static void
serialize_binding_dep_cb(rig_binding_t *binding,
                         rig_property_t *dependency,
                         void *user_data)
{
    deps_state_t *state = user_data;
    Rig__Controller__Property__Dependency *pb_dependency =
        &state->pb_dependencies[state->i++];
    uint64_t id = rig_pb_serializer_lookup_object_id(state->serializer,
                                                     dependency->object);

    c_warn_if_fail(id != 0);

    rig__controller__property__dependency__init(pb_dependency);

    pb_dependency->has_object_id = true;
    pb_dependency->object_id = id;

    pb_dependency->name =
        (char *)rig_pb_strdup(state->serializer, dependency->spec->name);
}

static void
serialize_controller_property_cb(rig_controller_prop_data_t *prop_data,
                                 void *user_data)
{
    rig_pb_serializer_t *serializer = user_data;
    rut_object_t *object;
    uint64_t id;

    Rig__Controller__Property *pb_property = rig_pb_new(
        serializer, Rig__Controller__Property, rig__controller__property__init);

    serializer->n_pb_properties++;
    serializer->pb_properties =
        c_llist_prepend(serializer->pb_properties, pb_property);

    object = prop_data->property->object;

    id = rig_pb_serializer_lookup_object_id(serializer, object);
    if (!id)
        c_warning("Failed to find id of object\n");

    pb_property->has_object_id = true;
    pb_property->object_id = id;

    pb_property->name = (char *)prop_data->property->spec->name;

    pb_property->has_method = true;
    switch (prop_data->method) {
    case RIG_CONTROLLER_METHOD_CONSTANT:
        pb_property->method = RIG__CONTROLLER__PROPERTY__METHOD__CONSTANT;
        break;
    case RIG_CONTROLLER_METHOD_PATH:
        pb_property->method = RIG__CONTROLLER__PROPERTY__METHOD__PATH;
        break;
    case RIG_CONTROLLER_METHOD_BINDING:
        pb_property->method = RIG__CONTROLLER__PROPERTY__METHOD__C_BINDING;
        break;
    }

    if (prop_data->binding) {
        int n_deps;
        int i;
        deps_state_t state;

        pb_property->has_binding_id = true;
        pb_property->binding_id = rig_binding_get_id(prop_data->binding);

        pb_property->c_expression = (char *)rig_pb_strdup(
            serializer, rig_binding_get_expression(prop_data->binding));

        n_deps = rig_binding_get_n_dependencies(prop_data->binding);
        pb_property->n_dependencies = n_deps;
        if (pb_property->n_dependencies) {
            Rig__Controller__Property__Dependency *pb_dependencies =
                rut_memory_stack_memalign(
                    serializer->stack,
                    (sizeof(Rig__Controller__Property__Dependency) * n_deps),
                    C_ALIGNOF(Rig__Controller__Property__Dependency));
            pb_property->dependencies =
                rut_memory_stack_memalign(serializer->stack,
                                          (sizeof(void *) * n_deps),
                                          C_ALIGNOF(void *));

            for (i = 0; i < n_deps; i++)
                pb_property->dependencies[i] = &pb_dependencies[i];

            state.i = 0;
            state.serializer = serializer;
            state.pb_dependencies = pb_dependencies;
            rig_binding_foreach_dependency(
                prop_data->binding, serialize_binding_dep_cb, &state);
        }
    }

    pb_property->constant =
        pb_property_value_new(serializer, &prop_data->constant_value);

    if (prop_data->path && prop_data->path->length)
        pb_property->path = pb_path_new(serializer, prop_data->path);
}

static uint64_t
default_serializer_register_object_cb(void *object,
                                      void *user_data)
{
    rig_pb_serializer_t *serializer = user_data;
    void *id_ptr = (void *)(intptr_t)serializer->next_id++;

    if (!serializer->object_to_id_map) {
        serializer->object_to_id_map =
            c_hash_table_new(NULL, /* direct hash */
                             NULL); /* direct key equal */
    }

    if (c_hash_table_lookup(serializer->object_to_id_map, object)) {
        c_critical("Duplicate save object id %p", id_ptr);
        return 0;
    }

    c_hash_table_insert(serializer->object_to_id_map, object, id_ptr);

    return (uint64_t)(intptr_t)id_ptr;
}

static uint64_t
serializer_register_pointer_id_object_cb(void *object,
                                         void *user_data)
{
    return (uint64_t)(intptr_t)object;
}

uint64_t
rig_pb_serializer_register_object(rig_pb_serializer_t *serializer,
                                  void *object)
{
    void *object_register_data = serializer->object_register_data;
    return serializer->object_register_callback(object, object_register_data);
}

uint64_t
default_serializer_lookup_object_id_cb(void *object, void *user_data)
{
    rig_pb_serializer_t *serializer = user_data;
    void *id_ptr;

    c_return_val_if_fail(object, 0);

    id_ptr = c_hash_table_lookup(serializer->object_to_id_map, object);

    c_return_val_if_fail(id_ptr, 0);

    return (uint64_t)(intptr_t)id_ptr;
}

uint64_t
serializer_lookup_pointer_object_id_cb(void *object, void *user_data)
{
    c_return_val_if_fail(object, 0);
    return (uint64_t)(intptr_t)object;
}

uint64_t
rig_pb_serializer_lookup_object_id(rig_pb_serializer_t *serializer,
                                   void *object)
{
    if (rut_object_get_type(object) == &rig_asset_type) {
        bool need_asset = true;

        if (serializer->asset_filter) {
            need_asset =
                serializer->asset_filter(object, serializer->asset_filter_data);
        }

        if (need_asset) {
            serializer->required_assets =
                c_llist_prepend(serializer->required_assets, object);
        }
    }

    return serializer->object_to_id_callback(object,
                                             serializer->object_to_id_data);
}

rig_pb_serializer_t *
rig_pb_serializer_new(rig_engine_t *engine)
{
    rig_pb_serializer_t *serializer = c_slice_new0(rig_pb_serializer_t);

    serializer->engine = engine;
    serializer->stack = engine->frame_stack;

    serializer->object_register_callback =
        default_serializer_register_object_cb;
    serializer->object_register_data = serializer;
    serializer->object_to_id_callback = default_serializer_lookup_object_id_cb;
    serializer->object_to_id_data = serializer;

    /* NB: We have to reserve 0 here so we can tell if lookups into the
     * id_map fail. */
    serializer->next_id = 1;

    return serializer;
}

void
rig_pb_serializer_set_stack(rig_pb_serializer_t *serializer,
                            rut_memory_stack_t *stack)
{
    serializer->stack = stack;
}

void
rig_pb_serializer_set_use_pointer_ids_enabled(rig_pb_serializer_t *serializer,
                                              bool use_pointers)
{
    if (use_pointers) {
        serializer->object_register_callback =
            serializer_register_pointer_id_object_cb;
        serializer->object_register_data = serializer;

        serializer->object_to_id_callback =
            serializer_lookup_pointer_object_id_cb;
        serializer->object_to_id_data = serializer;
    } else {
        /* we don't have a way to save/restore the above callbacks, so
         * really this function is currently just an internal
         * convenience for setting up the callbacks for the common case
         * where we want ids to simply correspond to pointers
         */
        c_warn_if_reached();
    }
}

void
rig_pb_serializer_set_asset_filter(rig_pb_serializer_t *serializer,
                                   rig_pb_asset_filter_t filter,
                                   void *user_data)
{
    serializer->asset_filter = filter;
    serializer->asset_filter_data = user_data;
}

void
rig_pb_serializer_set_only_asset_ids_enabled(rig_pb_serializer_t *serializer,
                                             bool only_ids)
{
    serializer->only_asset_ids = only_ids;
}

void
rig_pb_serializer_set_object_register_callback(
    rig_pb_serializer_t *serializer,
    rig_pb_serializer_object_register_callback_t callback,
    void *user_data)
{
    serializer->object_register_callback = callback;
    serializer->object_register_data = user_data;
}

void
rig_pb_serializer_set_object_to_id_callback(
    rig_pb_serializer_t *serializer,
    rig_pb_serializer_object_to_id_callback_t callback,
    void *user_data)
{
    serializer->object_to_id_callback = callback;
    serializer->object_to_id_data = user_data;
}

void
rig_pb_serializer_set_skip_image_data(rig_pb_serializer_t *serializer,
                                      bool skip)
{
    serializer->skip_image_data = skip;
}

void
rig_pb_serializer_destroy(rig_pb_serializer_t *serializer)
{
    if (serializer->required_assets)
        c_llist_free(serializer->required_assets);

    if (serializer->object_to_id_map)
        c_hash_table_destroy(serializer->object_to_id_map);

    c_slice_free(rig_pb_serializer_t, serializer);
}

static Rig__Asset *
serialize_mesh_asset(rig_pb_serializer_t *serializer,
                     rig_asset_t *asset)
{
    rut_mesh_t *mesh = rig_asset_get_mesh(asset);
    Rig__Asset *pb_asset;

    if (!mesh) {
        c_warning("Missing asset mesh");
        return NULL;
    }

    pb_asset = rig_pb_new(serializer, Rig__Asset, rig__asset__init);

    pb_asset->has_id = true;
    pb_asset->id = rig_pb_serializer_lookup_object_id(serializer, asset);

    pb_asset->path = (char *)rig_asset_get_path(asset);

    pb_asset->has_type = true;
    pb_asset->type = RIG_ASSET_TYPE_MESH;

    pb_asset->mesh = rig_pb_serialize_mesh(serializer, mesh);

    return pb_asset;
}

static bool
serialize_asset_file(Rig__Asset *pb_asset, rig_asset_t *asset)
{
    rut_shell_t *shell = rig_asset_get_shell(asset);
    const char *path = rig_asset_get_path(asset);
    char *full_path = c_build_filename(shell->assets_location, path, NULL);
    c_error_t *error = NULL;
    char *contents;
    size_t len;

    contents = rig_asset_get_data(asset);

    if (contents) {
        len = rig_asset_get_data_len(asset);
    } else {
        if (!c_file_get_contents(full_path, &contents, &len, &error)) {
            c_warning("Failed to read contents of asset: %s", error->message);
            c_error_free(error);
            c_free(full_path);
            return false;
        }
    }

    c_free(full_path);

    pb_asset->has_data = true;
    pb_asset->data.data = (uint8_t *)contents;
    pb_asset->data.len = len;

    return true;
}

static Rig__Asset *
serialize_asset(rig_pb_serializer_t *serializer,
                rig_asset_t *asset)
{
    Rig__Asset *pb_asset;
    rig_asset_type_t type;

    if (serializer->only_asset_ids) {
        pb_asset = rig_pb_new(serializer, Rig__Asset, rig__asset__init);

        pb_asset->has_id = true;
        pb_asset->id = rig_pb_serializer_lookup_object_id(serializer, asset);

        return pb_asset;
    }

    type = rig_asset_get_type(asset);

    switch (type) {
    case RIG_ASSET_TYPE_MESH:
        return serialize_mesh_asset(serializer, asset);
    case RIG_ASSET_TYPE_TEXTURE:
    case RIG_ASSET_TYPE_NORMAL_MAP:
    case RIG_ASSET_TYPE_ALPHA_MASK:
        pb_asset = rig_pb_new(serializer, Rig__Asset, rig__asset__init);

        pb_asset->has_id = true;
        pb_asset->id = rig_pb_serializer_lookup_object_id(serializer, asset);

        pb_asset->has_type = true;
        pb_asset->type = rig_asset_get_type(asset);

        pb_asset->has_width = true;
        pb_asset->has_height = true;
        rig_asset_get_image_size(asset, &pb_asset->width, &pb_asset->height);

        pb_asset->path = (char *)rig_asset_get_path(asset);
        pb_asset->mime_type = (char *)rig_asset_get_mime_type(asset);

        if (!serializer->skip_image_data) {
            if (!serialize_asset_file(pb_asset, asset))
                return NULL;
        }
        break;
    case RIG_ASSET_TYPE_FONT: {
        pb_asset = rig_pb_new(serializer, Rig__Asset, rig__asset__init);

        pb_asset->has_id = true;
        pb_asset->id = rig_pb_serializer_lookup_object_id(serializer, asset);

        pb_asset->has_type = true;
        pb_asset->type = rig_asset_get_type(asset);

        pb_asset->path = (char *)rig_asset_get_path(asset);

        if (!serialize_asset_file(pb_asset, asset))
            return NULL;
    } break;
    case RIG_ASSET_TYPE_BUILTIN:
        /* XXX: We should be aiming to remove the "builtin" asset type
         * and instead making the editor handle builtins specially
         * in how it lists search results.
         */
        c_warning("Can't serialize \"builtin\" asset type");
        return NULL;
    }

    return pb_asset;
}

static void
serialized_asset_destroy(Rig__Asset *serialized_asset)
{
    if (serialized_asset->has_data)
        c_free(serialized_asset->data.data);
}

static uint64_t
rig_pb_serializer_lookup_or_register_object(rig_pb_serializer_t *serializer,
                                            rut_object_t *object)
{
    uint64_t id = rig_pb_serializer_lookup_object_id(serializer, object);
    if (!id)
        id = rig_pb_serializer_register_object(serializer, object);
    return id;
}

Rig__Controller *
rig_pb_serialize_controller(rig_pb_serializer_t *serializer,
                            rig_controller_t *controller)
{
    Rig__Controller *pb_controller =
        rig_pb_new(serializer, Rig__Controller, rig__controller__init);
    c_llist_t *l;
    int i;

    pb_controller->has_id = true;
    pb_controller->id = rig_pb_serializer_lookup_or_register_object(serializer,
                                                                    controller);

    pb_controller->name = controller->label;

    serialize_instrospectable_properties(
        controller,
        &pb_controller->n_controller_properties,
        (void **)&pb_controller->controller_properties,
        serializer);

    serializer->n_pb_properties = 0;
    serializer->pb_properties = NULL;
    rig_controller_foreach_property(
        controller, serialize_controller_property_cb, serializer);

    pb_controller->n_properties = serializer->n_pb_properties;
    pb_controller->properties =
        rut_memory_stack_memalign(serializer->stack,
                                  sizeof(void *) * pb_controller->n_properties,
                                  C_ALIGNOF(void *));
    for (i = 0, l = serializer->pb_properties; l; i++, l = l->next)
        pb_controller->properties[i] = l->data;
    c_llist_free(serializer->pb_properties);
    serializer->n_pb_properties = 0;
    serializer->pb_properties = NULL;

    return pb_controller;
}

Rig__UI *
rig_pb_serialize_ui(rig_pb_serializer_t *serializer,
                    rig_ui_t *ui)
{
    c_llist_t *l;
    int i;
    Rig__UI *pb_ui;

    pb_ui = rig_pb_new(serializer, Rig__UI, rig__ui__init);

    pb_ui->n_buffers = c_llist_length(ui->buffers);
    pb_ui->buffers =
        rut_memory_stack_memalign(serializer->stack,
                                  sizeof(void *) * pb_ui->n_buffers,
                                  C_ALIGNOF(void *));
    for (i = 0, l = ui->buffers; l; i++, l = l->next)
        pb_ui->buffers[i] = rig_pb_serialize_buffer(serializer, l->data, true);

    /* Register all assets up front, but we only actually serialize those
     * assets that are referenced - indicated by a corresponding id lookup
     * in rig_pb_serializer_lookup_object_id()
     */
    for (l = ui->assets; l; l = l->next)
        rig_pb_serializer_register_object(serializer, l->data);

    serializer->n_pb_entities = 0;
    rut_graphable_traverse(ui->scene,
                           RUT_TRAVERSE_DEPTH_FIRST,
                           _rig_entitygraph_pre_serialize_cb,
                           NULL,
                           serializer);

    pb_ui->n_entities = serializer->n_pb_entities;
    pb_ui->entities =
        rut_memory_stack_memalign(serializer->stack,
                                  sizeof(void *) * pb_ui->n_entities,
                                  C_ALIGNOF(void *));
    for (i = 0, l = serializer->pb_entities; l; i++, l = l->next)
        pb_ui->entities[i] = l->data;
    c_llist_free(serializer->pb_entities);
    serializer->pb_entities = NULL;

    for (i = 0, l = ui->controllers; l; i++, l = l->next)
        rig_pb_serializer_register_object(serializer, l->data);

    pb_ui->n_controllers = c_llist_length(ui->controllers);
    if (pb_ui->n_controllers) {
        pb_ui->controllers =
            rut_memory_stack_memalign(serializer->stack,
                                      sizeof(void *) * pb_ui->n_controllers,
                                      C_ALIGNOF(void *));

        for (i = 0, l = ui->controllers; l; i++, l = l->next) {
            rig_controller_t *controller = l->data;
            Rig__Controller *pb_controller =
                rig_pb_serialize_controller(serializer, controller);

            pb_ui->controllers[i] = pb_controller;
        }
    }

    pb_ui->n_assets = c_llist_length(serializer->required_assets);
    if (pb_ui->n_assets) {
        rig_pb_asset_filter_t save_filter = serializer->asset_filter;
        int i;

        /* Temporarily disable the asset filter that is called in
         * rig_pb_serializer_lookup_object_id() since we have already
         * filtered all of the assets required and we now only need to
         * lookup the ids for serializing the assets themselves. */
        serializer->asset_filter = NULL;

        pb_ui->assets =
            rut_memory_stack_memalign(serializer->stack,
                                      pb_ui->n_assets * sizeof(void *),
                                      C_ALIGNOF(void *));
        for (i = 0, l = serializer->required_assets; l; l = l->next) {
            rig_asset_t *asset = l->data;
            Rig__Asset *pb_asset = serialize_asset(serializer, asset);

            if (pb_asset)
                pb_ui->assets[i++] = pb_asset;
        }
        pb_ui->n_assets = i;

        /* restore the asset filter */
        serializer->asset_filter = save_filter;
    }

    if (ui->dso_data) {
        pb_ui->has_dso = true;
        pb_ui->dso.data =
            rut_memory_stack_memalign(serializer->stack, ui->dso_len, 1);
        pb_ui->dso.len = ui->dso_len;
    }

    return pb_ui;
}

void
rig_pb_serialized_ui_destroy(Rig__UI *ui)
{
    int i;

    for (i = 0; i < ui->n_assets; i++)
        serialized_asset_destroy(ui->assets[i]);
}

Rig__Event **
rig_pb_serialize_input_events(rig_pb_serializer_t *serializer,
                              rut_input_queue_t *input_queue)
{
    int n_events = input_queue->n_events;
    rut_input_event_t *event, *tmp;
    Rig__Event **pb_events;
    int i;

    pb_events = rut_memory_stack_memalign(
        serializer->stack, n_events * sizeof(void *), C_ALIGNOF(void *));

    i = 0;
    c_list_for_each_safe(event, tmp, &input_queue->events, list_node)
    {
        Rig__Event *pb_event =
            rig_pb_new(serializer, Rig__Event, rig__event__init);
        rut_shell_onscreen_t *onscreen = event->onscreen;
        rut_object_t *camera = onscreen->input_camera;

        pb_event->has_type = true;

        switch (event->type) {
        case RUT_INPUT_EVENT_TYPE_MOTION: {
            rut_motion_event_action_t action =
                rut_motion_event_get_action(event);

            switch (action) {
            case RUT_MOTION_EVENT_ACTION_MOVE:
                //c_debug("Serialize move\n");
                pb_event->type = RIG__EVENT__TYPE__POINTER_MOVE;
                pb_event->pointer_move =
                    rig_pb_new(serializer,
                               Rig__Event__PointerMove,
                               rig__event__pointer_move__init);

                pb_event->pointer_move->has_x = true;
                pb_event->pointer_move->x =
                    rut_motion_event_get_x(event);
                pb_event->pointer_move->has_y = true;
                pb_event->pointer_move->y =
                    rut_motion_event_get_y(event);

                pb_event->pointer_move->mod_state =
                    rut_motion_event_get_modifier_state(event);
                break;
            case RUT_MOTION_EVENT_ACTION_DOWN:
                //c_debug("Serialize pointer down\n");
                pb_event->type = RIG__EVENT__TYPE__POINTER_DOWN;
                break;
            case RUT_MOTION_EVENT_ACTION_UP:
                //c_debug("Serialize pointer up\n");
                pb_event->type = RIG__EVENT__TYPE__POINTER_UP;
                break;
            }

            switch (action) {
            case RUT_MOTION_EVENT_ACTION_MOVE:
                break;
            case RUT_MOTION_EVENT_ACTION_UP:
            case RUT_MOTION_EVENT_ACTION_DOWN:
                pb_event->pointer_button =
                    rig_pb_new(serializer,
                               Rig__Event__PointerButton,
                               rig__event__pointer_button__init);
                pb_event->pointer_button->has_button = true;
                pb_event->pointer_button->button =
                    rut_motion_event_get_button(event);
                pb_event->pointer_button->mod_state =
                    rut_motion_event_get_modifier_state(event);
            }

            break;
        }
        case RUT_INPUT_EVENT_TYPE_KEY: {
            rut_key_event_action_t action = rut_key_event_get_action(event);

            switch (action) {
            case RUT_KEY_EVENT_ACTION_DOWN:
                //c_debug("Serialize key down\n");
                pb_event->type = RIG__EVENT__TYPE__KEY_DOWN;
                break;
            case RUT_KEY_EVENT_ACTION_UP:
                //c_debug("Serialize key up\n");
                pb_event->type = RIG__EVENT__TYPE__KEY_UP;
                break;
            }

            pb_event->key =
                rig_pb_new(serializer, Rig__Event__Key, rig__event__key__init);
            pb_event->key->has_keysym = true;
            pb_event->key->keysym = rut_key_event_get_keysym(event);
            pb_event->key->has_mod_state = true;
            pb_event->key->mod_state = rut_key_event_get_modifier_state(event);
        }

        case RUT_INPUT_EVENT_TYPE_TEXT:
        case RUT_INPUT_EVENT_TYPE_DROP_OFFER:
        case RUT_INPUT_EVENT_TYPE_DROP_CANCEL:
        case RUT_INPUT_EVENT_TYPE_DROP:
            break;
        }

        if (camera) {
            uint64_t id = rig_pb_serializer_lookup_object_id(serializer, camera);

            c_warn_if_fail(id != 0);

            pb_event->has_camera_id = true;
            pb_event->camera_id = id;
        }

        pb_events[i] = pb_event;

        i++;
    }

    return pb_events;
}

Rig__Operation **
rig_pb_serialize_ops_queue(rig_pb_serializer_t *serializer,
                           rut_queue_t *ops)
{
    Rig__Operation **pb_ops;
    rut_queue_item_t *item;
    int i;

    if (!ops->len)
        return NULL;

    pb_ops = rut_memory_stack_memalign(
        serializer->stack, sizeof(void *) * ops->len, C_ALIGNOF(void *));

    i = 0;
    c_list_for_each(item, &ops->items, list_node)
    {
        pb_ops[i++] = item->data;
    }

    return pb_ops;
}

static void
pb_init_color(rut_shell_t *shell, cg_color_t *color, Rig__Color *pb_color)
{
    if (pb_color && pb_color->hex)
        rut_color_init_from_string(shell, color, pb_color->hex);
    else
        cg_color_init_from_4f(color, 0, 0, 0, 1);
}

static void
pb_init_quaternion(c_quaternion_t *quaternion,
                   Rig__Rotation *pb_rotation)
{
    if (pb_rotation) {
        c_quaternion_init(quaternion,
                           pb_rotation->angle,
                           pb_rotation->x,
                           pb_rotation->y,
                           pb_rotation->z);
    } else
        c_quaternion_init(quaternion, 0, 1, 0, 0);
}

static void
pb_init_boxed_vec3(rut_boxed_t *boxed, Rig__Vec3 *pb_vec3)
{
    boxed->type = RUT_PROPERTY_TYPE_VEC3;
    if (pb_vec3) {
        boxed->d.vec3_val[0] = pb_vec3->x;
        boxed->d.vec3_val[1] = pb_vec3->y;
        boxed->d.vec3_val[2] = pb_vec3->z;
    } else {
        boxed->d.vec3_val[0] = 0;
        boxed->d.vec3_val[1] = 0;
        boxed->d.vec3_val[2] = 0;
    }
}

static void
pb_init_boxed_vec4(rut_boxed_t *boxed, Rig__Vec4 *pb_vec4)
{
    boxed->type = RUT_PROPERTY_TYPE_VEC4;
    if (pb_vec4) {
        boxed->d.vec4_val[0] = pb_vec4->x;
        boxed->d.vec4_val[1] = pb_vec4->y;
        boxed->d.vec4_val[2] = pb_vec4->z;
        boxed->d.vec4_val[3] = pb_vec4->w;
    } else {
        boxed->d.vec4_val[0] = 0;
        boxed->d.vec4_val[1] = 0;
        boxed->d.vec4_val[2] = 0;
        boxed->d.vec4_val[3] = 0;
    }
}

static rut_object_t *
unserializer_try_find_object(rig_pb_unserializer_t *unserializer,
                             uint64_t id)
{
    void *user_data = unserializer->id_to_object_data;

    return unserializer->id_to_object_callback(unserializer->engine->ui, id, user_data);
}

static rut_object_t *
unserializer_find_object(rig_pb_unserializer_t *unserializer,
                         uint64_t id)
{
    if (id == 0) {
        rig_pb_unserializer_collect_error(unserializer,
                                          "Unexpected Object ID == 0");
        return NULL;
    } else {
        rut_object_t *ret = unserializer_try_find_object(unserializer, id);

        if (!ret)
            rig_pb_unserializer_collect_error(
                unserializer, "Invalid object id=%llu", id);

        return ret;
    }
}

bool
rig_pb_init_boxed_value(rig_pb_unserializer_t *unserializer,
                        rut_boxed_t *boxed,
                        rig_property_type_t type,
                        Rig__PropertyValue *pb_value)
{
    boxed->type = type;

    switch (type) {
    case RUT_PROPERTY_TYPE_FLOAT:
        boxed->d.float_val = pb_value->float_value;
        return true;

    case RUT_PROPERTY_TYPE_DOUBLE:
        boxed->d.double_val = pb_value->double_value;
        return true;

    case RUT_PROPERTY_TYPE_INTEGER:
        boxed->d.integer_val = pb_value->integer_value;
        return true;

    case RUT_PROPERTY_TYPE_UINT32:
        boxed->d.uint32_val = pb_value->uint32_value;
        return true;

    case RUT_PROPERTY_TYPE_BOOLEAN:
        boxed->d.boolean_val = pb_value->boolean_value;
        return true;

    case RUT_PROPERTY_TYPE_TEXT:
        boxed->d.text_val = c_strdup(pb_value->text_value);
        return true;

    case RUT_PROPERTY_TYPE_QUATERNION:
        pb_init_quaternion(&boxed->d.quaternion_val,
                           pb_value->quaternion_value);
        return true;

    case RUT_PROPERTY_TYPE_VEC3:
        pb_init_boxed_vec3(boxed, pb_value->vec3_value);
        return true;

    case RUT_PROPERTY_TYPE_VEC4:
        pb_init_boxed_vec4(boxed, pb_value->vec4_value);
        return true;

    case RUT_PROPERTY_TYPE_COLOR:
        pb_init_color(unserializer->engine->shell,
                      &boxed->d.color_val,
                      pb_value->color_value);
        return true;

    case RUT_PROPERTY_TYPE_ENUM:
        /* XXX: this should possibly work in terms of string names rather than
         * the integer value? */
        boxed->d.enum_val = pb_value->enum_value;
        return true;

    case RUT_PROPERTY_TYPE_ASSET:
        if (pb_value->asset_value == 0) {
            boxed->d.asset_val = NULL;
            return true;
        } else {
            boxed->d.asset_val =
                unserializer_find_object(unserializer, pb_value->asset_value);
            return !!boxed->d.asset_val;
        }

    case RUT_PROPERTY_TYPE_OBJECT:

        if (pb_value->object_value == 0) {
            boxed->d.object_val = NULL;
            return true;
        } else {
            boxed->d.object_val =
                unserializer_find_object(unserializer, pb_value->object_value);
            return !!boxed->d.object_val;
        }

    case RUT_PROPERTY_TYPE_POINTER:
        rig_pb_unserializer_collect_error(unserializer,
                                          "Spurious _TYPE_POINTER boxed property value");
        return false;
    }

    rig_pb_unserializer_collect_error(unserializer,
                                      "Unknown boxed property type %d", type);
    return false;
}

void
rig_pb_unserializer_collect_error(rig_pb_unserializer_t *unserializer,
                                  const char *format,
                                  ...)
{
    struct unserializer_error *error = c_malloc0(sizeof(*error));
    va_list ap;

    va_start(ap, format);
    error->msg = c_strdup_vprintf(format, ap);
    va_end(ap);

    error->backtrace = c_backtrace_new();

    unserializer->errors = c_llist_prepend(unserializer->errors, error);
}

void
rig_pb_unserializer_unregister_object(rig_pb_unserializer_t *unserializer,
                                      uint64_t id)
{
    void *user_data = unserializer->object_unregister_data;

    unserializer->object_unregister_callback(unserializer->engine->ui,
                                             id, user_data);
}

bool
rig_pb_unserializer_register_object(rig_pb_unserializer_t *unserializer,
                                    void *object,
                                    uint64_t id)
{
#ifdef RIG_ENABLE_DEBUG
    if (C_UNLIKELY(id == 0)) {
        rig_pb_unserializer_collect_error(unserializer,
                                          "Can't register object %p with ID == 0",
                                          object);
        return false;
    }

    {
        void *existing = unserializer_try_find_object(unserializer, id);
        if (existing) {
            rig_pb_unserializer_collect_error(unserializer,
                                              "Unserializer ID %ld already mapped to %p, not registering %p",
                                              id,
                                              existing,
                                              object);
            return false;
        }
    }
#endif

    unserializer->object_register_callback(unserializer->engine->ui,
                                           object, id,
                                           unserializer->object_register_data);

    return true;
}

static bool
set_property_from_pb_boxed(rig_pb_unserializer_t *unserializer,
                           rig_property_t *property,
                           Rig__Boxed *pb_boxed)
{
    rig_property_type_t type = 0;
    rut_boxed_t boxed;

    if (!pb_boxed->value) {
        rig_pb_unserializer_collect_error(unserializer,
                                          "Boxed property has no value");
        return false;
    }

    if (!pb_boxed->has_type) {
        rig_pb_unserializer_collect_error(unserializer,
                                          "Boxed property has no type");
        return false;
    }

    switch (pb_boxed->type) {
    case RIG__PROPERTY_TYPE__FLOAT:
        type = RUT_PROPERTY_TYPE_FLOAT;
        break;
    case RIG__PROPERTY_TYPE__DOUBLE:
        type = RUT_PROPERTY_TYPE_DOUBLE;
        break;
    case RIG__PROPERTY_TYPE__INTEGER:
        type = RUT_PROPERTY_TYPE_INTEGER;
        break;
    case RIG__PROPERTY_TYPE__ENUM:
        type = RUT_PROPERTY_TYPE_ENUM;
        break;
    case RIG__PROPERTY_TYPE__UINT32:
        type = RUT_PROPERTY_TYPE_UINT32;
        break;
    case RIG__PROPERTY_TYPE__BOOLEAN:
        type = RUT_PROPERTY_TYPE_BOOLEAN;
        break;
    case RIG__PROPERTY_TYPE__OBJECT:
        type = RUT_PROPERTY_TYPE_OBJECT;
        break;
    case RIG__PROPERTY_TYPE__POINTER:
        type = RUT_PROPERTY_TYPE_POINTER;
        break;
    case RIG__PROPERTY_TYPE__QUATERNION:
        type = RUT_PROPERTY_TYPE_QUATERNION;
        break;
    case RIG__PROPERTY_TYPE__COLOR:
        type = RUT_PROPERTY_TYPE_COLOR;
        break;
    case RIG__PROPERTY_TYPE__VEC3:
        type = RUT_PROPERTY_TYPE_VEC3;
        break;
    case RIG__PROPERTY_TYPE__VEC4:
        type = RUT_PROPERTY_TYPE_VEC4;
        break;
    case RIG__PROPERTY_TYPE__TEXT:
        type = RUT_PROPERTY_TYPE_TEXT;
        break;
    case RIG__PROPERTY_TYPE__ASSET:
        type = RUT_PROPERTY_TYPE_ASSET;
        break;
    }

    if (!type) {
        rig_pb_unserializer_collect_error(unserializer,
                                          "Unknown boxed property type");
        return false;
    }

    if (rig_pb_init_boxed_value(unserializer, &boxed, type, pb_boxed->value)) {
        rig_property_set_boxed(&unserializer->engine->_property_ctx,
                               property, &boxed);
        return true;
    } else
        return false;
}

static void
set_properties_from_pb_boxed_values(rig_pb_unserializer_t *unserializer,
                                    rut_object_t *object,
                                    size_t n_properties,
                                    Rig__Boxed **properties)
{
    int i;

    for (i = 0; i < n_properties; i++) {
        Rig__Boxed *pb_boxed = properties[i];
        rig_property_t *property =
            rig_introspectable_lookup_property(object, pb_boxed->name);

        if (!property) {
            rig_pb_unserializer_collect_error(unserializer,
                                              "Unknown property \"%s\" for object of type %s",
                                              pb_boxed->name,
                                              rut_object_get_type_name(object));
            continue;
        }

        if (!set_property_from_pb_boxed(unserializer, property, pb_boxed)) {
            rig_pb_unserializer_collect_error(unserializer,
                                              "Failed to load boxed property \"%s\" for object of type %s",
                                              pb_boxed->name,
                                              rut_object_get_type_name(object));
        }
    }
}

rut_object_t *
rig_pb_unserialize_component(rig_pb_unserializer_t *unserializer,
                             Rig__Entity__Component *pb_component)
{
    uint64_t component_id;

    if (!pb_component->has_id)
        return NULL;

    component_id = pb_component->id;

    if (!pb_component->has_type)
        return NULL;

    switch (pb_component->type) {
    case RIG__ENTITY__COMPONENT__TYPE__LIGHT: {
        rig_light_t *light;

        light = rig_light_new(unserializer->engine);

        set_properties_from_pb_boxed_values(unserializer,
                                            light,
                                            pb_component->n_properties,
                                            pb_component->properties);

        if (!rig_pb_unserializer_register_object(unserializer, light, component_id)) {
            rut_object_unref(light);
            light = NULL;
        }
        return light;
    }
    case RIG__ENTITY__COMPONENT__TYPE__MATERIAL: {
        rig_material_t *material =
            rig_material_new(unserializer->engine);

        set_properties_from_pb_boxed_values(unserializer,
                                            material,
                                            pb_component->n_properties,
                                            pb_component->properties);

        if (!rig_pb_unserializer_register_object(unserializer, material, component_id)) {
            rut_object_unref(material);
            material = NULL;
        }
        return material;
    }
    case RIG__ENTITY__COMPONENT__TYPE__SOURCE: {
        rig_source_t *source =
            rig_source_new(unserializer->engine,
                           NULL, /* mime */
                           NULL, /* url */
                           NULL, /* data */
                           0, /* data len */
                           0, /* natural width */
                           0); /* natural height */

        set_properties_from_pb_boxed_values(unserializer,
                                            source,
                                            pb_component->n_properties,
                                            pb_component->properties);

        if (!rig_pb_unserializer_register_object(unserializer, source, component_id)) {
            rut_object_unref(source);
            source = NULL;
        }
        return source;
    }
    case RIG__ENTITY__COMPONENT__TYPE__MESH: {
        rut_mesh_t *rut_mesh;
        rig_mesh_t *mesh;

        rut_mesh = rig_pb_unserialize_rut_mesh(unserializer, pb_component->mesh);
        if (!rut_mesh)
            return NULL;

        mesh = rig_mesh_new_with_rut_mesh(unserializer->engine, rut_mesh);
        set_properties_from_pb_boxed_values(unserializer,
                                            mesh,
                                            pb_component->n_properties,
                                            pb_component->properties);

        if (!rig_pb_unserializer_register_object(unserializer, mesh, component_id)) {
            rut_object_unref(mesh);
            mesh = NULL;
        }
        return mesh;
    }
    case RIG__ENTITY__COMPONENT__TYPE__TEXT: {
        rig_text_t *text = rig_text_new(unserializer->engine);

        set_properties_from_pb_boxed_values(unserializer,
                                            text,
                                            pb_component->n_properties,
                                            pb_component->properties);

        if (!rig_pb_unserializer_register_object(unserializer, text, component_id)) {
            rut_object_unref(text);
            text = NULL;
        }
        return text;
    }
    case RIG__ENTITY__COMPONENT__TYPE__CAMERA: {
        rig_camera_t *camera = rig_camera_new(unserializer->engine,
                                              -1, /* ortho/vp width */
                                              -1, /* ortho/vp height */
                                              NULL);

        set_properties_from_pb_boxed_values(unserializer,
                                            camera,
                                            pb_component->n_properties,
                                            pb_component->properties);

        if (!rig_pb_unserializer_register_object(unserializer, camera, component_id)) {
            rut_object_unref(camera);
            camera = NULL;
        }
        return camera;
    }
    case RIG__ENTITY__COMPONENT__TYPE__BUTTON_INPUT: {
        rig_button_input_t *button_input =
            rig_button_input_new(unserializer->engine);

        set_properties_from_pb_boxed_values(unserializer,
                                            button_input,
                                            pb_component->n_properties,
                                            pb_component->properties);

        if (!rig_pb_unserializer_register_object(
            unserializer, button_input, component_id)) {
            rut_object_unref(button_input);
            button_input = NULL;
        }
        return button_input;
    }
    case RIG__ENTITY__COMPONENT__TYPE__NATIVE_MODULE: {
#ifdef USE_UV
        rig_native_module_t *module =
            rig_native_module_new(unserializer->engine);

        set_properties_from_pb_boxed_values(unserializer,
                                            module,
                                            pb_component->n_properties,
                                            pb_component->properties);

        if (!rig_pb_unserializer_register_object(unserializer, module, component_id)) {
            rut_object_unref(module);
            module = NULL;
        }
        return module;
#else
        rig_pb_unserializer_collect_error(unserializer,
                                          "Can't unserialize unsupported native module");
        c_warn_if_reached();
        return NULL;
#endif
    }
    case RIG__ENTITY__COMPONENT__TYPE__NINE_SLICE: {
        rig_nine_slice_t *nine_slice =
            rig_nine_slice_new(unserializer->engine,
                               0,
                               0,
                               0,
                               0, /* left, right, top, bottom */
                               0,
                               0); /* width, height */
        set_properties_from_pb_boxed_values(unserializer,
                                            nine_slice,
                                            pb_component->n_properties,
                                            pb_component->properties);

        if (!rig_pb_unserializer_register_object(unserializer, nine_slice,
                                                 component_id))
        {
            rut_object_unref(nine_slice);
            nine_slice = NULL;
        }

        return nine_slice;
    }
    }

    rig_pb_unserializer_collect_error(unserializer, "Unknown component type");
    c_warn_if_reached();

    return NULL;
}

static void
unserialize_components(rig_pb_unserializer_t *unserializer,
                       rig_entity_t *entity,
                       Rig__Entity *pb_entity)
{
    int i;

    for (i = 0; i < pb_entity->n_components; i++) {
        Rig__Entity__Component *pb_component = pb_entity->components[i];
        rut_component_t *component = rig_pb_unserialize_component(
            unserializer, pb_component);

        if (!component)
            continue;

        rig_entity_add_component(entity, component);
        rut_object_unref(component);
    }
}

rig_entity_t *
rig_pb_unserialize_entity(rig_pb_unserializer_t *unserializer,
                          Rig__Entity *pb_entity)
{
    rig_entity_t *entity;
    uint64_t id;

    if (!pb_entity->has_id)
        return NULL;

    id = pb_entity->id;
    if (unserializer_try_find_object(unserializer, id)) {
        rig_pb_unserializer_collect_error(
            unserializer, "Duplicate entity id %d", (int)id);
        return NULL;
    }

    entity = rig_entity_new(unserializer->engine);

    if (pb_entity->has_parent_id) {
        uint64_t parent_id = pb_entity->parent_id;
        rig_entity_t *parent =
            unserializer_find_object(unserializer, parent_id);

        if (!parent) {
            rig_pb_unserializer_collect_error(unserializer,
                                              "Invalid parent id referenced in "
                                              "entity element");
            rut_object_unref(entity);
            return NULL;
        }

        rut_graphable_add_child(parent, entity);

        /* Now that we know the entity has a parent we can drop our reference on
         * the entity... */
        rut_object_unref(entity);
    }

    if (pb_entity->label)
        rig_entity_set_label(entity, pb_entity->label);

    if (pb_entity->position) {
        Rig__Vec3 *pos = pb_entity->position;
        float position[3] = { pos->x, pos->y, pos->z };
        rig_entity_set_position(entity, position);
    }
    if (pb_entity->rotation) {
        c_quaternion_t q;

        pb_init_quaternion(&q, pb_entity->rotation);

        rig_entity_set_rotation(entity, &q);
    }
    if (pb_entity->has_scale)
        rig_entity_set_scale(entity, pb_entity->scale);

    unserialize_components(unserializer, entity, pb_entity);

    if (!rig_pb_unserializer_register_object(unserializer, entity, pb_entity->id)) {
        rut_object_unref(entity);
        entity = NULL;
    }

    return entity;
}

static void
unserialize_entities(rig_pb_unserializer_t *unserializer,
                     int n_entities,
                     Rig__Entity **entities)
{
    int i;

    for (i = 0; i < n_entities; i++) {
        rig_entity_t *entity =
            rig_pb_unserialize_entity(unserializer, entities[i]);

        if (!entity)
            continue;

        unserializer->entities = c_llist_prepend(unserializer->entities, entity);
    }
}

static void
unserialize_assets(rig_pb_unserializer_t *unserializer,
                   int n_assets,
                   Rig__Asset **assets)
{
    int i;

    for (i = 0; i < n_assets; i++) {
        Rig__Asset *pb_asset = assets[i];
        uint64_t id;
        rig_asset_t *asset = NULL;

        if (!pb_asset->has_id)
            continue;

        id = pb_asset->id;

        if (unserializer->unserialize_asset_callback) {
            void *user_data = unserializer->unserialize_asset_data;
            asset = unserializer->unserialize_asset_callback(
                unserializer, pb_asset, user_data);
        } else if (unserializer_try_find_object(unserializer, id)) {
            rig_pb_unserializer_collect_error(
                unserializer, "Duplicate asset id %d", (int)id);
            continue;
        } else {
            rut_exception_t *catch = NULL;
            asset = rig_asset_new_from_pb_asset(unserializer, pb_asset, &catch);
            if (!asset) {
                rig_pb_unserializer_collect_error(
                    unserializer,
                    "Error unserializing asset id %d: %s",
                    (int)id,
                    catch->message);
                rut_exception_free(catch);
            }
        }

        if (asset) {
            if (rig_pb_unserializer_register_object(unserializer, asset, id))
                unserializer->assets = c_llist_prepend(unserializer->assets, asset);
            else {
                rut_object_unref(asset);
                asset = NULL;
            }
        } else {
            rig_pb_unserializer_collect_error(unserializer,
                                              "Failed to load \"%s\" asset",
                                              pb_asset->path ? pb_asset->path : "");
        }
    }
}

rig_view_t *
rig_pb_unserialize_view(rig_pb_unserializer_t *unserializer,
                        Rig__SimpleObject *pb_view)
{
    rig_view_t *view = NULL;

    if (!pb_view->has_id)
        return NULL;

    view = rig_view_new(unserializer->engine);

    set_properties_from_pb_boxed_values(unserializer,
                                        view,
                                        pb_view->n_properties,
                                        pb_view->properties);

    if (!rig_pb_unserializer_register_object(unserializer, view, pb_view->id)) {
        rut_object_unref(view);
        view = NULL;
    }

    return view;
}

static void
unserialize_buffers(rig_pb_unserializer_t *unserializer,
                    int n_buffers,
                    Rig__Buffer **pb_buffers)
{
    int i;

    for (i = 0; i < n_buffers; i++) {
        rut_buffer_t *buffer  = rig_pb_unserialize_buffer(unserializer, pb_buffers[i]);

        if (buffer)
            unserializer->buffers = c_llist_prepend(unserializer->buffers, buffer);
    }
}

static void
unserialize_views(rig_pb_unserializer_t *unserializer,
                  int n_views,
                  Rig__SimpleObject **views)
{
    int i;

    for (i = 0; i < n_views; i++) {
        rig_view_t *view  = rig_pb_unserialize_view(unserializer, views[i]);

        if (view)
            unserializer->views = c_llist_prepend(unserializer->views, view);
    }
}

static void
unserialize_path_nodes(rig_pb_unserializer_t *unserializer,
                       rig_path_t *path,
                       int n_nodes,
                       Rig__Node **nodes)
{
    int i;

    for (i = 0; i < n_nodes; i++) {
        Rig__Node *pb_node = nodes[i];
        Rig__PropertyValue *pb_value;
        float t;

        if (!pb_node->has_t)
            continue;
        t = pb_node->t;

        if (!pb_node->value)
            continue;
        pb_value = pb_node->value;

        switch (path->type) {
        case RUT_PROPERTY_TYPE_FLOAT:
            rig_path_insert_float(path, t, pb_value->float_value);
            break;
        case RUT_PROPERTY_TYPE_DOUBLE:
            rig_path_insert_double(path, t, pb_value->double_value);
            break;
        case RUT_PROPERTY_TYPE_INTEGER:
            rig_path_insert_integer(path, t, pb_value->integer_value);
            break;
        case RUT_PROPERTY_TYPE_UINT32:
            rig_path_insert_uint32(path, t, pb_value->uint32_value);
            break;
        case RUT_PROPERTY_TYPE_VEC3: {
            float vec3[3] = { pb_value->vec3_value->x, pb_value->vec3_value->y,
                              pb_value->vec3_value->z };
            rig_path_insert_vec3(path, t, vec3);
            break;
        }
        case RUT_PROPERTY_TYPE_VEC4: {
            float vec4[4] = {
                pb_value->vec4_value->x, pb_value->vec4_value->y,
                pb_value->vec4_value->z, pb_value->vec4_value->w
            };
            rig_path_insert_vec4(path, t, vec4);
            break;
        }
        case RUT_PROPERTY_TYPE_COLOR: {
            cg_color_t color;
            pb_init_color(
                unserializer->engine->shell, &color, pb_value->color_value);
            rig_path_insert_color(path, t, &color);
            break;
        }
        case RUT_PROPERTY_TYPE_QUATERNION: {
            c_quaternion_t quaternion;
            pb_init_quaternion(&quaternion, pb_value->quaternion_value);
            rig_path_insert_quaternion(path, t, &quaternion);
            break;
        }

        /* These shouldn't be animatable */
        case RUT_PROPERTY_TYPE_BOOLEAN:
        case RUT_PROPERTY_TYPE_TEXT:
        case RUT_PROPERTY_TYPE_ENUM:
        case RUT_PROPERTY_TYPE_ASSET:
        case RUT_PROPERTY_TYPE_OBJECT:
        case RUT_PROPERTY_TYPE_POINTER:
            c_warn_if_reached();
        }
    }
}

void
rig_pb_unserialize_controller_properties(rig_pb_unserializer_t *unserializer,
                                         rig_controller_t *controller,
                                         int n_properties,
                                         Rig__Controller__Property **properties)
{
    int i;

    for (i = 0; i < n_properties; i++) {
        Rig__Controller__Property *pb_property = properties[i];
        uint64_t object_id;
        rut_object_t *object;
        rig_controller_method_t method;
        rig_property_t *property;

        if (!pb_property->has_object_id || pb_property->name == NULL)
            continue;

        if (pb_property->has_method) {
            switch (pb_property->method) {
            case RIG__CONTROLLER__PROPERTY__METHOD__CONSTANT:
                method = RIG_CONTROLLER_METHOD_CONSTANT;
                break;
            case RIG__CONTROLLER__PROPERTY__METHOD__PATH:
                method = RIG_CONTROLLER_METHOD_PATH;
                break;
            case RIG__CONTROLLER__PROPERTY__METHOD__C_BINDING:
                method = RIG_CONTROLLER_METHOD_BINDING;
                break;
            default:
                c_warn_if_reached();
                method = RIG_CONTROLLER_METHOD_CONSTANT;
            }
        } else
            method = RIG_CONTROLLER_METHOD_CONSTANT;

        object_id = pb_property->object_id;

        object = unserializer_find_object(unserializer, object_id);
        if (!object) {
            rig_pb_unserializer_collect_error(unserializer,
                                              "Invalid object id %d referenced "
                                              "in property element",
                                              (int)object_id);
            continue;
        }

        property =
            rig_introspectable_lookup_property(object, pb_property->name);

        if (!property) {
            rig_pb_unserializer_collect_error(unserializer,
                                              "Invalid object property name "
                                              "given for controller property");
            continue;
        }

        if (!property->spec->animatable &&
            method != RIG_CONTROLLER_METHOD_CONSTANT) {
            rig_pb_unserializer_collect_error(unserializer,
                                              "Can't dynamically control "
                                              "non-animatable property");
            continue;
        }

        rig_controller_add_property(controller, property);

        rig_controller_set_property_method(controller, property, method);

        if (pb_property->constant) {
            rut_boxed_t boxed_value;
            if (rig_pb_init_boxed_value(unserializer,
                                        &boxed_value,
                                        property->spec->type,
                                        pb_property->constant))
            {
                rig_controller_set_property_constant(
                    controller, property, &boxed_value);

                rut_boxed_destroy(&boxed_value);
            } else {
                rig_pb_unserializer_collect_error(unserializer,
                                                  "Failed to load controller constant");
            }
        }

        if (pb_property->path) {
            Rig__Path *pb_path = pb_property->path;
            rig_path_t *path =
                rig_path_new(unserializer->engine, property->spec->type);

            unserialize_path_nodes(
                unserializer, path, pb_path->n_nodes, pb_path->nodes);

            rig_controller_set_property_path(controller, property, path);

            rut_object_unref(path);
        }

        if (pb_property->has_binding_id) {
            int j;
            rig_binding_t *binding = rig_binding_new(
                unserializer->engine, property, pb_property->binding_id);

            rig_binding_set_expression(binding, pb_property->c_expression);

            for (j = 0; j < pb_property->n_dependencies; j++) {
                rig_property_t *dependency;
                rut_object_t *dependency_object;
                Rig__Controller__Property__Dependency *pb_dependency =
                    pb_property->dependencies[j];

                if (!pb_dependency->has_object_id) {
                    rig_pb_unserializer_collect_error(
                        unserializer,
                        "Property dependency with "
                        "no object ID");
                    break;
                }

                if (!pb_dependency->name) {
                    rig_pb_unserializer_collect_error(
                        unserializer,
                        "Property dependency with "
                        "no name");
                    break;
                }

                dependency_object = unserializer_find_object(
                    unserializer, pb_dependency->object_id);
                if (!dependency_object) {
                    rig_pb_unserializer_collect_error(
                        unserializer,
                        "Failed to find dependency "
                        "object for property");
                    break;
                }

                dependency = rig_introspectable_lookup_property(
                    dependency_object, pb_dependency->name);
                if (!dependency) {
                    rig_pb_unserializer_collect_error(unserializer,
                                                      "Failed to introspect "
                                                      "dependency object "
                                                      "for binding property");
                    break;
                }

                rig_binding_add_dependency(
                    binding, dependency, pb_dependency->name);
            }

            if (j != pb_property->n_dependencies) {
                rut_object_unref(binding);
                rig_pb_unserializer_collect_error(unserializer,
                                                  "Not able to resolve all "
                                                  "dependencies for "
                                                  "property binding "
                                                  "(skipping)");
                continue;
            }

            rig_controller_set_property_binding(controller, property, binding);
        }
    }
}

static bool
have_boxed_pb_property(Rig__Boxed **properties,
                       int n_properties,
                       const char *name)
{
    int i;
    for (i = 0; i < n_properties; i++) {
        Rig__Boxed *pb_boxed = properties[i];
        if (strcmp(pb_boxed->name, name) == 0)
            return true;
    }
    return false;
}

rig_controller_t *
rig_pb_unserialize_controller_bare(rig_pb_unserializer_t *unserializer,
                                   Rig__Controller *pb_controller)
{
    const char *name;
    rig_controller_t *controller;

    if (!pb_controller->has_id) {
        rig_pb_unserializer_collect_error(unserializer,
                                          "Controller missing ID");
        return NULL;
    }

    if (!pb_controller->name) {
        rig_pb_unserializer_collect_error(unserializer,
                                          "Controller missing name");
        return NULL;
    }

    name = pb_controller->name;

    controller = rig_controller_new(unserializer->engine, name);

    rig_controller_set_suspended(controller, true);

    if (strcmp(name, "Controller 0"))
        rig_controller_set_active(controller, true);

    /* Properties of the rig_controller_t itself */
    set_properties_from_pb_boxed_values(unserializer,
                                        controller,
                                        pb_controller->n_controller_properties,
                                        pb_controller->controller_properties);

    if (!rig_pb_unserializer_register_object(unserializer,
                                             controller, pb_controller->id)) {
        rut_object_unref(controller);
        controller = NULL;
    }

    return controller;
}

static void
unserialize_controllers(rig_pb_unserializer_t *unserializer,
                        int n_controllers,
                        Rig__Controller **controllers)
{
    int i;

    /* First we just allocate empty controllers and register them all
     * with ids before adding properties to them which may belong
     * to other controllers.
     */

    for (i = 0; i < n_controllers; i++) {
        Rig__Controller *pb_controller = controllers[i];
        rig_controller_t *controller;

        controller =
            rig_pb_unserialize_controller_bare(unserializer, pb_controller);

        if (controller)
            unserializer->controllers =
                c_llist_prepend(unserializer->controllers, controller);
    }

    for (i = 0; i < n_controllers; i++) {
        Rig__Controller *pb_controller = controllers[i];
        rig_controller_t *controller;

        if (!pb_controller->has_id)
            continue;

        controller = unserializer_find_object(unserializer, pb_controller->id);
        if (!controller) {
            c_warn_if_reached();
            continue;
        }

        /* Properties controlled by the rig_controller_t... */
        rig_pb_unserialize_controller_properties(unserializer,
                                                 controller,
                                                 pb_controller->n_properties,
                                                 pb_controller->properties);
    }
}

rig_pb_unserializer_t *
rig_pb_unserializer_new(rig_engine_t *engine,
                        rig_pb_unserializer_object_register_callback_t register_cb,
                        rig_pb_unserializer_object_un_register_callback_t unregister_cb,
                        rig_pb_unserializer_id_to_object_callback_t id_to_obj_cb,
                        void *user_data)
{
    rig_pb_unserializer_t *unserializer = c_slice_new0(rig_pb_unserializer_t);

    unserializer->engine = engine;
    unserializer->stack = engine->frame_stack;

    unserializer->object_register_callback = register_cb;
    unserializer->object_register_data = user_data;
    unserializer->object_unregister_callback = unregister_cb;
    unserializer->object_unregister_data = user_data;
    unserializer->id_to_object_callback = id_to_obj_cb;
    unserializer->id_to_object_data = user_data;

    return unserializer;
}

void
rig_pb_unserializer_set_stack(rig_pb_unserializer_t *unserializer,
                              rut_memory_stack_t *stack)
{
    unserializer->stack = stack;
}

void
rig_pb_unserializer_set_object_register_callback(
    rig_pb_unserializer_t *unserializer,
    rig_pb_unserializer_object_register_callback_t callback,
    void *user_data)
{
    unserializer->object_register_callback = callback;
    unserializer->object_register_data = user_data;
}

void
rig_pb_unserializer_set_object_unregister_callback(
    rig_pb_unserializer_t *unserializer,
    rig_pb_unserializer_object_un_register_callback_t callback,
    void *user_data)
{
    unserializer->object_unregister_callback = callback;
    unserializer->object_unregister_data = user_data;
}

void
rig_pb_unserializer_set_id_to_object_callback(
    rig_pb_unserializer_t *unserializer,
    rig_pb_unserializer_id_to_object_callback_t callback,
    void *user_data)
{
    unserializer->id_to_object_callback = callback;
    unserializer->id_to_object_data = user_data;
}

void
rig_pb_unserializer_set_asset_unserialize_callback(
    rig_pb_unserializer_t *unserializer,
    rig_pb_unserializer_asset_callback_t callback,
    void *user_data)
{
    unserializer->unserialize_asset_callback = callback;
    unserializer->unserialize_asset_data = user_data;
}

void
rig_pb_unserializer_log_errors(rig_pb_unserializer_t *unserializer)
{
    c_llist_t *l;

    /* XXX: The intention is that we shouldn't just immediately abort loading
     * like this but rather we should collect the errors and try our best to
     * continue loading. At the end we can report the errors to the user so they
     * realize that their document may be corrupt.
     */

    if (!unserializer->errors)
        return;

    c_warning("Unserializer errors:");
    for (l = unserializer->errors; l; l = l->next) {
        struct unserializer_error *error = l->data;
        int n_frames = c_backtrace_get_n_frames(error->backtrace);
        char *backtrace_symbols[n_frames];
        int i;

        c_backtrace_get_frame_symbols(error->backtrace, backtrace_symbols, n_frames);

        c_warning("  > %s\n", error->msg);

        c_warning("  backtrace:");
        for (i = 0; i < n_frames; i++)
            c_warning("    %d) %s", i, backtrace_symbols[i]);
    }
}

void
rig_pb_unserializer_clear_errors(rig_pb_unserializer_t *unserializer)
{
    c_llist_t *l;

    for (l = unserializer->errors; l; l = l->next) {
        struct unserializer_error *error = l->data;
        c_backtrace_free(error->backtrace);
        c_free(error);
    }
    c_llist_free(unserializer->errors);
    unserializer->errors = NULL;
}

void
rig_pb_unserializer_destroy(rig_pb_unserializer_t *unserializer)
{
    rig_pb_unserializer_log_errors(unserializer);
    rig_pb_unserializer_clear_errors(unserializer);
}

rig_ui_t *
rig_pb_unserialize_ui(rig_pb_unserializer_t *unserializer,
                      const Rig__UI *pb_ui)
{
    rig_ui_t *ui = rig_ui_new(unserializer->engine);
    rig_engine_t *engine = unserializer->engine;
    c_llist_t *l;

    unserialize_buffers(unserializer, pb_ui->n_buffers, pb_ui->buffers);

    unserialize_assets(unserializer, pb_ui->n_assets, pb_ui->assets);

    unserialize_entities(unserializer, pb_ui->n_entities, pb_ui->entities);

    unserialize_views(unserializer, pb_ui->n_views, pb_ui->views);

    unserialize_controllers(unserializer, pb_ui->n_controllers,
                            pb_ui->controllers);

    ui->scene = rig_entity_new(engine);

    int n_roots = 0;
    for (l = unserializer->entities; l; l = l->next) {
        if (rut_graphable_get_parent(l->data) == NULL) {
            ui->scene = l->data;
            n_roots++;
        }

        rig_ui_register_all_entity_components(ui, l->data);
    }
    if (n_roots > 1) {
        rig_pb_unserializer_collect_error(unserializer,
                                          "Multiple root entities found when loading UI");
    }

    c_llist_free(unserializer->entities);
    unserializer->entities = NULL;

    for (l = unserializer->views; l; l = l->next) {
        rig_ui_add_view(ui, l->data);
        rut_object_unref(l->data);
    }
    c_llist_free(unserializer->views);
    unserializer->views = NULL;

    for (l = unserializer->controllers; l; l = l->next) {
        rig_ui_add_controller(ui, l->data);
        rut_object_unref(l->data);
    }
    c_llist_free(unserializer->controllers);
    unserializer->controllers = NULL;

    c_debug("unserialized ui assets list  %p\n", unserializer->assets);
    ui->assets = unserializer->assets;
    unserializer->assets = NULL;

    for (l = unserializer->buffers; l; l = l->next) {
        rig_ui_add_buffer(ui, l->data);
        rut_object_unref(l->data);
    }
    c_llist_free(unserializer->buffers);
    unserializer->buffers = NULL;

    if (pb_ui->has_dso)
        rig_ui_set_dso_data(ui, pb_ui->dso.data, pb_ui->dso.len);

    return ui;
}

rut_buffer_t *
rig_pb_unserialize_buffer(rig_pb_unserializer_t *unserializer,
                          Rig__Buffer *pb_buffer)
{
    rut_buffer_t *buffer;

    if (!pb_buffer->has_id || pb_buffer->id == 0) {
        rig_pb_unserializer_collect_error(unserializer, "Missing buffer ID");
        return NULL;
    }

    if (!pb_buffer->size) {
        rig_pb_unserializer_collect_error(unserializer, "Invalid buffer size == 0");
        return NULL;
    }

    buffer = rut_buffer_new(pb_buffer->size);

    if (pb_buffer->data.data) {
        if (pb_buffer->data.len != pb_buffer->size) {
            rut_object_unref(buffer);
            rig_pb_unserializer_collect_error(unserializer, "Buffer data len != buffer size");
            return NULL;
        }
        memcpy(buffer->data, pb_buffer->data.data, pb_buffer->data.len);
    } else
        memset(buffer->data, 0, pb_buffer->size);

    if (!rig_pb_unserializer_register_object(unserializer, buffer, pb_buffer->id)) {
        rut_object_unref(buffer);
        return NULL;
    }

    return buffer;
}

/* NB: caller's responsibility to call rig_ui_add_buffer() for new
 * buffers as appropriate */
int
rig_pb_unserialize_buffers(rig_pb_unserializer_t *unserializer,
                           Rig__Buffer **pb_buffers,
                           rut_buffer_t **buffers,
                           int n_buffers)
{
    int n_new_buffers = 0;

    for (int i = 0; i < n_buffers; i++) {
        Rig__Buffer *pb_buffer = pb_buffers[i];
        buffers[n_new_buffers] = rig_pb_unserialize_buffer(unserializer, pb_buffer);
        if (!buffers[n_new_buffers])
            goto ERROR;
        n_new_buffers++;
    }

    return n_buffers;

ERROR:
    for (int i = 0; i < n_new_buffers; i++) {
        Rig__Buffer *pb_buffer = pb_buffers[i];
        rig_pb_unserializer_unregister_object(unserializer,
                                              pb_buffer->id);
        rut_object_unref(buffers[i]);
    }

    return 0;
}

/* Expects any required buffers to have already been registered... */
int
rig_pb_unserialize_attributes(rig_pb_unserializer_t *unserializer,
                              Rig__Attribute **pb_attributes,
                              rut_attribute_t **attributes,
                              int n_attributes)
{
    int n_new_attributes = 0;

    for (int i = 0; i < n_attributes; i++) {
        Rig__Attribute *pb_attribute = pb_attributes[i];

        if (!pb_attribute->name) {
            rig_pb_unserializer_collect_error(unserializer,
                                              "Attribute missing name");
            goto ERROR;
        }

        if (pb_attribute->has_buffer_id) {
            rut_buffer_t *buffer = NULL;
            rut_attribute_type_t type;

            if (!pb_attribute->has_stride) {
                rig_pb_unserializer_collect_error(unserializer,
                                                  "Attribute missing stride");
                goto ERROR;
            }

            if (!pb_attribute->has_offset) {
                rig_pb_unserializer_collect_error(unserializer,
                                                  "Attribute missing offset");
                goto ERROR;
            }

            if (!pb_attribute->has_n_components) {
                rig_pb_unserializer_collect_error(unserializer,
                                                  "Attribute missing N components");
                goto ERROR;
            }

            if (!pb_attribute->has_type) {
                rig_pb_unserializer_collect_error(unserializer, "Attribute missing type");
                goto ERROR;
            }

            buffer = unserializer_find_object(unserializer, pb_attribute->buffer_id);
            if (!buffer) {
                rig_pb_unserializer_collect_error(unserializer,
                                                  "Attribute buffer not found");
                goto ERROR;
            }

            switch (pb_attribute->type) {
                case RIG__ATTRIBUTE__TYPE__BYTE:
                    type = RUT_ATTRIBUTE_TYPE_BYTE;
                    break;
                case RIG__ATTRIBUTE__TYPE__UNSIGNED_BYTE:
                    type = RUT_ATTRIBUTE_TYPE_UNSIGNED_BYTE;
                    break;
                case RIG__ATTRIBUTE__TYPE__SHORT:
                    type = RUT_ATTRIBUTE_TYPE_SHORT;
                    break;
                case RIG__ATTRIBUTE__TYPE__UNSIGNED_SHORT:
                    type = RUT_ATTRIBUTE_TYPE_UNSIGNED_SHORT;
                    break;
                case RIG__ATTRIBUTE__TYPE__FLOAT:
                    type = RUT_ATTRIBUTE_TYPE_FLOAT;
                    break;
            }

            attributes[i] = rut_attribute_new(buffer,
                                              pb_attribute->name,
                                              pb_attribute->stride,
                                              pb_attribute->offset,
                                              pb_attribute->n_components,
                                              type);
        } else {
            int n_floats;

            if (!pb_attribute->has_n_components) {
                rig_pb_unserializer_collect_error(unserializer,
                                                  "Attribute missing N components");
                goto ERROR;
            }

            if (!pb_attribute->has_n_columns) {
                rig_pb_unserializer_collect_error(unserializer,
                                                  "Attribute missing N columns");
                goto ERROR;
            }

            if (!pb_attribute->has_transpose) {
                rig_pb_unserializer_collect_error(unserializer,
                                                  "Attribute missing transpose");
                goto ERROR;
            }

            n_floats = pb_attribute->n_components * pb_attribute->n_columns;
            if (pb_attribute->n_floats != n_floats) {
                rig_pb_unserializer_collect_error(unserializer,
                                                  "Attribute: Supurious N floats");
                goto ERROR;
            }

            attributes[i] = rut_attribute_new_const(pb_attribute->name,
                                                    pb_attribute->n_components,
                                                    pb_attribute->n_columns,
                                                    pb_attribute->transpose,
                                                    pb_attribute->floats);
        }

        if (pb_attribute->has_normalized && pb_attribute->normalized)
            rut_attribute_set_normalized(attributes[i], true);

        if (pb_attribute->has_instance_stride)
            rut_attribute_set_normalized(attributes[i], pb_attribute->instance_stride);

        n_new_attributes++;
    }

    return n_new_attributes;

ERROR:

    for (int i = 0; i < n_new_attributes; i++)
        rut_object_unref(attributes[i]);

    return 0;
}

rut_mesh_t *
rig_pb_unserialize_rut_mesh(rig_pb_unserializer_t *unserializer,
                            Rig__Mesh *pb_mesh)
{
    rut_buffer_t *buffers[pb_mesh->n_buffers];
    int n_buffers = 0;
    rut_attribute_t *attributes[pb_mesh->n_attributes];
    int n_attributes = 0;
    cg_vertices_mode_t mode;
    rut_mesh_t *mesh = NULL;

    if (pb_mesh->n_buffers) {
        n_buffers = rig_pb_unserialize_buffers(unserializer,
                                               pb_mesh->buffers,
                                               buffers,
                                               pb_mesh->n_buffers);
        if (!n_buffers)
            goto ERROR;
    }


    if (pb_mesh->n_attributes) {
        n_attributes = rig_pb_unserialize_attributes(unserializer,
                                                     pb_mesh->attributes,
                                                     attributes,
                                                     pb_mesh->n_attributes);
        if (!n_attributes)
            goto ERROR;
    }

    if (!pb_mesh->has_mode || !pb_mesh->has_n_vertices)
        goto ERROR;

    switch (pb_mesh->mode) {
    case RIG__MESH__MODE__POINTS:
        mode = CG_VERTICES_MODE_POINTS;
        break;
    case RIG__MESH__MODE__LINES:
        mode = CG_VERTICES_MODE_LINES;
        break;
    case RIG__MESH__MODE__LINE_LOOP:
        mode = CG_VERTICES_MODE_LINE_LOOP;
        break;
    case RIG__MESH__MODE__LINE_STRIP:
        mode = CG_VERTICES_MODE_LINE_STRIP;
        break;
    case RIG__MESH__MODE__TRIANGLES:
        mode = CG_VERTICES_MODE_TRIANGLES;
        break;
    case RIG__MESH__MODE__TRIANGLE_STRIP:
        mode = CG_VERTICES_MODE_TRIANGLE_STRIP;
        break;
    case RIG__MESH__MODE__TRIANGLE_FAN:
        mode = CG_VERTICES_MODE_TRIANGLE_FAN;
        break;
    }

    mesh = rut_mesh_new(mode, pb_mesh->n_vertices,
                        attributes, pb_mesh->n_attributes);

    if (pb_mesh->has_indices_buffer_id) {
        rut_buffer_t *buffer = NULL;
        cg_indices_type_t indices_type;

        buffer = unserializer_find_object(unserializer,
                                          pb_mesh->indices_buffer_id);
        if (!buffer) {
            rig_pb_unserializer_collect_error(unserializer,
                                              "Failed to lookup indices buffer");
            goto ERROR;
        }

        if (!pb_mesh->has_indices_type) {
            rig_pb_unserializer_collect_error(unserializer,
                                              "Indices buffer without type");
            goto ERROR;
        }

        if (!pb_mesh->has_n_indices) {
            rig_pb_unserializer_collect_error(unserializer,
                                              "Indices buffer without count");
            goto ERROR;
        }

        switch (pb_mesh->indices_type) {
        case RIG__MESH__INDICES_TYPE__UNSIGNED_BYTE:
            indices_type = CG_INDICES_TYPE_UNSIGNED_BYTE;
            break;
        case RIG__MESH__INDICES_TYPE__UNSIGNED_SHORT:
            indices_type = CG_INDICES_TYPE_UNSIGNED_SHORT;
            break;
        case RIG__MESH__INDICES_TYPE__UNSIGNED_INT:
            indices_type = CG_INDICES_TYPE_UNSIGNED_INT;
            break;
        }

        rut_mesh_set_indices(mesh, indices_type, buffer, pb_mesh->n_indices);
    }

    /* The mesh will take references on the attributes */
    for (int i = 0; i < n_attributes; i++)
        rut_object_unref(attributes[i]);

    /* The attributes will take their own references on the buffers */
    for (int i = 0; i < n_buffers; i++)
        rut_object_unref(buffers[i]);

    return mesh;

ERROR:

    rig_pb_unserializer_collect_error(unserializer, "Invalid mesh");

    if (mesh)
        rut_object_unref(mesh);

    for (int i = 0; i < n_attributes; i++)
        rut_object_unref(attributes[i]);

    for (int i = 0; i < n_buffers; i++) {
        Rig__Buffer *pb_buffer = pb_mesh->buffers[i];
        rig_pb_unserializer_unregister_object(unserializer,
                                              pb_buffer->id);
        rut_object_unref(buffers[i]);
    }

    return NULL;
}
