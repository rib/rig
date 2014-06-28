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

#include <rut.h>

#include "rig-code.h"
#include "rig-binding.h"
#include "rig-prop-inspector.h"

typedef struct _RigBindingView RigBindingView;

typedef struct _Dependency
{
  RigBindingView *binding_view;

  RutObject *object;
  RutProperty *property;

  bool preview;

  RutBoxLayout *hbox;
  RutText *label;
  RutText *variable_name_label;

} Dependency;

struct _RigBindingView
{
  RutObjectBase _base;

  RigEngine *engine;


  RutGraphableProps graphable;

  RutStack *top_stack;
  RutDragBin *drag_bin;

  RutBoxLayout *vbox;

  RutBoxLayout *dependencies_vbox;

  RutStack *drop_stack;
  RutInputRegion *drop_region;
  RutText *drop_label;

  RigBinding *binding;

  RutText *code_view;

  RutProperty *preview_dependency_prop;
  c_list_t *dependencies;

};

static void
_rig_binding_view_free (void *object)
{
  RigBindingView *binding_view = object;
  //RigControllerView *view = binding_view->view;

  rut_object_unref (binding_view->binding);

  rut_graphable_destroy (binding_view);

  rut_object_free (RigBindingView, binding_view);
}

RutType rig_binding_view_type;

static void
_rig_binding_view_init_type (void)
{
  static RutGraphableVTable graphable_vtable = { 0 };

  static RutSizableVTable sizable_vtable = {
    rut_composite_sizable_set_size,
    rut_composite_sizable_get_size,
    rut_composite_sizable_get_preferred_width,
    rut_composite_sizable_get_preferred_height,
    rut_composite_sizable_add_preferred_size_callback
  };

  RutType *type = &rig_binding_view_type;
#define TYPE RigBindingView

  rut_type_init (type, C_STRINGIFY (TYPE), _rig_binding_view_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_GRAPHABLE,
                      offsetof (TYPE, graphable),
                      &graphable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_SIZABLE,
                      0, /* no implied properties */
                      &sizable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_COMPOSITE_SIZABLE,
                      offsetof (TYPE, top_stack),
                      NULL); /* no vtable */

#undef TYPE
}

static void
remove_dependency (RigBindingView *binding_view,
                   RutProperty *property)
{
  c_list_t *l;

  for (l = binding_view->dependencies; l; l = l->next)
    {
      Dependency *dependency = l->data;
      if (dependency->property == property)
        {
          if (!dependency->preview)
            rig_binding_remove_dependency (binding_view->binding, property);

          rut_box_layout_remove (binding_view->dependencies_vbox, dependency->hbox);
          rut_object_unref (dependency->object);
          c_slice_free (Dependency, dependency);

          return;
        }
    }

  c_warn_if_reached ();
}

static void
on_dependency_delete_button_click_cb (RutIconButton *button, void *user_data)
{
  Dependency *dependency = user_data;

  remove_dependency (dependency->binding_view,
                     dependency->property);
}

static void
dependency_name_changed_cb (RutText *text,
                            void *user_data)
{
  Dependency *dependency = user_data;

  rig_binding_set_dependency_name (dependency->binding_view->binding,
                                   dependency->property,
                                   rut_text_get_text (text));
}

