/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012  Intel Corporation
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

#include "rig-path.h"
#include "rig-node.h"

static void
_rig_path_free(void *object)
{
    rig_path_t *path = object;
    rig_node_t *node, *t;

    rut_closure_list_disconnect_all_FIXME(&path->operation_cb_list);

    c_list_for_each_safe(node, t, &path->nodes, list_node)
    rig_node_free(node);

    rut_object_unref(path->shell);
    c_slice_free(rig_path_t, path);
}

rut_type_t rig_path_type;

void
_rig_path_init_type(void)
{
    rut_type_init(&rig_path_type, "rig_path_t", _rig_path_free);
}

rig_path_t *
rig_path_new(rut_shell_t *shell, rig_property_type_t type)
{
    rig_path_t *path =
        rut_object_alloc0(rig_path_t, &rig_path_type, _rig_path_init_type);

    path->shell = rut_object_ref(shell);

    path->type = type;

    c_list_init(&path->nodes);
    path->pos = NULL;
    path->length = 0;

    c_list_init(&path->operation_cb_list);

    return path;
}

rig_path_t *
rig_path_copy(rig_path_t *old_path)
{
    rig_path_t *new_path = rig_path_new(old_path->shell, old_path->type);
    rig_node_t *node;

    c_list_for_each(node, &old_path->nodes, list_node)
    {
        rig_node_t *new_node = rig_node_copy(node);
        c_list_insert(new_path->nodes.prev, &new_node->list_node);
    }
    new_path->length = old_path->length;

    return new_path;
}

/* Finds 1 point either side of the given t using the direction to resolve
 * which points to choose if t corresponds to a specific node.
 */
bool
rig_path_find_control_points2(rig_path_t *path,
                              float t,
                              rig_path_direction_t direction,
                              rig_node_t **n0,
                              rig_node_t **n1)
{
    rig_node_t *pos;

    if (C_UNLIKELY(c_list_empty(&path->nodes)))
        return false;

    if (C_UNLIKELY(path->pos == NULL))
        path->pos = rut_container_of(path->nodes.next, path->pos, list_node);

    pos = path->pos;

    /*
     * Note:
     *
     * A node with t exactly == t may only be considered as the first control
     * point moving in the current direction.
     */

    if (direction == RIG_PATH_DIRECTION_FORWARDS) {
        if (pos->t > t) {
            /* > --- T -------- Pos ---- */
            rig_node_t *tmp =
                rig_nodes_find_less_than_equal(pos, &path->nodes, t);
            if (!tmp) {
                *n0 = *n1 = path->pos =
                                rut_container_of(path->nodes.next, path->pos, list_node);
                return true;
            }
            pos = tmp;
        } else {
            /* > --- Pos -------- T ---- */
            rig_node_t *tmp = rig_nodes_find_greater_than(pos, &path->nodes, t);
            if (!tmp) {
                *n0 = *n1 = path->pos =
                                rut_container_of(path->nodes.prev, path->pos, list_node);
                return true;
            }
            pos = rut_container_of(tmp->list_node.prev, pos, list_node);
        }

        *n0 = pos;
        if (pos->list_node.next == &path->nodes)
            *n1 = pos;
        else
            *n1 = rut_container_of(pos->list_node.next, *n1, list_node);
    } else {
        if (pos->t > t) {
            /* < --- T -------- Pos ---- */
            rig_node_t *tmp = rig_nodes_find_less_than(pos, &path->nodes, t);
            if (!tmp) {
                *n0 = *n1 = path->pos =
                                rut_container_of(path->nodes.next, path->pos, list_node);
                return true;
            }
            pos = rut_container_of(tmp->list_node.next, pos, list_node);
        } else {
            /* < --- Pos -------- T ---- */
            rig_node_t *tmp =
                rig_nodes_find_greater_than_equal(pos, &path->nodes, t);
            if (!tmp) {
                *n0 = *n1 = path->pos =
                                rut_container_of(path->nodes.prev, path->pos, list_node);
                return true;
            }
            pos = tmp;
        }

        *n0 = pos;
        if (pos->list_node.prev == &path->nodes)
            *n1 = pos;
        else
            *n1 = rut_container_of(pos->list_node.prev, *n1, list_node);
    }

    path->pos = pos;

    return true;
}

