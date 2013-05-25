/*
 * Rig
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

#include <config.h>

#include <rut.h>

#include "rig-code.h"

typedef struct _RigBinding RigBinding;

typedef struct _Dependency
{
  RigBinding *binding;

  RutObject *object;
  RutProperty *property;

  char *variable_name;

} Dependency;

struct _RigBinding
{
  RutObjectBase _base;

  RigEngine *engine;

  RutProperty *property;

  char *expression;

  char *function_name;

  RigCodeNode *function_node;
  RigCodeNode *expression_node;

  GList *dependencies;
};

static void
_rig_binding_free (void *object)
{
  RigBinding *binding = object;

  if (binding->expression)
    g_free (binding->expression);

  g_free (binding->function_name);

  rut_graphable_remove_child (binding->function_node);

  rut_object_free (RigBinding, binding);
}

RutType rig_binding_type;

static void
_rig_binding_init_type (void)
{
  rut_type_init (&rig_binding_type, "RigBinding", _rig_binding_free);
}

static Dependency *
find_dependency (RigBinding *binding,
                 RutProperty *property)
{
  GList *l;

  for (l = binding->dependencies; l; l = l->next)
    {
      Dependency *dependency = l->data;
      if (dependency->property == property)
        return dependency;
    }

  return NULL;
}

static void
get_property_codegen_info (RutProperty *property,
                           const char **type_name,
                           const char **var_decl_pre,
                           const char **var_decl_post,
                           const char **get_val_pre)
{
  switch (property->spec->type)
    {
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
      *var_decl_pre = "RutObject *";
      *var_decl_post = "";
      *get_val_pre = "const RutObject *";
      break;
    case RUT_PROPERTY_TYPE_ASSET:
      *type_name = "asset";
      *var_decl_pre = "RutAsset *";
      *var_decl_post = "";
      *get_val_pre = "const RutAsset *";
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
      *var_decl_pre = "CoglQuaternion ";
      *var_decl_post = "";
      *get_val_pre = "const CoglQuaternion *";
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
      *var_decl_pre = "CoglColor ";
      *var_decl_post = "";
      *get_val_pre = "const CoglColor *";
      break;
    }
}

static void
update_function_node (RigBinding *binding)
{
  RigEngine *engine = binding->engine;
  const char *pre;
  const char *post;
  const char *out_type_name;
  const char *out_var_decl_pre;
  const char *out_var_decl_post;
  const char *out_var_get_pre;
  GList *l;
  int i;

  get_property_codegen_info (binding->property,
                             &out_type_name,
                             &out_var_decl_pre,
                             &out_var_decl_post,
                             &out_var_get_pre);

  g_string_set_size (engine->codegen_string0, 0);
  g_string_set_size (engine->codegen_string1, 0);

  pre =
    "\nvoid\n"
    "%s (RutProperty *_property, void *_user_data)\n"
    "{\n"
    "  RutPropertyContext *_property_ctx = _user_data;\n"
    "  RutProperty **deps = _property->binding->dependencies;\n"
    "  %sout%s;\n";

  g_string_append_printf (engine->codegen_string0, pre,
                          binding->function_name,
                          out_var_decl_pre,
                          out_var_decl_post);

  for (l = binding->dependencies, i = 0; l; l = l->next, i++)
    {
      Dependency *dependency = l->data;
      const char *dep_type_name;
      const char *dep_var_decl_pre;
      const char *dep_var_decl_post;
      const char *dep_get_var_pre;

      get_property_codegen_info (dependency->property,
                                 &dep_type_name,
                                 &dep_var_decl_pre,
                                 &dep_var_decl_post,
                                 &dep_get_var_pre);

      g_string_append_printf (engine->codegen_string0,
                              "  %s %s = rut_property_get_%s (deps[%d]);\n",
                              dep_get_var_pre,
                              dependency->variable_name,
                              dep_type_name,
                              i);
    }

  g_string_append (engine->codegen_string0, "  {\n");

  g_string_set_size (engine->codegen_string1, 0);

  post =
    "\n"
    "  }\n"
    "  rut_property_set_%s (_property_ctx, _property, out);\n"
    "}\n";

  g_string_append_printf (engine->codegen_string1, post,
                          out_type_name);

  rig_code_node_set_pre (binding->function_node,
                         engine->codegen_string0->str);
  rig_code_node_set_post (binding->function_node,
                          engine->codegen_string1->str);
}

static void
binding_relink_cb (RigCodeNode *node,
                   void *user_data)
{
  RigBinding *binding = user_data;
  RigEngine *engine = binding->engine;
  RutProperty **dependencies;
  RutBindingCallback callback;
  int n_dependencies;
  GList *l;
  int i;

  /* XXX: maybe we should only explicitly remove the binding if we know
   * we've previously set a binding. If we didn't previously set a binding
   * then it would indicate a bug if there were some other binding but we'd
   * hide that by removing it here...
   */
  rut_property_remove_binding (binding->property);

  callback = rig_code_resolve_symbol (engine, binding->function_name);
  if (!callback)
    {
      g_warning ("Failed to lookup binding function symbol \"%s\"",
                 binding->function_name);
      return;
    }

  n_dependencies = g_list_length (binding->dependencies);
  dependencies = g_alloca (sizeof (RutProperty *) * n_dependencies);

  for (l = binding->dependencies, i = 0;
       l;
       l = l->next, i++)
    {
      Dependency *dependency = l->data;
      dependencies[i] = dependency->property;
    }

  _rut_property_set_binding_full_array (binding->property,
                                        callback,
                                        &engine->ctx->property_ctx, /* user data */
                                        NULL, /* destroy */
                                        dependencies,
                                        n_dependencies);
}

