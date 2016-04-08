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

#ifndef _RUT_UNDO_JOURNAL_H_
#define _RUT_UNDO_JOURNAL_H_

#include <clib.h>

#include <rut.h>

typedef struct _rig_undo_journal_t rig_undo_journal_t;

#include "rig-types.h"
#include "rig-node.h"
#include "rig-path.h"
#include "rig-controller.h"
#include "rig-pb.h"
#include "rig-entity.h"

typedef enum _undo_redo_op_t {
    UNDO_REDO_SUBJOURNAL_OP,
    UNDO_REDO_SET_PROPERTY_OP,
    UNDO_REDO_SET_CONTROLLED_OP,
    UNDO_REDO_SET_CONTROL_METHOD_OP,
    UNDO_REDO_CONST_PROPERTY_CHANGE_OP,
    UNDO_REDO_PATH_ADD_OP,
    UNDO_REDO_PATH_REMOVE_OP,
    UNDO_REDO_PATH_MODIFY_OP,
    UNDO_REDO_ADD_ENTITY_OP,
    UNDO_REDO_DELETE_ENTITY_OP,
    UNDO_REDO_ADD_COMPONENT_OP,
    UNDO_REDO_DELETE_COMPONENT_OP,
    UNDO_REDO_ADD_CONTROLLER_OP,
    UNDO_REDO_REMOVE_CONTROLLER_OP,
    UNDO_REDO_N_OPS
} undo_redo_op_t;

typedef struct _undo_redo_set_property_t {
    rut_object_t *object;
    rig_property_t *property;
    rut_boxed_t value0;
    rut_boxed_t value1;
} undo_redo_set_property_t;

typedef struct _undo_redo_set_controller_const_t {
    rig_controller_t *controller;
    rut_object_t *object;
    rig_property_t *property;
    rut_boxed_t value0;
    rut_boxed_t value1;
} undo_redo_set_controller_const_t;

typedef struct _undo_redo_path_add_remove_t {
    rig_controller_t *controller;
    rut_object_t *object;
    rig_property_t *property;
    float t;

    //#warning "XXX: figure out how undo_redo_path_add_remove_t with async edits
    // via the simulator"
    /* When we initially log to remove a node then we won't save
     * a value until we actually apply the operation and so we
     * need to track when this boxed value is valid... */
    bool have_value;
    rut_boxed_t value;
} undo_redo_path_add_remove_t;

typedef struct _undo_redo_path_modify_t {
    rig_controller_t *controller;
    rut_object_t *object;
    rig_property_t *property;
    float t;
    rut_boxed_t value0;
    rut_boxed_t value1;
} undo_redo_path_modify_t;

typedef struct _undo_redo_set_controlled_t {
    rig_controller_t *controller;
    rut_object_t *object;
    rig_property_t *property;
    bool value;
} undo_redo_set_controlled_t;

typedef struct _undo_redo_set_control_method_t {
    rig_controller_t *controller;
    rut_object_t *object;
    rig_property_t *property;
    rig_controller_method_t prev_method;
    rig_controller_method_t method;
} undo_redo_set_control_method_t;

typedef struct {
    c_list_t link;

    rig_property_t *property;

    rig_controller_method_t method;
    rig_path_t *path;
    rut_boxed_t constant_value;
} undo_redo_prop_data_t;

typedef struct _undo_redo_add_delete_entity_t {
    rig_entity_t *parent_entity;
    rig_entity_t *deleted_entity;
    bool saved_controller_properties;
    c_list_t controller_properties;
} undo_redo_add_delete_entity_t;

typedef struct _undo_redo_add_delete_component_t {
    rig_entity_t *parent_entity;
    rut_object_t *deleted_component;
    bool saved_controller_properties;
    c_list_t controller_properties;
} undo_redo_add_delete_component_t;

typedef struct _undo_redo_add_remove_controller_t {
    rig_controller_t *controller;
    bool saved_controller_properties;
    c_list_t controller_properties;
} undo_redo_add_remove_controller_t;

typedef struct _undo_redo_t {
    c_list_t list_node;

    undo_redo_op_t op;
    bool mergable;
    union {
        undo_redo_set_property_t set_property;
        undo_redo_set_controller_const_t set_controller_const;
        undo_redo_path_add_remove_t path_add_remove;
        undo_redo_path_modify_t path_modify;
        undo_redo_set_controlled_t set_controlled;
        undo_redo_set_control_method_t set_control_method;
        undo_redo_add_delete_entity_t add_delete_entity;
        undo_redo_add_delete_component_t add_delete_component;
        undo_redo_add_remove_controller_t add_remove_controller;
        rig_undo_journal_t *subjournal;
    } d;
} undo_redo_t;

