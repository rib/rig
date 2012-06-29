/*
 * Rig.
 *
 * Copyright (C) 2008,2012  Intel Corporation.
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

#ifndef __RIG_TEXT_H__
#define __RIG_TEXT_H__

#include "rig-types.h"
#include "rig-context.h"
#include "rig-interfaces.h"
#include "rig-text-buffer.h"

#include <pango/pango.h>

G_BEGIN_DECLS

/**
 * SECTION:rig-text
 * @short_description: An actor for displaying and editing text
 *
 * #RigText is an actor that displays custom text using Pango
 * as the text rendering engine.
 *
 * #RigText also allows inline editing of the text if the
 * actor is set editable using rig_text_set_editable().
 *
 * Selection using keyboard or pointers can be enabled using
 * rig_text_set_selectable().
 */


typedef struct _RigText RigText;
#define RIG_TEXT(X) ((RigText *)X)
extern RigType rig_text_type;

RigContext *
rig_text_get_context (RigText *text);

typedef enum _RigTextDirection
{
  RIG_TEXT_DIRECTION_DEFAULT,
  RIG_TEXT_DIRECTION_LEFT_TO_RIGHT,
  RIG_TEXT_DIRECTION_RIGHT_TO_LEFT
} RigTextDirection;

RigTextDirection
rig_text_get_direction (RigText *text);

void
rig_text_set_direction (RigText *text,
                        RigTextDirection direction);

void
_rig_text_init_type (void);

void
rig_text_set_size (RigText *text,
                   float width,
                   float height);

void
rig_text_get_size (RigText *text,
                   float *width,
                   float *height);
#if 0
void
rig_text_allocate (RigText *text,
                   const RigBox *box);

void
rig_text_get_allocation_box (RigText *text,
                             RigBox *box);
#endif

void
rig_text_get_preferred_width (RigText *text,
                              float for_height,
                              float *min_width_p,
                              float *natural_width_p);

void
rig_text_get_preferred_height (RigText *text,
                               float for_width,
                               float *min_height_p,
                               float *natural_height_p);

CoglBool
rig_text_has_overlaps (RigText *text);

typedef void (* RigTextChangedCallback) (RigText *text,
                                         void *user_data);

/**
 * RigText::text-changed:
 * @text: the #RigText that emitted the signal
 *
 * The ::text-changed signal is emitted after @actor's text changes
 */
void
rig_text_set_text_changed_callback (RigText *text,
                                    RigTextChangedCallback callback,
                                    void *user_data);

typedef void (* RigTextActivateCallback) (RigText *text,
                                          void *user_data);

/**
 * RigText::activate
 * @text: the #RigText that emitted the signal
 *
 * The ::activate signal is emitted each time the actor is 'activated'
 * by the user, normally by pressing the 'Enter' key. The signal is
 * emitted only if #RigText:activatable is set to %TRUE.
 */
void
rig_text_set_activate_callback (RigText *text,
                                RigTextActivateCallback callback,
                                void *user_data);

typedef void (* RigTextCursorEventCallback) (RigText *text,
                                             const RigRectangleInt *rectangle,
                                             void *user_data);
/**
 * RigText::cursor-event:
 * @text: the #RigText that emitted the signal
 * @rectangle: the coordinates of the cursor
 *
 * The ::cursor-event signal is emitted whenever the cursor position
 * changes inside a #RigText actor. Inside @rectangle it is stored
 * the current position and size of the cursor, relative to the actor
 * ittext.
 */
void
rig_text_set_cursor_event_callback (RigText *text,
                                    RigTextCursorEventCallback callback,
                                    void *user_data);

typedef void (* RigTextInsertedCallback) (RigText *text,
                                          const char *text_str,
                                          int new_text_length,
                                          int *position,
                                          void *user_data);

/**
 * RigText::insert-text:
 * @text: the #RigText that emitted the signal
 * @new_text: the new text to insert
 * @new_text_length: the length of the new text, in bytes, or -1 if
 *     new_text is nul-terminated
 * @position: the position, in characters, at which to insert the
 *     new text. this is an in-out parameter.  After the signal
 *     emission is finished, it should point after the newly
 *     inserted text.
 *
 * This signal is emitted when text is inserted into the actor by
 * the user. It is emitted before @text text changes.
 */
