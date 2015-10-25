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

/* A few notes about how journal operations are applied:
 *
 * Applying journal operations will result in queuing lower-level
 * engine operations. Engine operations are then used to apply changes
 * to the edit-mode and play-mode uis; can be forwarded to the
 * simulator process and forwarded to all slave devices.
 *
 * When applying journal operations, we immediately apply the
 * corresponding engine operations to the edit-mode ui state. If we
 * didn't then it would be difficult to batch edit operations that
 * depend on each other.
 */

#include <rig-config.h>

#include "rig-undo-journal.h"
#include "rig-engine.h"
#include "rig-pb.h"

#include "rig.pb-c.h"

typedef struct _undo_redo_op_impl_t {
    void (*apply)(rig_undo_journal_t *journal, undo_redo_t *undo_redo);
    undo_redo_t *(*invert)(undo_redo_t *src);
    void (*free)(undo_redo_t *undo_redo);
} undo_redo_op_impl_t;

static void rig_undo_journal_insert(rig_undo_journal_t *journal,
                                    undo_redo_t *undo_redo);

static undo_redo_t *rig_undo_journal_revert(rig_undo_journal_t *journal);

static void undo_redo_apply(rig_undo_journal_t *journal,
                            undo_redo_t *undo_redo);

static undo_redo_t *undo_redo_invert(undo_redo_t *undo_redo);

static void undo_redo_free(undo_redo_t *undo_redo);

static void dump_journal(rig_undo_journal_t *journal, int indent);

static void
dump_op(undo_redo_t *op, int indent)
{
    switch (op->op) {
    case UNDO_REDO_SET_PROPERTY_OP: {
        char *v0_string = rut_boxed_to_string(
            &op->d.set_property.value0, op->d.set_property.property->spec);
        char *v1_string = rut_boxed_to_string(
            &op->d.set_property.value1, op->d.set_property.property->spec);

        c_debug("%*sproperty (\"%s\") change: %s → %s\n",
                indent,
                "",
                op->d.set_property.property->spec->name,
                v0_string,
                v1_string);

        c_free(v0_string);
        c_free(v1_string);
        break;
    }
    case UNDO_REDO_CONST_PROPERTY_CHANGE_OP: {
        char *v0_string =
            rut_boxed_to_string(&op->d.set_controller_const.value0,
                                op->d.set_controller_const.property->spec);
        char *v1_string =
            rut_boxed_to_string(&op->d.set_controller_const.value1,
                                op->d.set_controller_const.property->spec);

        c_debug(
            "%*scontroller (\"%s\") const property (\"%s\") change: %s → %s\n",
            indent,
            "",
            op->d.set_controller_const.controller->label,
            op->d.set_controller_const.property->spec->name,
            v0_string,
            v1_string);
        break;
    }
    case UNDO_REDO_SET_CONTROLLED_OP:
        c_debug("%*scontrolled=%s\n",
                indent,
                "",
                op->d.set_controlled.value ? "yes" : "no");
        break;

    case UNDO_REDO_PATH_ADD_OP:
        c_debug("%*spath add\n", indent, "");
        break;
    case UNDO_REDO_PATH_MODIFY_OP:
        c_debug("%*spath modify\n", indent, "");
        break;
    case UNDO_REDO_PATH_REMOVE_OP:
        c_debug("%*sremove path\n", indent, "");
        break;
    case UNDO_REDO_SET_CONTROL_METHOD_OP:
        c_debug("%*sset control method\n", indent, "");
        break;
    case UNDO_REDO_ADD_ENTITY_OP:
        c_debug("%*sadd entity\n", indent, "");
        break;
    case UNDO_REDO_DELETE_ENTITY_OP:
        c_debug("%*sdelete entity\n", indent, "");
        break;
    case UNDO_REDO_ADD_COMPONENT_OP:
        c_debug("%*sadd component\n", indent, "");
        break;
    case UNDO_REDO_DELETE_COMPONENT_OP:
        c_debug("%*sdelete component\n", indent, "");
        break;
    case UNDO_REDO_SUBJOURNAL_OP:
        c_debug("%*ssub-journal %p\n", indent, "", op->d.subjournal);
        dump_journal(op->d.subjournal, indent + 5);
        break;
    default:
        c_debug("%*sTODO\n", indent, "");
        break;
    }
}

static void
dump_journal(rig_undo_journal_t *journal, int indent)
{
    undo_redo_t *undo_redo;

    c_debug("\n\n%*sJournal %p\n", indent, "", journal);
    indent += 2;

    if (!c_list_empty(&journal->redo_ops)) {
        c_list_for_each(undo_redo, &journal->redo_ops, list_node)
        dump_op(undo_redo, indent);

        c_debug("%*s %25s REDO OPS\n", indent, "", "");
        c_debug("%*s %25s <-----\n", indent, "", "");
        c_debug("%*s %25s UNDO OPS\n", indent, "", "");
    }

    c_list_for_each_reverse(undo_redo, &journal->undo_ops, list_node)
    dump_op(undo_redo, indent);
}

static undo_redo_t *
revert_recent_controller_constant_change(rig_undo_journal_t *journal,
                                         rig_controller_t *controller,
                                         rut_property_t *property)
{
    if (!c_list_empty(&journal->undo_ops)) {
        undo_redo_t *last_op =
            rut_container_of(journal->undo_ops.prev, last_op, list_node);

        if (last_op->op == UNDO_REDO_CONST_PROPERTY_CHANGE_OP &&
            last_op->d.set_controller_const.controller == controller &&
            last_op->d.set_controller_const.property == property &&
            last_op->mergable) {
            return rig_undo_journal_revert(journal);
        }
    }

    return NULL;
}

void
rig_undo_journal_set_controller_constant(rig_undo_journal_t *journal,
                                         bool mergable,
                                         rig_controller_t *controller,
                                         const rut_boxed_t *value,
                                         rut_property_t *property)
{
    undo_redo_t *undo_redo;
    undo_redo_set_controller_const_t *prop_change;
    rig_controller_prop_data_t *prop_data =
        rig_controller_find_prop_data_for_property(controller, property);

    c_return_if_fail(prop_data != NULL);

    /* If we have a mergable entry then we can just update the final value */
    if (mergable && (undo_redo = revert_recent_controller_constant_change(
                         journal, controller, property))) {
        prop_change = &undo_redo->d.set_controller_const;
        rut_boxed_destroy(&prop_change->value1);
        rut_boxed_copy(&prop_change->value1, value);
    } else {
        undo_redo = c_slice_new(undo_redo_t);

        undo_redo->op = UNDO_REDO_CONST_PROPERTY_CHANGE_OP;
        undo_redo->mergable = mergable;

        prop_change = &undo_redo->d.set_controller_const;
        prop_change->controller = rut_object_ref(controller);

        rut_boxed_copy(&prop_change->value0, &prop_data->constant_value);
        rut_boxed_copy(&prop_change->value1, value);

        prop_change->object = rut_object_ref(property->object);
        prop_change->property = property;
    }

    rig_undo_journal_insert(journal, undo_redo);
}