static void
generate_function_node (RigBinding *binding)
{
  RigEngine *engine = binding->engine;

  binding->function_node =
    rig_code_node_new (engine,
                       engine->codegen_string0->str,
                       engine->codegen_string1->str);

  rut_graphable_add_child (engine->code_graph, binding->function_node);
  rut_object_unref (binding->function_node);

  rig_code_node_add_link_callback (binding->function_node,
                                   binding_relink_cb,
                                   binding,
                                   NULL); /* destroy */

  update_function_node (binding);
}

void
rig_binding_remove_dependency (RigBinding *binding,
                               RutProperty *property)
{
  Dependency *dependency = find_dependency (binding, property);

  g_return_if_fail (dependency);

  g_free (dependency->variable_name);
  g_slice_free (Dependency, dependency);

  update_function_node (binding);
}

void
rig_binding_add_dependency (RigBinding *binding,
                            RutProperty *property,
                            const char *name)
{
  Dependency *dependency = g_slice_new0 (Dependency);
  RutObject *object = property->object;

  dependency->object = rut_object_ref (object);
  dependency->binding = binding;

  dependency->property = property;

  dependency->variable_name = g_strdup (name);

  binding->dependencies =
    g_list_prepend (binding->dependencies, dependency);

  update_function_node (binding);
}

void
rig_binding_set_expression (RigBinding *binding,
                            const char *expression)
{
  g_return_if_fail (expression);

  if ((binding->expression &&
       strcmp (binding->expression, expression) == 0))
    return;

  if (binding->expression_node)
    {
      rig_code_node_remove_child (binding->expression_node);
      binding->expression_node = NULL;
    }

  binding->expression_node = rig_code_node_new (binding->engine,
                                                NULL, /* pre */
                                                expression); /* post */

  rig_code_node_add_child (binding->function_node,
                           binding->expression_node);
  rut_object_unref (binding->expression_node);

  update_function_node (binding);
}

void
rig_binding_set_dependency_name (RigBinding *binding,
                                 RutProperty *property,
                                 const char *name)
{
  Dependency *dependency = find_dependency (binding, property);

  g_return_if_fail (dependency);

  g_free (dependency->variable_name);

  dependency->variable_name = g_strdup (name);

  update_function_node (binding);
}

RigBinding *
rig_binding_new (RigEngine *engine,
                 RutProperty *property,
                 int id)
{
  RigBinding *binding =
    rut_object_alloc0 (RigBinding,
                       &rig_binding_type,
                       _rig_binding_init_type);

  binding->engine = engine;
  binding->property = property;
  binding->function_name = g_strdup_printf ("_binding%d", id);

  generate_function_node (binding);

  return binding;
}