void
rig_text_set_text_inserted_callback (RigText *text,
                                     RigTextInsertedCallback callback,
                                     void *user_data);

typedef void (* RigTextDeletedCallback) (RigText *text,
                                         int start_pos,
                                         int end_pos,
                                         void *user_data);

/**
 * RigText::delete-text:
 * @text: the #RigText that emitted the signal
 * @start_pos: the starting position
 * @end_pos: the end position
 *
 * This signal is emitted when text is deleted from the actor by
 * the user. It is emitted before @text text changes.
 */
void
rig_text_set_text_deleted_callback (RigText *text,
                                    RigTextDeletedCallback callback,
                                    void *user_data);

/**
 * rig_text_new:
 *
 * Creates a new #RigText actor. This actor can be used to
 * display and edit text.
 *
 * Return value: the newly created #RigText actor
 */
RigText *
rig_text_new (RigContext *ctx);

/**
 * rig_text_new_full:
 * @font_name: a string with a font description
 * @text: the contents of the actor
 * @buffer: The #RigTextBuffer to use for the text
 *
 * Creates a new #RigText actor, using @font_name as the font
 * description; @text will be used to set the contents of the actor;
 * and @color will be used as the color to render @text.
 *
 * This function is equivalent to calling rig_text_new(),
 * rig_text_set_font_name(), rig_text_set_text() and
 * rig_text_set_color().
 *
 * Return value: the newly created #RigText actor
 */
RigText *
rig_text_new_full (RigContext *ctx,
                   const char *font_name,
                   const char *text,
                   RigTextBuffer *buffer);

/**
 * rig_text_new_with_text:
 * @font_name: (allow-none): a string with a font description
 * @text: the contents of the actor
 *
 * Creates a new #RigText actor, using @font_name as the font
 * description; @text will be used to set the contents of the actor.
 *
 * This function is equivalent to calling rig_text_new(),
 * rig_text_set_font_name(), and rig_text_set_text().
 *
 * Return value: the newly created #RigText actor
 */
RigText *
rig_text_new_with_text (RigContext *ctx,
                        const char *font_name,
                        const char *text);

/**
 * rig_text_new_with_buffer:
 * @buffer: The buffer to use for the new #RigText.
 *
 * Creates a new entry with the specified text buffer.
 *
 * Return value: a new #RigText
 */
RigText *
rig_text_new_with_buffer (RigContext *ctx,
                          RigTextBuffer *buffer);

/**
 * rig_text_get_buffer:
 * @text: a #RigText
 *
 * Get the #RigTextBuffer object which holds the text for
 * this widget.
 *
 * Returns: (transfer none): A #RigTextBuffer object.
 */
RigTextBuffer *
rig_text_get_buffer (RigText *text);

/**
 * rig_text_set_buffer:
 * @text: a #RigText
 * @buffer: a #RigTextBuffer
 *
 * Set the #RigTextBuffer object which holds the text for
 * this widget.
 */
void
rig_text_set_buffer (RigText *text,
                     RigTextBuffer *buffer);

/**
 * rig_text_get_text:
 * @text: a #RigText
 *
 * Retrieves a pointer to the current contents of a #RigText
 * actor.
 *
 * If you need a copy of the contents for manipulating, either
 * use g_strdup() on the returned string, or use:
 *
 * |[
 *    copy = rig_text_get_chars (text, 0, -1);
 * ]|
 *
 * Which will return a newly allocated string.
 *
 * If the #RigText actor is empty, this function will return
 * an empty string, and not %NULL.
 *
 * Return value: (transfer none): the contents of the actor. The returned
 *   string is owned by the #RigText actor and should never be modified
 *   or freed
 */
const char *
rig_text_get_text (RigText *text);