static undo_redo_t *
revert_recent_controller_path_change(rig_undo_journal_t *journal,
                                     rig_controller_t *controller,
                                     float t,
                                     rut_property_t *property)
{
    if (!c_list_empty(&journal->undo_ops)) {
        undo_redo_t *last_op =
            rut_container_of(journal->undo_ops.prev, last_op, list_node);

        if (last_op->op == UNDO_REDO_PATH_ADD_OP &&
            last_op->d.path_add_remove.controller == controller &&
            last_op->d.path_add_remove.property == property &&
            last_op->d.path_add_remove.t == t && last_op->mergable) {
            return rig_undo_journal_revert(journal);
        }

        if (last_op->op == UNDO_REDO_PATH_MODIFY_OP &&
            last_op->d.path_add_remove.controller == controller &&
            last_op->d.path_modify.property == property &&
            last_op->d.path_modify.t == t && last_op->mergable) {
            return rig_undo_journal_revert(journal);
        }
    }

    return NULL;
}

void
rig_undo_journal_set_controller_path_node_value(rig_undo_journal_t *journal,
                                                bool mergable,
                                                rig_controller_t *controller,
                                                float t,
                                                const rut_boxed_t *value,
                                                rut_property_t *property)
{
    undo_redo_t *undo_redo;
    rig_path_t *path =
        rig_controller_get_path_for_property(controller, property);

    /* If we have a mergable entry then we can just update the final value */
    if (mergable && (undo_redo = revert_recent_controller_path_change(
                         journal, controller, t, property))) {
        if (undo_redo->op == UNDO_REDO_PATH_ADD_OP) {
            undo_redo_path_add_remove_t *add_remove =
                &undo_redo->d.path_add_remove;

            rut_boxed_destroy(&add_remove->value);
            rut_boxed_copy(&add_remove->value, value);
        } else {
            undo_redo_path_modify_t *modify = &undo_redo->d.path_modify;

            rut_boxed_destroy(&modify->value1);
            rut_boxed_copy(&modify->value1, value);
        }
    } else {
        rut_boxed_t old_value;
        float normalized_t = t / rig_controller_get_length(controller);

        undo_redo = c_slice_new(undo_redo_t);

        if (rig_path_get_boxed(path, normalized_t, &old_value)) {
            undo_redo_path_modify_t *modify = &undo_redo->d.path_modify;

            undo_redo->op = UNDO_REDO_PATH_MODIFY_OP;
            modify->controller = rut_object_ref(controller);
            modify->object = rut_object_ref(property->object);
            modify->property = property;
            modify->t = t;
            modify->value0 = old_value;
            rut_boxed_copy(&modify->value1, value);
        } else {
            undo_redo_path_add_remove_t *add_remove =
                &undo_redo->d.path_add_remove;

            undo_redo->op = UNDO_REDO_PATH_ADD_OP;
            add_remove->controller = rut_object_ref(controller);
            add_remove->object = rut_object_ref(property->object);
            add_remove->property = property;
            add_remove->t = t;
            rut_boxed_copy(&add_remove->value, value);
            add_remove->have_value = true;
        }

        undo_redo->mergable = mergable;
    }

    rig_undo_journal_insert(journal, undo_redo);
}

void
rig_undo_journal_remove_controller_path_node(rig_undo_journal_t *journal,
                                             rig_controller_t *controller,
                                             rut_property_t *property,
                                             float t)
{
    undo_redo_path_add_remove_t *add_remove;
    undo_redo_t *undo_redo = c_slice_new(undo_redo_t);

    add_remove = &undo_redo->d.path_add_remove;

    undo_redo->op = UNDO_REDO_PATH_REMOVE_OP;
    undo_redo->mergable = false;
    add_remove->controller = rut_object_ref(controller);
    add_remove->object = rut_object_ref(property->object);
    add_remove->property = property;
    add_remove->t = t;
    add_remove->have_value = false;

    rig_undo_journal_insert(journal, undo_redo);
}

void
rig_undo_journal_set_controlled(rig_undo_journal_t *journal,
                                rig_controller_t *controller,
                                rut_property_t *property,
                                bool value)
{
    undo_redo_t *undo_redo;
    undo_redo_set_controlled_t *set_controlled;

    undo_redo = c_slice_new(undo_redo_t);
    undo_redo->op = UNDO_REDO_SET_CONTROLLED_OP;
    undo_redo->mergable = false;

    set_controlled = &undo_redo->d.set_controlled;

    set_controlled->controller = rut_object_ref(controller);
    set_controlled->object = rut_object_ref(property->object);
    set_controlled->property = property;
    set_controlled->value = value;

    rig_undo_journal_insert(journal, undo_redo);
}

void
rig_undo_journal_set_control_method(rig_undo_journal_t *journal,
                                    rig_controller_t *controller,
                                    rut_property_t *property,
                                    rig_controller_method_t method)
{
    undo_redo_t *undo_redo;
    undo_redo_set_control_method_t *set_control_method;
    rig_controller_prop_data_t *prop_data =
        rig_controller_find_prop_data_for_property(controller, property);

    c_return_if_fail(prop_data != NULL);

    undo_redo = c_slice_new(undo_redo_t);
    undo_redo->op = UNDO_REDO_SET_CONTROL_METHOD_OP;
    undo_redo->mergable = false;

    set_control_method = &undo_redo->d.set_control_method;

    set_control_method->controller = rut_object_ref(controller);
    set_control_method->object = rut_object_ref(property->object);
    set_control_method->property = property;
    set_control_method->prev_method = prop_data->method;
    set_control_method->method = method;

    rig_undo_journal_insert(journal, undo_redo);
}

static undo_redo_t *
revert_recent_property_change(rig_undo_journal_t *journal,
                              rut_property_t *property)
{
    if (!c_list_empty(&journal->undo_ops)) {
        undo_redo_t *last_op =
            rut_container_of(journal->undo_ops.prev, last_op, list_node);

        if (last_op->op == UNDO_REDO_SET_PROPERTY_OP &&
            last_op->d.set_property.property == property && last_op->mergable) {
            return rig_undo_journal_revert(journal);
        }
    }

    return NULL;
}

void
rig_undo_journal_set_property(rig_undo_journal_t *journal,
                              bool mergable,
                              const rut_boxed_t *value,
                              rut_property_t *property)
{
    undo_redo_t *undo_redo;
    undo_redo_set_property_t *set_property;

    /* If we have a mergable entry then we can just update the final value */
    if (mergable &&
        (undo_redo = revert_recent_property_change(journal, property))) {
        set_property = &undo_redo->d.set_property;
        rut_boxed_destroy(&set_property->value1);
        rut_boxed_copy(&set_property->value1, value);
    } else {
        undo_redo = c_slice_new(undo_redo_t);

        undo_redo->op = UNDO_REDO_SET_PROPERTY_OP;
        undo_redo->mergable = mergable;

        set_property = &undo_redo->d.set_property;

        rut_property_box(property, &set_property->value0);
        rut_boxed_copy(&set_property->value1, value);

        set_property->object = rut_object_ref(property->object);
        set_property->property = property;
    }

    rig_undo_journal_insert(journal, undo_redo);
}

