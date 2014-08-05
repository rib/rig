/*
 * Rut.
 *
 * Copyright (C) 2008,2012,2013  Intel Corporation.
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Øyvind Kolås <pippin@o-hand.com>
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 *   Robert Bragg <robert@linux.intel.com>
 */

#include <config.h>

#include <string.h>
#include <math.h>

#include <cogl-path/cogl-path.h>

#include "rut-text.h"
#include "rut-mimable.h"
#include "rut-pickable.h"
#include "rut-inputable.h"
#include "rut-meshable.h"
#include "rut-input-region.h"
#include "rut-color.h"
#include "rut-meshable.h"
#include "rut-text-blob.h"

#include "rut-camera.h"

/* This is only defined since GLib 2.31.0. The documentation says that
 * it is available since 2.28 but that is a lie. */
#ifndef G_SOURCE_REMOVE
#define G_SOURCE_REMOVE false
#endif

#define RUT_NOTE(type, ...)                                                    \
    C_STMT_START                                                               \
    {                                                                          \
    }                                                                          \
    C_STMT_END

#ifdef __COUNTER__
#define RUT_STATIC_TIMER(A, B, C, D, E)                                        \
    extern void G_PASTE(_rut_dummy_decl, __COUNTER__) (void)
#define RUT_STATIC_COUNTER(A, B, C, D)                                         \
    extern void G_PASTE(_rut_dummy_decl, __COUNTER__) (void)
#else
#define RUT_STATIC_TIMER(A, B, C, D, E)                                        \
    extern void G_PASTE(_rut_dummy_decl, __LINE__) (void)
#define RUT_STATIC_COUNTER(A, B, C, D)                                         \
    extern void G_PASTE(_rut_dummy_decl, __LINE__) (void)
#endif
#define RUT_COUNTER_INC(A, B)                                                  \
    C_STMT_START                                                               \
    {                                                                          \
    }                                                                          \
    C_STMT_END
#define RUT_COUNTER_DEC(A, B)                                                  \
    C_STMT_START                                                               \
    {                                                                          \
    }                                                                          \
    C_STMT_END
#define RUT_TIMER_START(A, B)                                                  \
    C_STMT_START                                                               \
    {                                                                          \
    }                                                                          \
    C_STMT_END
#define RUT_TIMER_STOP(A, B)                                                   \
    C_STMT_START                                                               \
    {                                                                          \
    }                                                                          \
    C_STMT_END

/* cursor width in pixels */
#define DEFAULT_CURSOR_SIZE 2

#if 0
static const uint32_t default_cursor_color = 0xff0000ff;
static const uint32_t default_selection_color = 0x009ccfff;
static const uint32_t default_text_color = 0x00ff00ff;
static const uint32_t default_selected_text_color = 0xffffffffff;
#endif
static const uint32_t default_cursor_color = 0x000000ff;
static const uint32_t default_selection_color = 0x000000ff;
static const uint32_t default_text_color = 0x000000ff;
static const uint32_t default_selected_text_color = 0xffffffff;

static rut_property_spec_t _rut_text_prop_specs[] = {
    /**
     * rut_text_t:buffer:
     *
     * The buffer which stores the text for this #rut_text_t.
     *
     * If set to %NULL, a default buffer will be created.
     */
    { .name = "buffer",
      .type = RUT_PROPERTY_TYPE_OBJECT,
      .getter.object_type = rut_text_get_buffer,
      .setter.object_type = rut_text_set_buffer,
      .nick = "Buffer",
      .blurb = "The buffer for the text",
      .flags = RUT_PROPERTY_FLAG_READWRITE },

    /**
     * rut_text_t:font-name:
     *
     * The font to be used by the #rut_text_t, as a string
     * that can be parsed by pango_font_description_from_string().
     *
     * If set to %NULL, the default system font will be used instead.
     */
    { .name = "font-name",
      .type = RUT_PROPERTY_TYPE_TEXT,
      .getter.text_type = rut_text_get_font_name,
      .setter.text_type = rut_text_set_font_name,
      .nick = "Font Name",
      .blurb = "The font to be used by the text",
      .flags = RUT_PROPERTY_FLAG_READWRITE },

    /**
     * rut_text_t:font-description:
     *
     * The #PangoFontDescription that should be used by the #rut_text_t
     *
     * If you have a string describing the font then you should look at
     * #rut_text_t:font-name instead
     */
    { .name = "font-description",
      .type = RUT_PROPERTY_TYPE_POINTER,
      .getter.any_type = rut_text_get_font_description,
      .setter.any_type = rut_text_set_font_description,
      .nick = "Font Description",
      .blurb = "The font description to be used",
      .flags = RUT_PROPERTY_FLAG_READWRITE },

    /**
     * rut_text_t:text:
     *
     * The text to render inside the actor.
     */
    { .name = "text",
      .type = RUT_PROPERTY_TYPE_TEXT,
      .getter.text_type = rut_text_get_text,
      .setter.text_type = rut_text_set_text,
      .nick = "Text",
      .blurb = "The text to render",
      .flags = RUT_PROPERTY_FLAG_READWRITE },

    /**
     * rut_text_t:hint-text:
     *
     * The text to show as a hint when the entry box is empty and it
     * doesn't have keyboard focus.
     */
    { .name = "hint-text",
      .type = RUT_PROPERTY_TYPE_TEXT,
      .getter.text_type = rut_text_get_hint_text,
      .setter.text_type = rut_text_set_hint_text,
      .nick = "Hint Text",
      .blurb = "The text to show as a hint",
      .flags = RUT_PROPERTY_FLAG_READWRITE },

    /**
     * rut_text_t:color:
     *
     * The color used to render the text.
     */
    { .name = "color",
      .type = RUT_PROPERTY_TYPE_COLOR,
      .getter.color_type = rut_text_get_color,
      .setter.color_type = rut_text_set_color,
      .nick = "Font Color",
      .blurb = "Color of the font used by the text",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .default_value = { .pointer = &default_text_color } },

    /**
     * rut_text_t:editable:
     *
     * Whether key events delivered to the actor causes editing.
     */
    { .name = "editable",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .getter.boolean_type = rut_text_get_editable,
      .setter.boolean_type = rut_text_set_editable,
      .nick = "Editable",
      .blurb = "Whether the text is editable",
      .flags = RUT_PROPERTY_FLAG_READWRITE, },

    /**
     * rut_text_t:selectable:
     *
     * Whether it is possible to select text, either using the pointer
     * or the keyboard.
     */
    { .name = "selectable",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .getter.boolean_type = rut_text_get_selectable,
      .setter.boolean_type = rut_text_set_selectable,
      .nick = "Selectable",
      .blurb = "Whether the text is selectable",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .default_value = { .boolean = true } },

    /**
     * rut_text_t:activatable:
     *
     * Toggles whether return invokes the activate signal or not.
     */
    { .name = "activatable",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .getter.boolean_type = rut_text_get_activatable,
      .setter.boolean_type = rut_text_set_activatable,
      .nick = "Activatable",
      .blurb =
          "Whether pressing return causes the activate signal to be emitted",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .default_value = { .boolean = true } },

    /**
     * rut_text_t:cursor-visible:
     *
     * Whether the input cursor is visible or not, it will only be visible
     * if both #rut_text_t:cursor-visible and #rut_text_t:editable are
     * set to %true.
     */
    { .name = "cursor-visible",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .getter.boolean_type = rut_text_get_cursor_visible,
      .setter.boolean_type = rut_text_set_cursor_visible,
      .nick = "Cursor Visible",
      .blurb = "Whether the input cursor is visible",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .default_value = { .boolean = true } },

    /**
     * rut_text_t:cursor-color:
     *
     * The color of the cursor.
     */
    { .name = "cursor-color",
      .type = RUT_PROPERTY_TYPE_COLOR,
      .getter.color_type = rut_text_get_cursor_color,
      .setter.color_type = rut_text_set_cursor_color,
      .nick = "Cursor Color",
      .blurb = "Cursor Color",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .default_value = { .pointer = &default_cursor_color } },

    /**
     * rut_text_t:cursor-color-set:
     *
     * Will be set to %true if #rut_text_t:cursor-color has been set.
     */
    { .name = "cursor-color-set",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .nick = "Cursor Color Set",
      .blurb = "Whether the cursor color has been set",
      .flags = RUT_PROPERTY_FLAG_READABLE,
      .getter.boolean_type = rut_text_get_cursor_color_set },

    /**
     * rut_text_t:cursor-size:
     *
     * The size of the cursor, in pixels. If set to -1 the size used will
     * be the default cursor size of 2 pixels.
     */
    { .name = "cursor-size",
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .getter.integer_type = rut_text_get_cursor_size,
      .setter.integer_type = rut_text_set_cursor_size,
      .nick = "Cursor Size",
      .blurb = "The width of the cursor, in pixels",
      .flags = RUT_PROPERTY_FLAG_READWRITE | RUT_PROPERTY_FLAG_VALIDATE,
      .default_value = { .integer = DEFAULT_CURSOR_SIZE },
      .validation = { .int_range = { .min = -1, .max = INT_MAX } } },

    /**
     * rut_text_t:position:
     *
     * The current input cursor position. -1 is taken to be the end of the text
     */
    { .name = "position",
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .getter.integer_type = rut_text_get_cursor_position,
      .setter.integer_type = rut_text_set_cursor_position,
      .nick = "Cursor Position",
      .blurb = "The cursor position",
      .flags = RUT_PROPERTY_FLAG_READWRITE | RUT_PROPERTY_FLAG_VALIDATE,
      .default_value = { .integer = -1 },
      .validation = { .int_range = { .min = -1, .max = INT_MAX } } },

    /**
     * rut_text_t:selection-bound:
     *
     * The current input cursor position. -1 is taken to be the end of the text
     */
    { .name = "selection-bound",
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .getter.integer_type = rut_text_get_selection_bound,
      .setter.integer_type = rut_text_set_selection_bound,
      .nick = "Selection-bound",
      .blurb = "The cursor position of the other end of the selection",
      .flags = RUT_PROPERTY_FLAG_READWRITE | RUT_PROPERTY_FLAG_VALIDATE,
      .default_value = { .integer = -1 },
      .validation = { .int_range = { .min = -1, .max = INT_MAX } } },

    /**
     * rut_text_t:selection-color:
     *
     * The color of the selection.
     */
    { .name = "selection-color",
      .type = RUT_PROPERTY_TYPE_COLOR,
      .getter.color_type = rut_text_get_selection_color,
      .setter.color_type = rut_text_set_selection_color,
      .nick = "Selection Color",
      .blurb = "Selection Color",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .default_value = { .pointer = &default_selection_color } },

    /**
     * rut_text_t:selection-color-set:
     *
     * Will be set to %true if #rut_text_t:selection-color has been set.
     */
    { .name = "selection-color-set",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .nick = "Selection Color Set",
      .blurb = "Whether the selection color has been set",
      .flags = RUT_PROPERTY_FLAG_READABLE,
      .getter.boolean_type = rut_text_get_selection_color_set },

    /**
     * rut_text_t:attributes:
     *
     * A list of #PangoStyleAttribute<!-- -->s to be applied to the
     * contents of the #rut_text_t actor.
     */
    { .name = "attributes",
      .type = RUT_PROPERTY_TYPE_POINTER,
      .getter.any_type = rut_text_get_attributes,
      .setter.any_type = rut_text_set_attributes,
      .nick = "Attributes",
      .blurb =
          "A list of style attributes to apply to the contents of the actor",
      .flags = RUT_PROPERTY_FLAG_READWRITE },

    /**
     * rut_text_t:use-markup:
     *
     * Whether the text includes Pango markup.
     *
     * For more informations about the Pango markup format, see
     * pango_layout_set_markup() in the Pango documentation.
     *
     * <note>It is not possible to round-trip this property between
     * %true and %false. Once a string with markup has been set on
     * a #rut_text_t actor with :use-markup set to %true, the markup
     * is stripped from the string.</note>
     */
    { .name = "use-markup",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .getter.boolean_type = rut_text_get_use_markup,
      .setter.boolean_type = rut_text_set_use_markup,
      .nick = "Use markup",
      .blurb = "Whether or not the text includes Pango markup",
      .flags = RUT_PROPERTY_FLAG_READWRITE },

    /**
     * rut_text_t:line-wrap:
     *
     * Whether to wrap the lines of #rut_text_t:text if the contents
     * exceed the available allocation. The wrapping strategy is
     * controlled by the #rut_text_t:line-wrap-mode property.
     */
    { .name = "line-wrap",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .getter.boolean_type = rut_text_get_line_wrap,
      .setter.boolean_type = rut_text_set_line_wrap,
      .nick = "Line wrap",
      .blurb = "If set, wrap the lines if the text becomes too wide",
      .flags = RUT_PROPERTY_FLAG_READWRITE },

    /**
     * rut_text_t:line-wrap-mode:
     *
     * If #rut_text_t:line-wrap is set to %true, this property will
     * control how the text is wrapped.
     */
    { .name = "line-wrap-mode",
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .getter.any_type = rut_text_get_line_wrap_mode,
      .setter.any_type = rut_text_set_line_wrap_mode,
      .nick = "Line wrap mode",
      .blurb = "Control how line-wrapping is done",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .default_value = { .integer = PANGO_WRAP_WORD }
      /* FIXME: add better support for enum properties */
    },

    /**
     * rut_text_t:ellipsize:
     *
     * The preferred place to ellipsize the contents of the #rut_text_t actor
     */
    { .name = "ellipsize",
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .getter.any_type = rut_text_get_ellipsize,
      .setter.any_type = rut_text_set_ellipsize,
      .nick = "Ellipsize",
      .blurb = "The preferred place to ellipsize the string",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      /* FIXME: add better support for enum properties */
    },

    /**
     * rut_text_t:line-alignment:
     *
     * The preferred alignment for the text. This property controls
     * the alignment of multi-line paragraphs.
     */
    { .name = "line-alignment",
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .getter.any_type = rut_text_get_line_alignment,
      .setter.any_type = rut_text_set_line_alignment,
      .nick = "Line alignment_t",
      .blurb = "The preferred alignment for the string, for multi-line text",
      .flags = RUT_PROPERTY_FLAG_READWRITE },

    /**
     * rut_text_t:justify:
     *
     * Whether the contents of the #rut_text_t should be justified
     * on both margins.
     */
    { .name = "justify",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .getter.boolean_type = rut_text_get_justify,
      .setter.boolean_type = rut_text_set_justify,
      .nick = "Justify",
      .blurb = "Whether the text should be justified",
      .flags = RUT_PROPERTY_FLAG_READWRITE },

    /**
     * rut_text_t:password-char:
     *
     * If non-zero, the character that should be used in place of
     * the actual text in a password text actor.
     */
    { .name = "password-char",
      .type = RUT_PROPERTY_TYPE_UINT32,
      .getter.uint32_type = rut_text_get_password_char,
      .setter.uint32_type = rut_text_set_password_char,
      .nick = "Password Character",
      .blurb = "If non-zero, use this character to display the text contents",
      .flags = RUT_PROPERTY_FLAG_READWRITE },

    /**
     * rut_text_t:max-length:
     *
     * The maximum length of the contents of the #rut_text_t actor.
     */
    { .name = "max-length",
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .getter.integer_type = rut_text_get_max_length,
      .setter.integer_type = rut_text_set_max_length,
      .nick = "Max Length",
      .blurb = "Maximum length of the text inside the actor",
      .flags = RUT_PROPERTY_FLAG_READWRITE | RUT_PROPERTY_FLAG_VALIDATE,
      .validation = { .int_range = { .min = -1, .max = INT_MAX } } },

    /**
     * rut_text_t:single-line-mode:
     *
     * Whether the #rut_text_t actor should be in single line mode
     * or not. A single line #rut_text_t actor will only contain a
     * single line of text, scrolling it in case its length is bigger
     * than the allocated size.
     *
     * Setting this property will also set the #rut_text_t:activatable
     * property as a side-effect.
     *
     * The #rut_text_t:single-line-mode property is used only if the
     * #rut_text_t:editable property is set to %true.
     */
    { .name = "single-line-mode",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .getter.boolean_type = rut_text_get_single_line_mode,
      .setter.boolean_type = rut_text_set_single_line_mode,
      .nick = "Single Line Mode",
      .blurb = "Whether the text should be a single line",
      .flags = RUT_PROPERTY_FLAG_READWRITE },

    /**
     * rut_text_t:selected-text-color:
     *
     * The color of selected text.
     */
    { .name = "selected-text-color",
      .type = RUT_PROPERTY_TYPE_COLOR,
      .getter.color_type = rut_text_get_selected_text_color,
      .setter.color_type = rut_text_set_selected_text_color,
      .nick = "Selected Text Color",
      .blurb = "Selected Text Color",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .default_value = { .pointer = &default_selected_text_color } },

    /**
     * rut_text_t:selected-text-color-set:
     *
     * Will be set to %true if #rut_text_t:selected-text-color has been set.
     */
    { .name = "selected-text-color-set",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .nick = "Selected Text Color Set",
      .blurb = "Whether the selected text color has been set",
      .flags = RUT_PROPERTY_FLAG_READABLE,
      .getter.boolean_type = rut_text_get_selected_text_color_set,
      .default_value = { .boolean = true } },

    /**
     * rut_text_t:text-direction:
     *
     * The direction of the text
     */
    { .name = "text-direction",
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .data_offset = offsetof(rut_text_t, direction),
      .nick = "Text Direction",
      .blurb = "Direction of the text",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .getter.any_type = rut_text_get_direction,
      .setter.any_type = rut_text_set_direction,
      .default_value = { .integer = RUT_TEXT_DIRECTION_LEFT_TO_RIGHT } },
    { .name = "width",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rut_text_t, width),
      .setter.float_type = rut_text_set_width },
    { .name = "height",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rut_text_t, height),
      .setter.float_type = rut_text_set_height },
    { 0 }
};