/**
 * rig_text_set_text:
 * @text: a #RigText
 * @text_str: (allow-none): the text to set. Passing %NULL is the same
 *   as passing "" (the empty string)
 *
 * Sets the contents of a #RigText actor.
 *
 * If the #RigText:use-markup property was set to %TRUE it
 * will be reset to %FALSE as a side effect. If you want to
 * maintain the #RigText:use-markup you should use the
 * rig_text_set_markup() function instead
 */
void
rig_text_set_text (RigText *text,
                   const char *text_str);

/**
 * rig_text_set_markup:
 * @text: a #RigText
 * @markup: (allow-none): a string containing Pango markup.
 *   Passing %NULL is the same as passing "" (the empty string)
 *
 * Sets @markup as the contents of a #RigText.
 *
 * This is a convenience function for setting a string containing
 * Pango markup, and it is logically equivalent to:
 *
 * |[
 *   /&ast; the order is important &ast;/
 *   rig_text_set_text (RIG_TEXT (actor), markup);
 *   rig_text_set_use_markup (RIG_TEXT (actor), TRUE);
 * ]|
 */
void
rig_text_set_markup (RigText *text,
                     const char  *markup);

/**
 * rig_text_set_color:
 * @text: a #RigText
 * @color: a #RigColor
 *
 * Sets the color of the contents of a #RigText actor.
 *
 * The overall opacity of the #RigText actor will be the
 * result of the alpha value of @color and the composited
 * opacity of the actor ittext on the scenegraph, as returned
 * by rig_actor_get_paint_opacity().
 */
void
rig_text_set_color (RigText *text,
                    const RigColor *color);

void
rig_text_set_color_u32 (RigText *text,
                        uint32_t u32);

/**
 * rig_text_get_color:
 * @text: a #RigText
 * @color: (out caller-allocates): return location for a #RigColor
 *
 * Retrieves the text color as set by rig_text_set_color().
 */
void
rig_text_get_color (RigText *text,
                    RigColor *color);

/**
 * rig_text_set_font_name:
 * @text: a #RigText
 * @font_name: (allow-none): a font name, or %NULL to set the default font name
 *
 * Sets the font used by a #RigText. The @font_name string
 * must either be %NULL, which means that the font name from the
 * default #RigBackend will be used; or be something that can
 * be parsed by the pango_font_description_from_string() function,
 * like:
 *
 * |[
 *   rig_text_set_font_name (text, "Sans 10pt");
 *   rig_text_set_font_name (text, "Serif 16px");
 *   rig_text_set_font_name (text, "Helvetica 10");
 * ]|
 */
void
rig_text_set_font_name (RigText *text,
                        const char *font_name);

/**
 * rig_text_get_font_name:
 * @text: a #RigText
 *
 * Retrieves the font name as set by rig_text_set_font_name().
 *
 * Return value: a string containing the font name. The returned
 *   string is owned by the #RigText actor and should not be
 *   modified or freed
 */
const char *
rig_text_get_font_name (RigText *text);

/**
 * rig_text_set_font_description:
 * @text: a #RigText
 * @font_desc: a #PangoFontDescription
 *
 * Sets @font_desc as the font description for a #RigText
 *
 * The #PangoFontDescription is copied by the #RigText actor
 * so you can safely call pango_font_description_free() on it after
 * calling this function.
 */
void
rig_text_set_font_description (RigText *text,
                               PangoFontDescription *font_desc);

/**
 * rig_text_get_font_description:
 * @text: a #RigText
 *
 * Retrieves the #PangoFontDescription used by @text
 *
 * Return value: a #PangoFontDescription. The returned value is owned
 *   by the #RigText actor and it should not be modified or freed
 */
PangoFontDescription *
rig_text_get_font_description (RigText *text);

/**
 * rig_text_set_ellipsize:
 * @text: a #RigText
 * @mode: a #PangoEllipsizeMode
 *
 * Sets the mode used to ellipsize (add an ellipsis: "...") to the
 * text if there is not enough space to render the entire contents
 * of a #RigText actor
 */
void
rig_text_set_ellipsize (RigText *text,
                        PangoEllipsizeMode mode);

/**
 * rig_text_get_ellipsize:
 * @text: a #RigText
 *
 * Returns the ellipsizing position of a #RigText actor, as
 * set by rig_text_set_ellipsize().
 *
 * Return value: #PangoEllipsizeMode
 */