void
rig_undo_journal_add_entity(rig_undo_journal_t *journal,
                            rig_entity_t *parent_entity,
                            rig_entity_t *entity)
{
    undo_redo_t *undo_redo;
    undo_redo_add_delete_entity_t *add_entity;

    undo_redo = c_slice_new(undo_redo_t);
    undo_redo->op = UNDO_REDO_ADD_ENTITY_OP;
    undo_redo->mergable = false;

    add_entity = &undo_redo->d.add_delete_entity;

    add_entity->parent_entity = rut_object_ref(parent_entity);
    add_entity->deleted_entity = rut_object_ref(entity);

    /* We assume there aren't currently any controller references to
     * this entity. */
    c_list_init(&add_entity->controller_properties);
    add_entity->saved_controller_properties = true;

    rig_undo_journal_insert(journal, undo_redo);
}

static bool
delete_entity_component_cb(rut_object_t *component, void *user_data)
{
    rig_undo_journal_t *journal = user_data;
    rig_undo_journal_delete_component(journal, component);

    return true; /* continue */
}

void
rig_undo_journal_delete_entity(rig_undo_journal_t *journal,
                               rig_entity_t *entity)
{
    rig_undo_journal_t *sub_journal = rig_undo_journal_new(journal->editor);
    undo_redo_t *undo_redo;
    undo_redo_add_delete_entity_t *delete_entity;
    rig_entity_t *parent = rut_graphable_get_parent(entity);

    rig_entity_foreach_component_safe(
        entity, delete_entity_component_cb, sub_journal);

    undo_redo = c_slice_new(undo_redo_t);
    undo_redo->op = UNDO_REDO_DELETE_ENTITY_OP;
    undo_redo->mergable = false;

    delete_entity = &undo_redo->d.add_delete_entity;

    delete_entity->parent_entity = rut_object_ref(parent);
    delete_entity->deleted_entity = rut_object_ref(entity);

    delete_entity->saved_controller_properties = false;

    rig_undo_journal_insert(sub_journal, undo_redo);
    rig_undo_journal_log_subjournal(journal, sub_journal);
}

void
rig_undo_journal_add_component(rig_undo_journal_t *journal,
                               rig_entity_t *entity,
                               rut_object_t *component)
{
    undo_redo_t *undo_redo;
    undo_redo_add_delete_component_t *add_component;

    undo_redo = c_slice_new(undo_redo_t);
    undo_redo->op = UNDO_REDO_ADD_COMPONENT_OP;
    undo_redo->mergable = false;

    add_component = &undo_redo->d.add_delete_component;

    add_component->parent_entity = rut_object_ref(entity);
    add_component->deleted_component = rut_object_ref(component);

    /* We assume there are no controller references to the entity
     * currently */
    c_list_init(&add_component->controller_properties);
    add_component->saved_controller_properties = true;

    rig_undo_journal_insert(journal, undo_redo);
}

void
rig_undo_journal_delete_component(rig_undo_journal_t *journal,
                                  rut_object_t *component)
{
    undo_redo_t *undo_redo;
    undo_redo_add_delete_component_t *delete_component;
    rut_componentable_props_t *componentable =
        rut_object_get_properties(component, RUT_TRAIT_ID_COMPONENTABLE);
    rig_entity_t *entity = componentable->entity;

    undo_redo = c_slice_new(undo_redo_t);
    undo_redo->op = UNDO_REDO_DELETE_COMPONENT_OP;
    undo_redo->mergable = false;

    delete_component = &undo_redo->d.add_delete_component;

    delete_component->parent_entity = rut_object_ref(entity);
    delete_component->deleted_component = rut_object_ref(component);

    delete_component->saved_controller_properties = false;

    rig_undo_journal_insert(journal, undo_redo);
}

void
rig_undo_journal_log_add_controller(rig_undo_journal_t *journal,
                                    rig_controller_t *controller)
{
    undo_redo_t *undo_redo = c_slice_new0(undo_redo_t);
    undo_redo_add_remove_controller_t *add_controller =
        &undo_redo->d.add_remove_controller;

    undo_redo->op = UNDO_REDO_ADD_CONTROLLER_OP;
    undo_redo->mergable = false;

    add_controller = &undo_redo->d.add_remove_controller;

    add_controller->controller = rut_object_ref(controller);

    c_warn_if_fail(rig_controller_get_active(controller) == false);

    /* We assume there are no controller references to this controller
     * currently */
    c_list_init(&add_controller->controller_properties);
    add_controller->saved_controller_properties = true;

    rig_undo_journal_insert(journal, undo_redo);
}

void
rig_undo_journal_log_remove_controller(rig_undo_journal_t *journal,
                                       rig_controller_t *controller)
{
    undo_redo_t *undo_redo = c_slice_new0(undo_redo_t);
    undo_redo_add_remove_controller_t *remove_controller =
        &undo_redo->d.add_remove_controller;

    undo_redo->op = UNDO_REDO_REMOVE_CONTROLLER_OP;
    undo_redo->mergable = false;

    remove_controller->controller = rut_object_ref(controller);

    remove_controller->saved_controller_properties = false;

    rig_undo_journal_insert(journal, undo_redo);
}

void
rig_undo_journal_log_subjournal(rig_undo_journal_t *journal,
                                rig_undo_journal_t *subjournal)
{
    undo_redo_t *undo_redo;

    /* It indicates a programming error to be logging a subjournal with
     * ::apply_on_insert enabled into a journal with ::apply_on_insert
     * disabled. */

    c_return_if_fail(journal->apply_on_insert == true ||
                     subjournal->apply_on_insert == false);

    undo_redo = c_slice_new(undo_redo_t);
    undo_redo->op = UNDO_REDO_SUBJOURNAL_OP;
    undo_redo->mergable = false;

    undo_redo->d.subjournal = subjournal;

    rig_undo_journal_insert(journal, undo_redo);
}

static void
undo_redo_subjournal_apply(rig_undo_journal_t *journal,
                           undo_redo_t *undo_redo)
{
    rig_undo_journal_t *subjournal = undo_redo->d.subjournal;

    c_list_for_each(undo_redo, &subjournal->undo_ops, list_node)
    undo_redo_apply(journal, undo_redo);
}

static undo_redo_t *
undo_redo_subjournal_invert(undo_redo_t *undo_redo_src)
{
    rig_undo_journal_t *subjournal_src = undo_redo_src->d.subjournal;
    rig_undo_journal_t *subjournal_dst;
    undo_redo_t *inverse, *sub_undo_redo;

    subjournal_dst = rig_undo_journal_new(subjournal_src->editor);

    c_list_for_each(sub_undo_redo, &subjournal_src->undo_ops, list_node)
    {
        undo_redo_t *subinverse = undo_redo_invert(sub_undo_redo);
        /* Insert at the beginning so that the list will end up in the
         * reverse order */
        c_list_insert(&subjournal_dst->undo_ops, &subinverse->list_node);
    }

    inverse = c_slice_dup(undo_redo_t, undo_redo_src);
    inverse->d.subjournal = subjournal_dst;

    return inverse;
}