static void buffer_connect_signals(rut_text_t *text);
static void buffer_disconnect_signals(rut_text_t *text);
static rut_text_buffer_t *get_buffer(rut_text_t *text);
static rut_input_event_status_t rut_text_key_press(rut_input_event_t *event,
                                                   void *user_data);

#define offset_real(t, p) ((p) == -1 ? g_utf8_strlen((t), -1) : (p))

static int
offset_to_bytes(const char *text, int pos)
{
    const char *ptr;

    if (pos < 0)
        return strlen(text);

    /* Loop over each character in the string until we either reach the
       end or the requested position */
    for (ptr = text; *ptr && pos-- > 0; ptr = g_utf8_next_char(ptr))
        ;

    return ptr - text;
}

#define bytes_to_offset(t, p) (g_utf8_pointer_to_offset((t), (t) + (p)))

static void
_rut_text_get_size(rut_object_t *object, float *width, float *height)
{
    rut_text_t *text = object;

    *width = text->width;
    *height = text->height;
}
#if 0
void
rut_text_get_allocation_box (rut_text_t *text,
                             rut_box_t *box)
{
    *box = text->allocation;
}
#endif

static void
update_size(rut_text_t *text)
{
    float min_width, min_height, natural_width, natural_height;

    rut_sizable_get_preferred_width(text, 0, &min_width, &natural_width);
    rut_sizable_get_preferred_height(
        text, natural_width, &min_height, &natural_height);
    rut_sizable_set_size(text, natural_width, natural_height);
}

static void
rut_text_notify_preferred_size_changed(rut_text_t *text)
{
    rut_closure_list_invoke(&text->preferred_size_cb_list,
                            rut_sizeable_preferred_size_callback_t,
                            text);
}

static inline void
rut_text_clear_selection(rut_text_t *text)
{
    if (text->selection_bound != text->position) {
        text->selection_bound = text->position;
        rut_property_dirty(&text->shell->property_ctx,
                           &text->properties[RUT_TEXT_PROP_SELECTION_BOUND]);
        rut_shell_queue_redraw(text->shell);
    }
}

static char *
rut_text_get_display_text(rut_text_t *text)
{
    rut_text_buffer_t *buffer;
    const char *text_str;

    buffer = get_buffer(text);
    text_str = rut_text_buffer_get_text(buffer);

    /* simple short-circuit to avoid going through c_string_t
     * with an empty text and a password char set
     */
    if (text_str[0] == '\0')
        return c_strdup("");

    if (C_LIKELY(text->password_char == 0))
        return c_strdup(text_str);
    else {
        c_string_t *str;
        uint32_t invisible_char;
        char buf[7];
        int char_len, i;
        unsigned int n_chars;

        n_chars = rut_text_buffer_get_length(buffer);
        str = c_string_sized_new(rut_text_buffer_get_bytes(buffer));
        invisible_char = text->password_char;

        /* we need to convert the string built of invisible
         * characters into UTF-8 for it to be fed to the Pango
         * layout
         */
        memset(buf, 0, sizeof(buf));
        char_len = g_unichar_to_utf8(invisible_char, buf);

        if (text->show_password_hint && text->password_hint_visible) {
            char *last_char;

            for (i = 0; i < n_chars - 1; i++)
                c_string_append_len(str, buf, char_len);

            last_char = g_utf8_offset_to_pointer(text_str, n_chars - 1);
            c_string_append(str, last_char);
        } else {
            for (i = 0; i < n_chars; i++)
                c_string_append_len(str, buf, char_len);
        }

        return c_string_free(str, false);
    }
}

static inline void
rut_text_ensure_effective_attributes(rut_text_t *text)
{
    /* If we already have the effective attributes then we don't need to
       do anything */
    if (text->effective_attrs != NULL)
        return;

    /* same as if we don't have any attribute at all */
    if (text->attrs == NULL && text->markup_attrs == NULL)
        return;

    if (text->attrs != NULL) {
        /* If there are no markup attributes then we can just use
           these attributes directly */
        if (text->markup_attrs == NULL)
            text->effective_attrs = pango_attr_list_ref(text->attrs);
        else {
            /* Otherwise we need to merge the two lists */
            PangoAttrIterator *iter;
            GSList *attributes, *l;

            text->effective_attrs = pango_attr_list_copy(text->markup_attrs);

            iter = pango_attr_list_get_iterator(text->attrs);
            do {
                attributes = pango_attr_iterator_get_attrs(iter);

                for (l = attributes; l != NULL; l = l->next) {
                    PangoAttribute *attr = l->data;

                    pango_attr_list_insert(text->effective_attrs, attr);
                }

                g_slist_free(attributes);
            } while (pango_attr_iterator_next(iter));

            pango_attr_iterator_destroy(iter);
        }
    } else if (text->markup_attrs != NULL) {
        /* We can just use the markup attributes directly */
        text->effective_attrs = pango_attr_list_ref(text->markup_attrs);
    }
}

static PangoLayout *
rut_text_ensure_hint_text_layout(rut_text_t *text)
{
    PangoLayout *layout;

    if (text->hint_text_layout)
        return text->hint_text_layout;

    layout = pango_layout_new(text->shell->pango_context);
    pango_layout_set_font_description(layout, text->font_desc);
    pango_layout_set_single_paragraph_mode(layout, true);
    pango_layout_set_text(layout, text->hint_text, -1);

    text->hint_text_layout = layout;

    return layout;
}

static PangoLayout *
rut_text_create_layout_no_cache(
    rut_text_t *text, int width, int height, PangoEllipsizeMode ellipsize)
{
    PangoLayout *layout;
    char *contents;
    gsize contents_len;

    RUT_STATIC_TIMER(
        text_layout_timer, "Mainloop", "Text Layout", "Layout creation", 0);

    RUT_TIMER_START(_rut_uprof_context, text_layout_timer);

    layout = pango_layout_new(text->shell->pango_context);
    pango_layout_set_font_description(layout, text->font_desc);

    contents = rut_text_get_display_text(text);
    contents_len = strlen(contents);

    if (text->editable && text->preedit_set) {
        c_string_t *tmp = c_string_new(contents);
        PangoAttrList *tmp_attrs = pango_attr_list_new();
        int cursor_index;

        if (text->position == 0)
            cursor_index = 0;
        else
            cursor_index = offset_to_bytes(contents, text->position);

        c_string_insert(tmp, cursor_index, text->preedit_str);

        pango_layout_set_text(layout, tmp->str, tmp->len);

        if (text->preedit_attrs != NULL) {
            pango_attr_list_splice(tmp_attrs,
                                   text->preedit_attrs,
                                   cursor_index,
                                   strlen(text->preedit_str));

            pango_layout_set_attributes(layout, tmp_attrs);
        }

        c_string_free(tmp, true);
        pango_attr_list_unref(tmp_attrs);
    } else
        pango_layout_set_text(layout, contents, contents_len);

    if (!text->editable) {
        /* This will merge the markup attributes and the attributes
           property if needed */
        rut_text_ensure_effective_attributes(text);

        if (text->effective_attrs != NULL)
            pango_layout_set_attributes(layout, text->effective_attrs);
    }

    pango_layout_set_alignment(layout, text->alignment);
    pango_layout_set_single_paragraph_mode(layout, text->single_line_mode);
    pango_layout_set_justify(layout, text->justify);
    pango_layout_set_wrap(layout, text->wrap_mode);

    pango_layout_set_ellipsize(layout, ellipsize);
    pango_layout_set_width(layout, width);
    pango_layout_set_height(layout, height);

    c_free(contents);

    RUT_TIMER_STOP(_rut_uprof_context, text_layout_timer);

    return layout;
}

static void
rut_text_dirty_cache(rut_text_t *text)
{
    int i;

    /* Delete the cached layouts so they will be recreated the next time
       they are needed */
    for (i = 0; i < N_CACHED_LAYOUTS; i++)
        if (text->cached_layouts[i].layout) {
            g_object_unref(text->cached_layouts[i].layout);
            text->cached_layouts[i].layout = NULL;
        }
}

static void
rut_text_dirty_hint_text_layout(rut_text_t *text)
{
    if (text->hint_text_layout) {
        g_object_unref(text->hint_text_layout);
        text->hint_text_layout = NULL;
    }
}

/*
 * rut_text_set_font_description_internal:
 * @text: a #rut_text_t
 * @desc: a #PangoFontDescription
 *
 * Sets @desc as the font description to be used by the #rut_text_t
 * actor. The font description ownership is transferred to @text so
 * the #PangoFontDescription must not be freed after this function
 *
 * This function will also set the :font-name field as a side-effect
 *
 * This function will evict the layout cache, and queue a relayout if
 * the #rut_text_t actor has contents.
 */
static inline void
rut_text_set_font_description_internal(rut_text_t *text,
                                       PangoFontDescription *desc)
{
    if (text->font_desc == desc)
        return;

    if (text->font_desc != NULL)
        pango_font_description_free(text->font_desc);

    text->font_desc = desc;

    /* update the font name string we use */
    c_free(text->font_name);
    text->font_name = pango_font_description_to_string(text->font_desc);

    rut_text_dirty_cache(text);
    rut_text_dirty_hint_text_layout(text);

    if (rut_text_buffer_get_length(get_buffer(text)) != 0)
        rut_text_notify_preferred_size_changed(text);

    rut_property_dirty(&text->shell->property_ctx,
                       &text->properties[RUT_TEXT_PROP_FONT_DESCRIPTION]);
}

static void
rut_text_settings_changed_cb(rut_settings_t *settings,
                             void *user_data)
{
    rut_text_t *text = user_data;
    unsigned int password_hint_time = 0;

    password_hint_time =
        rut_settings_get_password_hint_time(text->shell->settings);

    text->show_password_hint = password_hint_time > 0;
    text->password_hint_timeout = password_hint_time;

    if (text->is_default_font) {
        PangoFontDescription *font_desc;
        char *font_name = NULL;

        font_name = rut_settings_get_font_name(text->shell->settings);

        RUT_NOTE(
            ACTOR, "Text[%p]: default font changed to '%s'", text, font_name);

        font_desc = pango_font_description_from_string(font_name);
        rut_text_set_font_description_internal(text, font_desc);

        c_free(font_name);
    }

    rut_text_dirty_cache(text);
    rut_text_dirty_hint_text_layout(text);
    rut_text_notify_preferred_size_changed(text);
}

/*
 * rut_text_create_layout:
 * @text: a #rut_text_t
 * @allocation_width: the allocation width
 * @allocation_height: the allocation height
 *
 * Like rut_text_create_layout_no_cache(), but will also ensure
 * the glyphs cache. If a previously cached layout generated using the
 * same width is available then that will be used instead of
 * generating a new one.
 */
