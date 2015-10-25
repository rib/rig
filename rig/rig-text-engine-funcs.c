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

#include <rig-config.h>

#include <stdbool.h>

#include <hb.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_ADVANCES_H

#include "rig-text-engine-funcs.h"

/*
 * Note: That all of this code is copied from Harfbuzz, since there
 * doesn't seem to be a way to query back the hb_font_funcs_t for
 * a given font to be able to just replace a few functions. We want
 * to provide our own _get_glyph() function, but for everything else
 * then we just want to use the Freetype based functions implemented
 * in Harfbuzz.
 */

#if __GNUC__ >= 4
#define HB_UNUSED __attribute__((unused))
#else
#define HB_UNUSED
#endif

#if defined(__GNUC__) && (__GNUC__ > 2) && defined(__OPTIMIZE__)
#define _HB_BOOLEAN_EXPR(expr) ((expr) ? 1 : 0)
#define likely(expr) (__builtin_expect(_HB_BOOLEAN_EXPR(expr), 1))
#define unlikely(expr) (__builtin_expect(_HB_BOOLEAN_EXPR(expr), 0))
#else
#define likely(expr) (expr)
#define unlikely(expr) (expr)
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))

hb_position_t
rig_text_engine_get_glyph_h_advance(hb_font_t *font HB_UNUSED,
                                    void *font_data,
                                    hb_codepoint_t glyph,
                                    void *user_data HB_UNUSED)
{
    FT_Face ft_face = (FT_Face)font_data;
    int load_flags = FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING;
    FT_Fixed v;

    if (unlikely(FT_Get_Advance(ft_face, glyph, load_flags, &v)))
        return 0;

    return (v + (1 << 9)) >> 10;
}

hb_position_t
rig_text_engine_get_glyph_v_advance(hb_font_t *font HB_UNUSED,
                                    void *font_data,
                                    hb_codepoint_t glyph,
                                    void *user_data HB_UNUSED)
{
    FT_Face ft_face = (FT_Face)font_data;
    int load_flags =
        FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING | FT_LOAD_VERTICAL_LAYOUT;
    FT_Fixed v;

    if (unlikely(FT_Get_Advance(ft_face, glyph, load_flags, &v)))
        return 0;

    /* Note: FreeType's vertical metrics grows downward while other FreeType
     * coordinates
     * have a Y growing upward.  Hence the extra negation. */
    return (-v + (1 << 9)) >> 10;
}

hb_bool_t
rig_text_engine_get_glyph_h_origin(hb_font_t *font HB_UNUSED,
                                   void *font_data HB_UNUSED,
                                   hb_codepoint_t glyph HB_UNUSED,
                                   hb_position_t *x HB_UNUSED,
                                   hb_position_t *y HB_UNUSED,
                                   void *user_data HB_UNUSED)
{
    /* We always work in the horizontal coordinates. */
    return true;
}

hb_bool_t
rig_text_engine_get_glyph_v_origin(hb_font_t *font HB_UNUSED,
                                   void *font_data,
                                   hb_codepoint_t glyph,
                                   hb_position_t *x,
                                   hb_position_t *y,
                                   void *user_data HB_UNUSED)
{
    FT_Face ft_face = (FT_Face)font_data;
    int load_flags = FT_LOAD_DEFAULT;

    if (unlikely(FT_Load_Glyph(ft_face, glyph, load_flags)))
        return false;

    /* Note: FreeType's vertical metrics grows downward while other FreeType
     * coordinates
     * have a Y growing upward.  Hence the extra negation. */
    *x = ft_face->glyph->metrics.horiBearingX -
         ft_face->glyph->metrics.vertBearingX;
    *y = ft_face->glyph->metrics.horiBearingY -
         (-ft_face->glyph->metrics.vertBearingY);

    return true;
}