static void
undo_redo_subjournal_free(undo_redo_t *undo_redo)
{
    rig_undo_journal_free(undo_redo->d.subjournal);
    c_slice_free(undo_redo_t, undo_redo);
}

static void
undo_redo_set_property_apply(rig_undo_journal_t *journal,
                             undo_redo_t *undo_redo)
{
    undo_redo_set_property_t *set_property = &undo_redo->d.set_property;
    rig_engine_t *engine = journal->engine;

    rig_engine_op_set_property(
        engine, set_property->property, &set_property->value1);
}

static undo_redo_t *
undo_redo_set_property_invert(undo_redo_t *undo_redo_src)
{
    undo_redo_set_property_t *src = &undo_redo_src->d.set_property;
    undo_redo_t *undo_redo_inverse = c_slice_new(undo_redo_t);
    undo_redo_set_property_t *inverse = &undo_redo_inverse->d.set_property;

    undo_redo_inverse->op = undo_redo_src->op;
    undo_redo_inverse->mergable = false;

    inverse->object = rut_object_ref(src->object);
    inverse->property = src->property;
    inverse->value0 = src->value1;
    inverse->value1 = src->value0;

    return undo_redo_inverse;
}

static void
undo_redo_set_property_free(undo_redo_t *undo_redo)
{
    undo_redo_set_property_t *set_property = &undo_redo->d.set_property;
    rut_object_unref(set_property->object);
    rut_boxed_destroy(&set_property->value0);
    rut_boxed_destroy(&set_property->value1);
    c_slice_free(undo_redo_t, undo_redo);
}

static void
undo_redo_set_controller_const_apply(rig_undo_journal_t *journal,
                                     undo_redo_t *undo_redo)
{
    undo_redo_set_controller_const_t *set_controller_const =
        &undo_redo->d.set_controller_const;

    rig_engine_op_controller_set_const(journal->engine,
                                       set_controller_const->controller,
                                       set_controller_const->property,
                                       &set_controller_const->value1);

    rig_reload_inspector_property(journal->editor,
                                  set_controller_const->property);
}

static undo_redo_t *
undo_redo_set_controller_const_invert(undo_redo_t *undo_redo_src)
{
    undo_redo_set_controller_const_t *src =
        &undo_redo_src->d.set_controller_const;
    undo_redo_t *undo_redo_inverse = c_slice_new(undo_redo_t);
    undo_redo_set_controller_const_t *inverse =
        &undo_redo_inverse->d.set_controller_const;

    undo_redo_inverse->op = undo_redo_src->op;
    undo_redo_inverse->mergable = false;

    inverse->controller = rut_object_ref(src->controller);
    inverse->object = rut_object_ref(src->object);
    inverse->property = src->property;
    inverse->value0 = src->value1;
    inverse->value1 = src->value0;

    return undo_redo_inverse;
}

static void
undo_redo_set_controller_const_free(undo_redo_t *undo_redo)
{
    undo_redo_set_controller_const_t *set_controller_const =
        &undo_redo->d.set_controller_const;
    rut_object_unref(set_controller_const->object);
    rut_object_unref(set_controller_const->controller);
    rut_boxed_destroy(&set_controller_const->value0);
    rut_boxed_destroy(&set_controller_const->value1);
    c_slice_free(undo_redo_t, undo_redo);
}

static void
undo_redo_path_add_apply(rig_undo_journal_t *journal,
                         undo_redo_t *undo_redo)
{
    undo_redo_path_add_remove_t *add_remove = &undo_redo->d.path_add_remove;
    rig_engine_t *engine = journal->engine;

    c_return_if_fail(add_remove->have_value == true);

    rig_engine_op_controller_path_add_node(engine,
                                           add_remove->controller,
                                           add_remove->property,
                                           add_remove->t,
                                           &add_remove->value);

    rig_reload_inspector_property(journal->editor, add_remove->property);
}

static undo_redo_t *
undo_redo_path_add_invert(undo_redo_t *undo_redo_src)
{
    undo_redo_t *inverse = c_slice_dup(undo_redo_t, undo_redo_src);

    inverse->op = UNDO_REDO_PATH_REMOVE_OP;
    inverse->d.path_add_remove.have_value =
        undo_redo_src->d.path_add_remove.have_value;
    if (inverse->d.path_add_remove.have_value) {
        rut_boxed_copy(&inverse->d.path_add_remove.value,
                       &undo_redo_src->d.path_add_remove.value);
    }
    rut_object_ref(inverse->d.path_add_remove.object);
    rut_object_ref(inverse->d.path_add_remove.controller);

    return inverse;
}

static void
undo_redo_path_remove_apply(rig_undo_journal_t *journal,
                            undo_redo_t *undo_redo)
{
    undo_redo_path_add_remove_t *add_remove = &undo_redo->d.path_add_remove;
    rig_engine_t *engine = journal->engine;

    if (!add_remove->have_value) {
        rig_controller_box_path_value(add_remove->controller,
                                      add_remove->property,
                                      add_remove->t,
                                      &add_remove->value);

        add_remove->have_value = true;
    }

    rig_engine_op_controller_path_delete_node(
        engine, add_remove->controller, add_remove->property, add_remove->t);

    rig_reload_inspector_property(journal->editor, add_remove->property);
}

static undo_redo_t *
undo_redo_path_remove_invert(undo_redo_t *undo_redo_src)
{
    undo_redo_t *inverse = c_slice_dup(undo_redo_t, undo_redo_src);

    inverse->op = UNDO_REDO_PATH_ADD_OP;
    inverse->d.path_add_remove.have_value =
        undo_redo_src->d.path_add_remove.have_value;
    if (inverse->d.path_add_remove.have_value) {
        rut_boxed_copy(&inverse->d.path_add_remove.value,
                       &undo_redo_src->d.path_add_remove.value);
    }
    rut_object_ref(inverse->d.path_add_remove.object);
    rut_object_ref(inverse->d.path_add_remove.controller);

    return inverse;
}

static void
undo_redo_path_add_remove_free(undo_redo_t *undo_redo)
{
    undo_redo_path_add_remove_t *add_remove = &undo_redo->d.path_add_remove;
    if (add_remove->have_value)
        rut_boxed_destroy(&add_remove->value);
    rut_object_unref(add_remove->object);
    rut_object_unref(add_remove->controller);
    c_slice_free(undo_redo_t, undo_redo);
}

static void
undo_redo_path_modify_apply(rig_undo_journal_t *journal,
                            undo_redo_t *undo_redo)
{
    undo_redo_path_modify_t *modify = &undo_redo->d.path_modify;
    rig_engine_t *engine = journal->engine;

    rig_engine_op_controller_path_set_node(engine,
                                           modify->controller,
                                           modify->property,
                                           modify->t,
                                           &modify->value1);

    rig_reload_inspector_property(journal->editor, modify->property);
}