static PangoLayout *
rut_text_create_layout(rut_text_t *text,
                       float allocation_width,
                       float allocation_height)
{

    layout_cache_t *oldest_cache = text->cached_layouts;
    bool found_free_cache = false;
    int width = -1;
    int height = -1;
    PangoEllipsizeMode ellipsize = PANGO_ELLIPSIZE_NONE;
    int i;

    RUT_STATIC_COUNTER(text_cache_hit_counter,
                       "Text layout cache hit counter",
                       "Increments for each layout cache hit",
                       0);
    RUT_STATIC_COUNTER(text_cache_miss_counter,
                       "Text layout cache miss counter",
                       "Increments for each layout cache miss",
                       0);

    /* First determine the width, height, and ellipsize mode that
     * we need for the layout. The ellipsize mode depends on
     * allocation_width/allocation_size as follows:
     *
     * Cases, assuming ellipsize != NONE on actor:
     *
     * Width request: ellipsization can be set or not on layout,
     * doesn't matter.
     *
     * Height request: ellipsization must never be set on layout
     * if wrap=true, because we need to measure the wrapped
     * height. It must always be set if wrap=false.
     *
     * Allocate: ellipsization must always be set.
     *
     * See http://bugzilla.gnome.org/show_bug.cgi?id=560931
     */

    if (text->ellipsize != PANGO_ELLIPSIZE_NONE) {
        if (allocation_height < 0 && text->wrap)
            ; /* must not set ellipsization on wrap=true height request */
        else {
            if (!text->editable)
                ellipsize = text->ellipsize;
        }
    }

    /* When painting, we always need to set the width, since
     * we might need to align to the right. When getting the
     * height, however, there are some cases where we know that
     * the width won't affect the width.
     *
     * - editable, single-line text actors, since those can
     *   scroll the layout.
     * - non-wrapping, non-ellipsizing actors.
     */
    if (allocation_width >= 0 &&
        (allocation_height >= 0 ||
         !((text->editable && text->single_line_mode) ||
           (text->ellipsize == PANGO_ELLIPSIZE_NONE && !text->wrap)))) {
        width = allocation_width * 1024 + 0.5f;
    }

    /* Pango only uses height if ellipsization is enabled, so don't set
     * height if ellipsize isn't set. Pango implicitly enables wrapping
     * if height is set, so don't set height if wrapping is disabled.
     * In other words, only set height if we want to both wrap then
     * ellipsize and we're not in single line mode.
     *
     * See http://bugzilla.gnome.org/show_bug.cgi?id=560931 if this
     * seems odd.
     */
    if (allocation_height >= 0 && text->wrap &&
        text->ellipsize != PANGO_ELLIPSIZE_NONE && !text->single_line_mode) {
        height = allocation_height * 1024 + 0.5f;
    }

    /* Search for a cached layout with the same width and keep
     * track of the oldest one
     */
    for (i = 0; i < N_CACHED_LAYOUTS; i++) {
        if (text->cached_layouts[i].layout == NULL) {
            /* Always prefer free cache spaces */
            found_free_cache = true;
            oldest_cache = text->cached_layouts + i;
        } else {
            PangoLayout *cached = text->cached_layouts[i].layout;
            int cached_width = pango_layout_get_width(cached);
            int cached_height = pango_layout_get_height(cached);
            int cached_ellipsize = pango_layout_get_ellipsize(cached);

            if (cached_width == width && cached_height == height &&
                cached_ellipsize == ellipsize) {
                /* If this cached layout is using the same size then we can
                 * just return that directly
                 */
                RUT_NOTE(ACTOR,
                         "rut_text_t: %p: cache hit for size %.2fx%.2f",
                         text,
                         allocation_width,
                         allocation_height);

                RUT_COUNTER_INC(_rut_uprof_context, text_cache_hit_counter);

                return text->cached_layouts[i].layout;
            }

            /* When getting the preferred height for a specific width,
             * we might be able to reuse the layout from getting the
             * preferred width. If the width that the layout gives
             * unconstrained is less than the width that we are using
             * than the height will be unaffected by that width.
             */
            if (allocation_height < 0 && cached_width == -1 &&
                cached_ellipsize == ellipsize) {
                PangoRectangle logical_rect;

                pango_layout_get_extents(
                    text->cached_layouts[i].layout, NULL, &logical_rect);

                if (logical_rect.width <= width) {
                    /* We've been asked for our height for the width we gave as
                     * a result
                     * of a _get_preferred_width call
                     */
                    RUT_NOTE(ACTOR,
                             "rut_text_t: %p: cache hit for size %.2fx%.2f "
                             "(unwrapped width narrower than given width)",
                             text,
                             allocation_width,
                             allocation_height);

                    RUT_COUNTER_INC(_rut_uprof_context, text_cache_hit_counter);

                    return text->cached_layouts[i].layout;
                }
            }

            if (!found_free_cache &&
                (text->cached_layouts[i].age < oldest_cache->age)) {
                oldest_cache = text->cached_layouts + i;
            }
        }
    }

    RUT_NOTE(ACTOR,
             "rut_text_t: %p: cache miss for size %.2fx%.2f",
             text,
             allocation_width,
             allocation_height);

    RUT_COUNTER_INC(_rut_uprof_context, text_cache_miss_counter);

    /* If we make it here then we didn't have a cached version so we
       need to recreate the layout */
    if (oldest_cache->layout)
        g_object_unref(oldest_cache->layout);

    oldest_cache->layout =
        rut_text_create_layout_no_cache(text, width, height, ellipsize);

    cg_pango_ensure_glyph_cache_for_layout(oldest_cache->layout);

    /* Mark the 'time' this cache was created and advance the time */
    oldest_cache->age = text->cache_age++;
    return oldest_cache->layout;
}

int
rut_text_coords_to_position(rut_text_t *text, float x, float y)
{
    int index_;
    int px, py;
    int trailing;

    /* Take any offset due to scrolling into account, and normalize
     * the coordinates to PangoScale units
     */
    px = (x - text->text_x) * PANGO_SCALE;
    py = (y - text->text_y) * PANGO_SCALE;

    pango_layout_xy_to_index(
        rut_text_get_layout(text), px, py, &index_, &trailing);

    return index_ + trailing;
}

bool
rut_text_position_to_coords(
    rut_text_t *text, int position, float *x, float *y, float *line_height)
{
    PangoRectangle rect;
    int n_chars;
    int password_char_bytes = 1;
    int index_;
    gsize n_bytes;

    n_chars = rut_text_buffer_get_length(get_buffer(text));
    if (text->preedit_set)
        n_chars += text->preedit_n_chars;

    if (position < -1 || position > n_chars)
        return false;

    if (text->password_char != 0)
        password_char_bytes = g_unichar_to_utf8(text->password_char, NULL);

    if (position == -1) {
        if (text->password_char == 0) {
            n_bytes = rut_text_buffer_get_bytes(get_buffer(text));
            if (text->editable && text->preedit_set)
                index_ = n_bytes + strlen(text->preedit_str);
            else
                index_ = n_bytes;
        } else
            index_ = n_chars * password_char_bytes;
    } else if (position == 0) {
        index_ = 0;
    } else {
        char *text_str = rut_text_get_display_text(text);
        c_string_t *tmp = c_string_new(text_str);
        int cursor_index;

        cursor_index = offset_to_bytes(text_str, text->position);

        if (text->preedit_str != NULL)
            c_string_insert(tmp, cursor_index, text->preedit_str);

        if (text->password_char == 0)
            index_ = offset_to_bytes(tmp->str, position);
        else
            index_ = position * password_char_bytes;

        c_free(text_str);
        c_string_free(tmp, true);
    }

    pango_layout_get_cursor_pos(rut_text_get_layout(text), index_, &rect, NULL);

    if (x) {
        *x = (float)rect.x / 1024.0f;

        /* Take any offset due to scrolling into account */
        if (text->single_line_mode)
            *x += text->text_x;
    }

    if (y)
        *y = (float)rect.y / 1024.0f;

    if (line_height)
        *line_height = (float)rect.height / 1024.0f;

    return true;
}

static inline void
rut_text_ensure_cursor_position(rut_text_t *text)
{
    float x, y, cursor_height;
    rut_rectangle_int_t cursor_pos = { 0, };
    bool x_changed, y_changed;
    bool width_changed, height_changed;
    int position;

    position = text->position;

    if (text->editable && text->preedit_set) {
        if (position == -1)
            position = rut_text_buffer_get_length(get_buffer(text));
        position += text->preedit_cursor_pos;
    }

    RUT_NOTE(MISC,
             "Cursor at %d (preedit %s at pos: %d)",
             position,
             text->preedit_set ? "set" : "unset",
             text->preedit_set ? text->preedit_cursor_pos : 0);

    x = y = cursor_height = 0;
    rut_text_position_to_coords(text, position, &x, &y, &cursor_height);

    cursor_pos.x = x;
    cursor_pos.y = y + 2;
    cursor_pos.width = text->cursor_size;
    cursor_pos.height = cursor_height - 4;

    x_changed = text->cursor_pos.x != cursor_pos.x;
    y_changed = text->cursor_pos.y != cursor_pos.y;
    width_changed = text->cursor_pos.width != cursor_pos.width;
    height_changed = text->cursor_pos.height != cursor_pos.height;

    if (x_changed || y_changed || width_changed || height_changed) {
        text->cursor_pos = cursor_pos;

        rut_closure_list_invoke(&text->cursor_event_cb_list,
                                rut_text_cursor_event_callback_t,
                                text,
                                &text->cursor_pos);
    }
}

bool
rut_text_delete_selection(rut_text_t *text)
{
    int start_index;
    int end_index;
    int old_position, old_selection;
    unsigned int n_chars;

    n_chars = rut_text_buffer_get_length(get_buffer(text));
    if (n_chars == 0)
        return true;

    start_index = text->position == -1 ? n_chars : text->position;
    end_index = text->selection_bound == -1 ? n_chars : text->selection_bound;

    if (end_index == start_index)
        return false;

    if (end_index < start_index) {
        int temp = start_index;
        start_index = end_index;
        end_index = temp;
    }

    old_position = text->position;
    old_selection = text->selection_bound;

    rut_text_delete_text(text, start_index, end_index);

    text->position = start_index;
    text->selection_bound = start_index;

    /* Not required to be guarded by g_object_freeze/thaw_notify */
    if (text->position != old_position)
        rut_property_dirty(&text->shell->property_ctx,
                           &text->properties[RUT_TEXT_PROP_POSITION]);

    if (text->selection_bound != old_selection)
        rut_property_dirty(&text->shell->property_ctx,
                           &text->properties[RUT_TEXT_PROP_SELECTION_BOUND]);

    return true;
}

/*
 * Utility function to update both cursor position and selection bound
 * at once
 */
static inline void
rut_text_set_positions(rut_text_t *text, int new_pos, int new_bound)
{
    rut_text_set_cursor_position(text, new_pos);
    rut_text_set_selection_bound(text, new_bound);
}

static inline void
rut_text_set_markup_internal(rut_text_t *text,
                             const char *str)
{
    GError *error;
    char *stripped_text = NULL;
    PangoAttrList *attrs = NULL;
    bool res;

    c_assert(str != NULL);

    error = NULL;
    res = pango_parse_markup(str, -1, 0, &attrs, &stripped_text, NULL, &error);

    if (!res) {
        if (C_LIKELY(error != NULL)) {
            c_warning("Failed to set the markup of rut_text_t object %p: %s",
                      text,
                      error->message);
            g_error_free(error);
        } else
            c_warning("Failed to set the markup of rut_text_t object %p", text);

        return;
    }

    if (stripped_text) {
        rut_text_buffer_set_text(get_buffer(text), stripped_text);
        c_free(stripped_text);
    }

    /* Store the new markup attributes */
    if (text->markup_attrs != NULL)
        pango_attr_list_unref(text->markup_attrs);

    text->markup_attrs = attrs;

    /* Clear the effective attributes so they will be regenerated when a
       layout is created */
    if (text->effective_attrs != NULL) {
        pango_attr_list_unref(text->effective_attrs);
        text->effective_attrs = NULL;
    }
}

static void
_rut_text_free(void *object)
{
    rut_text_t *text = object;

    rut_closure_list_disconnect_all_FIXME(&text->preferred_size_cb_list);
    rut_closure_list_disconnect_all_FIXME(&text->delete_text_cb_list);
    rut_closure_list_disconnect_all_FIXME(&text->insert_text_cb_list);
    rut_closure_list_disconnect_all_FIXME(&text->activate_cb_list);
    rut_closure_list_disconnect_all_FIXME(&text->cursor_event_cb_list);
    rut_closure_list_disconnect_all_FIXME(&text->text_changed_cb_list);
    rut_closure_list_disconnect_all_FIXME(&text->text_deleted_cb_list);
    rut_closure_list_disconnect_all_FIXME(&text->text_inserted_cb_list);

    if (text->has_focus)
        rut_text_ungrab_key_focus(text);

    /* get rid of the entire cache */
    rut_text_dirty_cache(text);

    rut_settings_remove_changed_callback(text->shell->settings,
                                         rut_text_settings_changed_cb);

    if (text->password_hint_id) {
        g_source_remove(text->password_hint_id);
        text->password_hint_id = 0;
    }

    rut_text_set_buffer(text, NULL);

    if (text->font_desc)
        pango_font_description_free(text->font_desc);

    if (text->attrs)
        pango_attr_list_unref(text->attrs);
    if (text->markup_attrs)
        pango_attr_list_unref(text->markup_attrs);
    if (text->effective_attrs)
        pango_attr_list_unref(text->effective_attrs);
    if (text->preedit_attrs)
        pango_attr_list_unref(text->preedit_attrs);

    c_free(text->hint_text);
    rut_text_dirty_hint_text_layout(text);

    rut_text_set_buffer(text, NULL);
    c_free(text->font_name);

    rut_object_unref(text->pick_mesh);
    rut_object_unref(text->input_region);

    rut_introspectable_destroy(text);
    rut_graphable_destroy(text);

    rut_object_free(rut_text_t, text);
}

typedef void (*rut_text_selection_func_t)(rut_text_t *text,
                                          const rut_box_t *box,
                                          void *user_data);

static void
rut_text_foreach_selection_rectangle(rut_text_t *text,
                                     rut_text_selection_func_t func,
                                     void *user_data)
{

    PangoLayout *layout = rut_text_get_layout(text);
    char *utf8 = rut_text_get_display_text(text);
    int lines;
    int start_index;
    int end_index;
    int line_no;

    if (text->position == 0)
        start_index = 0;
    else
        start_index = offset_to_bytes(utf8, text->position);

    if (text->selection_bound == 0)
        end_index = 0;
    else
        end_index = offset_to_bytes(utf8, text->selection_bound);

    if (start_index > end_index) {
        int temp = start_index;
        start_index = end_index;
        end_index = temp;
    }

    lines = pango_layout_get_line_count(layout);

    for (line_no = 0; line_no < lines; line_no++) {
        PangoLayoutLine *line;
        int n_ranges;
        int *ranges;
        int i;
        int index_;
        int maxindex;
        rut_box_t box;
        float y, height;

        line = pango_layout_get_line_readonly(layout, line_no);
        pango_layout_line_x_to_index(line, G_MAXINT, &maxindex, NULL);
        if (maxindex < start_index)
            continue;

        pango_layout_line_get_x_ranges(
            line, start_index, end_index, &ranges, &n_ranges);
        pango_layout_line_x_to_index(line, 0, &index_, NULL);

        rut_text_position_to_coords(
            text, bytes_to_offset(utf8, index_), NULL, &y, &height);

        box.y1 = y;
        box.y2 = y + height;

        for (i = 0; i < n_ranges; i++) {
            float range_x;
            float range_width;

            range_x = ranges[i * 2] / PANGO_SCALE;

            /* Account for any scrolling in single line mode */
            if (text->single_line_mode)
                range_x += text->text_x;

            range_width =
                ((float)ranges[i * 2 + 1] - (float)ranges[i * 2]) / PANGO_SCALE;

            box.x1 = range_x;
            box.x2 = ceilf(range_x + range_width + .5f);

            func(text, &box, user_data);
        }

        c_free(ranges);
    }

    c_free(utf8);
}