PangoEllipsizeMode
rig_text_get_ellipsize (RigText *text);

/**
 * rig_text_set_line_wrap:
 * @text: a #RigText
 * @line_wrap: whether the contents should wrap
 *
 * Sets whether the contents of a #RigText actor should wrap,
 * if they don't fit the size assigned to the actor.
 */
void
rig_text_set_line_wrap (RigText *text,
                        CoglBool line_wrap);

/**
 * rig_text_get_line_wrap:
 * @text: a #RigText
 *
 * Retrieves the value set using rig_text_set_line_wrap().
 *
 * Return value: %TRUE if the #RigText actor should wrap
 *   its contents
 */
CoglBool
rig_text_get_line_wrap (RigText *text);

/**
 * rig_text_set_line_wrap_mode:
 * @text: a #RigText
 * @wrap_mode: the line wrapping mode
 *
 * If line wrapping is enabled (see rig_text_set_line_wrap()) this
 * function controls how the line wrapping is performed. The default is
 * %PANGO_WRAP_WORD which means wrap on word boundaries.
 */
void
rig_text_set_line_wrap_mode (RigText *text,
                             PangoWrapMode wrap_mode);

/**
 * rig_text_get_line_wrap_mode:
 * @text: a #RigText
 *
 * Retrieves the line wrap mode used by the #RigText actor.
 *
 * See rig_text_set_line_wrap_mode ().
 *
 * Return value: the wrap mode used by the #RigText
 */
PangoWrapMode
rig_text_get_line_wrap_mode (RigText *text);

/**
 * rig_text_get_layout:
 * @text: a #RigText
 *
 * Retrieves the current #PangoLayout used by a #RigText actor.
 *
 * Return value: (transfer none): a #PangoLayout. The returned object is owned by
 *   the #RigText actor and should not be modified or freed
 */
PangoLayout *
rig_text_get_layout (RigText *text);

/**
 * rig_text_set_attributes:
 * @text: a #RigText
 * @attrs: a #PangoAttrList or %NULL to unset the attributes
 *
 * Sets the attributes list that are going to be applied to the
 * #RigText contents.
 *
 * The #RigText actor will take a reference on the #PangoAttrList
 * passed to this function.
 */
void
rig_text_set_attributes (RigText *text,
                         PangoAttrList *attrs);

/**
 * rig_text_get_attributes:
 * @text: a #RigText
 *
 * Gets the attribute list that was set on the #RigText actor
 * rig_text_set_attributes(), if any.
 *
 * Return value: (transfer none): the attribute list, or %NULL if none was set. The
 *  returned value is owned by the #RigText and should not be unreferenced.
 */
PangoAttrList *
rig_text_get_attributes (RigText *text);

/**
 * rig_text_set_use_markup:
 * @text: a #RigText
 * @setting: %TRUE if the text should be parsed for markup.
 *
 * Sets whether the contents of the #RigText actor contains markup
 * in <link linkend="PangoMarkupFormat">Pango's text markup language</link>.
 *
 * Setting #RigText:use-markup on an editable #RigText will
 * not have any effect except hiding the markup.
 *
 * See also #RigText:use-markup.
 */
void
rig_text_set_use_markup (RigText *text,
                         CoglBool setting);

/**
 * rig_text_get_use_markup:
 * @text: a #RigText
 *
 * Retrieves whether the contents of the #RigText actor should be
 * parsed for the Pango text markup.
 *
 * Return value: %TRUE if the contents will be parsed for markup
 */
CoglBool
rig_text_get_use_markup (RigText *text);

/**
 * rig_text_set_line_alignment:
 * @text: a #RigText
 * @alignment: A #PangoAlignment
 *
 * Sets the way that the lines of a wrapped label are aligned with
 * respect to each other. This does not affect the overall alignment
 * of the label within its allocated or specified width.
 *
 * To align a #RigText actor you should add it to a container
 * that supports alignment, or use the anchor point.
 */
void
rig_text_set_line_alignment (RigText *text,
                             PangoAlignment alignment);