hb_position_t
rig_text_engine_get_glyph_h_kerning(hb_font_t *font,
                                    void *font_data,
                                    hb_codepoint_t left_glyph,
                                    hb_codepoint_t right_glyph,
                                    void *user_data HB_UNUSED)
{
    FT_Face ft_face = (FT_Face)font_data;
    FT_Vector kerningv;
    unsigned int x_ppem;
    unsigned int y_ppem;
    FT_Kerning_Mode mode;

    hb_font_get_ppem(font, &x_ppem, &y_ppem);
    mode = x_ppem ? FT_KERNING_DEFAULT : FT_KERNING_UNFITTED;

    if (FT_Get_Kerning(ft_face, left_glyph, right_glyph, mode, &kerningv))
        return 0;

    return kerningv.x;
}

hb_position_t
rig_text_engine_get_glyph_v_kerning(hb_font_t *font HB_UNUSED,
                                    void *font_data HB_UNUSED,
                                    hb_codepoint_t top_glyph HB_UNUSED,
                                    hb_codepoint_t bottom_glyph HB_UNUSED,
                                    void *user_data HB_UNUSED)
{
    /* FreeType API doesn't support vertical kerning */
    return 0;
}

hb_bool_t
rig_text_engine_get_glyph_extents(hb_font_t *font HB_UNUSED,
                                  void *font_data,
                                  hb_codepoint_t glyph,
                                  hb_glyph_extents_t *extents,
                                  void *user_data HB_UNUSED)
{
    FT_Face ft_face = (FT_Face)font_data;
    int load_flags = FT_LOAD_DEFAULT;

    if (unlikely(FT_Load_Glyph(ft_face, glyph, load_flags)))
        return false;

    extents->x_bearing = ft_face->glyph->metrics.horiBearingX;
    extents->y_bearing = ft_face->glyph->metrics.horiBearingY;
    extents->width = ft_face->glyph->metrics.width;
    extents->height = -ft_face->glyph->metrics.height;
    return true;
}

hb_bool_t
rig_text_engine_get_glyph_contour_point(hb_font_t *font HB_UNUSED,
                                        void *font_data,
                                        hb_codepoint_t glyph,
                                        unsigned int point_index,
                                        hb_position_t *x,
                                        hb_position_t *y,
                                        void *user_data HB_UNUSED)
{
    FT_Face ft_face = (FT_Face)font_data;
    int load_flags = FT_LOAD_DEFAULT;

    if (unlikely(FT_Load_Glyph(ft_face, glyph, load_flags)))
        return false;

    if (unlikely(ft_face->glyph->format != FT_GLYPH_FORMAT_OUTLINE))
        return false;

    if (unlikely(point_index >= (unsigned int)ft_face->glyph->outline.n_points))
        return false;

    *x = ft_face->glyph->outline.points[point_index].x;
    *y = ft_face->glyph->outline.points[point_index].y;

    return true;
}

hb_bool_t
rig_text_engine_get_glyph_name(hb_font_t *font HB_UNUSED,
                               void *font_data,
                               hb_codepoint_t glyph,
                               char *name,
                               unsigned int size,
                               void *user_data HB_UNUSED)
{
    FT_Face ft_face = (FT_Face)font_data;

    hb_bool_t ret = !FT_Get_Glyph_Name(ft_face, glyph, name, size);
    if (ret && (size && !*name))
        ret = false;

    return ret;
}

hb_bool_t
rig_text_engine_get_glyph_from_name(hb_font_t *font HB_UNUSED,
                                    void *font_data,
                                    const char *name,
                                    int len, /* -1 means nul-terminated */
                                    hb_codepoint_t *glyph,
                                    void *user_data HB_UNUSED)
{
    FT_Face ft_face = (FT_Face)font_data;

    if (len < 0)
        *glyph = FT_Get_Name_Index(ft_face, (FT_String *)name);
    else {
        /* Make a nul-terminated version. */
        char buf[128];
        len = MIN(len, (int)sizeof(buf) - 1);
        strncpy(buf, name, len);
        buf[len] = '\0';
        *glyph = FT_Get_Name_Index(ft_face, buf);
    }

    if (*glyph == 0) {
        /* Check whether the given name was actually the name of glyph 0. */
        char buf[128];
        if (!FT_Get_Glyph_Name(ft_face, 0, buf, sizeof(buf)) && len < 0
            ? !strcmp(buf, name)
            : !strncmp(buf, name, len))
            return true;
    }

    return *glyph != 0;
}
