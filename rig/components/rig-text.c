/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2014  Intel Corporation
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

#include <rut.h>

#include "rig-entity.h"
#include "rig-entity-inlines.h"
#include "rig-text.h"
#include "rig-engine.h"
#include "rig-text-engine.h"
#include "rig-text-engine-private.h"

static const uint32_t default_text_color = 0x000000ff;

static rig_property_spec_t _rig_text_prop_specs[] = {
    { .name = "text",
      .type = RUT_PROPERTY_TYPE_TEXT,
      .getter.text_type = rig_text_get_text,
      .setter.text_type = rig_text_set_text,
      .nick = "Text",
      .blurb = "The text to render",
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
    },
    { .name = "font-family",
      .type = RUT_PROPERTY_TYPE_TEXT,
      .getter.text_type = rig_text_get_font_family,
      .setter.text_type = rig_text_set_font_family,
      .nick = "Font Family",
      .blurb = "The font family to be used by the text",
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
    },
    { .name = "font-size",
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rig_text_t, font_size),
      .setter.float_type = rig_text_set_font_size },
    { .name = "color",
      .type = RUT_PROPERTY_TYPE_COLOR,
      .getter.color_type = rig_text_get_color,
      .setter.color_type = rig_text_set_color,
      .nick = "Font Color",
      .blurb = "Color of the font used by the text",
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .default_value = { .pointer = &default_text_color } },
    { .name = "width",
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rig_text_t, width),
      .setter.float_type = rig_text_set_width },
    { .name = "height",
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rig_text_t, height),
      .setter.float_type = rig_text_set_height },
    { 0 }
};

static void
_rig_text_free(void *object)
{
    rig_text_t *text = object;

#ifdef RIG_ENABLE_DEBUG
    {
        rut_componentable_props_t *component =
            rut_object_get_properties(object, RUT_TRAIT_ID_COMPONENTABLE);
        c_return_if_fail(!component->parented);
    }
#endif

    rut_object_unref(text->text_engine);

    if (text->text)
        c_free(text->text);
    if (text->font_family)
        c_free(text->font_family);

    rut_closure_list_remove_all(&text->preferred_size_cb_list);

    rut_object_free(rig_text_t, text);
}

static rut_object_t *
_rig_text_copy(rut_object_t *object)
{
    rig_text_t *text = object;
    rig_engine_t *engine = rig_component_props_get_engine(&text->component);
    rig_text_t *copy = rig_text_new(engine);
    rig_property_context_t *prop_ctx =
        rig_component_props_get_property_context(&text->component);

    rut_introspectable_copy_properties(prop_ctx, text, copy);

    return copy;
}

static void
update_pick_mesh(rig_text_t *text)
{
    cg_vertex_p3_t *pick_vertices = (cg_vertex_p3_t *)
        text->pick_mesh->attributes[0]->buffered.buffer->data;

    pick_vertices[0].x = 0;
    pick_vertices[0].y = 0;
    pick_vertices[1].x = 0;
    pick_vertices[1].y = text->height;
    pick_vertices[2].x = text->width;
    pick_vertices[2].y = text->height;
    pick_vertices[3] = pick_vertices[0];
    pick_vertices[4] = pick_vertices[2];
    pick_vertices[5].x = text->width;
    pick_vertices[5].y = 0;
}

static rut_mesh_t *
rig_text_get_pick_mesh(rut_object_t *self)
{
    rig_text_t *text = self;

    if (!text->pick_mesh) {
        rut_buffer_t *pick_mesh_buffer =
            rut_buffer_new(sizeof(cg_vertex_p3_t) * 6);

        text->pick_mesh =
            rut_mesh_new_from_buffer_p3(CG_VERTICES_MODE_TRIANGLES, 6, pick_mesh_buffer);

        update_pick_mesh(text);
    }

    return text->pick_mesh;
}

static void
_rig_text_get_size(rut_object_t *object, float *width, float *height)
{
    rig_text_t *text = object;

    *width = text->width;
    *height = text->height;
}

static void
rig_text_notify_preferred_size_changed(rig_text_t *text)
{
    rut_closure_list_invoke(&text->preferred_size_cb_list,
                            rut_sizeable_preferred_size_callback_t,
                            text);
}

static void
_rig_text_set_size(rut_object_t *object, float width, float height)
{
    rig_text_t *text = object;
    rig_property_context_t *prop_ctx;

    if (text->width == width && text->height == height)
        return;

    text->width = width;
    text->height = height;

    if (text->pick_mesh)
        update_pick_mesh(text);

    rig_text_notify_preferred_size_changed(text);

    prop_ctx = rig_component_props_get_property_context(&text->component);
    rig_property_dirty(prop_ctx, &text->properties[RIG_TEXT_PROP_WIDTH]);
    rig_property_dirty(prop_ctx, &text->properties[RIG_TEXT_PROP_HEIGHT]);
}

void
rig_text_set_width(rut_object_t *obj, float width)
{
    rig_text_t *text = obj;

    _rig_text_set_size(text, width, text->height);
}

void
rig_text_set_height(rut_object_t *obj, float height)
{
    rig_text_t *text = obj;

    _rig_text_set_size(text, text->width, height);
}

static void
_rig_text_get_preferred_width(rut_object_t *object,
                              float for_height,
                              float *min_width_p,
                              float *natural_width_p)
{
    rig_text_t *text = object;

    if (min_width_p)
        *min_width_p = text->width;
    if (natural_width_p)
        *natural_width_p = text->width;
}