/* Finds 2 points either side of the given t using the direction to resolve
 * which points to choose if t corresponds to a specific node. */
#if 0
static bool
path_find_control_points4 (rig_path_t *path,
                           float t,
                           int direction,
                           rig_node_t **n0,
                           rig_node_t **n1,
                           rig_node_t **n2,
                           rig_node_t **n3)
{
    c_llist_t *l1, *l2;

    if (!path_find_control_links2 (path, t, direction, &l1, &l2))
        return false;

    if (direction > 0)
    {
        if (l1->prev == NULL || l2->next == NULL)
            return false;

        *n0 = l1->prev->engine;
        *n3 = l2->next->engine;
    }
    else
    {
        if (l1->next == NULL || l2->prev == NULL)
            return false;

        *n0 = l1->next->engine;
        *n3 = l2->prev->engine;
    }

    *n1 = l1->engine;
    *n2 = l2->engine;

    return true;
}
#endif

void
rig_path_print(rig_path_t *path)
{
    rig_property_type_t type = path->type;
    rig_node_t *node;

    c_debug("path=%p\n", path);
    c_list_for_each(node, &path->nodes, list_node)
    {
        switch (type) {
        case RUT_PROPERTY_TYPE_FLOAT: {
            c_debug(" t = %f value = %f\n", node->t, node->boxed.d.float_val);
            break;
        }

        case RUT_PROPERTY_TYPE_VEC3: {
            c_debug(" t = %f value.x = %f .y = %f .z = %f\n",
                    node->t,
                    node->boxed.d.vec3_val[0],
                    node->boxed.d.vec3_val[1],
                    node->boxed.d.vec3_val[2]);
            break;
        }

        case RUT_PROPERTY_TYPE_QUATERNION: {
            const c_quaternion_t *q = &node->boxed.d.quaternion_val;
            c_debug(
                " t = %f [%f (%f, %f, %f)]\n", node->t, q->w, q->x, q->y, q->z);
            break;
        }

        default:
            c_warn_if_reached();
        }
    }
}

static void
notify_node_added(rig_path_t *path, rig_node_t *node)
{
    rut_closure_list_invoke(&path->operation_cb_list,
                            rig_path_operation_callback_t,
                            path,
                            RIG_PATH_OPERATION_ADDED,
                            node);
}

static void
notify_node_modified(rig_path_t *path, rig_node_t *node)
{
    rut_closure_list_invoke(&path->operation_cb_list,
                            rig_path_operation_callback_t,
                            path,
                            RIG_PATH_OPERATION_MODIFIED,
                            node);
}

rig_node_t *
rig_path_find_node(rig_path_t *path, float t)
{
    rig_node_t *node;

    c_list_for_each(node, &path->nodes, list_node)
    if (node->t == t)
        return node;

    return NULL;
}

rig_node_t *
rig_path_find_nearest(rig_path_t *path, float t)
{
    float min_dt = FLT_MAX;
    rig_node_t *min_dt_node = NULL;
    rig_node_t *node;

    c_list_for_each(node, &path->nodes, list_node)
    {
        float dt = fabs(node->t - t);
        if (dt < min_dt) {
            min_dt = dt;
            min_dt_node = node;
        } else
            return min_dt_node;
    }

    return min_dt_node;
    ;
}

static void
insert_sorted_node(rig_path_t *path, rig_node_t *node)
{
    rig_node_t *insertion_point;

    c_list_for_each(insertion_point, &path->nodes, list_node)
    if (insertion_point->t >= node->t)
        break;

    /* If no node was found then insertion_point will be pointing to an
     * imaginary node containing the list head which is what we want
     * anyway */

    c_list_insert(insertion_point->list_node.prev, &node->list_node);

    path->length++;
}

void
rig_path_insert_node(rig_path_t *path, rig_node_t *node)
{
    c_return_if_fail(rig_path_find_node(path, node->t) == NULL);

    insert_sorted_node(path, node);
    notify_node_added(path, node);
}