static undo_redo_t *
undo_redo_path_modify_invert(undo_redo_t *undo_redo_src)
{
    undo_redo_t *inverse = c_slice_dup(undo_redo_t, undo_redo_src);

    rut_boxed_copy(&inverse->d.path_modify.value0,
                   &undo_redo_src->d.path_modify.value1);
    rut_boxed_copy(&inverse->d.path_modify.value1,
                   &undo_redo_src->d.path_modify.value0);
    rut_object_ref(inverse->d.path_modify.object);
    rut_object_ref(inverse->d.path_modify.controller);

    return inverse;
}

static void
undo_redo_path_modify_free(undo_redo_t *undo_redo)
{
    undo_redo_path_modify_t *modify = &undo_redo->d.path_modify;
    rut_boxed_destroy(&modify->value0);
    rut_boxed_destroy(&modify->value1);
    rut_object_unref(modify->object);
    rut_object_unref(modify->controller);
    c_slice_free(undo_redo_t, undo_redo);
}

static void
undo_redo_set_controlled_apply(rig_undo_journal_t *journal,
                               undo_redo_t *undo_redo)
{
    undo_redo_set_controlled_t *set_controlled = &undo_redo->d.set_controlled;
    rig_engine_t *engine = journal->engine;

    if (set_controlled->value)
        rig_engine_op_controller_add_property(
            engine, set_controlled->controller, set_controlled->property);
    else
        rig_engine_op_controller_remove_property(
            engine, set_controlled->controller, set_controlled->property);

    rig_reload_inspector_property(journal->editor, set_controlled->property);
}

static undo_redo_t *
undo_redo_set_controlled_invert(undo_redo_t *undo_redo_src)
{
    undo_redo_t *inverse = c_slice_dup(undo_redo_t, undo_redo_src);

    inverse->d.set_controlled.value = !inverse->d.set_controlled.value;

    rut_object_ref(inverse->d.set_controlled.object);
    rut_object_ref(inverse->d.set_controlled.controller);

    return inverse;
}

static void
undo_redo_set_controlled_free(undo_redo_t *undo_redo)
{
    undo_redo_set_controlled_t *set_controlled = &undo_redo->d.set_controlled;
    rut_object_unref(set_controlled->object);
    rut_object_unref(set_controlled->controller);
    c_slice_free(undo_redo_t, undo_redo);
}

static void
undo_redo_set_control_method_apply(rig_undo_journal_t *journal,
                                   undo_redo_t *undo_redo)
{
    undo_redo_set_control_method_t *set_control_method =
        &undo_redo->d.set_control_method;
    rig_engine_t *engine = journal->engine;

    rig_engine_op_controller_property_set_method(engine,
                                                 set_control_method->controller,
                                                 set_control_method->property,
                                                 set_control_method->method);

    rig_reload_inspector_property(journal->editor, set_control_method->property);
}

static undo_redo_t *
undo_redo_set_control_method_invert(undo_redo_t *undo_redo_src)
{
    undo_redo_t *inverse = c_slice_dup(undo_redo_t, undo_redo_src);
    rig_controller_method_t tmp;

    tmp = inverse->d.set_control_method.method;
    inverse->d.set_control_method.method =
        inverse->d.set_control_method.prev_method;
    inverse->d.set_control_method.prev_method = tmp;

    rut_object_ref(inverse->d.set_control_method.object);
    rut_object_ref(inverse->d.set_control_method.controller);

    return inverse;
}

static void
undo_redo_set_control_method_free(undo_redo_t *undo_redo)
{
    undo_redo_set_control_method_t *set_control_method =
        &undo_redo->d.set_control_method;
    rut_object_unref(set_control_method->object);
    rut_object_unref(set_control_method->controller);
    c_slice_free(undo_redo_t, undo_redo);
}

static void
copy_controller_property_list(c_list_t *src, c_list_t *dst)
{
    undo_redo_prop_data_t *prop_data;

    c_list_for_each(prop_data, src, link)
    {
        undo_redo_prop_data_t *prop_data_copy =
            c_slice_new(undo_redo_prop_data_t);

        prop_data_copy->property = prop_data->property;
        prop_data_copy->method = prop_data->method;
        prop_data_copy->path =
            prop_data->path ? rut_object_ref(prop_data->path) : NULL;
        rut_boxed_copy(&prop_data_copy->constant_value,
                       &prop_data->constant_value);

        c_list_insert(dst->prev, &prop_data_copy->link);
    }
}

typedef struct _undo_redo_controller_state_t {
    c_list_t link;

    rig_controller_t *controller;
    c_list_t properties;
} undo_redo_controller_state_t;

static void
copy_controller_references(c_list_t *src_controller_properties,
                           c_list_t *dst_controller_properties)
{
    undo_redo_controller_state_t *src_controller_state;

    c_list_init(dst_controller_properties);

    c_list_for_each(src_controller_state, src_controller_properties, link)
    {
        undo_redo_controller_state_t *dst_controller_state =
            c_slice_new(undo_redo_controller_state_t);

        dst_controller_state->controller =
            rut_object_ref(src_controller_state->controller);

        c_list_init(&dst_controller_state->properties);

        c_list_insert(dst_controller_properties->prev,
                        &dst_controller_state->link);

        copy_controller_property_list(&src_controller_state->properties,
                                      dst_controller_state->properties.prev);
    }
}

static undo_redo_t *
copy_add_delete_entity(undo_redo_t *undo_redo)
{
    undo_redo_t *copy = c_slice_dup(undo_redo_t, undo_redo);
    undo_redo_add_delete_entity_t *add_delete_entity =
        &copy->d.add_delete_entity;

    rut_object_ref(add_delete_entity->parent_entity);
    rut_object_ref(add_delete_entity->deleted_entity);

    copy_controller_references(
        &undo_redo->d.add_delete_entity.controller_properties,
        &add_delete_entity->controller_properties);

    return copy;
}

typedef struct {
    rut_object_t *object;
    c_list_t *properties;
} copy_controller_properties_data_t;

static void
copy_controller_property_cb(rig_controller_prop_data_t *prop_data,
                            void *user_data)
{
    copy_controller_properties_data_t *data = user_data;

    if (prop_data->property->object == data->object) {
        undo_redo_prop_data_t *undo_prop_data =
            c_slice_new(undo_redo_prop_data_t);

        undo_prop_data->method = prop_data->method;
        rut_boxed_copy(&undo_prop_data->constant_value,
                       &prop_data->constant_value);
        /* As the property's owner is being deleted we can safely just
         * take ownership of the path without worrying about it later
         * being modified.
         */
        undo_prop_data->path =
            prop_data->path ? rut_object_ref(prop_data->path) : NULL;
        undo_prop_data->property = prop_data->property;

        c_list_insert(data->properties->prev, &undo_prop_data->link);
    }
}

