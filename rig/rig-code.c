/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013 Intel Corporation.
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

#include <rig-config.h>

#include <dlfcn.h>
#include <clib.h>
#include <cmodule.h>

#include <rut.h>

#include "rig-code.h"
#if defined(RIG_EDITOR_ENABLED) && defined(USE_LLVM)
#include "rig-llvm.h"
#endif

struct _rig_code_node_t {
    rut_object_base_t _base;

    rig_engine_t *engine;

    rut_graphable_props_t graphable;

    c_list_t link_closures;

    char *pre;
    char *post;
};

static void
_rig_code_node_free(void *object)
{
    rig_code_node_t *node = object;

    if (node->pre)
        c_free(node->pre);
    if (node->post)
        c_free(node->post);

    rut_graphable_destroy(node);

    rut_object_free(rig_code_node_t, object);
}

rut_type_t rig_code_node_type;

static void
_rig_code_node_init_type(void)
{
    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child remove */
                                                       NULL, /* child add */
                                                       NULL /* parent changed */
    };

    rut_type_t *type = &rig_code_node_type;
#define TYPE rig_code_node_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_code_node_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_GRAPHABLE,
                       offsetof(TYPE, graphable),
                       &graphable_vtable);

#undef TYPE
}

rig_code_node_t *
rig_code_node_new(rig_engine_t *engine, const char *pre, const char *post)
{
    rig_code_node_t *node = rut_object_alloc0(
        rig_code_node_t, &rig_code_node_type, _rig_code_node_init_type);

    node->engine = engine;

    rut_graphable_init(node);

    c_list_init(&node->link_closures);

    /* Note: in device mode and in the simulator we avoid
     * tracking any source code. */

    if (pre)
        node->pre = c_strdup(pre);

    if (post)
        node->post = c_strdup(post);

    return node;
}

static rut_traverse_visit_flags_t
code_generate_pre_cb(rut_object_t *object, int depth, void *user_data)
{
    rig_code_node_t *node = object;
    c_string_t *code = user_data;

    if (node->pre)
        c_string_append(code, node->pre);

    return RUT_TRAVERSE_VISIT_CONTINUE;
}

static rut_traverse_visit_flags_t
code_generate_post_cb(rut_object_t *object, int depth, void *user_data)
{
    rig_code_node_t *node = object;
    c_string_t *code = user_data;

    if (node->post)
        c_string_append(code, node->post);

    return RUT_TRAVERSE_VISIT_CONTINUE;
}

static rut_traverse_visit_flags_t
notify_link_cb(rut_object_t *object, int depth, void *user_data)
{
    rig_code_node_t *node = object;

    rut_closure_list_invoke(
        &node->link_closures, rig_code_node_link_callback_t, node);

    return RUT_TRAVERSE_VISIT_CONTINUE;
}

void
rig_code_update_dso(rig_engine_t *engine, uint8_t *data, int len)
{
    char *tmp_filename;
    c_error_t *error = NULL;
    int dso_fd;
    rut_exception_t *e = NULL;
    c_module_t *module;

    if (engine->code_dso_module) {
        c_module_close(engine->code_dso_module);
        engine->code_dso_module = NULL;
    }

    if (!data)
        return;

    dso_fd = c_file_open_tmp(NULL, &tmp_filename, &error);
    if (dso_fd == -1) {
        c_warning("Failed to open a temporary file for shared object: %s",
                  error->message);
        c_error_free(error);
        return;
    }

    if (!rut_os_write(dso_fd, data, len, &e)) {
        c_warning("Failed to write shared object: %s", e->message);
        rut_exception_free(e);
        return;
    }

    module = c_module_open(tmp_filename);
    if (!module) {
        c_module_close(module);
        c_warning("Failed to open shared object");
        return;
    }
    engine->code_dso_module = module;

    rut_graphable_traverse(engine->code_graph,
                           RUT_TRAVERSE_DEPTH_FIRST,
                           NULL,
                           notify_link_cb,
                           NULL);
}