static Dependency *
add_dependency (RigBindingView *binding_view,
                RutProperty *property,
                bool drag_preview)
{
  Dependency *dependency = c_slice_new0 (Dependency);
  RutProperty *label_prop;
  char *dependency_label;
  RutObject *object = property->object;
  RutBin *bin;
  const char *component_str = NULL;
  const char *label_str;
  RutContext *ctx = binding_view->engine->ctx;

  dependency->object = rut_object_ref (object);
  dependency->binding_view = binding_view;

  dependency->property = property;

  dependency->preview = drag_preview;

  dependency->hbox = rut_box_layout_new (ctx,
                                         RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);

  if (!drag_preview)
    {
      RutIconButton *delete_button =
        rut_icon_button_new (ctx,
                             NULL, /* label */
                             0, /* ignore label position */
                             "delete-white.png", /* normal */
                             "delete-white.png", /* hover */
                             "delete.png", /* active */
                             "delete-white.png"); /* disabled */
      rut_box_layout_add (dependency->hbox, false, delete_button);
      rut_object_unref (delete_button);
      rut_icon_button_add_on_click_callback (delete_button,
                                             on_dependency_delete_button_click_cb,
                                             dependency,
                                             NULL); /* destroy notify */
    }

  /* XXX:
   * We want a clear way of showing a relationship to an object +
   * property here.
   *
   * Just showing a property name isn't really enough
   * */

  if (rut_object_is (object, RUT_TRAIT_ID_COMPONENTABLE))
    {
      RutComponentableProps *component =
        rut_object_get_properties (object, RUT_TRAIT_ID_COMPONENTABLE);
      RigEntity *entity = component->entity;
      label_prop = rut_introspectable_lookup_property (entity, "label");
      /* XXX: Hack to drop the "Rut" prefix from the name... */
      component_str = rut_object_get_type_name (object) + 3;
    }
  else
    label_prop = rut_introspectable_lookup_property (object, "label");

  if (label_prop)
    label_str = rut_property_get_text (label_prop);
  else
    label_str = "<Object>";

  if (component_str)
    {
      dependency_label = c_strdup_printf ("%s::%s::%s",
                                          label_str,
                                          component_str,
                                          property->spec->name);
    }
  else
    {
      dependency_label = c_strdup_printf ("%s::%s",
                                          label_str,
                                          property->spec->name);
    }

  dependency->label = rut_text_new_with_text (ctx, NULL,
                                              dependency_label);
  c_free (dependency_label);
  rut_box_layout_add (dependency->hbox, false, dependency->label);
  rut_object_unref (dependency->label);

  bin = rut_bin_new (ctx);
  rut_bin_set_left_padding (bin, 20);
  rut_box_layout_add (dependency->hbox, false, bin);
  rut_object_unref (bin);

  /* TODO: Check if the name is unique for the current binding... */
  dependency->variable_name_label =
    rut_text_new_with_text (ctx, NULL,
                            property->spec->name);
  rut_text_set_editable (dependency->variable_name_label, true);
  rut_bin_set_child (bin, dependency->variable_name_label);
  rut_object_unref (dependency->variable_name_label);

  rut_text_add_text_changed_callback (dependency->variable_name_label,
                                      dependency_name_changed_cb,
                                      dependency,
                                      NULL); /* destroy notify */

  binding_view->dependencies =
    c_list_prepend (binding_view->dependencies, dependency);

  rut_box_layout_add (binding_view->dependencies_vbox,
                      false, dependency->hbox);
  rut_object_unref (dependency->hbox);

  if (!drag_preview)
    {
      rig_binding_add_dependency (binding_view->binding,
                                  property,
                                  property->spec->name);
    }

  return dependency;
}