/**
 * rig_text_get_line_alignment:
 * @text: a #RigText
 *
 * Retrieves the alignment of a #RigText, as set by
 * rig_text_set_line_alignment().
 *
 * Return value: a #PangoAlignment
 */
PangoAlignment
rig_text_get_line_alignment (RigText *text);

/**
 * rig_text_set_justify:
 * @text: a #RigText
 * @justify: whether the text should be justified
 *
 * Sets whether the text of the #RigText actor should be justified
 * on both margins. This setting is ignored if Rig is compiled
 * against Pango &lt; 1.18.
 */
void
rig_text_set_justify (RigText *text,
                      CoglBool justify);

/**
 * rig_text_get_justify:
 * @text: a #RigText
 *
 * Retrieves whether the #RigText actor should justify its contents
 * on both margins.
 *
 * Return value: %TRUE if the text should be justified
 */
CoglBool
rig_text_get_justify (RigText *text);

/**
 * rig_text_insert_unichar:
 * @text: a #RigText
 * @wc: a Unicode character
 *
 * Inserts @wc at the current cursor position of a
 * #RigText actor.
 */
void
rig_text_insert_unichar (RigText *text,
                         uint32_t wc);

/**
 * rig_text_delete_chars:
 * @text: a #RigText
 * @n_chars: the number of characters to delete
 *
 * Deletes @n_chars inside a #RigText actor, starting from the
 * current cursor position.
 *
 * Somewhat awkwardly, the cursor position is decremented by the same
 * number of characters you've deleted.
 */
void
rig_text_delete_chars (RigText *text,
                       unsigned int n_chars);

/**
 * rig_text_insert_text:
 * @text: a #RigText
 * @text: the text to be inserted
 * @position: the position of the insertion, or -1
 *
 * Inserts @text into a #RigActor at the given position.
 *
 * If @position is a negative number, the text will be appended
 * at the end of the current contents of the #RigText.
 *
 * The position is expressed in characters, not in bytes.
 */
void
rig_text_insert_text (RigText *text,
                      const char *text_str,
                      int position);

/**
 * rig_text_delete_text:
 * @text: a #RigText
 * @start_pos: starting position
 * @end_pos: ending position
 *
 * Deletes the text inside a #RigText actor between @start_pos
 * and @end_pos.
 *
 * The starting and ending positions are expressed in characters,
 * not in bytes.
 */
void
rig_text_delete_text (RigText *text,
                      int start_pos,
                      int end_pos);

/**
 * rig_text_get_chars:
 * @text: a #RigText
 * @start_pos: start of text, in characters
 * @end_pos: end of text, in characters
 *
 * Retrieves the contents of the #RigText actor between
 * @start_pos and @end_pos, but not including @end_pos.
 *
 * The positions are specified in characters, not in bytes.
 *
 * Return value: a newly allocated string with the contents of
 *   the text actor between the specified positions. Use g_free()
 *   to free the resources when done
 */
char *
rig_text_get_chars (RigText *text,
                    int start_pos,
                    int end_pos);

/**
 * rig_text_set_editable:
 * @text: a #RigText
 * @editable: whether the #RigText should be editable
 *
 * Sets whether the #RigText actor should be editable.
 *
 * An editable #RigText with key focus set using
 * rig_actor_grab_key_focus() or rig_stage_set_key_focus()
 * will receive key events and will update its contents accordingly.
 */
void
rig_text_set_editable (RigText *text,
                       CoglBool editable);

/**
 * rig_text_get_editable:
 * @text: a #RigText
 *
 * Retrieves whether a #RigText is editable or not.
 *
 * Return value: %TRUE if the actor is editable
 */
CoglBool
rig_text_get_editable (RigText *text);

/**
 * rig_text_set_activatable:
 * @text: a #RigText
 * @activatable: whether the #RigText actor should be activatable
 *
 * Sets whether a #RigText actor should be activatable.
 *
 * An activatable #RigText actor will emit the #RigText::activate
 * signal whenever the 'Enter' (or 'Return') key is pressed; if it is not
 * activatable, a new line will be appended to the current content.
 *
 * An activatable #RigText must also be set as editable using
 * rig_text_set_editable().
 */
