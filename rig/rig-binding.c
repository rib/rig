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

#include <clib.h>

#include <rut.h>

#include "rig-code.h"

typedef struct _rig_binding_t rig_binding_t;

typedef struct _dependency_t {
    rig_binding_t *binding;

    rut_object_t *object;
    rut_property_t *property;

    char *variable_name;

    c_list_t link;

} dependency_t;

struct _rig_binding_t {
    rut_object_base_t _base;

    rig_engine_t *engine;

    rut_property_t *property;

    int binding_id;

    bool simple_copy;
    char *expression;

    char *function_name;

    rig_code_node_t *function_node;
    rig_code_node_t *expression_node;

    c_list_t dependencies;

    unsigned int active : 1;
};

static void
remove_and_free_dependency(dependency_t *dependency)
{
    c_list_remove(&dependency->link);

    if (dependency->variable_name)
        c_free(dependency->variable_name);

    c_slice_free(dependency_t, dependency);
}


static void
_rig_binding_free(void *object)
{
    rig_binding_t *binding = object;
    dependency_t *dependency, *tmp;

    c_list_for_each_safe(dependency, tmp, &binding->dependencies, link) {
        remove_and_free_dependency(dependency);
    }

    if (binding->expression)
        c_free(binding->expression);

    c_free(binding->function_name);

#ifdef USE_LLVM
    rut_graphable_remove_child(binding->function_node);
#endif

    rut_object_free(rig_binding_t, binding);
}

rut_type_t rig_binding_type;

static void
_rig_binding_init_type(void)
{
    rut_type_init(&rig_binding_type, "rig_binding_t", _rig_binding_free);
}

static dependency_t *
find_dependency(rig_binding_t *binding,
                rut_property_t *property)
{
    dependency_t *dependency;

    c_list_for_each(dependency, &binding->dependencies, link) {
        if (dependency->property == property)
            return dependency;
    }

    return NULL;
}

#if defined (RIG_EDITOR_ENABLED) && defined (USE_LLVM)
static void
get_property_codegen_info(rut_property_t *property,
                          const char **type_name,
                          const char **var_decl_pre,
                          const char **var_decl_post,
                          const char **get_val_pre)
{
    switch (property->spec->type) {
    case RUT_PROPERTY_TYPE_ENUM:
        *type_name = "enum";
        *var_decl_pre = "int ";
        *var_decl_post = "";
        *get_val_pre = "int ";
        break;
    case RUT_PROPERTY_TYPE_BOOLEAN:
        *type_name = "boolean";
        *var_decl_pre = "bool ";
        *var_decl_post = "";
        *get_val_pre = "bool ";
        break;
    case RUT_PROPERTY_TYPE_FLOAT:
        *type_name = "float";
        *var_decl_pre = "float ";
        *var_decl_post = "";
        *get_val_pre = "float ";
        break;

    /* FIXME: we want to avoid the use of pointers or "Rut" types in
     * UI logic code... */
    case RUT_PROPERTY_TYPE_OBJECT:
        *type_name = "object";
        *var_decl_pre = "rut_object_t *";
        *var_decl_post = "";
        *get_val_pre = "const rut_object_t *";
        break;
    case RUT_PROPERTY_TYPE_ASSET:
        *type_name = "asset";
        *var_decl_pre = "rig_asset_t *";
        *var_decl_post = "";
        *get_val_pre = "const rig_asset_t *";
        break;
    case RUT_PROPERTY_TYPE_POINTER:
        *type_name = "pointer";
        *var_decl_pre = "void *";
        *var_decl_post = ";\n";
        *get_val_pre = "const void *";
        break;
    case RUT_PROPERTY_TYPE_TEXT:
        *type_name = "text";
        *var_decl_pre = "char *";
        *var_decl_post = "";
        *get_val_pre = "const char *";
        break;
    case RUT_PROPERTY_TYPE_DOUBLE:
        *type_name = "double";
        *var_decl_pre = "double ";
        *var_decl_post = "";
        *get_val_pre = "double ";
        break;
    case RUT_PROPERTY_TYPE_INTEGER:
        *type_name = "integer";
        *var_decl_pre = "int ";
        *var_decl_post = "";
        *get_val_pre = "int ";
        break;
    case RUT_PROPERTY_TYPE_UINT32:
        *type_name = "uint32";
        *var_decl_pre = "uint32_t ";
        *var_decl_post = "";
        *get_val_pre = "uint32_t ";
        break;

    /* FIXME: we don't want to expose the Cogl api... */
    case RUT_PROPERTY_TYPE_QUATERNION:
        *type_name = "quaternion";
        *var_decl_pre = "c_quaternion_t ";
        *var_decl_post = "";
        *get_val_pre = "const c_quaternion_t *";
        break;
    case RUT_PROPERTY_TYPE_VEC3:
        *type_name = "vec3";
        *var_decl_pre = "float ";
        *var_decl_post = "[3]";
        *get_val_pre = "const float *";
        break;
    case RUT_PROPERTY_TYPE_VEC4:
        *type_name = "vec4";
        *var_decl_pre = "float ";
        *var_decl_post = "[4]";
        *get_val_pre = "const float *";
        break;
    case RUT_PROPERTY_TYPE_COLOR:
        *type_name = "color";
        *var_decl_pre = "cg_color_t ";
        *var_decl_post = "";
        *get_val_pre = "const cg_color_t *";
        break;
    }
}

