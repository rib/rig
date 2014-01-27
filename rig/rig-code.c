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

#include <dlfcn.h>
#include <glib.h>

#include <rut.h>

#include "rig-code.h"
#include "rig-llvm.h"


struct _RigCodeNode
{
  RutObjectProps _parent;
  int ref_count;

  RigEngine *engine;

  RutGraphableProps graphable;

  RutList link_closures;

  char *pre;
  char *post;
};

static void
_rig_code_node_free (void *object)
{
  RigCodeNode *node = object;

  if (node->pre)
    g_free (node->pre);
  if (node->post)
    g_free (node->post);

  rut_graphable_destroy (node);

  g_slice_free (RigCodeNode, object);
}

RutType rig_code_node_type;

static void
_rig_code_node_init_type (void)
{
  static RutGraphableVTable graphable_vtable = {
      NULL, /* child remove */
      NULL, /* child add */
      NULL /* parent changed */
  };

  RutType *type = &rig_code_node_type;
#define TYPE RigCodeNode

  rut_type_init (type, G_STRINGIFY (TYPE));
  rut_type_add_refable (type, ref_count, _rig_code_node_free);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (TYPE, graphable),
                          &graphable_vtable);

#undef TYPE
}

RigCodeNode *
rig_code_node_new (RigEngine *engine,
                   const char *pre,
                   const char *post)
{
  RigCodeNode *node =
    rut_object_alloc0 (RigCodeNode, &rig_code_node_type,
                       _rig_code_node_init_type);

  node->ref_count = 1;

  node->engine = engine;

  rut_graphable_init (node);

  rut_list_init (&node->link_closures);

  node->pre = g_strdup (pre);
  node->post = g_strdup (post);

  return node;
}

static RutTraverseVisitFlags
code_generate_pre_cb (RutObject *object,
                      int depth,
                      void *user_data)
{
  RigCodeNode *node = object;
  GString *code = user_data;

  if (node->pre)
    g_string_append (code, node->pre);

  return RUT_TRAVERSE_VISIT_CONTINUE;
}

static RutTraverseVisitFlags
code_generate_post_cb (RutObject *object,
                       int depth,
                       void *user_data)
{
  RigCodeNode *node = object;
  GString *code = user_data;

  if (node->post)
    g_string_append (code, node->post);

  return RUT_TRAVERSE_VISIT_CONTINUE;
}

static RutTraverseVisitFlags
notify_link_cb (RutObject *object,
                int depth,
                void *user_data)
{
  RigCodeNode *node = object;

  rut_closure_list_invoke (&node->link_closures,
                           RigCodeNodeLinkCallback,
                           node);

  return RUT_TRAVERSE_VISIT_CONTINUE;
}

static void
recompile (RigEngine *engine)
{
  RigLLVMModule *module;
  uint8_t *dso_data;
  size_t dso_len;
  void *handle;

  g_return_if_fail (engine->need_recompile);
  engine->need_recompile = false;

  /* To avoid fragmentation we re-use one allocating for codegen... */
  g_string_set_size (engine->code_string, 0);

  rut_graphable_traverse (engine->code_graph,
                          RUT_TRAVERSE_DEPTH_FIRST,
                          code_generate_pre_cb,
                          code_generate_post_cb,
                          engine->code_string);

  module = rig_llvm_compile_to_dso (engine->code_string->str,
                                    &engine->code_dso_filename,
                                    &dso_data,
                                    &dso_len);
  //rut_refable_unref (module);
#warning "fix freeing of llvm module - crashes due to null llvm context impl pointer"

  handle = dlopen (engine->code_dso_filename, RTLD_LAZY);
  if (handle)
    {
      if (engine->code_dso_handle)
        dlclose (engine->code_dso_handle);
      engine->code_dso_handle = handle;

      rut_graphable_traverse (engine->code_graph,
                              RUT_TRAVERSE_DEPTH_FIRST,
                              NULL,
                              notify_link_cb,
                              NULL);
    }
}

void *
rig_code_resolve_symbol (RigEngine *engine, const char *name)
{
  if (engine->code_dso_handle)
    return dlsym (engine->code_dso_handle, name);
  else
    return NULL;
}

static void
recompile_pre_paint_callback (RutObject *_null_graphable,
                              void *user_data)
{
  recompile (user_data);
}

static void
queue_recompile (RigEngine *engine)
{
  if (engine->need_recompile)
    return;

  engine->need_recompile = true;

#warning "TODO: use a separate thread for compiling code instead a pre_paint callback"

  rut_shell_add_pre_paint_callback (engine->shell,
                                    NULL, /* graphable */
                                    recompile_pre_paint_callback,
                                    engine);

  rut_shell_queue_redraw (engine->shell);
}

void
rig_code_node_set_pre (RigCodeNode *node,
                       const char *pre)
{
  if (node->pre)
    g_free (node->pre);

  node->pre = g_strdup (pre);

  queue_recompile (node->engine);
}

void
rig_code_node_set_post (RigCodeNode *node,
                        const char *post)
{
  if (node->post)
    g_free (node->post);

  node->post = g_strdup (post);

  queue_recompile (node->engine);
}

void
rig_code_node_add_child (RigCodeNode *node,
                         RigCodeNode *child)
{
  rut_graphable_add_child (node, child);

  queue_recompile (node->engine);
}

void
rig_code_node_remove_child (RigCodeNode *child)
{
  queue_recompile (child->engine);

  rut_graphable_remove_child (child);
}

RutClosure *
rig_code_node_add_link_callback (RigCodeNode *node,
                                 RigCodeNodeLinkCallback callback,
                                 void *user_data,
                                 RutClosureDestroyCallback destroy)
{
  return rut_closure_list_add (&node->link_closures,
                               callback,
                               user_data,
                               destroy);
}

void
_rig_code_init (RigEngine *engine)
{
  engine->code_string = g_string_new ("");
  engine->codegen_string0 = g_string_new ("");
  engine->codegen_string1 = g_string_new ("");

  engine->code_graph = rig_code_node_new (engine,
                                          "typedef struct _RutProperty RutProperty;\n",
                                          "");

  engine->next_code_id = 1;
  engine->need_recompile = false;
}

void
_rig_code_fini (RigEngine *engine)
{
  g_string_free (engine->code_string, TRUE);
  engine->code_string = NULL;

  g_string_free (engine->codegen_string0, TRUE);
  engine->codegen_string0 = NULL;

  g_string_free (engine->codegen_string1, TRUE);
  engine->codegen_string1 = NULL;

  rut_refable_unref (engine->code_graph);
  engine->code_graph = NULL;

  if (engine->code_dso_filename)
    g_free (engine->code_dso_filename);

  rut_shell_remove_pre_paint_callback (engine->shell,
                                       recompile_pre_paint_callback,
                                       engine);

  if (engine->code_dso_handle)
    dlclose (engine->code_dso_handle);
}