void
rig_text_set_activatable (RigText *text,
                          CoglBool activatable);

/**
 * rig_text_get_activatable:
 * @text: a #RigText
 *
 * Retrieves whether a #RigText is activatable or not.
 *
 * Return value: %TRUE if the actor is activatable
 */
CoglBool
rig_text_get_activatable (RigText *text);

/**
 * rig_text_get_cursor_position:
 * @text: a #RigText
 *
 * Retrieves the cursor position.
 *
 * Return value: the cursor position, in characters
 */
int
rig_text_get_cursor_position (RigText *text);

/**
 * rig_text_set_cursor_position:
 * @text: a #RigText
 * @position: the new cursor position, in characters
 *
 * Sets the cursor of a #RigText actor at @position.
 *
 * The position is expressed in characters, not in bytes.
 */
void
rig_text_set_cursor_position (RigText *text,
                              int position);

/**
 * rig_text_set_cursor_visible:
 * @text: a #RigText
 * @cursor_visible: whether the cursor should be visible
 *
 * Sets whether the cursor of a #RigText actor should be
 * visible or not.
 *
 * The color of the cursor will be the same as the text color
 * unless rig_text_set_cursor_color() has been called.
 *
 * The size of the cursor can be set using rig_text_set_cursor_size().
 *
 * The position of the cursor can be changed programmatically using
 * rig_text_set_cursor_position().
 */
void
rig_text_set_cursor_visible (RigText *text,
                             CoglBool cursor_visible);

/**
 * rig_text_get_cursor_visible:
 * @text: a #RigText
 *
 * Retrieves whether the cursor of a #RigText actor is visible.
 *
 * Return value: %TRUE if the cursor is visible
 */
CoglBool
rig_text_get_cursor_visible (RigText *text);

/**
 * rig_text_set_cursor_color:
 * @text: a #RigText
 * @color: the color of the cursor, or %NULL to unset it
 *
 * Sets the color of the cursor of a #RigText actor.
 *
 * If @color is %NULL, the cursor color will be the same as the
 * text color.
 */
void
rig_text_set_cursor_color (RigText *text,
                           const RigColor *color);

void
rig_text_set_cursor_color_u32 (RigText *text,
                               uint32_t u32);

/**
 * rig_text_get_cursor_color:
 * @text: a #RigText
 * @color: (out): return location for a #RigColor
 *
 * Retrieves the color of the cursor of a #RigText actor.
 */
void
rig_text_get_cursor_color (RigText *text,
                           RigColor *color);

/**
 * rig_text_set_cursor_size:
 * @text: a #RigText
 * @size: the size of the cursor, in pixels, or -1 to use the
 *   default value
 *
 * Sets the size of the cursor of a #RigText. The cursor
 * will only be visible if the #RigText:cursor-visible property
 * is set to %TRUE.
 */
void
rig_text_set_cursor_size (RigText *text,
                          int size);

/**
 * rig_text_get_cursor_size:
 * @text: a #RigText
 *
 * Retrieves the size of the cursor of a #RigText actor.
 *
 * Return value: the size of the cursor, in pixels
 */
unsigned int
rig_text_get_cursor_size (RigText *text);

/**
 * rig_text_set_selectable:
 * @text: a #RigText
 * @selectable: whether the #RigText actor should be selectable
 *
 * Sets whether a #RigText actor should be selectable.
 *
 * A selectable #RigText will allow selecting its contents using
 * the pointer or the keyboard.
 */
void
rig_text_set_selectable (RigText *text,
                         CoglBool selectable);

/**
 * rig_text_get_selectable:
 * @text: a #RigText
 *
 * Retrieves whether a #RigText is selectable or not.
 *
 * Return value: %TRUE if the actor is selectable
 */
CoglBool
rig_text_get_selectable (RigText *text);

/**
 * rig_text_set_selection_bound:
 * @text: a #RigText
 * @selection_bound: the position of the end of the selection, in characters
 *
 * Sets the other end of the selection, starting from the current
 * cursor position.
 *
 * If @selection_bound is -1, the selection unset.
 */
