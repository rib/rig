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

#ifndef _RIG_EDITOR_H_
#define _RIG_EDITOR_H_

#include <rut.h>

typedef struct _rig_editor_t rig_editor_t;

#include "rig-types.h"
#include "rig-entity.h"
#include "rig-engine.h"
#include "rig-undo-journal.h"
#include "rig-controller-view.h"

#include "rig.pb-c.h"

extern c_llist_t *rig_editor_slave_address_options;

extern rut_type_t rig_editor_type;

rig_editor_t *rig_editor_new(const char *filename);

void rig_editor_run(rig_editor_t *editor);

void rig_editor_load_file(rig_editor_t *editor, const char *filename);

void rig_editor_apply_last_op(rig_engine_t *engine);

/* XXX: This rather esoteric prototype is used as a 'read_callback' to
 * rig_asset_thumbnail and is called whenever an asset's thumnail has
 * been updated.
 *
 * It would probably be better to just have a
 * rig_editor_reload_thumbnails(rig_editor_t *editor) considering that
 * all this function does is trigger an asset search to refresh the
 * assets view.
 */
void rig_editor_refresh_thumbnails(rig_asset_t *video, void *user_data);

void rig_editor_clear_search_results(rig_editor_t *editor);

void rig_editor_free_result_input_closures(rig_editor_t *editor);

void rig_reload_inspector_property(rig_editor_t *editor,
                                   rig_property_t *property);

void rig_reload_position_inspector(rig_editor_t *editor, rig_entity_t *entity);

/* TODO: Update to take a rig_editor_t */
void rig_editor_update_inspector(rig_editor_t *editor);

void rig_select_object(rig_editor_t *editor,
                       rut_object_t *object,
                       rut_select_action_t action);

extern rut_type_t rig_objects_selection_type;

typedef struct _rig_entites_selection_t {
    rut_object_base_t _base;
    rig_editor_t *editor;
    c_llist_t *objects;
    c_list_t selection_events_cb_list;
} rig_objects_selection_t;

rig_objects_selection_t *_rig_objects_selection_new(rig_editor_t *editor);

rig_objects_selection_t *rig_editor_get_objects_selection(rig_editor_t *editor);

typedef enum _rig_objects_selection_event_t {
    RIG_OBJECTS_SELECTION_ADD_EVENT,
    RIG_OBJECTS_SELECTION_REMOVE_EVENT
} rig_objects_selection_event_t;

typedef void (*rig_objects_selection_event_callback_t)(
    rig_objects_selection_t *selection,
    rig_objects_selection_event_t event,
    rut_object_t *object,
    void *user_data);

rut_closure_t *rig_objects_selection_add_event_callback(
    rig_objects_selection_t *selection,
    rig_objects_selection_event_callback_t callback,
    void *user_data,
    rut_closure_destroy_callback_t destroy_cb);

void rig_editor_push_undo_subjournal(rig_editor_t *editor);
rig_undo_journal_t *rig_editor_pop_undo_subjournal(rig_editor_t *editor);

void rig_editor_free_builtin_assets(rig_editor_t *editor);

void rig_editor_print_mappings(rig_editor_t *editor);

void rig_editor_save(rig_editor_t *editor);

typedef void (*rig_tool_changed_callback_t)(rig_editor_t *editor,
                                            rig_tool_id_t tool_id,
                                            void *user_data);

void
rig_add_tool_changed_callback(rig_editor_t *editor,
                              rig_tool_changed_callback_t callback,
                              void *user_data,
                              rut_closure_destroy_callback_t destroy_notify);

cg_primitive_t *rig_editor_get_grid_prim(rig_editor_t *editor);

rig_controller_view_t *rig_editor_get_controller_view(rig_editor_t *editor);

rig_engine_t *rig_editor_get_engine(rig_editor_t *editor);

void rig_editor_reset(rig_editor_t *editor);

#endif /* _RIG_EDITOR_H_ */