static void
_rig_text_get_preferred_height(rut_object_t *object,
                               float for_width,
                               float *min_height_p,
                               float *natural_height_p)
{
    rig_text_t *text = object;

    if (min_height_p)
        *min_height_p = text->height;
    if (natural_height_p)
        *natural_height_p = text->height;
}

static void
_rig_text_add_preferred_size_callback(void *object,
                                      rut_closure_t *closure)
{
    rig_text_t *text = object;

    return rut_closure_list_add(&text->preferred_size_cb_list, closure);
}


rut_type_t rig_text_type;

void
_rig_text_init_type(void)
{
    static rut_componentable_vtable_t componentable_vtable = {
        .copy = _rig_text_copy
    };
    static rut_sizable_vtable_t sizable_vtable = {
        _rig_text_set_size,
        _rig_text_get_size,
        _rig_text_get_preferred_width,
        _rig_text_get_preferred_height,
        _rig_text_add_preferred_size_callback
    };

    static rut_meshable_vtable_t meshable_vtable = {
        .get_mesh = rig_text_get_pick_mesh
    };

    rut_type_t *type = &rig_text_type;
#define TYPE rig_text_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_text_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_COMPONENTABLE,
                       offsetof(TYPE, component),
                       &componentable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_SIZABLE,
                       0, /* no implied properties */
                       &sizable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_MESHABLE,
                       0, /* no associated properties */
                       &meshable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */

#undef TYPE
}

static void
on_wrap_cb(rig_text_engine_t *text_engine,
           void *user_data)
{
    _rig_text_set_size(user_data,
                       text_engine->width,
                       text_engine->height);
}

rig_text_t *
rig_text_new(rig_engine_t *engine)
{
    rig_text_t *text =
        rut_object_alloc0(rig_text_t, &rig_text_type, _rig_text_init_type);

    text->component.type = RUT_COMPONENT_TYPE_GEOMETRY;
    text->component.parented = false;
    text->component.engine = engine;

    text->width = 100;
    text->height = 100;

    c_list_init(&text->preferred_size_cb_list);

    rut_introspectable_init(text, _rig_text_prop_specs, text->properties);

    text->text_engine = rig_text_engine_new(engine->text_state);
    rig_text_engine_add_on_wrap_callback(text->text_engine,
                                         on_wrap_cb,
                                         text,
                                         NULL); /* destroy */

    rig_text_set_font_size(text, 18);

    /* Set the family to its default value... */
    rig_text_set_font_family(text, NULL);

    rig_text_set_text(text, "");

    return text;
}

void
rig_text_free(rig_text_t *text)
{
    rut_introspectable_destroy(text);

    if (text->pick_mesh)
        rut_object_unref(text->pick_mesh);

    rut_object_free(rig_text_t, text);
}

void
rig_text_set_text(rut_object_t *obj, const char *text_str)
{
    rig_text_t *text = obj;
    rig_property_context_t *prop_ctx;

    if (text->text) {
        c_free(text->text);
        text->text = NULL;
    }

    text->text = c_strdup(text_str ? text_str : "");

    rig_text_engine_set_utf8_static (text->text_engine,
                                     text->text,
                                     //"aaa bbb ccc ddd eee fff ggg hhh iii jjj kkk lll mmm",
                                     //"‮hello‭world",
                                     -1);

    prop_ctx = rig_component_props_get_property_context(&text->component);
    rig_property_dirty(prop_ctx, &text->properties[RIG_TEXT_PROP_TEXT]);
}

const char *
rig_text_get_text(rut_object_t *obj)
{
    rig_text_t *text = obj;

    return text->text;
}

const char *
rig_text_get_font_family(rut_object_t *obj)
{
    rig_text_t *text = obj;

    return text->font_family;
}

void
rig_text_set_font_family(rut_object_t *obj, const char *font_family)
{
    rig_text_t *text = obj;
    rig_property_context_t *prop_ctx;

    if (font_family == NULL || font_family[0] == '\0')
        font_family = "Sans 12";

    if (c_strcmp0(text->font_family, font_family) == 0)
        return;

    text->font_family = c_strdup(font_family);

    prop_ctx = rig_component_props_get_property_context(&text->component);
    rig_property_dirty(prop_ctx, &text->properties[RIG_TEXT_PROP_FONT_FAMILY]);
}

float
rig_text_get_font_size(rut_object_t *obj)
{
    rig_text_t *text = obj;

    return text->font_size;
}

void
rig_text_set_font_size(rut_object_t *obj, float font_size)
{
    rig_text_t *text = obj;
    rig_property_context_t *prop_ctx;

    if (text->font_size == font_size)
        return;

    text->font_size = font_size;

    prop_ctx = rig_component_props_get_property_context(&text->component);
    rig_property_dirty(prop_ctx, &text->properties[RIG_TEXT_PROP_FONT_SIZE]);
}

void
rig_text_set_color(rut_object_t *obj, const cg_color_t *color)
{
    rig_text_t *text = obj;
    rig_property_context_t *prop_ctx;

    text->color = *color;

    prop_ctx = rig_component_props_get_property_context(&text->component);
    rig_property_dirty(prop_ctx, &text->properties[RIG_TEXT_PROP_COLOR]);
}

const cg_color_t *
rig_text_get_color(rut_object_t *obj)
{
    rig_text_t *text = obj;

    return &text->color;
}
