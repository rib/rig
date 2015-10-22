/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009 Intel Corporation.
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

#include <cglib-config.h>

#include <stdlib.h>

#include "cg-i18n-private.h"
#include "cg-private.h"
#include "cg-debug.h"

/* XXX: If you add a debug option, please also add an option
 * definition to cg-debug-options.h. This will enable us - for
 * example - to emit a "help" description for the option.
 */

/* NB: Only these options get enabled if CG_DEBUG=all is
 * used since they don't affect the behaviour of CGlib they
 * simply print out verbose information */
static const c_debug_key_t cg_log_debug_keys[] = {
    { "object", CG_DEBUG_OBJECT },
    { "slicing", CG_DEBUG_SLICING },
    { "atlas", CG_DEBUG_ATLAS },
    { "blend-strings", CG_DEBUG_BLEND_STRINGS },
    { "matrices", CG_DEBUG_MATRICES },
    { "draw", CG_DEBUG_DRAW },
    { "opengl", CG_DEBUG_OPENGL },
    { "pango", CG_DEBUG_PANGO },
    { "show-source", CG_DEBUG_SHOW_SOURCE },
    { "offscreen", CG_DEBUG_OFFSCREEN },
    { "texture-pixmap", CG_DEBUG_TEXTURE_PIXMAP },
    { "bitmap", CG_DEBUG_BITMAP },
    { "clipping", CG_DEBUG_CLIPPING },
    { "winsys", CG_DEBUG_WINSYS },
    { "performance", CG_DEBUG_PERFORMANCE }
};
static const int n_cg_log_debug_keys = C_N_ELEMENTS(cg_log_debug_keys);

static const c_debug_key_t cg_behavioural_debug_keys[] = {
    { "rectangles", CG_DEBUG_RECTANGLES },
    { "disable-batching", CG_DEBUG_DISABLE_BATCHING },
    { "disable-vbos", CG_DEBUG_DISABLE_VBOS },
    { "disable-pbos", CG_DEBUG_DISABLE_PBOS },
    { "disable-software-transform", CG_DEBUG_DISABLE_SOFTWARE_TRANSFORM },
    { "dump-atlas-image", CG_DEBUG_DUMP_ATLAS_IMAGE },
    { "disable-atlas", CG_DEBUG_DISABLE_ATLAS },
    { "disable-shared-atlas", CG_DEBUG_DISABLE_SHARED_ATLAS },
    { "disable-texturing", CG_DEBUG_DISABLE_TEXTURING },
    { "disable-glsl", CG_DEBUG_DISABLE_GLSL },
    { "disable-blending", CG_DEBUG_DISABLE_BLENDING },
    { "disable-npot-textures", CG_DEBUG_DISABLE_NPOT_TEXTURES },
    { "wireframe", CG_DEBUG_WIREFRAME },
    { "disable-software-clip", CG_DEBUG_DISABLE_SOFTWARE_CLIP },
    { "disable-program-caches", CG_DEBUG_DISABLE_PROGRAM_CACHES },
    { "disable-fast-read-pixel", CG_DEBUG_DISABLE_FAST_READ_PIXEL }
};
static const int n_cg_behavioural_debug_keys =
    C_N_ELEMENTS(cg_behavioural_debug_keys);

unsigned long _cg_debug_flags[CG_DEBUG_N_LONGS];
c_hash_table_t *_cg_debug_instances;