struct _rig_undo_journal_t {
    rig_editor_t *editor;
    rig_engine_t *engine;

    /* List of operations that can be undone. The operations are
     * appended to the end of this list so that they are kept in order
     * from the earliest added operation to the last added operation.
     * The operations are not stored inverted so each operation
     * represents the action that user made. */
    c_list_t undo_ops;

    /* List of operations that can be redone. As the user presses undo,
     * the operations are added to the tail of this list. Therefore the
     * list is in the order of earliest undone operation to the latest.
     * The operations represent the original action that the user made
     * so it will not need to be inverted before redoing the
     * operation. */
    c_list_t redo_ops;

    /* Detect recursion on insertion which indicates a bug */
    bool inserting;

    /* Whether or not operations should be applied as they are inserted
     * into the journal. By default this is false, so we can create
     * subjournals, log operations and then apply them all together when
     * inserting the subjournal into the master journal. Normally only
     * the top-level, master journal would set this to true.
     */
    bool apply_on_insert;
};

void rig_undo_journal_log_add_controller(rig_undo_journal_t *journal,
                                         rig_controller_t *controller);

void rig_undo_journal_log_remove_controller(rig_undo_journal_t *journal,
                                            rig_controller_t *controller);

void rig_undo_journal_set_controlled(rig_undo_journal_t *journal,
                                     rig_controller_t *controller,
                                     rig_property_t *property,
                                     bool value);

void rig_undo_journal_set_control_method(rig_undo_journal_t *journal,
                                         rig_controller_t *controller,
                                         rig_property_t *property,
                                         rig_controller_method_t method);

void rig_undo_journal_set_controller_constant(rig_undo_journal_t *journal,
                                              bool mergable,
                                              rig_controller_t *controller,
                                              const rut_boxed_t *value,
                                              rig_property_t *property);

void
rig_undo_journal_set_controller_path_node_value(rig_undo_journal_t *journal,
                                                bool mergable,
                                                rig_controller_t *controller,
                                                float t,
                                                const rut_boxed_t *value,
                                                rig_property_t *property);

void rig_undo_journal_remove_controller_path_node(rig_undo_journal_t *journal,
                                                  rig_controller_t *controller,
                                                  rig_property_t *property,
                                                  float t);

#if 0
typedef struct
{
    rig_controller_prop_data_t *prop_data;
    rig_node_t *node;
} rig_undo_journal_path_node_t;

void
rig_undo_journal_move_controller_path_nodes (rig_undo_journal_t *journal,
                                             rig_controller_t *controller,
                                             float offset,
                                             const rig_undo_journal_path_node_t *nodes,
                                             int n_path_nodes);
#endif

void rig_undo_journal_set_property(rig_undo_journal_t *journal,
                                   bool mergable,
                                   const rut_boxed_t *value,
                                   rig_property_t *property);

void rig_undo_journal_add_entity(rig_undo_journal_t *journal,
                                 rig_entity_t *parent_entity,
                                 rig_entity_t *entity);

void rig_undo_journal_delete_entity(rig_undo_journal_t *journal,
                                    rig_entity_t *entity);

void rig_undo_journal_add_component(rig_undo_journal_t *journal,
                                    rig_entity_t *entity,
                                    rut_object_t *component);

void rig_undo_journal_delete_component(rig_undo_journal_t *journal,
                                       rut_object_t *component);

/**
 * rig_undo_journal_log_subjournal:
 * @journal: The main journal
 * @subjournal: A subjournal to add to @journal
 *
 * Logs a collection of undo entries as a single meta-entry in
 * @journal. The collection is stored in @subjournal which can be
 * built up using the existing journal logging commands. When an undo
 * is performed on the main journal, all of the entries in the
 * subjournal will be performed as a single action. The main journal
 * will take ownership of @subjournal.
 */
void rig_undo_journal_log_subjournal(rig_undo_journal_t *journal,
                                     rig_undo_journal_t *subjournal);

bool rig_undo_journal_undo(rig_undo_journal_t *journal);

bool rig_undo_journal_redo(rig_undo_journal_t *journal);

rig_undo_journal_t *rig_undo_journal_new(rig_editor_t *editor);

void rig_undo_journal_set_apply_on_insert(rig_undo_journal_t *journal,
                                          bool apply_on_insert);

bool rig_undo_journal_is_empty(rig_undo_journal_t *journal);

void rig_undo_journal_free(rig_undo_journal_t *journal);

#endif /* _RUT_UNDO_JOURNAL_H_ */