static void
add_selection_rectangle_to_path(rut_text_t *text,
                                const rut_box_t *box,
                                void *user_data)
{
    cg_path_rectangle(user_data, box->x1, box->y1, box->x2, box->y2);
}

static float
rut_text_get_paint_opacity(rut_text_t *text)
{
    return 1.0;
}

/* Draws the selected text, its background, and the cursor */
static void
selection_paint(rut_text_t *text, rut_paint_context_t *paint_ctx)
{
    rut_object_t *camera = paint_ctx->camera;
    cg_framebuffer_t *fb = rut_camera_get_framebuffer(camera);
    float paint_opacity = rut_text_get_paint_opacity(text);

    if (!text->has_focus)
        return;

    if (text->editable && text->cursor_visible) {
        const cg_color_t *color;
        int position;

        position = text->position;

        if (position == text->selection_bound) {
            cg_pipeline_t *pipeline = cg_pipeline_new(text->shell->cg_device);

            /* No selection, just draw the cursor */
            if (text->cursor_color_set)
                color = &text->cursor_color;
            else
                color = &text->text_color;

            cg_pipeline_set_color4f(pipeline,
                                    color->red,
                                    color->green,
                                    color->blue,
                                    paint_opacity * color->alpha);

            cg_framebuffer_draw_rectangle(
                fb,
                pipeline,
                text->cursor_pos.x,
                text->cursor_pos.y,
                text->cursor_pos.x + text->cursor_pos.width,
                text->cursor_pos.y + text->cursor_pos.height);
        } else {
            /* Paint selection background first */
            PangoLayout *layout = rut_text_get_layout(text);
            cg_path_t *selection_path = cg_path_new(text->shell->cg_device);
            cg_color_t cg_color = { 0, };
            cg_pipeline_t *pipeline = cg_pipeline_new(text->shell->cg_device);

            /* Paint selection background */
            if (text->selection_color_set)
                color = &text->selection_color;
            else if (text->cursor_color_set)
                color = &text->cursor_color;
            else
                color = &text->text_color;

            cg_pipeline_set_color4f(pipeline,
                                    color->red,
                                    color->green,
                                    color->blue,
                                    paint_opacity * color->alpha);

            rut_text_foreach_selection_rectangle(
                text, add_selection_rectangle_to_path, selection_path);

            cg_path_fill(selection_path, fb, pipeline);

            /* Paint selected text */
            cg_framebuffer_push_path_clip(fb, selection_path);
            cg_object_unref(selection_path);
            cg_object_unref(pipeline);

            if (text->selected_text_color_set)
                color = &text->selected_text_color;
            else
                color = &text->text_color;

            cg_color_init_from_4f(&cg_color,
                                  color->red,
                                  color->green,
                                  color->blue,
                                  paint_opacity * color->alpha);

            cg_pango_show_layout(fb, layout, text->text_x, 0, &cg_color);

            cg_framebuffer_pop_clip(fb);
        }
    }
}

static int
rut_text_move_word_backward(rut_text_t *text, int start)
{
    int retval = start;

    if (rut_text_buffer_get_length(get_buffer(text)) > 0 && start > 0) {
        PangoLayout *layout = rut_text_get_layout(text);
        PangoLogAttr *log_attrs = NULL;
        int n_attrs = 0;

        pango_layout_get_log_attrs(layout, &log_attrs, &n_attrs);

        retval = start - 1;
        while (retval > 0 && !log_attrs[retval].is_word_start)
            retval -= 1;

        c_free(log_attrs);
    }

    return retval;
}

static int
rut_text_move_word_forward(rut_text_t *text, int start)
{
    int retval = start;
    unsigned int n_chars;

    n_chars = rut_text_buffer_get_length(get_buffer(text));
    if (n_chars > 0 && start < n_chars) {
        PangoLayout *layout = rut_text_get_layout(text);
        PangoLogAttr *log_attrs = NULL;
        int n_attrs = 0;

        pango_layout_get_log_attrs(layout, &log_attrs, &n_attrs);

        retval = start + 1;
        while (retval < n_chars && !log_attrs[retval].is_word_end)
            retval += 1;

        c_free(log_attrs);
    }

    return retval;
}

static int
rut_text_move_line_start(rut_text_t *text, int start)
{
    PangoLayoutLine *layout_line;
    PangoLayout *layout;
    int line_no;
    int index_;
    int position;
    const char *text_str;

    layout = rut_text_get_layout(text);
    text_str = rut_text_buffer_get_text(get_buffer(text));

    if (start == 0)
        index_ = 0;
    else
        index_ = offset_to_bytes(text_str, start);

    pango_layout_index_to_line_x(layout, index_, 0, &line_no, NULL);

    layout_line = pango_layout_get_line_readonly(layout, line_no);
    if (!layout_line)
        return false;

    pango_layout_line_x_to_index(layout_line, 0, &index_, NULL);

    position = bytes_to_offset(text_str, index_);

    return position;
}

static int
rut_text_move_line_end(rut_text_t *text, int start)
{
    PangoLayoutLine *layout_line;
    PangoLayout *layout;
    int line_no;
    int index_;
    int trailing;
    int position;
    const char *text_str;

    layout = rut_text_get_layout(text);
    text_str = rut_text_buffer_get_text(get_buffer(text));

    if (start == 0)
        index_ = 0;
    else
        index_ = offset_to_bytes(text_str, text->position);

    pango_layout_index_to_line_x(layout, index_, 0, &line_no, NULL);

    layout_line = pango_layout_get_line_readonly(layout, line_no);
    if (!layout_line)
        return false;

    pango_layout_line_x_to_index(layout_line, G_MAXINT, &index_, &trailing);
    index_ += trailing;

    position = bytes_to_offset(text_str, index_);

    return position;
}

static void
rut_text_select_word(rut_text_t *text)
{
    int cursor_pos = text->position;
    int start_pos, end_pos;

    start_pos = rut_text_move_word_backward(text, cursor_pos);
    end_pos = rut_text_move_word_forward(text, cursor_pos);

    rut_text_set_selection(text, start_pos, end_pos);
}

static void
rut_text_select_line(rut_text_t *text)
{
    int cursor_pos = text->position;
    int start_pos, end_pos;

    if (text->single_line_mode) {
        start_pos = 0;
        end_pos = -1;
    } else {
        start_pos = rut_text_move_line_start(text, cursor_pos);
        end_pos = rut_text_move_line_end(text, cursor_pos);
    }

    rut_text_set_selection(text, start_pos, end_pos);
}

static bool
rut_text_real_move_left(rut_text_t *text, rut_input_event_t *event)
{
    rut_modifier_state_t modifiers = rut_key_event_get_modifier_state(event);
    int pos = text->position;
    int new_pos = 0;
    int len;

    len = rut_text_buffer_get_length(get_buffer(text));

    if (pos != 0 && len != 0) {
        if (modifiers & RUT_MODIFIER_CTRL_ON) {
            if (pos == -1)
                new_pos = rut_text_move_word_backward(text, len);
            else
                new_pos = rut_text_move_word_backward(text, pos);
        } else {
            if (pos == -1)
                new_pos = len - 1;
            else
                new_pos = pos - 1;
        }

        rut_text_set_cursor_position(text, new_pos);
    }

    if (!(text->selectable && (modifiers & RUT_MODIFIER_SHIFT_ON)))
        rut_text_clear_selection(text);

    return true;
}

static bool
rut_text_real_move_right(rut_text_t *text, rut_input_event_t *event)
{
    rut_modifier_state_t modifiers = rut_key_event_get_modifier_state(event);
    int pos = text->position;
    int len = rut_text_buffer_get_length(get_buffer(text));
    int new_pos = 0;

    if (pos != -1 && len != 0) {
        if (modifiers & RUT_MODIFIER_CTRL_ON) {
            if (pos != len)
                new_pos = rut_text_move_word_forward(text, pos);
        } else {
            if (pos != len)
                new_pos = pos + 1;
        }

        rut_text_set_cursor_position(text, new_pos);
    }

    if (!(text->selectable && (modifiers & RUT_MODIFIER_SHIFT_ON)))
        rut_text_clear_selection(text);

    return true;
}

static bool
rut_text_real_move_up(rut_text_t *text, rut_input_event_t *event)
{
    rut_modifier_state_t modifiers = rut_key_event_get_modifier_state(event);
    PangoLayoutLine *layout_line;
    PangoLayout *layout;
    int line_no;
    int index_, trailing;
    int pos;
    int x;
    const char *text_str;

    layout = rut_text_get_layout(text);
    text_str = rut_text_buffer_get_text(get_buffer(text));

    if (text->position == 0)
        index_ = 0;
    else
        index_ = offset_to_bytes(text_str, text->position);

    pango_layout_index_to_line_x(layout, index_, 0, &line_no, &x);

    line_no -= 1;
    if (line_no < 0)
        return false;

    if (text->x_pos != -1)
        x = text->x_pos;

    layout_line = pango_layout_get_line_readonly(layout, line_no);
    if (!layout_line)
        return false;

    pango_layout_line_x_to_index(layout_line, x, &index_, &trailing);

    pos = bytes_to_offset(text_str, index_);
    rut_text_set_cursor_position(text, pos + trailing);

    /* Store the target x position to avoid drifting left and right when
       moving the cursor up and down */
    text->x_pos = x;

    if (!(text->selectable && (modifiers & RUT_MODIFIER_SHIFT_ON)))
        rut_text_clear_selection(text);

    return true;
}

static bool
rut_text_real_move_down(rut_text_t *text, rut_input_event_t *event)
{
    rut_modifier_state_t modifiers = rut_key_event_get_modifier_state(event);
    PangoLayoutLine *layout_line;
    PangoLayout *layout;
    int line_no;
    int index_, trailing;
    int x;
    int pos;
    const char *text_str;

    layout = rut_text_get_layout(text);
    text_str = rut_text_buffer_get_text(get_buffer(text));

    if (text->position == 0)
        index_ = 0;
    else
        index_ = offset_to_bytes(text_str, text->position);

    pango_layout_index_to_line_x(layout, index_, 0, &line_no, &x);

    if (text->x_pos != -1)
        x = text->x_pos;

    layout_line = pango_layout_get_line_readonly(layout, line_no + 1);
    if (!layout_line)
        return false;

    pango_layout_line_x_to_index(layout_line, x, &index_, &trailing);

    pos = bytes_to_offset(text_str, index_);
    rut_text_set_cursor_position(text, pos + trailing);

    /* Store the target x position to avoid drifting left and right when
       moving the cursor up and down */
    text->x_pos = x;

    if (!(text->selectable && (modifiers & RUT_MODIFIER_SHIFT_ON)))
        rut_text_clear_selection(text);

    return true;
}

static bool
rut_text_real_line_start(rut_text_t *text, rut_input_event_t *event)
{
    rut_modifier_state_t modifiers = rut_key_event_get_modifier_state(event);
    int position;

    position = rut_text_move_line_start(text, text->position);
    rut_text_set_cursor_position(text, position);

    if (!(text->selectable && (modifiers & RUT_MODIFIER_SHIFT_ON)))
        rut_text_clear_selection(text);

    return true;
}

static bool
rut_text_real_line_end(rut_text_t *text, rut_input_event_t *event)
{
    rut_modifier_state_t modifiers = rut_key_event_get_modifier_state(event);
    int position;

    position = rut_text_move_line_end(text, text->position);
    rut_text_set_cursor_position(text, position);

    if (!(text->selectable && (modifiers & RUT_MODIFIER_SHIFT_ON)))
        rut_text_clear_selection(text);

    return true;
}

static bool
rut_text_real_select_all(rut_text_t *text, rut_input_event_t *event)
{
    rut_modifier_state_t modifiers = rut_key_event_get_modifier_state(event);
    unsigned int n_chars;

    if (!(modifiers & RUT_MODIFIER_CTRL_ON))
        return false;

    n_chars = rut_text_buffer_get_length(get_buffer(text));
    rut_text_set_positions(text, 0, n_chars);

    return true;
}

static bool
rut_text_real_del_next(rut_text_t *text, rut_input_event_t *event)
{
    int pos;
    int len;

    if (rut_text_delete_selection(text))
        return true;

    pos = text->position;
    len = rut_text_buffer_get_length(get_buffer(text));

    if (len && pos != -1 && pos < len)
        rut_text_delete_text(text, pos, pos + 1);

    return true;
}

static bool
rut_text_real_del_word_next(rut_text_t *text,
                            rut_input_event_t *event)
{
    int pos;
    int len;

    pos = text->position;
    len = rut_text_buffer_get_length(get_buffer(text));

    if (len && pos != -1 && pos < len) {
        int end;

        end = rut_text_move_word_forward(text, pos);
        rut_text_delete_text(text, pos, end);

        if (text->selection_bound >= end) {
            int new_bound;

            new_bound = text->selection_bound - (end - pos);
            rut_text_set_selection_bound(text, new_bound);
        } else if (text->selection_bound > pos) {
            rut_text_set_selection_bound(text, pos);
        }
    }

    return true;
}

static bool
rut_text_real_del_prev(rut_text_t *text, rut_input_event_t *event)
{
    int pos;
    int len;

    if (rut_text_delete_selection(text))
        return true;

    pos = text->position;
    len = rut_text_buffer_get_length(get_buffer(text));

    if (pos != 0 && len != 0) {
        if (pos == -1) {
            rut_text_delete_text(text, len - 1, len);

            rut_text_set_positions(text, -1, -1);
        } else {
            rut_text_delete_text(text, pos - 1, pos);

            rut_text_set_positions(text, pos - 1, pos - 1);
        }
    }

    return true;
}

static bool
rut_text_real_del_word_prev(rut_text_t *text,
                            rut_input_event_t *event)
{
    int pos;
    int len;

    pos = text->position;
    len = rut_text_buffer_get_length(get_buffer(text));

    if (pos != 0 && len != 0) {
        int new_pos;

        if (pos == -1) {
            new_pos = rut_text_move_word_backward(text, len);
            rut_text_delete_text(text, new_pos, len);

            rut_text_set_positions(text, -1, -1);
        } else {
            new_pos = rut_text_move_word_backward(text, pos);
            rut_text_delete_text(text, new_pos, pos);

            rut_text_set_cursor_position(text, new_pos);
            if (text->selection_bound >= pos) {
                int new_bound;

                new_bound = text->selection_bound - (pos - new_pos);
                rut_text_set_selection_bound(text, new_bound);
            } else if (text->selection_bound >= new_pos) {
                rut_text_set_selection_bound(text, new_pos);
            }
        }
    }

    return true;
}

static bool
rut_text_real_activate(rut_text_t *text, rut_input_event_t *event)
{
    return rut_text_activate(text);
}

