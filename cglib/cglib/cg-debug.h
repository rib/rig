/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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
 *
 */

#ifndef __CG_DEBUG_H__
#define __CG_DEBUG_H__

#include "cg-profile.h"
#include "cg-flags.h"
#include "cg-util.h"

#include <clib.h>

CG_BEGIN_DECLS

typedef enum {
    CG_DEBUG_SLICING,
    CG_DEBUG_OFFSCREEN,
    CG_DEBUG_DRAW,
    CG_DEBUG_PANGO,
    CG_DEBUG_RECTANGLES,
    CG_DEBUG_OBJECT,
    CG_DEBUG_BLEND_STRINGS,
    CG_DEBUG_DISABLE_BATCHING,
    CG_DEBUG_DISABLE_VBOS,
    CG_DEBUG_DISABLE_PBOS,
    CG_DEBUG_JOURNAL,
    CG_DEBUG_BATCHING,
    CG_DEBUG_DISABLE_SOFTWARE_TRANSFORM,
    CG_DEBUG_MATRICES,
    CG_DEBUG_ATLAS,
    CG_DEBUG_DUMP_ATLAS_IMAGE,
    CG_DEBUG_DISABLE_ATLAS,
    CG_DEBUG_DISABLE_SHARED_ATLAS,
    CG_DEBUG_OPENGL,
    CG_DEBUG_DISABLE_TEXTURING,
    CG_DEBUG_DISABLE_GLSL,
    CG_DEBUG_SHOW_SOURCE,
    CG_DEBUG_DISABLE_BLENDING,
    CG_DEBUG_TEXTURE_PIXMAP,
    CG_DEBUG_BITMAP,
    CG_DEBUG_DISABLE_NPOT_TEXTURES,
    CG_DEBUG_WIREFRAME,
    CG_DEBUG_DISABLE_SOFTWARE_CLIP,
    CG_DEBUG_DISABLE_PROGRAM_CACHES,
    CG_DEBUG_DISABLE_FAST_READ_PIXEL,
    CG_DEBUG_CLIPPING,
    CG_DEBUG_WINSYS,
    CG_DEBUG_PERFORMANCE,
    CG_DEBUG_N_FLAGS
} cg_debug_flags_t;

extern c_hash_table_t *_cg_debug_instances;
#define CG_DEBUG_N_LONGS CG_FLAGS_N_LONGS_FOR_SIZE(CG_DEBUG_N_FLAGS)

extern unsigned long _cg_debug_flags[CG_DEBUG_N_LONGS];

#define CG_DEBUG_ENABLED(flag) CG_FLAGS_GET(_cg_debug_flags, flag)

#define CG_DEBUG_SET_FLAG(flag) CG_FLAGS_SET(_cg_debug_flags, flag, true)

#define CG_DEBUG_CLEAR_FLAG(flag) CG_FLAGS_SET(_cg_debug_flags, flag, false)

#ifdef __GNUC__
#define CG_NOTE(type, x, a ...)                                                 \
    C_STMT_START                                                               \
    {                                                                          \
        if (C_UNLIKELY(CG_DEBUG_ENABLED(CG_DEBUG_##type))) {                   \
            _cg_profile_trace_message("[" #type "] " C_STRLOC " & " x, ##a);   \
        }                                                                      \
    }                                                                          \
    C_STMT_END

#else
#define CG_NOTE(type, ...)                                                     \
    C_STMT_START                                                               \
    {                                                                          \
        if (C_UNLIKELY(CG_DEBUG_ENABLED(CG_DEBUG_##type))) {                   \
            char *_fmt = c_strdup_printf(__VA_ARGS__);                         \
            _cg_profile_trace_message("[" #type "] " C_STRLOC " & %s", _fmt);  \
            c_free(_fmt);                                                      \
        }                                                                      \
    }                                                                          \
    C_STMT_END

#endif /* __GNUC__ */

void _cg_debug_check_environment(void);

void _cg_parse_debug_string(const char *value, bool enable, bool ignore_help);

CG_END_DECLS

#endif /* __CG_DEBUG_H__ */
