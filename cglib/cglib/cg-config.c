/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
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
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#include <cglib-config.h>

#include "cg-debug.h"
#include "cg-config-private.h"

#ifdef CG_HAS_GLIB_SUPPORT
#include <glib.h>
#endif

char *_cg_config_driver;
char *_cg_config_renderer;
char *_cg_config_disable_gl_extensions;
char *_cg_config_override_gl_version;

#ifndef CG_HAS_GLIB_SUPPORT

void
_cg_config_read(void)
{
}

#else /* CG_HAS_GLIB_SUPPORT */

/* Array of config options that just set a global string */
static const struct {
    const char *conf_name;
    char **variable;
} cg_config_string_options[] = {
    { "CG_DRIVER", &_cg_config_driver },
    { "CG_RENDERER", &_cg_config_renderer },
    { "CG_DISABLE_GL_EXTENSIONS", &_cg_config_disable_gl_extensions },
    { "CG_OVERRIDE_GL_VERSION", &_cg_config_override_gl_version }
};

static void
_cg_config_process(GKeyFile *key_file)
{
    char *value;
    int i;

    value = g_key_file_get_string(key_file, "global", "CG_DEBUG", NULL);
    if (value) {
        _cg_parse_debug_string(
            value, true /* enable the flags */, true /* ignore help option */);
        g_free(value);
    }

    value = g_key_file_get_string(key_file, "global", "CG_NO_DEBUG", NULL);
    if (value) {
        _cg_parse_debug_string(value,
                               false /* disable the flags */,
                               true /* ignore help option */);
        g_free(value);
    }

    for (i = 0; i < C_N_ELEMENTS(cg_config_string_options); i++) {
        const char *conf_name = cg_config_string_options[i].conf_name;
        char **variable = cg_config_string_options[i].variable;

        value = g_key_file_get_string(key_file, "global", conf_name, NULL);
        if (value) {
            g_free(*variable);
            *variable = value;
        }
    }
}

void
_cg_config_read(void)
{
    GKeyFile *key_file = g_key_file_new();
    const char *const *system_dirs = g_get_system_config_dirs();
    char *filename;
    bool status = false;
    int i;

    for (i = 0; system_dirs[i]; i++) {
        filename = g_build_filename(system_dirs[i], "cg", "cg.conf", NULL);
        status = g_key_file_load_from_file(key_file, filename, 0, NULL);
        g_free(filename);
        if (status) {
            _cg_config_process(key_file);
            g_key_file_free(key_file);
            key_file = g_key_file_new();
            break;
        }
    }

    filename =
        g_build_filename(g_get_user_config_dir(), "cg", "cg.conf", NULL);
    status = g_key_file_load_from_file(key_file, filename, 0, NULL);
    g_free(filename);

    if (status)
        _cg_config_process(key_file);

    g_key_file_free(key_file);
}

#endif /* CG_HAS_GLIB_SUPPORT */
