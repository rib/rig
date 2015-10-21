/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012,2013  Intel Corporation
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

#include <config.h>

#include <cglib/cglib.h>
#include <string.h>
#include <math.h>

#include "rut-text.h"
#include "rut-interfaces.h"
#include "rut-transform.h"
#include "rut-entry.h"
#include "rut-nine-slice.h"
#include "rut-texture-cache.h"

enum {
    RUT_ENTRY_PROP_WIDTH,
    RUT_ENTRY_PROP_HEIGHT,
    RUT_ENTRY_N_PROPS
};

struct _rut_entry_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    rut_graphable_props_t graphable;

    rut_nine_slice_t *background;

    rut_icon_t *icon;
    rut_transform_t *icon_transform;

    rut_text_t *text;
    rut_transform_t *text_transform;

    float width;
    float height;

    rut_introspectable_props_t introspectable;
    rut_property_t properties[RUT_ENTRY_N_PROPS];
};

static rut_property_spec_t _rut_entry_prop_specs[] = {
    { .name = "width",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rut_entry_t, width),
      .setter.float_type = rut_entry_set_width },
    { .name = "height",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rut_entry_t, height),
      .setter.float_type = rut_entry_set_height },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
remove_icon(rut_entry_t *entry)
{
    if (!entry->icon)
        return;

    /* NB: we don't keep any addition references on the icon and icon
     * transform other those for adding them to the scene graph... */

    rut_graphable_remove_child(entry->icon_transform);
    entry->icon = NULL;
    entry->icon_transform = NULL;
}

static void
_rut_entry_free(void *object)
{
    rut_entry_t *entry = object;

    rut_object_unref(entry->shell);

    remove_icon(entry);

    rut_introspectable_destroy(entry);

    rut_graphable_remove_child(entry->text);
    rut_object_unref(entry->text);

    rut_graphable_remove_child(entry->text_transform);
    rut_object_unref(entry->text_transform);

    rut_graphable_destroy(entry);

    rut_object_free(rut_entry_t, entry);
}

#if 0
static cg_primitive_t *
create_entry_prim (rut_entry_t *entry)
{
    cg_vertex_p2c4_t lines[] =
    {
        {0, 0, 0x80, 0x80, 0x80, 0x80},
        {0, entry->height, 0x80, 0x80, 0x80, 0x80},

        {0, entry->height, 0x80, 0x80, 0x80, 0x80},
        {entry->width, entry->height, 0x80, 0x80, 0x80, 0x80},

        {entry->width, entry->height, 0x80, 0x80, 0x80, 0x80},
        {entry->width, 0, 0x80, 0x80, 0x80, 0x80},

        {entry->width, 0, 0x80, 0x80, 0x80, 0x80},
        {0, 0, 0x80, 0x80, 0x80, 0x80},
    };

    return cg_primitive_new_p2c4 (entry->shell->cg_device,
                                  CG_VERTICES_MODE_LINES,
                                  8,
                                  lines);
}
#endif

static void
allocate(rut_entry_t *entry)
{
    float width = entry->width;
    float height = entry->height;
    float icon_width = 0;
    float icon_height = 0;

    rut_sizable_set_size(entry->background, width, height);

    if (entry->icon) {
        rut_sizable_get_size(entry->icon, &icon_width, &icon_height);

        rut_transform_init_identity(entry->icon_transform);
        rut_transform_translate(
            entry->icon_transform, (int)(height / 2.0f), 0.0f, 0.0f);
    }

    rut_transform_init_identity(entry->text_transform);
    rut_transform_translate(
        entry->text_transform, (int)(height / 2.0f) + icon_width, 0.0f, 0.0f);

    rut_sizable_set_size(entry->text, width - height, height);
}

void
rut_entry_set_size(rut_object_t *object, float width, float height)
{
    rut_entry_t *entry = object;

    if (entry->width == width && entry->height == height)
        return;

    entry->width = width;
    entry->height = height;

    allocate(entry);

    rut_property_dirty(&entry->shell->property_ctx,
                       &entry->properties[RUT_ENTRY_PROP_WIDTH]);
    rut_property_dirty(&entry->shell->property_ctx,
                       &entry->properties[RUT_ENTRY_PROP_HEIGHT]);
}

void
rut_entry_get_size(rut_object_t *object, float *width, float *height)
{
    rut_entry_t *entry = object;

    *width = entry->width;
    *height = entry->height;
}