static rut_input_event_status_t
rut_text_motion_grab(rut_input_event_t *event,
                     void *user_data)
{
    rut_text_t *text = user_data;
    rut_object_t *camera = rut_input_event_get_camera(event);
    float x, y;
    int index_, offset;
    const char *text_str;
    c_matrix_t transform;
    c_matrix_t inverse_transform;

    c_return_val_if_fail(text->in_select_drag,
                         RUT_INPUT_EVENT_STATUS_UNHANDLED);

    if (rut_input_event_get_type(event) != RUT_INPUT_EVENT_TYPE_MOTION)
        return RUT_INPUT_EVENT_STATUS_UNHANDLED;

    c_debug("Grab\n");
    if (rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_MOVE) {
        const c_matrix_t *view = rut_camera_get_view_transform(camera);

        transform = *view;
        rut_graphable_apply_transform(text, &transform);

        if (!c_matrix_get_inverse(&transform, &inverse_transform)) {
            c_debug("Failed to get inverse\n");
            return RUT_INPUT_EVENT_STATUS_UNHANDLED;
        }

        x = rut_motion_event_get_x(event);
        y = rut_motion_event_get_y(event);
        rut_camera_unproject_coord(
            camera, &transform, &inverse_transform, 0, &x, &y);

        c_debug("Grab x=%f y=%f\n", x, y);

        index_ = rut_text_coords_to_position(text, x, y);
        text_str = rut_text_buffer_get_text(get_buffer(text));
        offset = bytes_to_offset(text_str, index_);

        if (text->selectable) {
            rut_text_set_cursor_position(text, offset);
            rut_shell_set_selection(text->shell, text);
        } else
            rut_text_set_positions(text, offset, offset);
    } else if (rut_motion_event_get_action(event) ==
               RUT_MOTION_EVENT_ACTION_UP) {
        rut_shell_ungrab_input(
            text->shell, rut_text_motion_grab, user_data);
        text->in_select_drag = false;
        return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

    return RUT_INPUT_EVENT_STATUS_HANDLED;
}

static gboolean
rut_text_remove_password_hint(void *data)
{
    rut_text_t *text = data;

    text->password_hint_visible = false;
    text->password_hint_id = 0;

    rut_text_dirty_cache(data);
    rut_shell_queue_redraw(text->shell);

    return G_SOURCE_REMOVE;
}

static rut_input_event_status_t
rut_text_button_press(rut_text_t *text,
                      rut_input_event_t *event)
{
    // bool res = false;
    float x, y;
    int index_;
    c_matrix_t transform;
    c_matrix_t inverse_transform;
    rut_object_t *camera;

    c_debug("rut_text_t Button Press!\n");
    /* we'll steal keyfocus if we need it */
    if (text->editable || text->selectable)
        rut_text_grab_key_focus(text);

    x = rut_motion_event_get_x(event);
    y = rut_motion_event_get_y(event);

    camera = rut_input_event_get_camera(event);

    if (text->has_focus && !rut_pickable_pick(text->input_region,
                                              camera,
                                              NULL, /* pre-computed modelview */
                                              x,
                                              y)) {
        rut_text_ungrab_key_focus(text);

        /* Note: we don't want to claim this event by returning _HANDLED
         * here since that would mean, for example, that the user goes
         * to grab a scrollbar when typing then they would have to click
         * the scrollbar twice, once to drop the text entry grab and
         * then again to actually grab the scrollbar. */
        c_debug("Ungrab\n");
        return RUT_INPUT_EVENT_STATUS_UNHANDLED;
    }

    /* if the actor is empty we just reset everything and not
     * set up the dragging of the selection since there's nothing
     * to select
     */
    if (rut_text_buffer_get_length(get_buffer(text)) == 0) {
        rut_text_set_positions(text, -1, -1);

        return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

    rut_graphable_get_modelview(text, camera, &transform);
    if (c_matrix_get_inverse(&transform, &inverse_transform)) {
        int offset;
        const char *text_str;

        rut_camera_unproject_coord(
            camera, &transform, &inverse_transform, 0, &x, &y);

        index_ = rut_text_coords_to_position(text, x, y);
        text_str = rut_text_buffer_get_text(get_buffer(text));
        offset = bytes_to_offset(text_str, index_);

#warning "TODO: handle single vs double vs tripple click"
#if 0
        /* what we select depends on the number of button clicks we
         * receive:
         *
         *   1: just position the cursor and the selection
         *   2: select the current word
         *   3: select the contents of the whole actor
         */
        if (event->click_count == 1)
        {
            rut_text_set_positions (text, offset, offset);
        }
        else if (event->click_count == 2)
        {
            rut_text_select_word (text);
        }
        else if (event->click_count == 3)
        {
            rut_text_select_line (text);
        }
#else
        rut_text_set_positions(text, offset, offset);
#endif
    }

    /* grab the pointer */
    text->in_select_drag = true;
    rut_shell_grab_input(text->shell, camera, rut_text_motion_grab, text);

    return RUT_INPUT_EVENT_STATUS_HANDLED;
}

static rut_input_event_status_t
rut_text_input_cb(rut_input_event_t *event,
                  void *user_data)
{
    rut_text_t *text = user_data;

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION &&
        rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_DOWN)
        return rut_text_button_press(text, event);
    else if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_DROP &&
             rut_text_get_editable(text) == true) {
        rut_object_t *data = rut_drop_event_get_data(event);

        if (rut_mimable_has_text(data)) {
            char *text_data = rut_mimable_get_text(data);
            rut_text_clear_selection(text);
            rut_text_insert_text(text, text_data, text->position);
            c_free(text_data);
        }

        return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static rut_input_event_status_t
rut_text_handle_key_event(rut_text_t *text, rut_input_event_t *event)
{
    // rut_bin_tdingPool *pool;
    // bool res;
    bool handled = false;

    if (rut_key_event_get_action(event) != RUT_KEY_EVENT_ACTION_DOWN)
        return RUT_INPUT_EVENT_STATUS_HANDLED;

    if (!text->editable)
        return RUT_INPUT_EVENT_STATUS_HANDLED;

#if 0
    /* we need to use the rut_text_t type name to find our own
     * key bindings; subclasses will override or chain up this
     * event handler, so they can do whatever they want there
     */
    pool = rut_binding_pool_find (g_type_name (RUT_TYPE_TEXT));
    c_assert (pool != NULL);

    /* we allow passing synthetic events that only contain
     * the Unicode value and not the key symbol
     */
    if (event->keyval == 0 && (event->flags & RUT_EVENT_FLAG_SYNTHETIC))
        res = false;
    else
        res = rut_binding_pool_activate (pool, event->keyval,
                                         event->modifier_state,
                                         G_OBJECT (actor));

    /* if the key binding has handled the event we bail out
     * as fast as we can; otherwise, we try to insert the
     * Unicode character inside the key event into the text
     * actor
     */
    if (res)
        return RUT_EVENT_STOP;
    else
#endif

    switch (rut_key_event_get_keysym(event)) {
    case RUT_KEY_Left:
    case RUT_KEY_KP_Left:
        handled = rut_text_real_move_left(text, event);
        break;
    case RUT_KEY_Right:
    case RUT_KEY_KP_Right:
        handled = rut_text_real_move_right(text, event);
        break;
    case RUT_KEY_Up:
    case RUT_KEY_KP_Up:
        handled = rut_text_real_move_up(text, event);
        break;
    case RUT_KEY_Down:
    case RUT_KEY_KP_Down:
        handled = rut_text_real_move_down(text, event);
        break;
    case RUT_KEY_Home:
    case RUT_KEY_KP_Home:
    case RUT_KEY_Begin:
        handled = rut_text_real_line_start(text, event);
        break;
    case RUT_KEY_End:
    case RUT_KEY_KP_End:
        handled = rut_text_real_line_end(text, event);
        break;
    case RUT_KEY_a:
        handled = rut_text_real_select_all(text, event);
        break;
    case RUT_KEY_Delete:
    case RUT_KEY_KP_Delete:
        if (rut_key_event_get_modifier_state(event) & RUT_MODIFIER_CTRL_ON)
            handled = rut_text_real_del_word_next(text, event);
        else
            handled = rut_text_real_del_next(text, event);
        break;
    case RUT_KEY_BackSpace:
        if (rut_key_event_get_modifier_state(event) & RUT_MODIFIER_CTRL_ON)
            handled = rut_text_real_del_word_prev(text, event);
        else
            handled = rut_text_real_del_prev(text, event);
        break;
    case RUT_KEY_Return:
    case RUT_KEY_KP_Enter:
    case RUT_KEY_ISO_Enter:
        handled = rut_text_real_activate(text, event);
        break;
    case RUT_KEY_Escape:
        rut_text_ungrab_key_focus(text);
        handled = true;
        break;
    }

    return (handled ? RUT_INPUT_EVENT_STATUS_HANDLED
            : RUT_INPUT_EVENT_STATUS_UNHANDLED);
}

static rut_input_event_status_t
rut_text_handle_text_event(rut_text_t *text, rut_input_event_t *event)
{
    const char *text_str = rut_text_event_get_text(event);
    const char *next;
    char *text_buf = c_alloca(strlen(text_str) + 1);
    char *p;

    /* Ignore text events when the control key is down */
    if ((rut_key_event_get_modifier_state(event) & RUT_MODIFIER_CTRL_ON))
        return RUT_INPUT_EVENT_STATUS_UNHANDLED;

    for (p = text_buf; *text_str; text_str = next) {
        gunichar key_unichar = g_utf8_get_char(text_str);

        next = g_utf8_next_char(text_str);

        /* return is reported as CR, but we want LF */
        if (key_unichar == '\r' || key_unichar == '\n')
            *(p++) = '\n';
        else if (g_unichar_validate(key_unichar) &&
                 !g_unichar_iscntrl(key_unichar)) {
            memcpy(p, text_str, next - text_str);
            p += next - text_str;
        }
    }

    if (p > text_buf) {
        *p = '\0';

        /* truncate the eventual selection so that the
         * Unicode character can replace it
         */
        rut_text_delete_selection(text);
        rut_text_insert_text(text, text_buf, text->position);

        if (text->show_password_hint) {
            if (text->password_hint_id != 0)
                g_source_remove(text->password_hint_id);

            text->password_hint_visible = true;
            text->password_hint_id =
                g_timeout_add(text->password_hint_timeout,
                              rut_text_remove_password_hint,
                              text);
        }

        return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

    return RUT_INPUT_EVENT_STATUS_HANDLED;
}

static rut_input_event_status_t
rut_text_key_press(rut_input_event_t *event,
                   void *user_data)
{
    rut_text_t *text = user_data;

    switch (rut_input_event_get_type(event)) {
    case RUT_INPUT_EVENT_TYPE_KEY:
        return rut_text_handle_key_event(text, event);

    case RUT_INPUT_EVENT_TYPE_TEXT:
        return rut_text_handle_text_event(text, event);

    default:
        return rut_text_input_cb(event, user_data);
    }
}

void
rut_text_grab_key_focus(rut_text_t *text)
{
    if (!text->has_focus) {
        text->has_focus = true;

        /* Note: we don't use rut_shell_grab_key_focus here becase we also want
         * to grab mouse input that might otherwise slopily to move focus to
         * other
         * parts of the UI.
         */
        rut_shell_grab_input(text->shell, NULL, rut_text_key_press, text);
        rut_shell_queue_redraw(text->shell);
    }
}

void
rut_text_ungrab_key_focus(rut_text_t *text)
{
    if (text->has_focus) {
        rut_shell_ungrab_input(text->shell, rut_text_key_press, text);
        text->has_focus = false;
        rut_shell_queue_redraw(text->shell);
    }
}

#define TEXT_PADDING 2

static void
rut_text_paint(rut_object_t *object, rut_paint_context_t *paint_ctx)
{
    rut_text_t *text = object;
    rut_object_t *camera = paint_ctx->camera;
    cg_framebuffer_t *fb = rut_camera_get_framebuffer(camera);
    PangoLayout *layout;
    // rut_box_t alloc;
    cg_color_t color = { 0, };
    float real_opacity;
    int text_x = text->text_x;
    bool clip_set = false;
    // bool bg_color_set = false;
    unsigned int n_chars;
    float width, height;

    n_chars = rut_text_buffer_get_length(get_buffer(text));

    /* don't bother painting an empty text actor, unless it's
     * editable, in which case we want to paint at least the
     * cursor
     */
    if (n_chars == 0 && (!text->editable || !text->cursor_visible))
        return;

    rut_sizable_get_size(text, &width, &height);
// rut_text_get_allocation_box (text, &alloc);

#if 0
    g_object_get (text, "background-color-set", &bg_color_set, NULL);
    if (bg_color_set)
    {
        cg_color_t bg_color;

        rut_actor_get_background_color (text, &bg_color);
        bg_color.alpha = rut_text_get_paint_opacity (text)
                         * bg_color.alpha;

        cg_set_source_color4ub (bg_color.red,
                                bg_color.green,
                                bg_color.blue,
                                bg_color.alpha);
        cg_rectangle (0, 0, alloc.x2 - alloc.x1, alloc.y2 - alloc.y1);
    }
#endif

    if (text->editable && text->single_line_mode) {
        if (n_chars == 0 && text->hint_text && !text->has_focus)
            layout = rut_text_ensure_hint_text_layout(text);
        else
            layout = rut_text_create_layout(text, -1, -1);
    } else {
        /* the only time when we create the PangoLayout using the full
         * width and height of the allocation is when we can both wrap
         * and ellipsize
         */
        if (text->wrap && text->ellipsize) {
#if 0
            layout = rut_text_create_layout (text,
                                             alloc.x2 - alloc.x1,
                                             alloc.y2 - alloc.y1);
#endif
            layout = rut_text_create_layout(text, width, height);
        } else {
/* if we're not wrapping we cannot set the height of the
 * layout, otherwise Pango will happily wrap the text to
 * fit in the rectangle - thus making the :wrap property
 * useless
 *
 * see bug:
 *
 *   http://bugzilla.rut-project.org/show_bug.cgi?id=2339
 *
 * in order to fix this, we create a layout that would fit
 * in the assigned width, then we clip the actor if the
 * logical rectangle overflows the allocation.
 */
#if 0
            layout = rut_text_create_layout (text,
                                             alloc.x2 - alloc.x1,
                                             -1);
#endif
            layout = rut_text_create_layout(text, width, -1);
        }
    }

    if (text->editable && text->cursor_visible)
        rut_text_ensure_cursor_position(text);

    if (text->editable && text->single_line_mode) {
        PangoRectangle logical_rect = { 0, };
        int actor_width, text_width;

        pango_layout_get_extents(layout, NULL, &logical_rect);

        cg_framebuffer_push_rectangle_clip(fb, 0, 0, width, height);
        //(alloc.x2 - alloc.x1),
        //(alloc.y2 - alloc.y1));
        clip_set = true;

        // actor_width = (alloc.x2 - alloc.x1)
        actor_width = width - 2 * TEXT_PADDING;
        text_width = logical_rect.width / PANGO_SCALE;

        if (actor_width < text_width) {
            int cursor_x = text->cursor_pos.x;

            if (text->position == -1) {
                text_x = actor_width - text_width;
            } else if (text->position == 0) {
                text_x = TEXT_PADDING;
            } else {
                if (cursor_x < 0) {
                    text_x = text_x - cursor_x - TEXT_PADDING;
                } else if (cursor_x > actor_width) {
                    text_x = text_x + (actor_width - cursor_x) - TEXT_PADDING;
                }
            }
        } else {
            text_x = TEXT_PADDING;
        }
    } else if (!text->editable && !(text->wrap && text->ellipsize)) {
        PangoRectangle logical_rect = { 0, };

        pango_layout_get_pixel_extents(layout, NULL, &logical_rect);

        /* don't clip if the layout managed to fit inside our allocation */
        // if (logical_rect.width > (alloc.x2 - alloc.x1) ||
        //    logical_rect.height > (alloc.y2 - alloc.y1))
        if (logical_rect.width > width || logical_rect.height > height) {
            cg_framebuffer_push_rectangle_clip(fb,
                                               0,
                                               0,
                                               // alloc.x2 - alloc.x1,
                                               // alloc.y2 - alloc.y1);
                                               width,
                                               height);
            clip_set = true;
        }

        text_x = 0;
    } else
        text_x = 0;

    if (text->text_x != text_x) {
        text->text_x = text_x;
        rut_text_ensure_cursor_position(text);
    }

    real_opacity = rut_text_get_paint_opacity(text) * text->text_color.alpha;

    cg_color_init_from_4f(&color,
                          text->text_color.red,
                          text->text_color.green,
                          text->text_color.blue,
                          real_opacity);
    cg_pango_show_layout(fb, layout, text_x, text->text_y, &color);

    selection_paint(text, paint_ctx);

    if (clip_set)
        cg_framebuffer_pop_clip(fb);
}

static void
_rut_text_get_preferred_width(rut_object_t *object,
                              float for_height,
                              float *min_width_p,
                              float *natural_width_p)
{
    rut_text_t *text = object;
    PangoRectangle logical_rect = { 0, };
    PangoLayout *layout = NULL;
    int logical_width;
    float layout_width;

    if (text->editable && text->single_line_mode) {
        int n_chars = rut_text_buffer_get_length(get_buffer(text));
        if (n_chars == 0 && text->hint_text)
            layout = rut_text_ensure_hint_text_layout(text);
    }

    if (!layout)
        layout = rut_text_create_layout(text, -1, -1);

    pango_layout_get_extents(layout, NULL, &logical_rect);

    /* the X coordinate of the logical rectangle might be non-zero
     * according to the Pango documentation; hence, we need to offset
     * the width accordingly
     */
    logical_width = logical_rect.x + logical_rect.width;

    layout_width = logical_width > 0 ? ceilf(logical_width / 1024.0f) : 1;

    if (min_width_p) {
        if (text->wrap || text->ellipsize || text->editable)
            *min_width_p = 1;
        else
            *min_width_p = layout_width;
    }

    if (natural_width_p) {
        if (text->editable && text->single_line_mode)
            *natural_width_p = layout_width + TEXT_PADDING * 2;
        else
            *natural_width_p = layout_width;
    }
}

static void
_rut_text_get_preferred_height(rut_object_t *object,
                               float for_width,
                               float *min_height_p,
                               float *natural_height_p)
{
    rut_text_t *text = object;

    if (for_width == 0) {
        if (min_height_p)
            *min_height_p = 0;

        if (natural_height_p)
            *natural_height_p = 0;
    } else {
        PangoLayout *layout = NULL;
        PangoRectangle logical_rect = { 0, };
        int logical_height;
        float layout_height;

        if (text->single_line_mode)
            for_width = -1;

        if (text->editable && text->single_line_mode) {
            int n_chars = rut_text_buffer_get_length(get_buffer(text));
            if (n_chars == 0 && text->hint_text)
                layout = rut_text_ensure_hint_text_layout(text);
        }

        if (!layout)
            layout = rut_text_create_layout(text, for_width, -1);

        pango_layout_get_extents(layout, NULL, &logical_rect);

        /* the Y coordinate of the logical rectangle might be non-zero
         * according to the Pango documentation; hence, we need to offset
         * the height accordingly
         */
        logical_height = logical_rect.y + logical_rect.height;
        layout_height = ceilf(logical_height / 1024.0f);

        if (min_height_p) {
            /* if we wrap and ellipsize then the minimum height is
             * going to be at least the size of the first line
             */
            if ((text->ellipsize && text->wrap) && !text->single_line_mode) {
                PangoLayoutLine *line;
                float line_height;

                line = pango_layout_get_line_readonly(layout, 0);
                pango_layout_line_get_extents(line, NULL, &logical_rect);

                logical_height = logical_rect.y + logical_rect.height;
                line_height = ceilf(logical_height / 1024.0f);

                *min_height_p = line_height;
            } else
                *min_height_p = layout_height;
        }

        if (natural_height_p)
            *natural_height_p = layout_height;
    }
}

#if 0
void
rut_text_allocate (rut_text_t *text,
                   const rut_box_t *box)
{
    text->allocation = *box;

    /* Ensure that there is a cached layout with the right width so
     * that we don't need to create the text during the paint run
     *
     * if the Text is editable and in single line mode we don't want
     * to have any limit on the layout size, since the paint will clip
     * it to the allocation of the actor
     */
    if (text->editable && text->single_line_mode)
        rut_text_create_layout (text, -1, -1);
    else
        rut_text_create_layout (text,
                                box->x2 - box->x1,
                                box->y2 - box->y1);
}
#endif

static void
_rut_text_set_size(rut_object_t *object, float width, float height)
{
    rut_text_t *text = object;
    cg_vertex_p3_t *pick_vertices;

    if (text->width == width && text->height == height)
        return;

    text->width = width;
    text->height = height;

    /* Ensure that there is a cached layout with the right width so
     * that we don't need to create the text during the paint run
     *
     * if the Text is editable and in single line mode we don't want
     * to have any limit on the layout size, since the paint will clip
     * it to the allocation of the actor
     */
    if (text->editable && text->single_line_mode)
        rut_text_create_layout(text, -1, -1);
    else
        rut_text_create_layout(text, width, height);

    if (text->input_region)
        rut_input_region_set_rectangle(text->input_region, 0, 0, width, height);

    pick_vertices =
        (cg_vertex_p3_t *)text->pick_mesh->attributes[0]->buffered.buffer->data;
    pick_vertices[0].x = 0;
    pick_vertices[0].y = 0;
    pick_vertices[1].x = 0;
    pick_vertices[1].y = height;
    pick_vertices[2].x = width;
    pick_vertices[2].y = height;
    pick_vertices[3] = pick_vertices[0];
    pick_vertices[4] = pick_vertices[2];
    pick_vertices[5].x = width;
    pick_vertices[5].y = 0;

    rut_property_dirty(&text->shell->property_ctx,
                       &text->properties[RUT_TEXT_PROP_WIDTH]);
    rut_property_dirty(&text->shell->property_ctx,
                       &text->properties[RUT_TEXT_PROP_HEIGHT]);
}

void
rut_text_set_width(rut_object_t *obj, float width)
{
    rut_text_t *text = obj;

    _rut_text_set_size(text, width, text->height);
}

void
rut_text_set_height(rut_object_t *obj, float height)
{
    rut_text_t *text = obj;

    _rut_text_set_size(text, text->width, height);
}

static rut_closure_t *
_rut_text_add_preferred_size_callback(void *object,
                                      rut_sizeable_preferred_size_callback_t cb,
                                      void *user_data,
                                      rut_closure_destroy_callback_t destroy)
{
    rut_text_t *text = object;

    return rut_closure_list_add_FIXME(
        &text->preferred_size_cb_list, cb, user_data, destroy);
}

bool
rut_text_has_overlaps(rut_text_t *text)
{
    return text->editable || text->selectable || text->cursor_visible;
}

static rut_input_event_status_t
rut_text_input_region_cb(
    rut_input_region_t *region, rut_input_event_t *event, void *user_data)
{
    return rut_text_input_cb(event, user_data);
}

#if 0
static inline void
rut_text_add_move_binding (rut_bin_tdingPool  *pool,
                           const char *action,
                           unsigned int key_val,
                           RutModifierType additional_modifiers,
                           GCallback callback)
{
    rut_binding_pool_install_action (pool, action,
                                     key_val,
                                     0,
                                     callback,
                                     NULL, NULL);
    rut_binding_pool_install_action (pool, action,
                                     key_val,
                                     RUT_MODIFIER_SHIFT_ON,
                                     callback,
                                     NULL, NULL);

    if (additional_modifiers != 0)
    {
        rut_binding_pool_install_action (pool, action,
                                         key_val,
                                         additional_modifiers,
                                         callback,
                                         NULL, NULL);
        rut_binding_pool_install_action (pool, action,
                                         key_val,
                                         RUT_MODIFIER_SHIFT_ON |
                                         additional_modifiers,
                                         callback,
                                         NULL, NULL);
    }
}
#endif

static void
_rut_text_selectable_cancel(rut_object_t *object)
{
    rut_text_t *text = object;

    rut_text_clear_selection(text);
}

static rut_object_t *
_rut_text_selectable_copy(rut_object_t *object)
{
    rut_text_t *text = object;
    char *text_data = rut_text_get_selection(text);
    rut_text_blob_t *copy = rut_text_blob_new(text_data);

    c_free(text_data);

    return copy;
}

static void
_rut_text_selectable_delete(rut_object_t *object)
{
    rut_text_t *text = object;

    rut_text_delete_selection(text);
}

rut_type_t rut_text_type;

void
_rut_text_init_type(void)
{
    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child remove */
                                                       NULL, /* child add */
                                                       NULL /* parent changed */
    };

    static rut_paintable_vtable_t paintable_vtable = { rut_text_paint };

    static rut_sizable_vtable_t sizable_vtable = {
        _rut_text_set_size,                   _rut_text_get_size,
        _rut_text_get_preferred_width,        _rut_text_get_preferred_height,
        _rut_text_add_preferred_size_callback
    };

    static rut_meshable_vtable_t meshable_vtable = {
        .get_mesh = rut_text_get_pick_mesh
    };

    static rut_selectable_vtable_t selectable_vtable = {
        .cancel = _rut_text_selectable_cancel,
        .copy = _rut_text_selectable_copy,
        .del = _rut_text_selectable_delete,
    };

    rut_type_t *type = &rut_text_type;
#define TYPE rut_text_t

    rut_type_init(&rut_text_type, C_STRINGIFY(TYPE), _rut_text_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_GRAPHABLE,
                       offsetof(TYPE, graphable),
                       &graphable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_PAINTABLE,
                       offsetof(TYPE, paintable),
                       &paintable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_MESHABLE,
                       0, /* no associated properties */
                       &meshable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_SIZABLE,
                       0, /* no implied properties */
                       &sizable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_SELECTABLE,
                       0, /* no associated properties */
                       &selectable_vtable);
#undef TYPE
}

rut_text_t *
rut_text_new_full(rut_shell_t *shell,
                  const char *font_name,
                  const char *text_str,
                  rut_text_buffer_t *buffer)
{
    rut_text_t *text =
        rut_object_alloc0(rut_text_t, &rut_text_type, _rut_text_init_type);
    rut_buffer_t *mesh_buffer = rut_buffer_new(sizeof(cg_vertex_p3_t) * 6);
    rut_mesh_t *pick_mesh =
        rut_mesh_new_from_buffer_p3(CG_VERTICES_MODE_TRIANGLES, 6, mesh_buffer);
    int i, password_hint_time;

    rut_object_unref(mesh_buffer);

    c_list_init(&text->preferred_size_cb_list);
    c_list_init(&text->delete_text_cb_list);
    c_list_init(&text->insert_text_cb_list);
    c_list_init(&text->activate_cb_list);
    c_list_init(&text->cursor_event_cb_list);
    c_list_init(&text->text_changed_cb_list);
    c_list_init(&text->text_deleted_cb_list);
    c_list_init(&text->text_inserted_cb_list);

    rut_graphable_init(text);
    rut_paintable_init(text);

    rut_introspectable_init(text, _rut_text_prop_specs, text->properties);

    text->shell = shell;

    text->alignment = PANGO_ALIGN_LEFT;
    text->wrap = false;
    text->wrap_mode = PANGO_WRAP_WORD;
    text->ellipsize = PANGO_ELLIPSIZE_NONE;
    text->use_underline = false;
    text->use_markup = false;
    text->justify = false;
    text->activatable = true;
    text->pick_mesh = pick_mesh;

    for (i = 0; i < N_CACHED_LAYOUTS; i++)
        text->cached_layouts[i].layout = NULL;

    /* default to "" so that rut_text_get_text() will
     * return a valid string and we can safely call strlen()
     * or strcmp() on it
     */
    text->buffer = buffer;

    rut_color_init_from_uint32(&text->text_color, default_text_color);
    rut_color_init_from_uint32(&text->cursor_color, default_cursor_color);
    rut_color_init_from_uint32(&text->selection_color, default_selection_color);
    rut_color_init_from_uint32(&text->selected_text_color,
                               default_selected_text_color);

    text->direction = rut_shell_get_text_direction(shell);

    /* get the default font name from the context; we don't use
     * set_font_description() here because we are initializing
     * the Text and we don't need notifications and sanity checks
     */
    password_hint_time =
        rut_settings_get_password_hint_time(text->shell->settings);

    text->font_name =
        font_name ? c_strdup(font_name)
        : rut_settings_get_font_name(
            text->shell->settings);             /* font_name is allocated */
    text->font_desc = pango_font_description_from_string(text->font_name);
    text->is_default_font = true;

    text->position = -1;
    text->selection_bound = -1;

    text->x_pos = -1;
    text->cursor_visible = true;
    text->editable = false;
    text->selectable = true;
    text->single_line_mode = true;

    text->selection_color_set = false;
    text->cursor_color_set = false;
    text->selected_text_color_set = true;
    text->preedit_set = false;

    text->password_char = 0;
    text->show_password_hint = password_hint_time > 0;
    text->password_hint_timeout = password_hint_time;

    text->text_y = 0;

    text->cursor_size = DEFAULT_CURSOR_SIZE;
    memset(&text->cursor_pos, 0, sizeof(rut_rectangle_int_t));

    rut_settings_add_changed_callback(shell->settings,
                                      rut_text_settings_changed_cb, NULL,
                                      text);

    rut_text_set_text(text, text_str);

    text->input_region = rut_input_region_new_rectangle(
        0, 0, 0, 0, rut_text_input_region_cb, text);
    rut_graphable_add_child(text, text->input_region);

    update_size(text);

    return text;
}

rut_text_t *
rut_text_new(rut_shell_t *shell)
{
    return rut_text_new_full(shell, NULL, "", NULL);
}

rut_text_t *
rut_text_new_with_text(rut_shell_t *shell,
                       const char *font_name,
                       const char *text)
{
    return rut_text_new_full(shell, font_name, text, NULL);
}

static rut_text_buffer_t *
get_buffer(rut_text_t *text)
{
    if (text->buffer == NULL) {
        rut_text_buffer_t *buffer;
        buffer = rut_text_buffer_new(text->shell);
        rut_text_set_buffer(text, buffer);
        rut_object_unref(buffer);
    }

    return text->buffer;
}

/* GtkEntryBuffer signal handlers
 */
static void
buffer_inserted_text(rut_text_buffer_t *buffer,
                     int position,
                     const char *chars,
                     int n_chars,
                     void *user_data)
{
    rut_text_t *text = user_data;
    int new_position;
    int new_selection_bound;
    gsize n_bytes;

    if (text->position >= 0 || text->selection_bound >= 0) {
        new_position = text->position;
        new_selection_bound = text->selection_bound;

        if (position <= new_position)
            new_position += n_chars;
        if (position <= new_selection_bound)
            new_selection_bound += n_chars;

        if (text->position != new_position ||
            text->selection_bound != new_selection_bound)
            rut_text_set_positions(text, new_position, new_selection_bound);
    }

    n_bytes = g_utf8_offset_to_pointer(chars, n_chars) - chars;

    rut_closure_list_invoke(&text->text_inserted_cb_list,
                            rut_text_inserted_callback_t,
                            text,
                            chars,
                            n_bytes,
                            &position);

    /* TODO: What are we supposed to with the out value of position? */
}

static void
buffer_deleted_text(rut_text_buffer_t *buffer,
                    int position,
                    int n_chars,
                    void *user_data)
{
    rut_text_t *text = user_data;
    int new_position;
    int new_selection_bound;

    if (text->position >= 0 || text->selection_bound >= 0) {
        new_position = text->position;
        new_selection_bound = text->selection_bound;

        if (position < new_position)
            new_position -= n_chars;
        if (position < new_selection_bound)
            new_selection_bound -= n_chars;

        if (text->position != new_position ||
            text->selection_bound != new_selection_bound)
            rut_text_set_positions(text, new_position, new_selection_bound);
    }

    rut_closure_list_invoke(&text->delete_text_cb_list,
                            rut_text_deleted_callback_t,
                            text,
                            position,
                            position + n_chars);
}

static void
text_property_binding_cb(rut_property_t *target_property,
                         void *user_data)
{
    rut_text_t *text = user_data;

    rut_text_dirty_cache(text);

    rut_text_notify_preferred_size_changed(text);

    rut_closure_list_invoke(
        &text->text_changed_cb_list, rut_text_changed_callback_t, text);

    rut_property_dirty(&text->shell->property_ctx,
                       &text->properties[RUT_TEXT_PROP_TEXT]);

    rut_shell_queue_redraw(text->shell);
}

static void
max_length_property_binding_cb(rut_property_t *target_property,
                               void *user_data)
{
    rut_text_t *text = user_data;
    rut_property_dirty(&text->shell->property_ctx,
                       &text->properties[RUT_TEXT_PROP_MAX_LENGTH]);
}

static void
buffer_connect_signals(rut_text_t *text)
{
    rut_property_t *buffer_text_prop;
    rut_property_t *buffer_max_len_prop;

    text->buffer_insert_text_closure = rut_text_buffer_add_insert_text_callback(
        text->buffer, buffer_inserted_text, text, NULL);

    text->buffer_delete_text_closure = rut_text_buffer_add_delete_text_callback(
        text->buffer, buffer_deleted_text, text, NULL);

    buffer_text_prop = rut_introspectable_lookup_property(text->buffer, "text");
    rut_property_set_binding(&text->properties[RUT_TEXT_PROP_TEXT],
                             text_property_binding_cb,
                             text,
                             buffer_text_prop,
                             NULL);
    buffer_max_len_prop =
        rut_introspectable_lookup_property(text->buffer, "max-length");
    rut_property_set_binding(&text->properties[RUT_TEXT_PROP_MAX_LENGTH],
                             max_length_property_binding_cb,
                             text,
                             buffer_max_len_prop,
                             NULL);
}

static void
buffer_disconnect_signals(rut_text_t *text)
{
    rut_closure_disconnect_FIXME(text->buffer_insert_text_closure);
    rut_closure_disconnect_FIXME(text->buffer_delete_text_closure);

    rut_property_set_binding(
        &text->properties[RUT_TEXT_PROP_TEXT], NULL, NULL, NULL);
    rut_property_set_binding(
        &text->properties[RUT_TEXT_PROP_MAX_LENGTH], NULL, NULL, NULL);
}

rut_text_t *
rut_text_new_with_buffer(rut_shell_t *shell,
                         rut_text_buffer_t *buffer)
{
    return rut_text_new_full(shell, NULL, "", buffer);
}

rut_object_t *
rut_text_get_buffer(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return get_buffer(text);
}

void
rut_text_set_buffer(rut_object_t *obj, rut_object_t *buffer)
{
    rut_text_t *text = obj;

    if (buffer)
        rut_object_ref(buffer);

    if (text->buffer) {
        buffer_disconnect_signals(text);
        rut_object_unref(text->buffer);
    }

    text->buffer = buffer;

    if (text->buffer)
        buffer_connect_signals(text);

    rut_property_dirty(&text->shell->property_ctx,
                       &text->properties[RUT_TEXT_PROP_BUFFER]);
    rut_property_dirty(&text->shell->property_ctx,
                       &text->properties[RUT_TEXT_PROP_TEXT]);
    rut_property_dirty(&text->shell->property_ctx,
                       &text->properties[RUT_TEXT_PROP_MAX_LENGTH]);
}

static void
add_remove_input_region(rut_text_t *text)
{
    if (text->editable || text->selectable)
        rut_graphable_add_child(text, text->input_region);
    else
        rut_graphable_remove_child(text->input_region);
}

void
rut_text_set_editable(rut_object_t *obj, bool editable)
{
    rut_text_t *text = obj;

    if (text->editable != editable) {
        text->editable = editable;

        add_remove_input_region(text);

        rut_shell_queue_redraw(text->shell);

        rut_property_dirty(&text->shell->property_ctx,
                           &text->properties[RUT_TEXT_PROP_EDITABLE]);
    }
}

bool
rut_text_get_editable(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return text->editable;
}

void
rut_text_set_selectable(rut_object_t *obj, bool selectable)
{
    rut_text_t *text = obj;

    if (text->selectable != selectable) {
        text->selectable = selectable;

        add_remove_input_region(text);

        rut_shell_queue_redraw(text->shell);

        rut_property_dirty(&text->shell->property_ctx,
                           &text->properties[RUT_TEXT_PROP_SELECTABLE]);
    }
}

bool
rut_text_get_selectable(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return text->selectable;
}

void
rut_text_set_activatable(rut_object_t *obj, bool activatable)
{
    rut_text_t *text = obj;

    if (text->activatable != activatable) {
        text->activatable = activatable;

        rut_shell_queue_redraw(text->shell);

        rut_property_dirty(&text->shell->property_ctx,
                           &text->properties[RUT_TEXT_PROP_ACTIVATABLE]);
    }
}

bool
rut_text_get_activatable(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return text->activatable;
}

bool
rut_text_activate(rut_text_t *text)
{
    if (text->activatable) {
        rut_closure_list_invoke(
            &text->activate_cb_list, rut_text_activate_callback_t, text);
        return true;
    }

    rut_text_ungrab_key_focus(text);

    return false;
}

void
rut_text_set_cursor_visible(rut_object_t *obj, bool cursor_visible)
{
    rut_text_t *text = obj;

    if (text->cursor_visible != cursor_visible) {
        text->cursor_visible = cursor_visible;

        rut_shell_queue_redraw(text->shell);

        rut_property_dirty(&text->shell->property_ctx,
                           &text->properties[RUT_TEXT_PROP_CURSOR_VISIBLE]);
    }
}

bool
rut_text_get_cursor_visible(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return text->cursor_visible;
}

void
rut_text_set_cursor_color(rut_object_t *obj, const cg_color_t *color)
{
    rut_text_t *text = obj;

    if (color) {
        text->cursor_color = *color;
        text->cursor_color_set = true;
    } else
        text->cursor_color_set = false;

    rut_shell_queue_redraw(text->shell);

    rut_property_dirty(&text->shell->property_ctx,
                       &text->properties[RUT_TEXT_PROP_CURSOR_COLOR]);
    rut_property_dirty(&text->shell->property_ctx,
                       &text->properties[RUT_TEXT_PROP_CURSOR_COLOR_SET]);
}

void
rut_text_set_cursor_color_u32(rut_text_t *text, uint32_t u32)
{
    cg_color_t color;

    rut_color_init_from_uint32(&color, u32);
    rut_text_set_cursor_color(text, &color);
}

const cg_color_t *
rut_text_get_cursor_color(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return &text->cursor_color;
}

bool
rut_text_get_cursor_color_set(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return !!text->cursor_color_set;
}

void
rut_text_set_selection(rut_text_t *text, int start_pos, int end_pos)
{
    unsigned int n_chars;

    n_chars = rut_text_buffer_get_length(get_buffer(text));
    if (end_pos < 0)
        end_pos = n_chars;

    start_pos = MIN(n_chars, start_pos);
    end_pos = MIN(n_chars, end_pos);

    rut_text_set_positions(text, start_pos, end_pos);
}

char *
rut_text_get_selection(rut_text_t *text)
{
    char *str;
    int len;
    int start_index, end_index;
    int start_offset, end_offset;
    const char *text_str;

    start_index = text->position;
    end_index = text->selection_bound;

    if (end_index == start_index)
        return c_strdup("");

    if ((end_index != -1 && end_index < start_index) || start_index == -1) {
        int temp = start_index;
        start_index = end_index;
        end_index = temp;
    }

    text_str = rut_text_buffer_get_text(get_buffer(text));
    start_offset = offset_to_bytes(text_str, start_index);
    end_offset = offset_to_bytes(text_str, end_index);
    len = end_offset - start_offset;

    str = c_malloc(len + 1);
    g_utf8_strncpy(str, text_str + start_offset, end_index - start_index);

    return str;
}

void
rut_text_set_selection_bound(rut_object_t *obj, int selection_bound)
{
    rut_text_t *text = obj;

    if (text->selection_bound != selection_bound) {
        int len = rut_text_buffer_get_length(get_buffer(text));
        ;

        if (selection_bound < 0 || selection_bound >= len)
            text->selection_bound = -1;
        else
            text->selection_bound = selection_bound;

        rut_shell_queue_redraw(text->shell);

        rut_property_dirty(&text->shell->property_ctx,
                           &text->properties[RUT_TEXT_PROP_SELECTION_BOUND]);
    }
}

int
rut_text_get_selection_bound(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return text->selection_bound;
}

void
rut_text_set_selection_color(rut_object_t *obj, const cg_color_t *color)
{
    rut_text_t *text = obj;

    if (color) {
        text->selection_color = *color;
        text->selection_color_set = true;
    } else
        text->selection_color_set = false;

    rut_shell_queue_redraw(text->shell);

    rut_property_dirty(&text->shell->property_ctx,
                       &text->properties[RUT_TEXT_PROP_SELECTION_COLOR]);
    rut_property_dirty(&text->shell->property_ctx,
                       &text->properties[RUT_TEXT_PROP_SELECTION_COLOR_SET]);
}

void
rut_text_set_selection_color_u32(rut_text_t *text, uint32_t u32)
{
    cg_color_t color;

    rut_color_init_from_uint32(&color, u32);
    rut_text_set_selection_color(text, &color);
}

const cg_color_t *
rut_text_get_selection_color(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return &text->selection_color;
}

bool
rut_text_get_selection_color_set(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return !!text->selection_color_set;
}

void
rut_text_set_selected_text_color(rut_object_t *obj,
                                 const cg_color_t *color)
{
    rut_text_t *text = obj;

    if (color) {
        text->selected_text_color = *color;
        text->selected_text_color_set = true;
    } else
        text->selected_text_color_set = false;

    rut_shell_queue_redraw(text->shell);

    rut_property_dirty(&text->shell->property_ctx,
                       &text->properties[RUT_TEXT_PROP_SELECTED_TEXT_COLOR]);
    rut_property_dirty(
        &text->shell->property_ctx,
        &text->properties[RUT_TEXT_PROP_SELECTED_TEXT_COLOR_SET]);
}

void
rut_text_set_selected_text_color_u32(rut_text_t *text, uint32_t u32)
{
    cg_color_t color;

    rut_color_init_from_uint32(&color, u32);
    rut_text_set_selected_text_color(text, &color);
}

const cg_color_t *
rut_text_get_selected_text_color(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return &text->selected_text_color;
}

bool
rut_text_get_selected_text_color_set(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return !!text->selected_text_color_set;
}

void
rut_text_set_font_description(rut_text_t *text,
                              PangoFontDescription *font_desc)
{
    PangoFontDescription *copy;

    copy = pango_font_description_copy(font_desc);
    rut_text_set_font_description_internal(text, copy);
}

PangoFontDescription *
rut_text_get_font_description(rut_text_t *text)
{
    return text->font_desc;
}

const char *
rut_text_get_font_name(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return text->font_name;
}

void
rut_text_set_font_name(rut_object_t *obj, const char *font_name)
{
    rut_text_t *text = obj;
    PangoFontDescription *desc;
    bool is_default_font;

    /* get the default font name from the backend */
    if (font_name == NULL || font_name[0] == '\0') {
        char *default_font_name = NULL;

        default_font_name = rut_settings_get_font_name(text->shell->settings);

        if (default_font_name != NULL)
            font_name = default_font_name;
        else {
            /* last fallback */
            default_font_name = c_strdup("Sans 12");
        }

        is_default_font = true;
    } else
        is_default_font = false;

    if (c_strcmp0(text->font_name, font_name) == 0)
        goto out;

    desc = pango_font_description_from_string(font_name);
    if (!desc) {
        c_warning("Attempting to create a PangoFontDescription for "
                  "font name '%s', but failed.",
                  font_name);
        goto out;
    }

    /* this will set the font_name field as well */
    rut_text_set_font_description_internal(text, desc);
    text->is_default_font = is_default_font;

    rut_property_dirty(&text->shell->property_ctx,
                       &text->properties[RUT_TEXT_PROP_FONT_NAME]);

out:
    if (is_default_font)
        c_free((char *)font_name);
}

const char *
rut_text_get_text(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return rut_text_buffer_get_text(get_buffer(text));
}

static inline void
rut_text_set_use_markup_internal(rut_text_t *text,
                                 bool use_markup)
{
    if (text->use_markup != use_markup) {
        text->use_markup = use_markup;

        /* reset the attributes lists so that they can be
         * re-generated
         */
        if (text->effective_attrs != NULL) {
            pango_attr_list_unref(text->effective_attrs);
            text->effective_attrs = NULL;
        }

        if (text->markup_attrs) {
            pango_attr_list_unref(text->markup_attrs);
            text->markup_attrs = NULL;
        }

        rut_property_dirty(&text->shell->property_ctx,
                           &text->properties[RUT_TEXT_PROP_USE_MARKUP]);
    }
}

void
rut_text_set_text(rut_object_t *obj, const char *text_str)
{
    rut_text_t *text = obj;

    if (!text_str)
        text_str = "";

    /* if the text is editable (i.e. there is not markup flag to reset) then
     * changing the contents will result in selection and cursor changes that
     * we should avoid
     */
    if (text->editable) {
        if (strcmp(rut_text_buffer_get_text(get_buffer(text)), text_str) == 0)
            return;
    }

    rut_text_set_use_markup_internal(text, false);
    rut_text_buffer_set_text(get_buffer(text), text_str);
}

const char *
rut_text_get_hint_text(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return text->hint_text ? text->hint_text : "";
}

void
rut_text_set_hint_text(rut_object_t *obj, const char *hint_str)
{
    rut_text_t *text = obj;

    c_free(text->hint_text);
    text->hint_text = c_strdup(hint_str);

    if (!text->has_focus &&
        (text->buffer == NULL || rut_text_buffer_get_length(text->buffer) == 0))
        rut_shell_queue_redraw(text->shell);

    rut_property_dirty(&text->shell->property_ctx,
                       &text->properties[RUT_TEXT_PROP_HINT_TEXT]);
}

void
rut_text_set_markup(rut_text_t *text, const char *markup)
{
    rut_text_set_use_markup_internal(text, true);
    if (markup != NULL && *markup != '\0')
        rut_text_set_markup_internal(text, markup);
    else
        rut_text_buffer_set_text(get_buffer(text), "");
}

PangoLayout *
rut_text_get_layout(rut_text_t *text)
{
    float width, height;

    if (text->editable && text->single_line_mode)
        return rut_text_create_layout(text, -1, -1);

    rut_sizable_get_size(text, &width, &height);

    return rut_text_create_layout(text, width, height);
}

void
rut_text_set_color(rut_object_t *obj, const cg_color_t *color)
{
    rut_text_t *text = obj;

    c_return_if_fail(color != NULL);

    text->text_color = *color;

    rut_shell_queue_redraw(text->shell);

    rut_property_dirty(&text->shell->property_ctx,
                       &text->properties[RUT_TEXT_PROP_COLOR]);
}

void
rut_text_set_color_u32(rut_text_t *text, uint32_t u32)
{
    cg_color_t color;

    rut_color_init_from_uint32(&color, u32);
    rut_text_set_color(text, &color);
}

const cg_color_t *
rut_text_get_color(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return &text->text_color;
}

void
rut_text_set_ellipsize(rut_text_t *text, PangoEllipsizeMode mode)
{
    c_return_if_fail(mode >= PANGO_ELLIPSIZE_NONE &&
                     mode <= PANGO_ELLIPSIZE_END);

    if ((PangoEllipsizeMode)text->ellipsize != mode) {
        text->ellipsize = mode;

        rut_text_dirty_cache(text);

        rut_text_notify_preferred_size_changed(text);

        rut_property_dirty(&text->shell->property_ctx,
                           &text->properties[RUT_TEXT_PROP_ELLIPSIZE]);
    }
}

PangoEllipsizeMode
rut_text_get_ellipsize(rut_text_t *text)
{
    return text->ellipsize;
}

bool
rut_text_get_line_wrap(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return text->wrap;
}

void
rut_text_set_line_wrap(rut_object_t *obj, bool line_wrap)
{
    rut_text_t *text = obj;

    if (text->wrap != line_wrap) {
        text->wrap = line_wrap;

        rut_text_dirty_cache(text);

        rut_text_notify_preferred_size_changed(text);

        rut_property_dirty(&text->shell->property_ctx,
                           &text->properties[RUT_TEXT_PROP_LINE_WRAP]);
    }
}

void
rut_text_set_line_wrap_mode(rut_text_t *text, PangoWrapMode wrap_mode)
{
    if (text->wrap_mode != wrap_mode) {
        text->wrap_mode = wrap_mode;

        rut_text_dirty_cache(text);

        rut_text_notify_preferred_size_changed(text);

        rut_property_dirty(&text->shell->property_ctx,
                           &text->properties[RUT_TEXT_PROP_LINE_WRAP_MODE]);
    }
}

PangoWrapMode
rut_text_get_line_wrap_mode(rut_text_t *text)
{
    return text->wrap_mode;
}

void
rut_text_set_attributes(rut_text_t *text, PangoAttrList *attrs)
{
    if (attrs)
        pango_attr_list_ref(attrs);

    if (text->attrs)
        pango_attr_list_unref(text->attrs);

    text->attrs = attrs;

    /* Clear the effective attributes so they will be regenerated when a
       layout is created */
    if (text->effective_attrs) {
        pango_attr_list_unref(text->effective_attrs);
        text->effective_attrs = NULL;
    }

    rut_text_dirty_cache(text);

    rut_property_dirty(&text->shell->property_ctx,
                       &text->properties[RUT_TEXT_PROP_ATTRIBUTES]);

    rut_text_notify_preferred_size_changed(text);
}

PangoAttrList *
rut_text_get_attributes(rut_text_t *text)
{
    return text->attrs;
}

void
rut_text_set_line_alignment(rut_text_t *text, PangoAlignment alignment)
{
    if (text->alignment != alignment) {
        text->alignment = alignment;

        rut_text_dirty_cache(text);

        rut_text_notify_preferred_size_changed(text);

        rut_property_dirty(&text->shell->property_ctx,
                           &text->properties[RUT_TEXT_PROP_LINE_ALIGNMENT]);
    }
}

PangoAlignment
rut_text_get_line_alignment(rut_text_t *text)
{
    return text->alignment;
}

void
rut_text_set_use_markup(rut_object_t *obj, bool setting)
{
    rut_text_t *text = obj;
    const char *text_str;

    text_str = rut_text_buffer_get_text(get_buffer(text));

    rut_text_set_use_markup_internal(text, setting);

    if (setting)
        rut_text_set_markup_internal(text, text_str);

    rut_text_dirty_cache(text);

    rut_text_notify_preferred_size_changed(text);
}

bool
rut_text_get_use_markup(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return text->use_markup;
}

void
rut_text_set_justify(rut_object_t *obj, bool justify)
{
    rut_text_t *text = obj;

    if (text->justify != justify) {
        text->justify = justify;

        rut_text_dirty_cache(text);

        rut_text_notify_preferred_size_changed(text);

        rut_property_dirty(&text->shell->property_ctx,
                           &text->properties[RUT_TEXT_PROP_JUSTIFY]);
    }
}

bool
rut_text_get_justify(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return text->justify;
}

int
rut_text_get_cursor_position(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return text->position;
}

void
rut_text_set_cursor_position(rut_object_t *obj, int position)
{
    rut_text_t *text = obj;
    int len;

    if (text->position == position)
        return;

    len = rut_text_buffer_get_length(get_buffer(text));

    if (position < 0 || position >= len)
        text->position = -1;
    else
        text->position = position;

    /* Forget the target x position so that it will be recalculated next
       time the cursor is moved up or down */
    text->x_pos = -1;

    rut_shell_queue_redraw(text->shell);

    rut_property_dirty(&text->shell->property_ctx,
                       &text->properties[RUT_TEXT_PROP_POSITION]);
}

void
rut_text_set_cursor_size(rut_object_t *obj, int size)
{
    rut_text_t *text = obj;

    if (text->cursor_size != size) {
        if (size < 0)
            size = DEFAULT_CURSOR_SIZE;

        text->cursor_size = size;

        rut_shell_queue_redraw(text->shell);

        rut_property_dirty(&text->shell->property_ctx,
                           &text->properties[RUT_TEXT_PROP_CURSOR_SIZE]);
    }
}

int
rut_text_get_cursor_size(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return text->cursor_size;
}

void
rut_text_set_password_char(rut_object_t *obj, uint32_t wc)
{
    rut_text_t *text = obj;

    if (text->password_char != wc) {
        text->password_char = wc;

        rut_text_dirty_cache(text);

        rut_text_notify_preferred_size_changed(text);

        rut_property_dirty(&text->shell->property_ctx,
                           &text->properties[RUT_TEXT_PROP_PASSWORD_CHAR]);
    }
}

uint32_t
rut_text_get_password_char(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return text->password_char;
}

void
rut_text_set_max_length(rut_object_t *obj, int max)
{
    rut_text_t *text = obj;

    rut_text_buffer_set_max_length(get_buffer(text), max);
}

int
rut_text_get_max_length(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return rut_text_buffer_get_max_length(get_buffer(text));
}

void
rut_text_insert_unichar(rut_text_t *text, uint32_t wc)
{
    c_string_t *new;

    new = c_string_new("");
    c_string_append_unichar(new, wc);

    rut_text_buffer_insert_text(get_buffer(text), text->position, new->str, 1);

    c_string_free(new, true);
}

void
rut_text_insert_text(rut_text_t *text, const char *text_str, int position)
{
    c_return_if_fail(text_str != NULL);

    rut_text_buffer_insert_text(
        get_buffer(text), position, text_str, g_utf8_strlen(text_str, -1));
}

void
rut_text_delete_text(rut_text_t *text, int start_pos, int end_pos)
{
    rut_text_buffer_delete_text(
        get_buffer(text), start_pos, end_pos - start_pos);
}

void
rut_text_delete_chars(rut_text_t *text, unsigned int n_chars)
{
    rut_text_buffer_delete_text(get_buffer(text), text->position, n_chars);

    if (text->position > 0)
        rut_text_set_cursor_position(text, text->position - n_chars);
}

char *
rut_text_get_chars(rut_text_t *text, int start_pos, int end_pos)
{
    int start_index, end_index;
    unsigned int n_chars;
    const char *text_str;

    n_chars = rut_text_buffer_get_length(get_buffer(text));
    text_str = rut_text_buffer_get_text(get_buffer(text));

    if (end_pos < 0)
        end_pos = n_chars;

    start_pos = MIN(n_chars, start_pos);
    end_pos = MIN(n_chars, end_pos);

    start_index = g_utf8_offset_to_pointer(text_str, start_pos) - text_str;
    end_index = g_utf8_offset_to_pointer(text_str, end_pos) - text_str;

    return g_strndup(text_str + start_index, end_index - start_index);
}

void
rut_text_set_single_line_mode(rut_object_t *obj, bool single_line)
{
    rut_text_t *text = obj;

    if (text->single_line_mode != single_line) {
        text->single_line_mode = single_line;

        if (text->single_line_mode) {
            text->activatable = true;

            rut_property_dirty(&text->shell->property_ctx,
                               &text->properties[RUT_TEXT_PROP_ACTIVATABLE]);
        }

        rut_text_dirty_cache(text);
        rut_text_notify_preferred_size_changed(text);

        rut_property_dirty(&text->shell->property_ctx,
                           &text->properties[RUT_TEXT_PROP_SINGLE_LINE_MODE]);
    }
}

bool
rut_text_get_single_line_mode(rut_object_t *obj)
{
    rut_text_t *text = obj;

    return text->single_line_mode;
}

void
rut_text_set_preedit_string(rut_text_t *text,
                            const char *preedit_str,
                            PangoAttrList *preedit_attrs,
                            unsigned int cursor_pos)
{
    c_free(text->preedit_str);
    text->preedit_str = NULL;

    if (text->preedit_attrs != NULL) {
        pango_attr_list_unref(text->preedit_attrs);
        text->preedit_attrs = NULL;
    }

    text->preedit_n_chars = 0;
    text->preedit_cursor_pos = 0;

    if (preedit_str == NULL || *preedit_str == '\0')
        text->preedit_set = false;
    else {
        text->preedit_str = c_strdup(preedit_str);

        if (text->preedit_str != NULL)
            text->preedit_n_chars = g_utf8_strlen(text->preedit_str, -1);
        else
            text->preedit_n_chars = 0;

        if (preedit_attrs != NULL)
            text->preedit_attrs = pango_attr_list_ref(preedit_attrs);

        text->preedit_cursor_pos = CLAMP(cursor_pos, 0, text->preedit_n_chars);

        text->preedit_set = true;
    }

    rut_text_dirty_cache(text);
    rut_text_notify_preferred_size_changed(text);
}

void
rut_text_get_layout_offsets(rut_text_t *text, int *x, int *y)
{
    if (x != NULL)
        *x = text->text_x;

    if (y != NULL)
        *y = text->text_y;
}

rut_closure_t *
rut_text_add_text_inserted_callback(rut_text_t *text,
                                    rut_text_inserted_callback_t callback,
                                    void *user_data,
                                    rut_closure_destroy_callback_t destroy_cb)
{
    c_return_val_if_fail(callback != NULL, NULL);
    return rut_closure_list_add_FIXME(
        &text->text_inserted_cb_list, callback, user_data, destroy_cb);
}

rut_closure_t *
rut_text_add_text_deleted_callback(rut_text_t *text,
                                   rut_text_deleted_callback_t callback,
                                   void *user_data,
                                   rut_closure_destroy_callback_t destroy_cb)
{
    c_return_val_if_fail(callback != NULL, NULL);
    return rut_closure_list_add_FIXME(
        &text->text_deleted_cb_list, callback, user_data, destroy_cb);
}

rut_closure_t *
rut_text_add_text_changed_callback(rut_text_t *text,
                                   rut_text_changed_callback_t callback,
                                   void *user_data,
                                   rut_closure_destroy_callback_t destroy_cb)
{
    c_return_val_if_fail(callback != NULL, NULL);
    return rut_closure_list_add_FIXME(
        &text->text_changed_cb_list, callback, user_data, destroy_cb);
}

rut_closure_t *
rut_text_add_activate_callback(rut_text_t *text,
                               rut_text_activate_callback_t callback,
                               void *user_data,
                               rut_closure_destroy_callback_t destroy_cb)
{
    c_return_val_if_fail(callback != NULL, NULL);
    return rut_closure_list_add_FIXME(
        &text->activate_cb_list, callback, user_data, destroy_cb);
}

rut_closure_t *
rut_text_add_cursor_event_callback(rut_text_t *text,
                                   rut_text_cursor_event_callback_t callback,
                                   void *user_data,
                                   rut_closure_destroy_callback_t destroy_cb)
{
    c_return_val_if_fail(callback != NULL, NULL);
    return rut_closure_list_add_FIXME(
        &text->cursor_event_cb_list, callback, user_data, destroy_cb);
}

void
rut_text_set_direction(rut_text_t *text, rut_text_direction_t direction)
{
    if (text->direction == direction)
        return;

    text->direction = direction;

    rut_text_dirty_cache(text);
}

rut_text_direction_t
rut_text_get_direction(rut_text_t *text)
{
    return text->direction;
}

rut_mesh_t *
rut_text_get_pick_mesh(rut_object_t *object)
{
    rut_text_t *text = object;
    return text->pick_mesh;
}