static void
save_controller_properties(rig_engine_t *engine,
                           rut_object_t *object,
                           c_list_t *controller_properties)
{
    copy_controller_properties_data_t copy_properties_data;
    c_llist_t *l;

    c_list_init(controller_properties);

    for (l = engine->edit_mode_ui->controllers; l; l = l->next) {
        rig_controller_t *controller = l->data;
        undo_redo_controller_state_t *controller_state =
            c_slice_new(undo_redo_controller_state_t);

        /* Grab a copy of the controller data for all the properties of the
         * entity */
        c_list_init(&controller_state->properties);

        copy_properties_data.object = object;
        copy_properties_data.properties = &controller_state->properties;

        rig_controller_foreach_property(
            controller, copy_controller_property_cb, &copy_properties_data);

        if (c_list_empty(&controller_state->properties)) {
            c_slice_free(undo_redo_controller_state_t, controller_state);
            continue;
        }

        controller_state->controller = rut_object_ref(controller);
        c_list_insert(controller_properties, &controller_state->link);
    }
}

static void
undo_redo_delete_entity_apply(rig_undo_journal_t *journal,
                              undo_redo_t *undo_redo)
{
    undo_redo_add_delete_entity_t *delete_entity =
        &undo_redo->d.add_delete_entity;
    undo_redo_controller_state_t *controller_state;
    rig_engine_t *engine = journal->engine;

    if (!delete_entity->saved_controller_properties) {
        save_controller_properties(engine,
                                   delete_entity->deleted_entity,
                                   &delete_entity->controller_properties);
        delete_entity->saved_controller_properties = true;
    }

    rig_engine_op_delete_entity(engine, delete_entity->deleted_entity);

    c_list_for_each(
        controller_state, &delete_entity->controller_properties, link)
    {
        undo_redo_prop_data_t *prop_data;

        c_list_for_each(prop_data, &controller_state->properties, link)
        {
            rig_engine_op_controller_remove_property(
                engine, controller_state->controller, prop_data->property);
        }
    }
}

static undo_redo_t *
undo_redo_delete_entity_invert(undo_redo_t *undo_redo_src)
{
    undo_redo_t *inverse = copy_add_delete_entity(undo_redo_src);

    inverse->op = UNDO_REDO_ADD_ENTITY_OP;

    return inverse;
}

typedef struct _add_nodes_state_t {
    rig_engine_t *engine;
    rig_controller_t *controller;
    rut_property_t *property;
} add_nodes_state_t;

static void
add_path_node_callback(rig_node_t *node, void *user_data)
{
    add_nodes_state_t *state = user_data;

    rig_engine_op_controller_path_add_node(state->engine,
                                           state->controller,
                                           state->property,
                                           node->t,
                                           &node->boxed);
}

static void
add_controller_properties(rig_engine_t *engine,
                          rig_controller_t *controller,
                          c_list_t *properties)
{
    undo_redo_prop_data_t *undo_prop_data;

    c_list_for_each(undo_prop_data, properties, link)
    {
        rig_engine_op_controller_add_property(
            engine, controller, undo_prop_data->property);

        if (undo_prop_data->path) {
            add_nodes_state_t state;

            state.engine = engine;
            state.controller = controller;
            state.property = undo_prop_data->property;

            rut_path_foreach_node(
                undo_prop_data->path, add_path_node_callback, &state);
        }

        rig_engine_op_controller_set_const(engine,
                                           controller,
                                           undo_prop_data->property,
                                           &undo_prop_data->constant_value);

        rig_engine_op_controller_property_set_method(engine,
                                                     controller,
                                                     undo_prop_data->property,
                                                     undo_prop_data->method);
    }
}

static void
undo_redo_add_entity_apply(rig_undo_journal_t *journal,
                           undo_redo_t *undo_redo)
{
    undo_redo_add_delete_entity_t *add_entity = &undo_redo->d.add_delete_entity;
    undo_redo_controller_state_t *controller_state;
    rig_engine_t *engine = journal->engine;

    rig_engine_op_add_entity(
        engine, add_entity->parent_entity, add_entity->deleted_entity);

    c_list_for_each(
        controller_state, &add_entity->controller_properties, link)
    {
        add_controller_properties(engine,
                                  controller_state->controller,
                                  &controller_state->properties);
    }

    rut_shell_queue_redraw(journal->engine->shell);
}

static undo_redo_t *
undo_redo_add_entity_invert(undo_redo_t *undo_redo_src)
{
    undo_redo_t *inverse = copy_add_delete_entity(undo_redo_src);

    inverse->op = UNDO_REDO_DELETE_ENTITY_OP;

    return inverse;
}

static void
free_controller_properties(c_list_t *controller_properties)
{
    undo_redo_controller_state_t *controller_state, *tmp;

    c_list_for_each_safe(controller_state, tmp, controller_properties, link)
    {
        undo_redo_prop_data_t *prop_data, *tmp1;
        c_list_for_each_safe(
            prop_data, tmp1, &controller_state->properties, link)
        {
            if (prop_data->path)
                rut_object_unref(prop_data->path);

            rut_boxed_destroy(&prop_data->constant_value);

            c_slice_free(undo_redo_prop_data_t, prop_data);
        }

        c_slice_free(undo_redo_controller_state_t, controller_state);
    }
}

static void
undo_redo_add_delete_entity_free(undo_redo_t *undo_redo)
{
    undo_redo_add_delete_entity_t *add_delete_entity =
        &undo_redo->d.add_delete_entity;

    rut_object_unref(add_delete_entity->parent_entity);
    rut_object_unref(add_delete_entity->deleted_entity);

    free_controller_properties(&add_delete_entity->controller_properties);

    c_slice_free(undo_redo_t, undo_redo);
}

static undo_redo_t *
copy_add_delete_component(undo_redo_t *undo_redo)
{
    undo_redo_t *copy = c_slice_dup(undo_redo_t, undo_redo);
    undo_redo_add_delete_component_t *add_delete_component =
        &copy->d.add_delete_component;

    rut_object_ref(add_delete_component->parent_entity);
    rut_object_ref(add_delete_component->deleted_component);

    copy_controller_references(
        &undo_redo->d.add_delete_component.controller_properties,
        &add_delete_component->controller_properties);

    return copy;
}

static void
undo_redo_delete_component_apply(rig_undo_journal_t *journal,
                                 undo_redo_t *undo_redo)
{
    undo_redo_add_delete_component_t *delete_component =
        &undo_redo->d.add_delete_component;
    undo_redo_controller_state_t *controller_state;
    rig_engine_t *engine = journal->engine;

    if (!delete_component->saved_controller_properties) {
        save_controller_properties(engine,
                                   delete_component->deleted_component,
                                   &delete_component->controller_properties);
        delete_component->saved_controller_properties = true;
    }

    c_list_for_each(
        controller_state, &delete_component->controller_properties, link)
    {
        undo_redo_prop_data_t *prop_data;

        c_list_for_each(prop_data, &controller_state->properties, link)
        {
            rig_engine_op_controller_remove_property(
                engine, controller_state->controller, prop_data->property);
        }
    }

    rig_engine_op_delete_component(engine, delete_component->deleted_component);

    rig_editor_update_inspector(journal->editor);
}

static undo_redo_t *
undo_redo_delete_component_invert(undo_redo_t *undo_redo_src)
{
    undo_redo_t *inverse = copy_add_delete_component(undo_redo_src);

    inverse->op = UNDO_REDO_ADD_COMPONENT_OP;

    return inverse;
}