static void
codegen_function_node(rig_binding_t *binding)
{
    rig_engine_t *engine = binding->engine;
    const char *pre;
    const char *post;
    const char *out_type_name;
    const char *out_var_decl_pre;
    const char *out_var_decl_post;
    const char *out_var_get_pre;
    dependency_t *dependency;
    int i;

    get_property_codegen_info(binding->property,
                              &out_type_name,
                              &out_var_decl_pre,
                              &out_var_decl_post,
                              &out_var_get_pre);

    c_string_set_size(engine->codegen_string0, 0);
    c_string_set_size(engine->codegen_string1, 0);

    pre = "\nvoid\n"
          "%s (rut_property_t *_property, void *_user_data)\n"
          "{\n"
          "  rut_property_context_t *_property_ctx = _user_data;\n"
          "  rut_property_t **deps = _property->binding->dependencies;\n"
          "  %sout%s;\n";

    c_string_append_printf(engine->codegen_string0,
                           pre,
                           binding->function_name,
                           out_var_decl_pre,
                           out_var_decl_post);

    c_list_for_each(dependency, &binding->dependencies, link) {
        const char *dep_type_name;
        const char *dep_var_decl_pre;
        const char *dep_var_decl_post;
        const char *dep_get_var_pre;

        get_property_codegen_info(dependency->property,
                                  &dep_type_name,
                                  &dep_var_decl_pre,
                                  &dep_var_decl_post,
                                  &dep_get_var_pre);

        c_string_append_printf(engine->codegen_string0,
                               "  %s %s = rut_property_get_%s (deps[%d]);\n",
                               dep_get_var_pre,
                               dependency->variable_name,
                               dep_type_name,
                               i);
    }

    c_string_append(engine->codegen_string0, "  {\n");

    c_string_set_size(engine->codegen_string1, 0);

    post = "\n"
           "  }\n"
           "  rut_property_set_%s (_property_ctx, _property, out);\n"
           "}\n";

    c_string_append_printf(engine->codegen_string1, post, out_type_name);

    rig_code_node_set_pre(binding->function_node, engine->codegen_string0->str);
    rig_code_node_set_post(binding->function_node,
                           engine->codegen_string1->str);
}
#endif /* RIG_EDITOR_ENABLED */

#ifdef USE_LLVM
void
rig_binding_activate(rig_binding_t *binding)
{
    rig_engine_t *engine = binding->engine;
    rut_property_t **dependencies;
    rut_binding_callback_t callback;
    int n_dependencies;
    dependency_t *dependency;
    int i;

    c_return_if_fail(!binding->active);

    /* XXX: maybe we should only explicitly remove the binding if we know
     * we've previously set a binding. If we didn't previously set a binding
     * then it would indicate a bug if there were some other binding but we'd
     * hide that by removing it here...
     */
    rut_property_remove_binding(binding->property);

    callback = rig_code_resolve_symbol(engine, binding->function_name);
    if (!callback) {
        c_warning("Failed to lookup binding function symbol \"%s\"",
                  binding->function_name);
        return;
    }
    n_dependencies = c_list_length(&binding->dependencies);
    dependencies = c_alloca(sizeof(rut_property_t *) * n_dependencies);

    c_list_for_each(dependency, &binding->dependencies, link) {
        dependencies[i] = dependency->property;
    }

    _rut_property_set_binding_full_array(
        binding->property,
        callback,
        &engine->shell->property_ctx, /* user data */
        NULL, /* destroy */
        dependencies,
        n_dependencies);

    binding->active = true;
}

#else /* USE_LLVM */

void
rig_binding_activate(rig_binding_t *binding)
{
    rig_engine_t *engine = binding->engine;

    c_return_if_fail(!binding->active);

    if (binding->simple_copy) {
        dependency_t *dependency = c_list_first(&binding->dependencies, dependency_t, link);
        if (dependency) {
            if (dependency->property->spec->type ==
                binding->property->spec->type)
            {
                rut_property_set_copy_binding(&engine->shell->property_ctx,
                                              binding->property,
                                              dependency->property);
            } else {
                rut_property_set_cast_scalar_binding(&engine->shell->property_ctx,
                                                     binding->property,
                                                     dependency->property);
            }
        } else
            c_warning("Unable activate simple copy binding with no dependency set");
    } else
        c_warning("Unable to active expression based binding without LLVM support");
}

#endif /* USE_LLVM */
void
rig_binding_deactivate(rig_binding_t *binding)
{
    c_return_if_fail(binding->active);

    rut_property_remove_binding(binding->property);

    binding->active = false;
}

