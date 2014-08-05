/*
 * Copyright © 2009  Red Hat, Inc.
 * Copyright © 2009  Keith Stribley
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Red Hat Author(s): Behdad Esfahbod
 */

#ifndef _RIG_TEXT_RENDERER_FUNCS_H_
#define _RIG_TEXT_RENDERER_FUNCS_H_

#include <hb.h>

hb_position_t rig_text_engine_get_glyph_h_advance(hb_font_t *font,
                                                  void *font_data,
                                                  hb_codepoint_t glyph,
                                                  void *user_data);

hb_position_t rig_text_engine_get_glyph_v_advance(hb_font_t *font,
                                                  void *font_data,
                                                  hb_codepoint_t glyph,
                                                  void *user_data);

hb_bool_t rig_text_engine_get_glyph_h_origin(hb_font_t *font,
                                             void *font_data,
                                             hb_codepoint_t glyph,
                                             hb_position_t *x,
                                             hb_position_t *y,
                                             void *user_data);

hb_bool_t rig_text_engine_get_glyph_v_origin(hb_font_t *font,
                                             void *font_data,
                                             hb_codepoint_t glyph,
                                             hb_position_t *x,
                                             hb_position_t *y,
                                             void *user_data);

hb_position_t rig_text_engine_get_glyph_h_kerning(hb_font_t *font,
                                                  void *font_data,
                                                  hb_codepoint_t left_glyph,
                                                  hb_codepoint_t right_glyph,
                                                  void *user_data);

hb_position_t rig_text_engine_get_glyph_v_kerning(hb_font_t *font,
                                                  void *font_data,
                                                  hb_codepoint_t top_glyph,
                                                  hb_codepoint_t bottom_glyph,
                                                  void *user_data);

hb_bool_t rig_text_engine_get_glyph_extents(hb_font_t *font,
                                            void *font_data,
                                            hb_codepoint_t glyph,
                                            hb_glyph_extents_t *extents,
                                            void *user_data);

hb_bool_t rig_text_engine_get_glyph_contour_point(hb_font_t *font,
                                                  void *font_data,
                                                  hb_codepoint_t glyph,
                                                  unsigned int point_index,
                                                  hb_position_t *x,
                                                  hb_position_t *y,
                                                  void *user_data);

hb_bool_t rig_text_engine_get_glyph_name(hb_font_t *font,
                                         void *font_data,
                                         hb_codepoint_t glyph,
                                         char *name,
                                         unsigned int size,
                                         void *user_data);

hb_bool_t
rig_text_engine_get_glyph_from_name(hb_font_t *font,
                                    void *font_data,
                                    const char *name,
                                    int len, /* -1 means nul-terminated */
                                    hb_codepoint_t *glyph,
                                    void *user_data);

#endif /* _RIG_TEXT_RENDERER_FUNCS_H_ */
