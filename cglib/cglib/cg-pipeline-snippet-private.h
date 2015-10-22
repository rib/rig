/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011, 2013 Intel Corporation.
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
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifndef __CG_PIPELINE_SNIPPET_PRIVATE_H
#define __CG_PIPELINE_SNIPPET_PRIVATE_H

#include <clib.h>

#include "cg-snippet.h"

typedef struct {
    c_llist_t *entries;
} cg_pipeline_snippet_list_t;

/* Arguments to pass to _cg_pipeline_snippet_generate_code() */
typedef struct {
    cg_pipeline_snippet_list_t *snippets;

    /* Only snippets at this hook point will be used */
    cg_snippet_hook_t hook;

    /* The final function to chain on to after all of the snippets code
       has been run */
    const char *chain_function;
    const char *chain_function_suffix;

    /* The name of the final generated function */
    const char *final_function;
    const char *final_function_suffix;

    /* The return type of all of the functions, or NULL to use void */
    const char *return_type;

    /* A variable to return from the functions. The snippets are
       expected to modify this variable. Ignored if return_type is
       NULL */
    const char *return_variable;

    /* If this is true then it won't allocate a separate variable for
       the return value. Instead it is expected that the snippet will
       modify one of the argument variables directly and that will be
       returned */
    bool return_variable_is_argument;

    /* The argument names or NULL if there are none */
    const char *arguments;

    /* The argument types or NULL */
    const char *argument_declarations;

    /* The string to generate the source into */
    c_string_t *source_buf;
} cg_pipeline_snippet_data_t;

void _cg_pipeline_snippet_generate_code(const cg_pipeline_snippet_data_t *data);

void
_cg_pipeline_snippet_generate_declarations(c_string_t *declarations_buf,
                                           cg_snippet_hook_t hook,
                                           cg_pipeline_snippet_list_t *list);

void _cg_pipeline_snippet_list_free(cg_pipeline_snippet_list_t *list);

void _cg_pipeline_snippet_list_add(cg_pipeline_snippet_list_t *list,
                                   cg_snippet_t *snippet);

void _cg_pipeline_snippet_list_copy(cg_pipeline_snippet_list_t *dst,
                                    const cg_pipeline_snippet_list_t *src);

void _cg_pipeline_snippet_list_hash(cg_pipeline_snippet_list_t *list,
                                    unsigned int *hash);

bool _cg_pipeline_snippet_list_equal(cg_pipeline_snippet_list_t *list0,
                                     cg_pipeline_snippet_list_t *list1);

#endif /* __CG_PIPELINE_SNIPPET_PRIVATE_H */