#ifdef USE_LLVM
static void
binding_relink_cb(rig_code_node_t *node, void *user_data)
{
    rig_binding_t *binding = user_data;

    if (binding->active) {
        rig_binding_deactivate(binding);
        rig_binding_activate(binding);
    }
}

static void
generate_function_node(rig_binding_t *binding)
{
    rig_engine_t *engine = binding->engine;

    binding->function_node = rig_code_node_new(engine,
                                               NULL, /* pre */
                                               NULL); /* post */

    rut_graphable_add_child(engine->code_graph, binding->function_node);
    rut_object_unref(binding->function_node);

    rig_code_node_add_link_callback(
        binding->function_node, binding_relink_cb, binding, NULL); /* destroy */

#ifdef RIG_EDITOR_ENABLED
    if (!engine->simulator)
        codegen_function_node(binding);
#endif
}
#endif /* USE_LLVM */

void
rig_binding_remove_dependency(rig_binding_t *binding,
                              rut_property_t *property)
{
    dependency_t *dependency = find_dependency(binding, property);

    c_return_if_fail(dependency);

    remove_and_free_dependency(dependency);

#if defined (RIG_EDITOR_ENABLED) && defined (USE_LLVM)
    if (!binding->engine->simulator)
        codegen_function_node(binding);
#endif
}

void
rig_binding_add_dependency(rig_binding_t *binding,
                           rut_property_t *property,
                           const char *name)
{
    dependency_t *dependency = c_slice_new0(dependency_t);
    rut_object_t *object = property->object;

    dependency->object = rut_object_ref(object);
    dependency->binding = binding;

    dependency->property = property;

    if (name)
        dependency->variable_name = c_strdup(name);

    c_list_insert(binding->dependencies.prev, &dependency->link);

#if defined (RIG_EDITOR_ENABLED) && defined (USE_LLVM)
    if (!binding->engine->simulator)
        codegen_function_node(binding);
#endif
}

char *
rig_binding_get_expression(rig_binding_t *binding)
{
    return binding->expression ? binding->expression : "";
}

void
rig_binding_set_expression(rig_binding_t *binding, const char *expression)
{
    c_return_if_fail(expression);

#ifdef USE_LLVM
    if ((binding->expression && strcmp(binding->expression, expression) == 0))
        return;

    if (binding->expression_node) {
        rig_code_node_remove_child(binding->expression_node);
        binding->expression_node = NULL;
    }

    binding->expression_node = rig_code_node_new(binding->engine,
                                                 NULL, /* pre */
                                                 expression); /* post */

    rig_code_node_add_child(binding->function_node, binding->expression_node);
    rut_object_unref(binding->expression_node);

    if (!binding->engine->simulator)
        codegen_function_node(binding);
#endif
}

void
rig_binding_set_dependency_name(rig_binding_t *binding,
                                rut_property_t *property,
                                const char *name)
{
    dependency_t *dependency = find_dependency(binding, property);

    c_return_if_fail(dependency);

    c_free(dependency->variable_name);

    dependency->variable_name = c_strdup(name);

#if defined (RIG_EDITOR_ENABLED) && defined (USE_LLVM)
    if (!binding->engine->simulator)
        codegen_function_node(binding);
#endif
}

rig_binding_t *
rig_binding_new(rig_engine_t *engine, rut_property_t *property, int binding_id)
{
    rig_binding_t *binding = rut_object_alloc0(
        rig_binding_t, &rig_binding_type, _rig_binding_init_type);

    binding->engine = engine;
    binding->property = property;
    binding->function_name = c_strdup_printf("_binding%d", binding_id);
    binding->binding_id = binding_id;

    c_list_init(&binding->dependencies);

#ifdef USE_LLVM
    generate_function_node(binding);
#endif

    return binding;
}

int
rig_binding_get_id(rig_binding_t *binding)
{
    return binding->binding_id;
}

rig_binding_t *
rig_binding_new_simple_copy(rig_engine_t *engine,
                            rut_property_t *dst_prop,
                            rut_property_t *src_prop)
{
    rig_binding_t *binding = rut_object_alloc0(
        rig_binding_t, &rig_binding_type, _rig_binding_init_type);

    binding->engine = engine;
    binding->property = dst_prop;
    binding->binding_id = -1;

    c_list_init(&binding->dependencies);

    rig_binding_add_dependency(binding, src_prop,
                               NULL /* no variable name for dep */);

    binding->simple_copy = true;

    return binding;
}

int
rig_binding_get_n_dependencies(rig_binding_t *binding)
{
    return c_list_length(&binding->dependencies);
}

void
rig_binding_foreach_dependency(rig_binding_t *binding,
                               void (*callback)(rig_binding_t *binding,
                                                rut_property_t *dependency,
                                                void *user_data),
                               void *user_data)
{
    dependency_t *dependency;

    c_list_for_each(dependency, &binding->dependencies, link) {
        callback(binding, dependency->property, user_data);
    }
}
