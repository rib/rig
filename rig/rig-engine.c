/*
 * Rig
 *
 * UI Engine & Editor
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

#include <rig-config.h>

#include <sys/stat.h>
#include <math.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <clib.h>

#include <rut.h>

#include "rig-engine.h"
#include "rig-controller.h"
#include "rig-load-save.h"
#include "rig-renderer.h"
#include "rig-defines.h"
#include "rig-rpc-network.h"
#include "rig.pb-c.h"
#include "rig-frontend.h"
#include "rig-simulator.h"
#include "rig-code-module.h"
#include "rig-timeline.h"

#include "components/rig-camera.h"
#include "components/rig-source.h"

#ifdef USE_UV
#include "components/rig-native-module.h"
#endif

static rut_property_spec_t _rig_engine_prop_specs[] = {
    { 0 }
};

void
rig_engine_set_ui(rig_engine_t *engine, rig_ui_t *ui)
{
    if (engine->ui == ui)
        return;

    if (engine->ui) {
        rig_ui_reap(engine->ui);
        rut_object_release(engine->ui, engine);
        engine->ui = NULL;
    }

    if (ui) {
        engine->ui = rut_object_claim(ui, engine);

#ifdef RIG_ENABLE_DEBUG
        c_debug("New UI set:");
        rig_ui_print(ui);
#endif
    }

    rut_shell_queue_redraw(engine->shell);
}

static void
_rig_engine_free(void *object)
{
    rig_engine_t *engine = object;

    rig_engine_set_ui(engine, NULL);

    rig_text_engine_state_destroy(engine->text_state);

    if (engine->queued_deletes->len) {
        c_warning("Leaking %d un-garbage-collected objects",
                  engine->queued_deletes->len);
    }
    rut_queue_free(engine->queued_deletes);

    rig_pb_serializer_destroy(engine->ops_serializer);

    rut_memory_stack_free(engine->frame_stack);
    if (engine->sim_frame_stack)
        rut_memory_stack_free(engine->sim_frame_stack);

    rut_magazine_free(engine->object_id_magazine);

    rut_introspectable_destroy(engine);

    rut_object_free(rig_engine_t, engine);
}

rut_type_t rig_engine_type;

static void
_rig_engine_init_type(void)
{
    rut_type_t *type = &rig_engine_type;
#define TYPE rig_engine_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_engine_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */

#undef TYPE
}

static rig_engine_t *
_rig_engine_new_full(rut_shell_t *shell,
                     rig_frontend_t *frontend,
                     rig_simulator_t *simulator)
{
    rig_engine_t *engine = rut_object_alloc0(
        rig_engine_t, &rig_engine_type, _rig_engine_init_type);

    engine->shell = shell;
    engine->property_ctx = &shell->property_ctx;

    engine->headless = engine->shell->headless;

    engine->frontend = frontend;
    engine->simulator = simulator;

    c_matrix_init_identity(&engine->identity);

    rut_introspectable_init(engine, _rig_engine_prop_specs, engine->properties);

    engine->object_id_magazine = rut_magazine_new(sizeof(uint64_t), 1000);

    /* The frame stack is a very cheap way to allocate memory that will
     * be automatically freed at the end of the next frame (or current
     * frame if one is already being processed.)
     */
    engine->frame_stack = rut_memory_stack_new(8192);

    /* Since the frame rate of the frontend may not match the frame rate
     * of the simulator, we maintain a separate frame stack for
     * allocations whose lifetime is tied to a simulation frame, not a
     * frontend frame...
     */
    if (frontend)
        engine->sim_frame_stack = rut_memory_stack_new(8192);

    engine->ops_serializer = rig_pb_serializer_new(engine);

    if (frontend) {
        /* By default a rig_pb_serializer_t will use engine->frame_stack,
         * but operations generated in a frontend need to be batched
         * until they can be sent to the simulator which may be longer
         * than one frontend frame so we need to use the sim_frame_stack
         * instead...
         */
        rig_pb_serializer_set_stack(engine->ops_serializer,
                                    engine->sim_frame_stack);
    }

    rig_pb_serializer_set_use_pointer_ids_enabled(engine->ops_serializer, true);

    engine->queued_deletes = rut_queue_new();

    engine->text_state = rig_text_engine_state_new(engine);

    _rig_code_init(engine);

    return engine;
}

rig_engine_t *
rig_engine_new_for_simulator(rut_shell_t *shell,
                             rig_simulator_t *simulator)
{
    return _rig_engine_new_full(shell, NULL, simulator);
}

rig_engine_t *
rig_engine_new_for_frontend(rut_shell_t *shell,
                            rig_frontend_t *frontend)
{
    return _rig_engine_new_full(shell, frontend, NULL);
}

void
rig_engine_set_log_op_callback(rig_engine_t *engine,
                               void (*callback)(Rig__Operation *pb_op,
                                                void *user_data),
                               void *user_data)
{
    engine->log_op_callback = callback;
    engine->log_op_data = user_data;
}

void
rig_engine_queue_delete(rig_engine_t *engine, rut_object_t *object)
{
    rut_object_claim(object, engine);
    rut_queue_push_tail(engine->queued_deletes, object);
}

void
rig_engine_garbage_collect(rig_engine_t *engine)
{
    rut_queue_item_t *item;
    void (*object_callback)(rut_object_t *object,
                            void *user_data);
    void *user_data = engine->garbage_collect_data;

    object_callback = engine->garbage_collect_callback;

    c_list_for_each(item, &engine->queued_deletes->items, list_node)
    {
        if (object_callback)
            object_callback(item->data, user_data);
        rut_object_release(item->data, engine);
    }
    rut_queue_clear(engine->queued_deletes);
}

char *
rig_engine_get_object_debug_name(rut_object_t *object)
{
    if (rut_object_get_type(object) == &rig_entity_type)
        return c_strdup_printf(
            "%p(label=\"%s\")", object, rig_entity_get_label(object));
    else if (rut_object_is(object, RUT_TRAIT_ID_COMPONENTABLE)) {
        rut_componentable_props_t *component_props =
            rut_object_get_properties(object, RUT_TRAIT_ID_COMPONENTABLE);
        rig_entity_t *entity = component_props->entity;

        if (entity) {
            const char *entity_label =
                entity ? rig_entity_get_label(entity) : "";
            return c_strdup_printf("%p(label=\"%s\"::%s)",
                                   object,
                                   entity_label,
                                   rut_object_get_type_name(object));
        } else
            return c_strdup_printf(
                "%p(<orphaned>::%s)", object, rut_object_get_type_name(object));
    } else
        return c_strdup_printf(
            "%p(%s)", object, rut_object_get_type_name(object));
}

void
rig_engine_set_apply_op_context(rig_engine_t *engine,
                                rig_engine_op_apply_context_t *ctx)
{
    engine->apply_op_ctx = ctx;
}

void
rig_engine_progress_timelines(rig_engine_t *engine, double delta)
{
    c_sllist_t *l;

    for (l = engine->timelines; l; l = l->next)
        _rig_timeline_progress(l->data, delta);
}

bool
rig_engine_check_timelines(rig_engine_t *engine)
{
    c_sllist_t *l;

    for (l = engine->timelines; l; l = l->next)
        if (rig_timeline_is_running(l->data))
            return true;

    return false;
}