void
rig_text_set_selection_bound (RigText *text,
                              int selection_bound);

/**
 * rig_text_get_selection_bound:
 * @text: a #RigText
 *
 * Retrieves the other end of the selection of a #RigText actor,
 * in characters from the current cursor position.
 *
 * Return value: the position of the other end of the selection
 */
int
rig_text_get_selection_bound (RigText *text);

/**
 * rig_text_set_selection:
 * @text: a #RigText
 * @start_pos: start of the selection, in characters
 * @end_pos: end of the selection, in characters
 *
 * Selects the region of text between @start_pos and @end_pos.
 *
 * This function changes the position of the cursor to match
 * @start_pos and the selection bound to match @end_pos.
 */
void
rig_text_set_selection (RigText *text,
                        int start_pos,
                        int end_pos);

/**
 * rig_text_get_selection:
 * @text: a #RigText
 *
 * Retrieves the currently selected text.
 *
 * Return value: a newly allocated string containing the currently
 *   selected text, or %NULL. Use g_free() to free the returned
 *   string.
 */
char *
rig_text_get_selection (RigText *text);

/**
 * rig_text_set_selection_color:
 * @text: a #RigText
 * @color: the color of the selection, or %NULL to unset it
 *
 * Sets the color of the selection of a #RigText actor.
 *
 * If @color is %NULL, the selection color will be the same as the
 * cursor color, or if no cursor color is set either then it will be
 * the same as the text color.
 */
void
rig_text_set_selection_color (RigText *text,
                              const RigColor *color);

void
rig_text_set_selection_color_u32 (RigText *text,
                                  uint32_t u32);

/**
 * rig_text_get_selection_color:
 * @text: a #RigText
 * @color: (out caller-allocates): return location for a #RigColor
 *
 * Retrieves the color of the selection of a #RigText actor.
 */
void
rig_text_get_selection_color (RigText *text,
                              RigColor *color);

/**
 * rig_text_delete_selection:
 * @text: a #RigText
 *
 * Deletes the currently selected text
 *
 * This function is only useful in subclasses of #RigText
 *
 * Return value: %TRUE if text was deleted or if the text actor
 *   is empty, and %FALSE otherwise
 */
CoglBool
rig_text_delete_selection (RigText *text);

/**
 * rig_text_set_password_char:
 * @text: a #RigText
 * @wc: a Unicode character, or 0 to unset the password character
 *
 * Sets the character to use in place of the actual text in a
 * password text actor.
 *
 * If @wc is 0 the text will be displayed as it is entered in the
 * #RigText actor.
 */
void
rig_text_set_password_char (RigText *text,
                            uint32_t wc);

/**
 * rig_text_get_password_char:
 * @text: a #RigText
 *
 * Retrieves the character to use in place of the actual text
 * as set by rig_text_set_password_char().
 *
 * Return value: a Unicode character or 0 if the password
 *   character is not set
 */
uint32_t
rig_text_get_password_char (RigText *text);

/**
 * rig_text_set_max_length:
 * @text: a #RigText
 * @max: the maximum number of characters allowed in the text actor; 0
 *   to disable or -1 to set the length of the current string
 *
 * Sets the maximum allowed length of the contents of the actor. If the
 * current contents are longer than the given length, then they will be
 * truncated to fit.
 */
void
rig_text_set_max_length (RigText *text,
                         int max);

/**
 * rig_text_get_max_length:
 * @text: a #RigText
 *
 * Gets the maximum length of text that can be set into a text actor.
 *
 * See rig_text_set_max_length().
 *
 * Return value: the maximum number of characters.
 */
int
rig_text_get_max_length (RigText *text);

/**
 * rig_text_set_single_line_mode:
 * @text: a #RigText
 * @single_line: whether to enable single line mode
 *
 * Sets whether a #RigText actor should be in single line mode
 * or not. Only editable #RigText<!-- -->s can be in single line
 * mode.
 *
 * A text actor in single line mode will not wrap text and will clip
 * the visible area to the predefined size. The contents of the
 * text actor will scroll to display the end of the text if its length
 * is bigger than the allocated width.
 *
 * When setting the single line mode the #RigText:activatable
 * property is also set as a side effect. Instead of entering a new
 * line character, the text actor will emit the #RigText::activate
 * signal.
 */
