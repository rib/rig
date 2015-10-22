/*
 * Copyright (C) 2015 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <clib-config.h>

#include <clib.h>
#include <cmodule.h>

#if defined(USE_UV)
#include <uv.h>

#ifdef OS_WIN32
#define LIBPREFIX ""
#define LIBSUFFIX ".dll"
#else
#define LIBPREFIX "lib"
#define LIBSUFFIX ".so"
#endif

struct _c_module_t {
    uv_lib_t lib;
};

c_module_t *
c_module_open(const char *file)
{
    c_module_t *module = c_new(c_module_t, 1);

    if (uv_dlopen(file, &module->lib)) {
        c_free(module);
        return NULL;
    } else
        return module;
}

bool
c_module_symbol(c_module_t *module, const char *symbol_name, void **symbol)
{
    c_return_val_if_fail(module && symbol_name && symbol, false);

    return uv_dlsym(&module->lib, symbol_name, symbol) == 0;
}

#if 0
const char *
c_module_error(c_module_t *module)
{
    return uv_dlerror(&module->lib);
}
#endif

void
c_module_close(c_module_t *module)
{
    c_return_if_fail(module);

    uv_dlclose(&module->lib);
    c_free(module);
}

#else

#define LIBSUFFIX ""
#define LIBPREFIX ""

c_module_t *
c_module_open(const char *file)
{
    c_error("%s", "c_module_open not implemented on this platform");
    return NULL;
}

bool
c_module_symbol(c_module_t *module, const char *symbol_name, void **symbol)
{
    c_error("%s", "c_module_open not implemented on this platform");
    return false;
}

void
c_module_close(c_module_t *module)
{
    c_error("%s", "c_module_open not implemented on this platform");
}
#endif

char *
c_module_build_path(const char *directory, const char *module_name)
{
    char *lib_prefix = "";
    char *lib_suffix = "";
    int suffix_len = strlen(LIBSUFFIX);
    int name_len = strlen(module_name);

    if (module_name == NULL)
        return NULL;

    if (strncmp(module_name, LIBPREFIX, strlen(LIBPREFIX)) != 0)
        lib_prefix = LIBPREFIX;

    if (name_len > suffix_len &&
        strncmp(module_name + name_len - suffix_len,
                LIBSUFFIX, suffix_len) != 0)
    {
        lib_suffix = LIBSUFFIX;
    }

    if (directory && *directory)
        return c_strconcat("%s%s%s%s", directory, C_DIR_SEPARATOR_S,
                           lib_prefix, module_name, lib_suffix, NULL);
    else
        return c_strconcat("%s%s", lib_prefix, module_name, lib_suffix, NULL);
}