static void
undo_redo_add_component_apply(rig_undo_journal_t *journal,
                              undo_redo_t *undo_redo)
{
    undo_redo_add_delete_component_t *add_component =
        &undo_redo->d.add_delete_component;
    undo_redo_controller_state_t *controller_state;
    rig_engine_t *engine = journal->engine;

    rig_engine_op_add_component(
        engine, add_component->parent_entity, add_component->deleted_component);

    c_list_for_each(
        controller_state, &add_component->controller_properties, link)
    {
        add_controller_properties(engine,
                                  controller_state->controller,
                                  &controller_state->properties);
    }

    rig_editor_update_inspector(journal->editor);
}

static undo_redo_t *
undo_redo_add_component_invert(undo_redo_t *undo_redo_src)
{
    undo_redo_t *inverse = copy_add_delete_component(undo_redo_src);

    inverse->op = UNDO_REDO_DELETE_COMPONENT_OP;

    return inverse;
}

static void
undo_redo_add_delete_component_free(undo_redo_t *undo_redo)
{
    undo_redo_add_delete_component_t *add_delete_component =
        &undo_redo->d.add_delete_component;

    rut_object_unref(add_delete_component->parent_entity);
    rut_object_unref(add_delete_component->deleted_component);

    free_controller_properties(&add_delete_component->controller_properties);

    c_slice_free(undo_redo_t, undo_redo);
}

static undo_redo_t *
copy_add_remove_controller(undo_redo_t *undo_redo)
{
    undo_redo_t *copy = c_slice_dup(undo_redo_t, undo_redo);
    undo_redo_add_remove_controller_t *add_remove_controller =
        &copy->d.add_remove_controller;

    rut_object_ref(add_remove_controller->controller);

    copy_controller_references(
        &undo_redo->d.add_remove_controller.controller_properties,
        &add_remove_controller->controller_properties);

    return copy;
}

static void
undo_redo_add_controller_apply(rig_undo_journal_t *journal,
                               undo_redo_t *undo_redo)
{
    undo_redo_add_remove_controller_t *add_controller =
        &undo_redo->d.add_remove_controller;
    undo_redo_controller_state_t *controller_state;
    rig_engine_t *engine = journal->engine;
    rig_controller_view_t *controller_view =
        rig_editor_get_controller_view(journal->editor);

    rig_engine_op_add_controller(engine, add_controller->controller);

    c_list_for_each(
        controller_state, &add_controller->controller_properties, link)
    {
        add_controller_properties(engine,
                                  controller_state->controller,
                                  &controller_state->properties);
    }

    rig_controller_view_update_controller_list(controller_view);

    rig_controller_view_set_controller(controller_view,
                                       add_controller->controller);
}

static undo_redo_t *
undo_redo_add_controller_invert(undo_redo_t *undo_redo_src)
{
    undo_redo_t *inverse = copy_add_remove_controller(undo_redo_src);

    inverse->op = UNDO_REDO_REMOVE_CONTROLLER_OP;

    return inverse;
}

static void
undo_redo_remove_controller_apply(rig_undo_journal_t *journal,
                                  undo_redo_t *undo_redo)
{
    undo_redo_add_remove_controller_t *remove_controller =
        &undo_redo->d.add_remove_controller;
    undo_redo_controller_state_t *controller_state;
    rig_engine_t *engine = journal->engine;
    rig_ui_t *edit_mode_ui = engine->edit_mode_ui;
    rig_controller_view_t *controller_view =
        rig_editor_get_controller_view(journal->editor);

    if (!remove_controller->saved_controller_properties) {
        save_controller_properties(journal->engine,
                                   remove_controller->controller,
                                   &remove_controller->controller_properties);
        remove_controller->saved_controller_properties = true;
    }

    rig_controller_set_suspended(remove_controller->controller, true);

    c_list_for_each(
        controller_state, &remove_controller->controller_properties, link)
    {
        undo_redo_prop_data_t *prop_data;

        c_list_for_each(prop_data, &controller_state->properties, link)
        {
            rig_engine_op_controller_remove_property(
                engine, controller_state->controller, prop_data->property);
        }
    }

    rig_engine_op_delete_controller(engine, remove_controller->controller);

    rig_controller_view_update_controller_list(controller_view);

    if (rig_controller_view_get_controller(controller_view) ==
        remove_controller->controller) {
        rig_controller_view_set_controller(controller_view,
                                           edit_mode_ui->controllers->data);
    }
}

static undo_redo_t *
undo_redo_remove_controller_invert(undo_redo_t *undo_redo_src)
{
    undo_redo_t *inverse = copy_add_remove_controller(undo_redo_src);

    inverse->op = UNDO_REDO_ADD_CONTROLLER_OP;

    return inverse;
}

static void
undo_redo_add_remove_controller_free(undo_redo_t *undo_redo)
{
    undo_redo_add_remove_controller_t *add_remove_controller =
        &undo_redo->d.add_remove_controller;

    rut_object_unref(add_remove_controller->controller);

    free_controller_properties(&add_remove_controller->controller_properties);

    c_slice_free(undo_redo_t, undo_redo);
}

static undo_redo_op_impl_t undo_redo_ops[] = {
    { undo_redo_subjournal_apply, undo_redo_subjournal_invert,
      undo_redo_subjournal_free, },
    { undo_redo_set_property_apply, undo_redo_set_property_invert,
      undo_redo_set_property_free, },
    { undo_redo_set_controlled_apply, undo_redo_set_controlled_invert,
      undo_redo_set_controlled_free, },
    { undo_redo_set_control_method_apply, undo_redo_set_control_method_invert,
      undo_redo_set_control_method_free, },
    { undo_redo_set_controller_const_apply,
      undo_redo_set_controller_const_invert,
      undo_redo_set_controller_const_free, },
    { undo_redo_path_add_apply,       undo_redo_path_add_invert,
      undo_redo_path_add_remove_free, },
    { undo_redo_path_remove_apply,    undo_redo_path_remove_invert,
      undo_redo_path_add_remove_free, },
    { undo_redo_path_modify_apply, undo_redo_path_modify_invert,
      undo_redo_path_modify_free, },
    { undo_redo_add_entity_apply,       undo_redo_add_entity_invert,
      undo_redo_add_delete_entity_free, },
    { undo_redo_delete_entity_apply,    undo_redo_delete_entity_invert,
      undo_redo_add_delete_entity_free, },
    { undo_redo_add_component_apply,       undo_redo_add_component_invert,
      undo_redo_add_delete_component_free, },
    { undo_redo_delete_component_apply,    undo_redo_delete_component_invert,
      undo_redo_add_delete_component_free, },
    { undo_redo_add_controller_apply,       undo_redo_add_controller_invert,
      undo_redo_add_remove_controller_free, },
    { undo_redo_remove_controller_apply,    undo_redo_remove_controller_invert,
      undo_redo_add_remove_controller_free, },
};

static void
undo_redo_apply(rig_undo_journal_t *journal, undo_redo_t *undo_redo)
{
    c_return_if_fail(undo_redo->op < UNDO_REDO_N_OPS);

    undo_redo_ops[undo_redo->op].apply(journal, undo_redo);
}