#if defined(RIG_EDITOR_ENABLED) && defined(USE_LLVM)
static void
recompile(rig_engine_t *engine)
{
    rig_llvm_module_t *module;
    uint8_t *dso_data;
    size_t dso_len;

    c_return_if_fail(engine->need_recompile);
    engine->need_recompile = false;

    /* To avoid fragmentation we re-use one allocation for all codegen... */
    c_string_set_size(engine->code_string, 0);

    rut_graphable_traverse(engine->code_graph,
                           RUT_TRAVERSE_DEPTH_FIRST,
                           code_generate_pre_cb,
                           code_generate_post_cb,
                           engine->code_string);

    module = rig_llvm_compile_to_dso(engine->code_string->str,
                                     &engine->code_dso_filename,
                                     &dso_data,
                                     &dso_len);

    if (module) {
        rig_frontend_update_simulator_dso(engine->frontend, dso_data, dso_len);

#warning                                                                       \
        "fix freeing of llvm module - crashes due to null llvm context impl pointer"
        // rut_object_unref (module);
    }
}

static void
recompile_pre_paint_callback(rut_object_t *_null_graphable,
                             void *user_data)
{
    recompile(user_data);
}
#endif /* RIG_EDITOR_ENABLED && USE_LLVM */

static void
queue_recompile(rig_engine_t *engine)
{
#if defined(RIG_EDITOR_ENABLED) && defined(USE_LLVM)
    if (engine->need_recompile)
        return;

    engine->need_recompile = true;

    rut_shell_add_pre_paint_callback(engine->shell,
                                     NULL, /* graphable */
                                     recompile_pre_paint_callback,
                                     engine);

    rut_shell_queue_redraw(engine->shell);
#else

    c_error("Runtime code recompilation not enabled");

#endif
}

void
rig_code_node_set_pre(rig_code_node_t *node, const char *pre)
{
    if (node->pre)
        c_free(node->pre);

    node->pre = c_strdup(pre);

    queue_recompile(node->engine);
}

void
rig_code_node_set_post(rig_code_node_t *node, const char *post)
{
    if (node->post)
        c_free(node->post);

    node->post = c_strdup(post);

    queue_recompile(node->engine);
}

void
rig_code_node_add_child(rig_code_node_t *node, rig_code_node_t *child)
{
    rut_graphable_add_child(node, child);

    queue_recompile(node->engine);
}

void
rig_code_node_remove_child(rig_code_node_t *child)
{
    queue_recompile(child->engine);

    rut_graphable_remove_child(child);
}

rut_closure_t *
rig_code_node_add_link_callback(rig_code_node_t *node,
                                rig_code_node_link_callback_t callback,
                                void *user_data,
                                rut_closure_destroy_callback_t destroy)
{
    return rut_closure_list_add_FIXME(
        &node->link_closures, callback, user_data, destroy);
}

void *
rig_code_resolve_symbol(rig_engine_t *engine, const char *name)
{
    if (engine->code_dso_module) {
        void *sym;
        if (c_module_symbol(engine->code_dso_module, name, &sym))
            return sym;
        else
            return NULL;
    } else
        return NULL;
}

void
_rig_code_init(rig_engine_t *engine)
{
#if defined(RIG_EDITOR_ENABLED) && defined(USE_LLVM)
    engine->code_string = c_string_new("");
    engine->codegen_string0 = c_string_new("");
    engine->codegen_string1 = c_string_new("");

    engine->next_code_id = 1;
    engine->need_recompile = false;
#endif

    engine->code_graph = rig_code_node_new(
        engine, "typedef struct _rig_property_t rig_property_t;\n", "");
}

void
_rig_code_fini(rig_engine_t *engine)
{
#if defined(RIG_EDITOR_ENABLED) && defined(USE_LLVM)
    c_string_free(engine->code_string, true);
    engine->code_string = NULL;

    c_string_free(engine->codegen_string0, true);
    engine->codegen_string0 = NULL;

    c_string_free(engine->codegen_string1, true);
    engine->codegen_string1 = NULL;

    if (engine->code_dso_filename)
        c_free(engine->code_dso_filename);

    rut_shell_remove_pre_paint_callback(
        engine->shell, recompile_pre_paint_callback, engine);
#endif

    rut_object_unref(engine->code_graph);
    engine->code_graph = NULL;

    if (engine->code_dso_module)
        c_module_close(engine->code_dso_module);
}