void
rig_path_insert_float(rig_path_t *path, float t, float value)
{
    rig_node_t *node = rig_path_find_node(path, t);

    if (node) {
        node->boxed.d.float_val = value;
        notify_node_modified(path, node);
    } else {
        node = rig_node_new_for_float(t, value);
        insert_sorted_node(path, node);
        notify_node_added(path, node);
    }
}

void
rig_path_insert_vec3(rig_path_t *path, float t, const float value[3])
{
    rig_node_t *node = rig_path_find_node(path, t);

    if (node) {
        memcpy(node->boxed.d.vec3_val, value, sizeof(float) * 3);
        notify_node_modified(path, node);
    } else {
        node = rig_node_new_for_vec3(t, value);
        insert_sorted_node(path, node);
        notify_node_added(path, node);
    }
}

void
rig_path_insert_vec4(rig_path_t *path, float t, const float value[4])
{
    rig_node_t *node = rig_path_find_node(path, t);

    if (node) {
        memcpy(node->boxed.d.vec4_val, value, sizeof(float) * 4);
        notify_node_modified(path, node);
    } else {
        node = rig_node_new_for_vec4(t, value);
        insert_sorted_node(path, node);
        notify_node_added(path, node);
    }
}

void
rig_path_insert_quaternion(rig_path_t *path,
                           float t,
                           const c_quaternion_t *value)
{
    rig_node_t *node = rig_path_find_node(path, t);

    if (node) {
        node->boxed.d.quaternion_val = *value;
        notify_node_modified(path, node);
    } else {
        node = rig_node_new_for_quaternion(t, value);
        insert_sorted_node(path, node);
        notify_node_added(path, node);
    }
}

void
rig_path_insert_double(rig_path_t *path, float t, double value)
{
    rig_node_t *node = rig_path_find_node(path, t);

    if (node) {
        node->boxed.d.double_val = value;
        notify_node_modified(path, node);
    } else {
        node = rig_node_new_for_double(t, value);
        insert_sorted_node(path, node);
        notify_node_added(path, node);
    }
}

void
rig_path_insert_integer(rig_path_t *path, float t, int value)
{
    rig_node_t *node = rig_path_find_node(path, t);

    if (node) {
        node->boxed.d.integer_val = value;
        notify_node_modified(path, node);
    } else {
        node = rig_node_new_for_integer(t, value);
        insert_sorted_node(path, node);
        notify_node_added(path, node);
    }
}

void
rig_path_insert_uint32(rig_path_t *path, float t, uint32_t value)
{
    rig_node_t *node = rig_path_find_node(path, t);

    if (node) {
        node->boxed.d.uint32_val = value;
        notify_node_modified(path, node);
    } else {
        node = rig_node_new_for_uint32(t, value);
        insert_sorted_node(path, node);
        notify_node_added(path, node);
    }
}

void
rig_path_insert_color(rig_path_t *path, float t, const cg_color_t *value)
{
    rig_node_t *node = rig_path_find_node(path, t);

    if (node) {
        node->boxed.d.color_val = *value;
        notify_node_modified(path, node);
    } else {
        node = rig_node_new_for_color(t, value);
        insert_sorted_node(path, node);
        notify_node_added(path, node);
    }
}

void
rig_path_insert_enum(rig_path_t *path, float t, int value)
{
    rig_node_t *node = rig_path_find_node(path, t);

    if (node) {
        node->boxed.d.enum_val = value;
        notify_node_modified(path, node);
    } else {
        node = rig_node_new_for_enum(t, value);
        insert_sorted_node(path, node);
        notify_node_added(path, node);
    }
}

void
rig_path_insert_boolean(rig_path_t *path, float t, bool value)
{
    rig_node_t *node = rig_path_find_node(path, t);

    if (node) {
        node->boxed.d.boolean_val = value;
        notify_node_modified(path, node);
    } else {
        node = rig_node_new_for_boolean(t, value);
        insert_sorted_node(path, node);
        notify_node_added(path, node);
    }
}

void
rig_path_insert_text(rig_path_t *path, float t, const char *value)
{
    rig_node_t *node = rig_path_find_node(path, t);

    if (node) {
        if (node->boxed.d.text_val)
            c_free(node->boxed.d.text_val);
        node->boxed.d.text_val = c_strdup(value);
        notify_node_modified(path, node);
    } else {
        node = rig_node_new_for_text(t, value);
        insert_sorted_node(path, node);
        notify_node_added(path, node);
    }
}