void
rig_text_set_single_line_mode (RigText *text,
                               CoglBool single_line);

/**
 * rig_text_get_single_line_mode:
 * @text: a #RigText
 *
 * Retrieves whether the #RigText actor is in single line mode.
 *
 * Return value: %TRUE if the #RigText actor is in single line mode
 */
CoglBool
rig_text_get_single_line_mode (RigText *text);

/**
 * rig_text_set_selected_text_color:
 * @text: a #RigText
 * @color: the selected text color, or %NULL to unset it
 *
 * Sets the selected text color of a #RigText actor.
 *
 * If @color is %NULL, the selected text color will be the same as the
 * selection color, which then falls back to cursor, and then text color.
 */
void
rig_text_set_selected_text_color (RigText *text,
                                  const RigColor *color);

void
rig_text_set_selected_text_color_u32 (RigText *text,
                                      uint32_t u32);

/**
 * rig_text_get_selected_text_color:
 * @text: a #RigText
 * @color: (out caller-allocates): return location for a #RigColor
 *
 * Retrieves the color of selected text of a #RigText actor.
 */
void
rig_text_get_selected_text_color (RigText *text,
                                  RigColor *color);

/**
 * rig_text_activate:
 * @text: a #RigText
 *
 * Emits the #RigText::activate signal, if @text has been set
 * as activatable using rig_text_set_activatable().
 *
 * This function can be used to emit the ::activate signal inside
 * a #RigActor::captured-event or #RigActor::key-press-event
 * signal handlers before the default signal handler for the
 * #RigText is invoked.
 *
 * Return value: %TRUE if the ::activate signal has been emitted,
 *   and %FALSE otherwise
 */
CoglBool
rig_text_activate (RigText *text);

/**
 * rig_text_coords_to_position:
 * @text: a #RigText
 * @x: the X coordinate, relative to the actor
 * @y: the Y coordinate, relative to the actor
 *
 * Retrieves the position of the character at the given coordinates.
 *
 * Return: the position of the character
 */
int
rig_text_coords_to_position (RigText *text,
                             float x,
                             float y);

/**
 * rig_text_position_to_coords:
 * @text: a #RigText
 * @position: position in characters
 * @x: (out): return location for the X coordinate, or %NULL
 * @y: (out): return location for the Y coordinate, or %NULL
 * @line_height: (out): return location for the line height, or %NULL
 *
 * Retrieves the coordinates of the given @position.
 *
 * Return value: %TRUE if the conversion was successful
 */
CoglBool
rig_text_position_to_coords (RigText *text,
                             int position,
                             float *x,
                             float *y,
                             float *line_height);

/**
 * rig_text_set_preedit_string:
 * @text: a #RigText
 * @preedit_str: (allow-none): the pre-edit string, or %NULL to unset it
 * @preedit_attrs: (allow-none): the pre-edit string attributes
 * @cursor_pos: the cursor position for the pre-edit string
 *
 * Sets, or unsets, the pre-edit string. This function is useful
 * for input methods to display a string (with eventual specific
 * Pango attributes) before it is entered inside the #RigText
 * buffer.
 *
 * The preedit string and attributes are ignored if the #RigText
 * actor is not editable.
 *
 * This function should not be used by applications
 */
void
rig_text_set_preedit_string (RigText *text,
                             const char *preedit_str,
                             PangoAttrList *preedit_attrs,
                             unsigned int cursor_pos);

/**
 * rig_text_get_layout_offsets:
 * @text: a #RigText
 * @x: (out): location to store X offset of layout, or %NULL
 * @y: (out): location to store Y offset of layout, or %NULL
 *
 * Obtains the coordinates where the #RigText will draw the #PangoLayout
 * representing the text.
 */
void
rig_text_get_layout_offsets (RigText *text,
                             int *x,
                             int *y);

G_END_DECLS

#endif /* __RIG_TEXT_H__ */