static RutInputEventStatus
drop_region_input_cb (RutInputRegion *region,
                      RutInputEvent *event,
                      void *user_data)
{
  RigBindingView *binding_view = user_data;
  RutContext *ctx = binding_view->engine->ctx;

  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_DROP_OFFER)
    {
      RutObject *payload = rut_drop_offer_event_get_payload (event);

      if (rut_object_get_type (payload) == &rig_prop_inspector_type)
        {
          RutProperty *property = rig_prop_inspector_get_property (payload);

          c_print ("Drop Offer\n");

          binding_view->preview_dependency_prop = property;
          add_dependency (binding_view, property, true);

          rut_shell_take_drop_offer (ctx->shell,
                                     binding_view->drop_region);
          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }
  else if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_DROP)
    {
      RutObject *payload = rut_drop_offer_event_get_payload (event);

      /* We should be able to assume a _DROP_OFFER was accepted before
       * we'll be sent a _DROP */
      g_warn_if_fail (binding_view->preview_dependency_prop);

      remove_dependency (binding_view, binding_view->preview_dependency_prop);
      binding_view->preview_dependency_prop = NULL;

      if (rut_object_get_type (payload) == &rig_prop_inspector_type)
        {
          RutProperty *property = rig_prop_inspector_get_property (payload);

          add_dependency (binding_view, property, false);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }
  else if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_DROP_CANCEL)
    {
      /* NB: This may be cleared by a _DROP */
      if (binding_view->preview_dependency_prop)
        {
          remove_dependency (binding_view, binding_view->preview_dependency_prop);
          binding_view->preview_dependency_prop = NULL;
        }

      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
text_changed_cb (RutText *text,
                 void *user_data)
{
  RigBindingView *binding_view = user_data;

  rig_binding_set_expression (binding_view->binding,
                              rut_text_get_text (text));
}

RigBindingView *
rig_binding_view_new (RigEngine *engine,
                      RutProperty *property,
                      RigBinding *binding)
{
  RutContext *ctx = engine->ctx;
  RigBindingView *binding_view =
    rut_object_alloc0 (RigBindingView,
                       &rig_binding_view_type,
                       _rig_binding_view_init_type);
  RutBin *dependencies_indent;
  RutBoxLayout *hbox;
  RutText *equals;

  binding_view->engine = engine;

  rut_graphable_init (binding_view);

  binding_view->binding = rut_object_ref (binding);

  binding_view->top_stack = rut_stack_new (ctx, 1, 1);
  rut_graphable_add_child (binding_view, binding_view->top_stack);
  rut_object_unref (binding_view->top_stack);

  binding_view->vbox =
    rut_box_layout_new (ctx, RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);
  rut_stack_add (binding_view->top_stack, binding_view->vbox);
  rut_object_unref (binding_view->vbox);

  binding_view->drop_stack = rut_stack_new (ctx, 1, 1);
  rut_box_layout_add (binding_view->vbox, false, binding_view->drop_stack);
  rut_object_unref (binding_view->drop_stack);

  binding_view->drop_label = rut_text_new_with_text (ctx, NULL, "Dependencies...");
  rut_stack_add (binding_view->drop_stack, binding_view->drop_label);
  rut_object_unref (binding_view->drop_label);

  binding_view->drop_region =
    rut_input_region_new_rectangle (0, 0, 1, 1,
                                    drop_region_input_cb,
                                    binding_view);
  rut_stack_add (binding_view->drop_stack, binding_view->drop_region);
  rut_object_unref (binding_view->drop_region);

  dependencies_indent = rut_bin_new (ctx);
  rut_box_layout_add (binding_view->vbox, false, dependencies_indent);
  rut_object_unref (dependencies_indent);
  rut_bin_set_left_padding (dependencies_indent, 10);

  binding_view->dependencies_vbox =
    rut_box_layout_new (ctx, RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);
  rut_bin_set_child (dependencies_indent, binding_view->dependencies_vbox);
  rut_object_unref (binding_view->dependencies_vbox);

  hbox = rut_box_layout_new (ctx, RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);
  rut_box_layout_add (binding_view->vbox, false, hbox);
  rut_object_unref (hbox);

  equals = rut_text_new_with_text (ctx, "bold", "=");
  rut_box_layout_add (hbox, false, equals);
  rut_object_unref (equals);

  binding_view->code_view = rut_text_new_with_text (ctx, "monospace", "");
  rut_text_set_hint_text (binding_view->code_view, "Expression...");
  rut_text_set_editable (binding_view->code_view, true);
  rut_box_layout_add (hbox, false, binding_view->code_view);
  rut_object_unref (binding_view->code_view);

  rut_text_add_text_changed_callback (binding_view->code_view,
                                      text_changed_cb,
                                      binding_view,
                                      NULL); /* destroy notify */
  return binding_view;
}