void
rig_path_insert_asset(rig_path_t *path, float t, rig_asset_t *value)
{
    rig_node_t *node = rig_path_find_node(path, t);

    if (node) {
        if (node->boxed.d.asset_val)
            rut_object_unref(node->boxed.d.asset_val);
        node->boxed.d.asset_val = rut_object_ref(value);
        notify_node_modified(path, node);
    } else {
        node = rig_node_new_for_asset(t, value);
        insert_sorted_node(path, node);
        notify_node_added(path, node);
    }
}

void
rig_path_insert_object(rig_path_t *path, float t, rut_object_t *value)
{
    rig_node_t *node = rig_path_find_node(path, t);

    if (node) {
        if (node->boxed.d.object_val)
            rut_object_unref(node->boxed.d.object_val);
        node->boxed.d.object_val = rut_object_ref(value);
        notify_node_modified(path, node);
    } else {
        node = rig_node_new_for_object(t, value);
        insert_sorted_node(path, node);
        notify_node_added(path, node);
    }
}

bool
rig_path_lerp_property(rig_path_t *path, rig_property_t *property, float t)
{
    rig_node_t *n0, *n1;

    c_return_val_if_fail(property->spec->type == path->type, false);

    if (!rig_path_find_control_points2(
            path, t, RIG_PATH_DIRECTION_FORWARDS, &n0, &n1))
        return false;

    switch (path->type) {
    case RUT_PROPERTY_TYPE_FLOAT: {
        float value;

        rig_node_float_lerp(n0, n1, t, &value);
        rig_property_set_float(&path->shell->property_ctx, property, value);
        break;
    }
    case RUT_PROPERTY_TYPE_DOUBLE: {
        double value;

        rig_node_double_lerp(n0, n1, t, &value);
        rig_property_set_double(&path->shell->property_ctx, property, value);
        break;
    }
    case RUT_PROPERTY_TYPE_INTEGER: {
        int value;

        rig_node_integer_lerp(n0, n1, t, &value);
        rig_property_set_integer(&path->shell->property_ctx, property, value);
        break;
    }
    case RUT_PROPERTY_TYPE_UINT32: {
        uint32_t value;

        rig_node_uint32_lerp(n0, n1, t, &value);
        rig_property_set_uint32(&path->shell->property_ctx, property, value);
        break;
    }
    case RUT_PROPERTY_TYPE_VEC3: {
        float value[3];
        rig_node_vec3_lerp(n0, n1, t, value);
        rig_property_set_vec3(&path->shell->property_ctx, property, value);
        break;
    }
    case RUT_PROPERTY_TYPE_VEC4: {
        float value[4];
        rig_node_vec4_lerp(n0, n1, t, value);
        rig_property_set_vec4(&path->shell->property_ctx, property, value);
        break;
    }
    case RUT_PROPERTY_TYPE_COLOR: {
        cg_color_t value;
        rig_node_color_lerp(n0, n1, t, &value);
        rig_property_set_color(&path->shell->property_ctx, property, &value);
        break;
    }
    case RUT_PROPERTY_TYPE_QUATERNION: {
        c_quaternion_t value;
        rig_node_quaternion_lerp(n0, n1, t, &value);
        rig_property_set_quaternion(&path->shell->property_ctx, property, &value);
        break;
    }

    /* These types of properties can't be interoplated so they
     * probably shouldn't end up in a path */
    case RUT_PROPERTY_TYPE_ENUM: {
        int value;
        rig_node_enum_lerp(n0, n1, t, &value);
        rig_property_set_enum(&path->shell->property_ctx, property, value);
        break;
    }
    case RUT_PROPERTY_TYPE_BOOLEAN: {
        bool value;
        rig_node_boolean_lerp(n0, n1, t, &value);
        rig_property_set_boolean(&path->shell->property_ctx, property, value);
        break;
    }
    case RUT_PROPERTY_TYPE_TEXT: {
        const char *value;
        rig_node_text_lerp(n0, n1, t, &value);
        rig_property_set_text(&path->shell->property_ctx, property, value);
        break;
    }
    case RUT_PROPERTY_TYPE_ASSET: {
        rig_asset_t *value;
        rig_node_asset_lerp(n0, n1, t, &value);
        rig_property_set_asset(&path->shell->property_ctx, property, value);
        break;
    }
    case RUT_PROPERTY_TYPE_OBJECT: {
        rut_object_t *value;
        rig_node_object_lerp(n0, n1, t, &value);
        rig_property_set_object(&path->shell->property_ctx, property, value);
        break;
    }
    case RUT_PROPERTY_TYPE_POINTER:
        c_warn_if_reached();
        break;
    }

    return true;
}