static void
_rut_entry_get_preferred_width(rut_object_t *object,
                               float for_height,
                               float *min_width_p,
                               float *natural_width_p)
{
    rut_entry_t *entry = object;
    float min_width, natural_width;
    float natural_height;

    rut_sizable_get_preferred_width(
        entry->text, for_height, &min_width, &natural_width);
    rut_sizable_get_preferred_height(
        entry->text, natural_width, NULL, &natural_height);

    /* The entry will add a half circle with a diameter of the height of
     * the control to either side of the text widget */
    min_width += natural_height;
    natural_width += natural_height;

    if (entry->icon) {
        float width, height;
        rut_sizable_get_size(entry->icon, &width, &height);
        min_width += width;
        natural_width += width;
    }

    if (min_width_p)
        *min_width_p = min_width;
    if (natural_width_p)
        *natural_width_p = natural_width;
}

static void
_rut_entry_get_preferred_height(rut_object_t *object,
                                float for_width,
                                float *min_height_p,
                                float *natural_height_p)
{
    rut_entry_t *entry = object;

    /* We can't pass on the for_width parameter because the width that
     * the text widget will actually get depends on the height that it
     * returns */
    rut_sizable_get_preferred_height(
        entry->text, -1, min_height_p, natural_height_p);

    if (entry->icon) {
        float width, height;
        rut_sizable_get_size(entry->icon, &width, &height);
        if (min_height_p)
            *min_height_p = MAX(*min_height_p, height);
        if (natural_height_p)
            *natural_height_p = MAX(*natural_height_p, height);
    }
}

rut_type_t rut_entry_type;

static void
_rut_entry_init_type(void)
{
    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child removed */
                                                       NULL, /* child addded */
                                                       NULL /* parent changed */
    };

    static rut_sizable_vtable_t sizable_vtable = {
        rut_entry_set_size,             rut_entry_get_size,
        _rut_entry_get_preferred_width, _rut_entry_get_preferred_height,
        NULL /* add_preferred_size_callback */
    };

    rut_type_t *type = &rut_entry_type;
#define TYPE rut_entry_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_entry_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_GRAPHABLE,
                       offsetof(TYPE, graphable),
                       &graphable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_SIZABLE,
                       0, /* no implied properties */
                       &sizable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */

#undef TYPE
}

void
rut_entry_set_width(rut_object_t *obj, float width)
{
    rut_entry_t *entry = obj;

    rut_entry_set_size(entry, width, entry->height);
}

void
rut_entry_set_height(rut_object_t *obj, float height)
{
    rut_entry_t *entry = obj;

    rut_entry_set_size(entry, entry->width, height);
}

rut_entry_t *
rut_entry_new(rut_shell_t *shell)
{
    rut_entry_t *entry =
        rut_object_alloc0(rut_entry_t, &rut_entry_type, _rut_entry_init_type);
    float width, height;
    cg_texture_t *bg_texture;

    entry->shell = rut_object_ref(shell);

    rut_introspectable_init(entry, _rut_entry_prop_specs, entry->properties);

    rut_graphable_init(entry);

    bg_texture = rut_load_texture_from_data_file(shell,
                                                 "number-slider-background.png",
                                                 NULL);

    entry->background = rut_nine_slice_new(shell, bg_texture, 7, 7, 7, 7,
                                           0, 0);
    cg_object_unref(bg_texture);
    rut_graphable_add_child(entry, entry->background);
    rut_object_unref(entry->background);

    entry->text = rut_text_new(shell);
    rut_text_set_editable(entry->text, true);

    entry->text_transform = rut_transform_new(shell);
    rut_graphable_add_child(entry->text_transform, entry->text);

    rut_graphable_add_child(entry, entry->text_transform);

    rut_sizable_get_preferred_width(entry,
                                    -1, /* for_height */
                                    NULL, /* min_width */
                                    &width);
    rut_sizable_get_preferred_height(entry,
                                     width, /* for_width */
                                     NULL, /* min_height */
                                     &height);
    rut_sizable_set_size(entry, width, height);

    return entry;
}

rut_text_t *
rut_entry_get_text(rut_entry_t *entry)
{
    return entry->text;
}

void
rut_entry_set_icon(rut_entry_t *entry, rut_icon_t *icon)
{
    if (entry->icon == icon)
        return;

    remove_icon(entry);

    if (icon) {
        /* XXX: note we don't keep any additional reference on the
         * icon and icon transform other than those for adding
         * them to the scene graph... */

        entry->icon_transform = rut_transform_new(entry->shell);
        rut_graphable_add_child(entry, entry->icon_transform);
        rut_object_unref(entry->icon_transform);

        rut_graphable_add_child(entry->icon_transform, icon);
        entry->icon = icon;

        allocate(entry);
    }

    rut_shell_queue_redraw(entry->shell);
}