static void
_cg_parse_debug_string_for_keys(const char *value,
                                bool enable,
                                const c_debug_key_t *keys,
                                unsigned int nkeys)
{
    int long_num, key_num;

    /* c_parse_debug_string expects the value field in c_debug_key_t to be a
       mask in a guint but the flags is stored in an array of multiple
       longs so we need to build a separate array for each possible
       guint */

    for (long_num = 0; long_num < CG_DEBUG_N_LONGS; long_num++) {
        int int_num;

        for (int_num = 0;
             int_num < sizeof(unsigned long) / sizeof(unsigned int);
             int_num++) {
            c_debug_key_t keys_for_int[sizeof(unsigned int) * 8];
            int nkeys_for_int = 0;

            for (key_num = 0; key_num < nkeys; key_num++) {
                int long_index = CG_FLAGS_GET_INDEX(keys[key_num].value);
                int int_index =
                    (keys[key_num].value % (sizeof(unsigned long) * 8) /
                     (sizeof(unsigned int) * 8));

                if (long_index == long_num && int_index == int_num) {
                    keys_for_int[nkeys_for_int] = keys[key_num];
                    keys_for_int[nkeys_for_int].value =
                        CG_FLAGS_GET_MASK(keys[key_num].value) >>
                        (int_num * sizeof(unsigned int) * 8);
                    nkeys_for_int++;
                }
            }

            if (nkeys_for_int > 0) {
                unsigned long mask = ((unsigned long)c_parse_debug_string(
                                          value, keys_for_int, nkeys_for_int))
                                     << (int_num * sizeof(unsigned int) * 8);

                if (enable)
                    _cg_debug_flags[long_num] |= mask;
                else
                    _cg_debug_flags[long_num] &= ~mask;
            }
        }
    }
}

void
_cg_parse_debug_string(const char *value, bool enable, bool ignore_help)
{
    if (ignore_help && strcmp(value, "help") == 0)
        return;

    /* We don't want to let c_parse_debug_string handle "all" because
     * literally enabling all the debug options wouldn't be useful to
     * anyone; instead the all option enables all non behavioural
     * options.
     */
    if (strcmp(value, "all") == 0 || strcmp(value, "verbose") == 0) {
        int i;
        for (i = 0; i < n_cg_log_debug_keys; i++)
            if (enable)
                CG_DEBUG_SET_FLAG(cg_log_debug_keys[i].value);
            else
                CG_DEBUG_CLEAR_FLAG(cg_log_debug_keys[i].value);
    } else if (c_ascii_strcasecmp(value, "help") == 0) {
        c_printerr("\n\n%28s\n", _("Supported debug values:"));
#define OPT(MASK_NAME, GROUP, NAME, NAME_FORMATTED, DESCRIPTION)               \
    c_printerr("%28s %s\n", NAME ":", _(DESCRIPTION));
#include "cg-debug-options.h"
        c_printerr("\n%28s\n", _("Special debug values:"));
        OPT(IGNORED,
            "ignored",
            "all",
            "ignored",
            N_("Enables all non-behavioural debug options"));
        OPT(IGNORED,
            "ignored",
            "verbose",
            "ignored",
            N_("Enables all non-behavioural debug options"));
#undef OPT

        c_printerr("\n"
                   "%28s\n"
                   " CG_DISABLE_GL_EXTENSIONS: %s\n"
                   "   CG_OVERRIDE_GL_VERSION: %s\n",
                   _("Additional environment variables:"),
                   _("Comma-separated list of GL extensions to pretend are "
                     "disabled"),
                   _("Override the GL version that CGlib will assume the driver "
                     "supports"));
        exit(1);
    } else {
        _cg_parse_debug_string_for_keys(
            value, enable, cg_log_debug_keys, n_cg_log_debug_keys);
        _cg_parse_debug_string_for_keys(value,
                                        enable,
                                        cg_behavioural_debug_keys,
                                        n_cg_behavioural_debug_keys);
    }
}

void
_cg_debug_check_environment(void)
{
    const char *env_string;

#ifdef __EMSCRIPTEN__
    //env_string = "all";
    env_string = NULL;
#else
    env_string = c_getenv("CG_DEBUG");
#endif
    if (env_string != NULL) {
        _cg_parse_debug_string(env_string,
                               true /* enable the flags */,
                               false /* don't ignore help */);
        env_string = NULL;
    }

    env_string = c_getenv("CG_NO_DEBUG");
    if (env_string != NULL) {
        _cg_parse_debug_string(env_string,
                               false /* disable the flags */,
                               false /* don't ignore help */);
        env_string = NULL;
    }
}