bool
rig_path_get_boxed(rig_path_t *path, float t, rut_boxed_t *value)
{
    rig_node_t *node;

    node = rig_path_find_node(path, t);

    if (node == NULL)
        return false;

    return rig_node_box(path->type, node, value);
}

void
rig_path_insert_boxed(rig_path_t *path, float t, const rut_boxed_t *value)
{
    c_return_if_fail(value->type == path->type);

    switch (path->type) {
    case RUT_PROPERTY_TYPE_FLOAT:
        rig_path_insert_float(path, t, value->d.float_val);
        return;

    case RUT_PROPERTY_TYPE_DOUBLE:
        rig_path_insert_double(path, t, value->d.double_val);
        return;

    case RUT_PROPERTY_TYPE_INTEGER:
        rig_path_insert_integer(path, t, value->d.integer_val);
        return;

    case RUT_PROPERTY_TYPE_UINT32:
        rig_path_insert_uint32(path, t, value->d.uint32_val);
        return;

    case RUT_PROPERTY_TYPE_VEC3:
        rig_path_insert_vec3(path, t, value->d.vec3_val);
        return;

    case RUT_PROPERTY_TYPE_VEC4:
        rig_path_insert_vec4(path, t, value->d.vec4_val);
        return;

    case RUT_PROPERTY_TYPE_COLOR:
        rig_path_insert_color(path, t, &value->d.color_val);
        return;

    case RUT_PROPERTY_TYPE_QUATERNION:
        rig_path_insert_quaternion(path, t, &value->d.quaternion_val);
        return;

    case RUT_PROPERTY_TYPE_ENUM:
        rig_path_insert_enum(path, t, value->d.enum_val);
        return;

    case RUT_PROPERTY_TYPE_BOOLEAN:
        rig_path_insert_boolean(path, t, value->d.boolean_val);
        return;

    case RUT_PROPERTY_TYPE_TEXT:
        rig_path_insert_text(path, t, value->d.text_val);
        return;

    case RUT_PROPERTY_TYPE_ASSET:
        rig_path_insert_asset(path, t, value->d.asset_val);
        return;

    case RUT_PROPERTY_TYPE_OBJECT:
        rig_path_insert_object(path, t, value->d.object_val);
        return;

    case RUT_PROPERTY_TYPE_POINTER:
        break;
    }

    c_warn_if_reached();
}

void
rig_path_remove(rig_path_t *path, float t)
{
    rig_node_t *node;

    node = rig_path_find_node(path, t);

    if (node)
        rig_path_remove_node(path, node);
}

void
rig_path_remove_node(rig_path_t *path, rig_node_t *node)
{
    rut_closure_list_invoke(&path->operation_cb_list,
                            rig_path_operation_callback_t,
                            path,
                            RIG_PATH_OPERATION_REMOVED,
                            node);
    c_list_remove(&node->list_node);
    rig_node_free(node);
    path->length--;

    if (path->pos == node)
        path->pos = NULL;
}

rut_closure_t *
rig_path_add_operation_callback(rig_path_t *path,
                                rig_path_operation_callback_t callback,
                                void *user_data,
                                rut_closure_destroy_callback_t destroy_cb)
{
    return rut_closure_list_add_FIXME(
        &path->operation_cb_list, callback, user_data, destroy_cb);
}

void
rut_path_foreach_node(rig_path_t *path,
                      rig_path_node_callback_t callback,
                      void *user_data)
{
    rig_node_t *node;

    c_list_for_each(node, &path->nodes, list_node)
    callback(node, user_data);
}
