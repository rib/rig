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

#include <config.h>

#include <dlfcn.h>
#include <clib.h>

#include <rut.h>

#include "rig-code.h"
#include "rig-llvm.h"


struct _RigCodeNode
{
  RutObjectBase _base;

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
    c_free (node->pre);
  if (node->post)
    c_free (node->post);

  rut_graphable_destroy (node);

  rut_object_free (RigCodeNode, object);
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

  rut_type_init (type, C_STRINGIFY (TYPE), _rig_code_node_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_GRAPHABLE,
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

  node->engine = engine;

  rut_graphable_init (node);

  rut_list_init (&node->link_closures);

  /* Note: in device mode and in the simulator we avoid
   * tracking any source code. */

  if (pre)
    node->pre = c_strdup (pre);

  if (post)
    node->post = c_strdup (post);

  return node;
}

static RutTraverseVisitFlags
code_generate_pre_cb (RutObject *object,
                      int depth,
                      void *user_data)
{
  RigCodeNode *node = object;
  CString *code = user_data;

  if (node->pre)
    c_string_append (code, node->pre);

  return RUT_TRAVERSE_VISIT_CONTINUE;
}

static RutTraverseVisitFlags
code_generate_post_cb (RutObject *object,
                       int depth,
                       void *user_data)
{
  RigCodeNode *node = object;
  CString *code = user_data;

  if (node->post)
    c_string_append (code, node->post);

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

void
rig_code_update_dso (RigEngine *engine, uint8_t *data, int len)
{
  char *tmp_filename;
  GError *error = NULL;
  int dso_fd;
  RutException *e = NULL;
  GModule *module;

  if (engine->code_dso_module)
    {
      g_module_close (engine->code_dso_module);
      engine->code_dso_module = NULL;
    }

  if (!data)
    return;

  dso_fd = g_file_open_tmp (NULL, &tmp_filename, &error);
  if (dso_fd == -1)
    {
      c_warning ("Failed to open a temporary file for shared object: %s",
                 error->message);
      g_error_free (error);
      return;
    }

  if (!rut_os_write (dso_fd, data, len, &e))
    {
      c_warning ("Failed to write shared object: %s", e->message);
      rut_exception_free (e);
      return;
    }

  module = g_module_open (tmp_filename, G_MODULE_BIND_LAZY);
  if (!module)
    {
      g_module_close (module);
      c_warning ("Failed to open shared object");
      return;
    }
  engine->code_dso_module = module;

  rut_graphable_traverse (engine->code_graph,
                          RUT_TRAVERSE_DEPTH_FIRST,
                          NULL,
                          notify_link_cb,
                          NULL);
}

#ifdef RIG_EDITOR_ENABLED
static void
recompile (RigEngine *engine)
{
  RigLLVMModule *module;
  uint8_t *dso_data;
  size_t dso_len;

  c_return_if_fail (engine->need_recompile);
  engine->need_recompile = false;

  /* To avoid fragmentation we re-use one allocation for all codegen... */
  c_string_set_size (engine->code_string, 0);

  rut_graphable_traverse (engine->code_graph,
                          RUT_TRAVERSE_DEPTH_FIRST,
                          code_generate_pre_cb,
                          code_generate_post_cb,
                          engine->code_string);

  module = rig_llvm_compile_to_dso (engine->code_string->str,
                                    &engine->code_dso_filename,
                                    &dso_data,
                                    &dso_len);

  if (module)
    {
      rig_frontend_update_simulator_dso (engine->frontend,
                                         dso_data, dso_len);

#warning "fix freeing of llvm module - crashes due to null llvm context impl pointer"
      //rut_object_unref (module);
    }
}

static void
recompile_pre_paint_callback (RutObject *_null_graphable,
                              void *user_data)
{
  recompile (user_data);
}
#endif /* RIG_EDITOR_ENABLED */

static void
queue_recompile (RigEngine *engine)
{
  c_return_if_fail (engine->frontend &&
                    engine->frontend_id == RIG_FRONTEND_ID_EDITOR);

#ifdef RIG_EDITOR_ENABLED
  if (engine->need_recompile)
    return;

  engine->need_recompile = true;

  rut_shell_add_pre_paint_callback (engine->shell,
                                    NULL, /* graphable */
                                    recompile_pre_paint_callback,
                                    engine);

  rut_shell_queue_redraw (engine->shell);
#endif
}

void
rig_code_node_set_pre (RigCodeNode *node,
                       const char *pre)
{
  if (node->pre)
    c_free (node->pre);

  node->pre = c_strdup (pre);

  queue_recompile (node->engine);
}

void
rig_code_node_set_post (RigCodeNode *node,
                        const char *post)
{
  if (node->post)
    c_free (node->post);

  node->post = c_strdup (post);

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

void *
rig_code_resolve_symbol (RigEngine *engine, const char *name)
{
  if (engine->code_dso_module)
    {
      void *sym;
      if (g_module_symbol (engine->code_dso_module, name, &sym))
        return sym;
      else
        return NULL;
    }
  else
    return NULL;
}

void
_rig_code_init (RigEngine *engine)
{
#ifdef RIG_EDITOR_ENABLED
  engine->code_string = c_string_new ("");
  engine->codegen_string0 = c_string_new ("");
  engine->codegen_string1 = c_string_new ("");

  engine->next_code_id = 1;
  engine->need_recompile = false;
#endif

  engine->code_graph = rig_code_node_new (engine,
                                          "typedef struct _RutProperty RutProperty;\n",
                                          "");
}

void
_rig_code_fini (RigEngine *engine)
{
#ifdef RIG_EDITOR_ENABLED
  c_string_free (engine->code_string, TRUE);
  engine->code_string = NULL;

  c_string_free (engine->codegen_string0, TRUE);
  engine->codegen_string0 = NULL;

  c_string_free (engine->codegen_string1, TRUE);
  engine->codegen_string1 = NULL;

  if (engine->code_dso_filename)
    c_free (engine->code_dso_filename);

  rut_shell_remove_pre_paint_callback (engine->shell,
                                       recompile_pre_paint_callback,
                                       engine);
#endif

  rut_object_unref (engine->code_graph);
  engine->code_graph = NULL;

  if (engine->code_dso_module)
    g_module_close (engine->code_dso_module);
}