static undo_redo_t *
undo_redo_invert(undo_redo_t *undo_redo)
{
    c_return_val_if_fail(undo_redo->op < UNDO_REDO_N_OPS, NULL);

    return undo_redo_ops[undo_redo->op].invert(undo_redo);
}

static void
undo_redo_free(undo_redo_t *undo_redo)
{
    c_return_if_fail(undo_redo->op < UNDO_REDO_N_OPS);

    undo_redo_ops[undo_redo->op].free(undo_redo);
}

static void
rig_undo_journal_flush_redos(rig_undo_journal_t *journal)
{
    c_list_t reversed_operations;
    undo_redo_t *l, *tmp;

    /* Build a list of inverted operations out of the redo list. These
     * will be added two the end of the undo list so that the previously
     * undone actions themselves become undoable actions */
    c_list_init(&reversed_operations);

    c_list_for_each(l, &journal->redo_ops, list_node)
    {
        undo_redo_t *inverted = undo_redo_invert(l);

        /* Add the inverted node to the end so it will keep the same
         * order */
        if (inverted)
            c_list_insert(reversed_operations.prev, &inverted->list_node);
    }

    /* Add all of the redo operations again in reverse order so that if
     * the user undoes past all of the redoes to put them back into the
     * state they were before the undoes, they will be able to continue
     * undoing to undo those actions again */
    c_list_for_each_reverse_safe(l, tmp, &journal->redo_ops, list_node)
    c_list_insert(journal->undo_ops.prev, &l->list_node);
    c_list_init(&journal->redo_ops);

    c_list_insert_list(journal->undo_ops.prev, &reversed_operations);
}

static void
rig_undo_journal_insert(rig_undo_journal_t *journal,
                        undo_redo_t *undo_redo)
{
    rig_engine_t *engine = journal->engine;
    bool apply;

    c_return_if_fail(undo_redo != NULL);
    c_return_if_fail(journal->inserting == false);

    if (engine->play_mode) {
        c_warning("Ignoring attempt to edit UI while in play mode");
        undo_redo_free(undo_redo);
        return;
    }

    rig_undo_journal_flush_redos(journal);

    journal->inserting = true;

    apply = journal->apply_on_insert;

    /* If we are inserting a journal where the operations have already
     * been applied then we don't want to re-apply them if this journal
     * normally also applys operations when inserting them... */
    if (undo_redo->op == UNDO_REDO_SUBJOURNAL_OP &&
        undo_redo->d.subjournal->apply_on_insert) {
        apply = false;
    }

    if (apply) {
        // undo_redo_t *inverse;

        undo_redo_apply(journal, undo_redo);

/* Purely for testing purposes we now redundantly apply the
 * operation followed by the inverse of the operation so we are
 * alway verifying our ability to invert operations correctly...
 *
 * XXX: This is disabled for now, because it causes problems in
 * cases where we add + register new objects, then delete them
 * and then add + register them again. Since objects are garbage
 * collected lazily they won't have been unregistered before we
 * try and re-register them and so we hit various assertions.
 */
#if 0
        /* XXX: Some operations can't be inverted until they have been
         * applied once. For example the undo_redo_path_add_remove_t operation
         * will save the value of a path node when it is removed so the
         * node can be re-added later, but until we have saved that
         * value we can't invert the operation */
        inverse = undo_redo_invert (undo_redo);
        c_warn_if_fail (inverse != NULL);

        if (inverse)
        {
            undo_redo_apply (journal, inverse);
            undo_redo_apply (journal, undo_redo);
            undo_redo_free (inverse);
        }
#endif
    }

    c_list_insert(journal->undo_ops.prev, &undo_redo->list_node);

    dump_journal(journal, 0);

    journal->inserting = false;
}

/* This api can be used to undo the last operation without freeing
 * the undo_redo_t struct so that it can be modified and re-inserted.
 *
 * We use this to handle modifying mergable operations so we avoid
 * having to special case applying the changes of a modification.
 */
static undo_redo_t *
rig_undo_journal_revert(rig_undo_journal_t *journal)
{
    undo_redo_t *op = rut_container_of(journal->undo_ops.prev, op, list_node);

    if (journal->apply_on_insert) {
        /* XXX: we should probably be making sure to sync with the
         * simulator here. Some operations can't be inverted until
         * they have been applied first.
         */

        undo_redo_t *inverse = undo_redo_invert(op);

        if (!inverse) {
            c_warning("Not allowing undo of operation that can't be inverted");
            return NULL;
        }

        undo_redo_apply(journal, inverse);
        undo_redo_free(inverse);
    }

    c_list_remove(&op->list_node);

    return op;
}

bool
rig_undo_journal_undo(rig_undo_journal_t *journal)
{
    if (journal->engine->play_mode) {
        c_warning("Ignoring attempt to edit UI while in play mode");
        return false;
    }

    if (!c_list_empty(&journal->undo_ops)) {
        undo_redo_t *op = rig_undo_journal_revert(journal);
        if (!op)
            return false;

        c_list_insert(journal->redo_ops.prev, &op->list_node);

        rut_shell_queue_redraw(journal->engine->shell);

        dump_journal(journal, 0);

        return true;
    } else
        return false;
}

bool
rig_undo_journal_redo(rig_undo_journal_t *journal)
{
    undo_redo_t *op;

    if (journal->engine->play_mode) {
        c_warning("Ignoring attempt to edit UI while in play mode");
        return false;
    }

    if (c_list_empty(&journal->redo_ops))
        return false;

    op = rut_container_of(journal->redo_ops.prev, op, list_node);

    undo_redo_apply(journal, op);
    c_list_remove(&op->list_node);
    c_list_insert(journal->undo_ops.prev, &op->list_node);

    rut_shell_queue_redraw(journal->engine->shell);

    dump_journal(journal, 0);

    return true;
}

rig_undo_journal_t *
rig_undo_journal_new(rig_editor_t *editor)
{
    rig_undo_journal_t *journal = c_slice_new0(rig_undo_journal_t);

    journal->editor = editor;
    journal->engine = rig_editor_get_engine(editor);

    c_list_init(&journal->undo_ops);
    c_list_init(&journal->redo_ops);

    return journal;
}

void
rig_undo_journal_set_apply_on_insert(rig_undo_journal_t *journal,
                                     bool apply_on_insert)
{
    journal->apply_on_insert = apply_on_insert;
}

bool
rig_undo_journal_is_empty(rig_undo_journal_t *journal)
{
    return (c_list_length(&journal->undo_ops) == 0 &&
            c_list_length(&journal->redo_ops) == 0);
}

void
rig_undo_journal_free(rig_undo_journal_t *journal)
{
    undo_redo_t *node, *tmp;

    c_list_for_each_safe(node, tmp, &journal->undo_ops, list_node)
    undo_redo_free(node);
    c_list_for_each_safe(node, tmp, &journal->redo_ops, list_node)
    undo_redo_free(node);

    c_slice_free(rig_undo_journal_t, journal);
}
