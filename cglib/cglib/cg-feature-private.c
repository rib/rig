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

#include <string.h>

#include "cg-device-private.h"

#include "cg-feature-private.h"
#include "cg-renderer-private.h"
#include "cg-private.h"

bool
_cg_feature_check(cg_renderer_t *renderer,
                  const char *driver_prefix,
                  const cg_feature_data_t *data,
                  int gl_major,
                  int gl_minor,
                  cg_driver_t driver,
                  char *const *extensions,
                  void *function_table)

{
    const char *suffix = NULL;
    int func_num;
    cg_ext_gles_availability_t gles_availability;
    bool in_core;

    if (driver == CG_DRIVER_GLES2) {
        gles_availability = CG_EXT_IN_GLES2;

        if (CG_CHECK_GL_VERSION(gl_major, gl_minor, 3, 0))
            gles_availability |= CG_EXT_IN_GLES3;
    } else {
        gles_availability = 0;
    }

    /* First check whether the functions should be directly provided by
       GL */
    if (((driver == CG_DRIVER_GL || driver == CG_DRIVER_GL3) &&
         CG_CHECK_GL_VERSION(
             gl_major, gl_minor, data->min_gl_major, data->min_gl_minor)) ||
        (data->gles_availability & gles_availability)) {
        suffix = "";
        in_core = true;
    } else {
        /* Otherwise try all of the extensions */
        const char *namespace, *namespace_suffix;
        unsigned int namespace_len;

        for (namespace = data->namespaces; *namespace;
             namespace += strlen(namespace) + 1) {
        const char *extension;
        c_string_t *full_extension_name = c_string_new("");

        /* If the namespace part contains a ':' then the suffix for
           the function names is different from the name space */
        if ((namespace_suffix = strchr(namespace, ':'))) {
        namespace_len = namespace_suffix - namespace;
        namespace_suffix++;
        } else {
            namespace_len = strlen(namespace);
            namespace_suffix = namespace;
        }

        for (extension = data->extension_names; *extension;
             extension += strlen(extension) + 1) {
            c_string_assign(full_extension_name, driver_prefix);
            c_string_append_c(full_extension_name, '_');
            c_string_append_len(
                full_extension_name, namespace, namespace_len);
            c_string_append_c(full_extension_name, '_');
            c_string_append(full_extension_name, extension);
            if (_cg_check_extension(full_extension_name->str, extensions))
                break;
        }

        c_string_free(full_extension_name, true);

        /* If we found an extension with this namespace then use it
           as the suffix */
        if (*extension) {
            suffix = namespace_suffix;
            break;
        }
        }

        in_core = false;
    }

    /* If we couldn't find anything that provides the functions then
       give up */
    if (suffix == NULL)
        goto error;

    /* Try to get all of the entry points */
    for (func_num = 0; data->functions[func_num].name; func_num++) {
        void *func;
        char *full_function_name;

        full_function_name =
            c_strconcat(data->functions[func_num].name, suffix, NULL);
        func = _cg_renderer_get_proc_address(
            renderer, full_function_name, in_core);
        c_free(full_function_name);

        if (func == NULL)
            goto error;

        /* Set the function pointer in the context */
        *(void **)((uint8_t *)function_table +
                   data->functions[func_num].pointer_offset) = func;
    }

    return true;

/* If the extension isn't found or one of the functions wasn't found
 * then set all of the functions pointers to NULL so CGlib can safely
 * do feature testing by just looking at the function pointers */
error:
    for (func_num = 0; data->functions[func_num].name; func_num++)
        *(void **)((uint8_t *)function_table +
                   data->functions[func_num].pointer_offset) = NULL;

    return false;
}

/* Define a set of arrays containing the functions required from GL
   for each feature */
#define CG_EXT_BEGIN(name,                                                     \
                     min_gl_major,                                             \
                     min_gl_minor,                                             \
                     gles_availability,                                        \
                     namespaces,                                               \
                     extension_names)                                          \
    static const cg_feature_function_t cg_ext_##name##_funcs[] = {
#define CG_EXT_FUNCTION(ret, name, args)                                       \
    {                                                                          \
        C_STRINGIFY(name), C_STRUCT_OFFSET(cg_device_t, name)                 \
    }                                                                          \
    ,
#define CG_EXT_END()                                                           \
    {                                                                          \
        NULL, 0                                                                \
    }                                                                          \
    ,                                                                          \
    }                                                                          \
    ;
#include "gl-prototypes/cg-all-functions.h"

/* Define an array of features */
#undef CG_EXT_BEGIN
#define CG_EXT_BEGIN(name,                                                     \
                     min_gl_major,                                             \
                     min_gl_minor,                                             \
                     gles_availability,                                        \
                     namespaces,                                               \
                     extension_names)                                          \
    {                                                                          \
        min_gl_major, min_gl_minor, gles_availability, namespaces,             \
        extension_names, 0, 0, cg_ext_##name##_funcs                       \
    }                                                                          \
    ,
#undef CG_EXT_FUNCTION
#define CG_EXT_FUNCTION(ret, name, args)
#undef CG_EXT_END
#define CG_EXT_END()

static const cg_feature_data_t cg_feature_ext_functions_data[] = {
#include "gl-prototypes/cg-all-functions.h"
};

void
_cg_feature_check_ext_functions(cg_device_t *dev,
                                int gl_major,
                                int gl_minor,
                                char *const *gl_extensions)
{
    int i;

    for (i = 0; i < C_N_ELEMENTS(cg_feature_ext_functions_data); i++)
        _cg_feature_check(dev->display->renderer,
                          "GL",
                          cg_feature_ext_functions_data + i,
                          gl_major,
                          gl_minor,
                          dev->driver,
                          gl_extensions,
                          dev);
}
