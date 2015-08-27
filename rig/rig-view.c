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

#include "rig-engine.h"
#include "rig-view.h"

struct _rig_view_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    c_list_t preferred_size_cb_list;

    rut_box_layout_t *vbox;
    rut_box_layout_t *hbox;
    rut_closure_t *vbox_preferred_size_closure;

    float width, height;

    rut_graphable_props_t graphable;
};

static void
_rig_view_free(void *object)
{
    rig_view_t *view = object;

    rut_closure_list_disconnect_all_FIXME(&view->preferred_size_cb_list);

    // rig_view_set_child (view, NULL);

    rut_shell_remove_pre_paint_callback_by_graphable(view->shell,
                                                     view);

    rut_graphable_destroy(view);

    rut_graphable_remove_child(view->hbox);
    rut_graphable_remove_child(view->vbox);

    c_slice_free(rig_view_t, view);
}

static void
allocate_cb(rut_object_t *graphable, void *user_data)
{
    rig_view_t *view = RIG_VIEW(graphable);
    rut_sizable_set_size(view->vbox, view->width, view->height);
}

static void
queue_allocation(rig_view_t *view)
{
    rut_shell_add_pre_paint_callback(
        view->shell, view, allocate_cb, NULL /* user_data */);
}

static void
rig_view_set_size(void *object, float width, float height)
{
    rig_view_t *view = object;

    if (width == view->width && height == view->height)
        return;

    view->width = width;
    view->height = height;

    queue_allocation(view);
}

static void
rig_view_get_preferred_width(void *sizable,
                             float for_height,
                             float *min_width_p,
                             float *natural_width_p)
{
    rig_view_t *view = RIG_VIEW(sizable);

    rut_sizable_get_preferred_width(
        view->vbox, for_height, min_width_p, natural_width_p);
}

static void
rig_view_get_preferred_height(void *sizable,
                              float for_width,
                              float *min_height_p,
                              float *natural_height_p)
{
    rig_view_t *view = RIG_VIEW(sizable);

    rut_sizable_get_preferred_height(
        view->vbox, for_width, min_height_p, natural_height_p);
}

static rut_closure_t *
rig_view_add_preferred_size_callback(void *object,
                                     rut_sizeable_preferred_size_callback_t cb,
                                     void *user_data,
                                     rut_closure_destroy_callback_t destroy)
{
    rig_view_t *view = object;

    return rut_closure_list_add_FIXME(
        &view->preferred_size_cb_list, cb, user_data, destroy);
}

static void
rig_view_get_size(void *object, float *width, float *height)
{
    rig_view_t *view = object;

    *width = view->width;
    *height = view->height;
}

rut_type_t rig_view_type;

static void
_rig_view_init_type(void)
{
    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child removed */
                                                       NULL, /* child addded */
                                                       NULL /* parent changed */
    };

    static rut_sizable_vtable_t sizable_vtable = {
        rig_view_set_size,                   rig_view_get_size,
        rig_view_get_preferred_width,        rig_view_get_preferred_height,
        rig_view_add_preferred_size_callback
    };

    rut_type_t *type = &rig_view_type;
#define TYPE rig_view_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_view_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_GRAPHABLE,
                       offsetof(TYPE, graphable),
                       &graphable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_SIZABLE,
                       0, /* no implied properties */
                       &sizable_vtable);

#undef TYPE
}

rig_view_t *
rig_view_new(rig_engine_t *engine)
{
    rut_shell_t *shell = engine->shell;
    rig_view_t *view =
        rut_object_alloc0(rig_view_t, &rig_view_type, _rig_view_init_type);

    view->shell = shell;

    c_list_init(&view->preferred_size_cb_list);

    rut_graphable_init(view);

    view->vbox = rut_box_layout_new(shell, RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);
    rut_graphable_add_child(view, view->vbox);
    rut_object_unref(view->vbox);
    view->hbox = rut_box_layout_new(shell, RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);
    rut_graphable_add_child(view->vbox, view->hbox);
    rut_object_unref(view->hbox);

    return view;
}

#if 0
static void
preferred_size_changed (rig_view_t *view)
{
    rut_closure_list_invoke (&view->preferred_size_cb_list,
                             rut_sizeable_preferred_size_callback_t,
                             view);
}

static void
child_preferred_size_cb (rut_object_t *sizable,
                         void *user_data)
{
    rig_view_t *view = user_data;

    preferred_size_changed (view);
    queue_allocation (view);
}

void
rig_view_set_child (rig_view_t *view,
                    rut_object_t *child_widget)
{
    if (child_widget)
        rut_object_ref (child_widget);

    if (view->child)
    {
        rut_graphable_remove_child (view->child);
        rut_closure_disconnect_FIXME (view->vbox_preferred_size_closure);
        view->vbox_preferred_size_closure = NULL;
        rut_object_unref (view->child);
    }

    view->child = child_widget;
    rut_graphable_add_child (view->child_transform, child_widget);

    if (child_widget)
    {
        view->vbox_preferred_size_closure =
            rut_sizable_add_preferred_size_callback (child_widget,
                                                     child_preferred_size_cb,
                                                     view,
                                                     NULL /* destroy */);
    }

    preferred_size_changed (view);
    queue_allocation (view);
}
#endif
