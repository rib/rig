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

#include <cglib-config.h>

#include <string.h>

#include "cg-types.h"
#include "cg-pipeline-snippet-private.h"
#include "cg-snippet-private.h"
#include "cg-util.h"

/* Helper functions that are used by both GLSL pipeline backends */

void
_cg_pipeline_snippet_generate_code(const cg_pipeline_snippet_data_t *data)
{
    c_llist_t *first_snippet, *l;
    cg_snippet_t *snippet;
    int snippet_num = 0;
    int n_snippets = 0;
    const char *chain_func = data->chain_function;
    const char *chain_func_suffix = data->chain_function_suffix ?
        data->chain_function_suffix : "";
    const char *final_func = data->final_function;
    const char *final_func_suffix = data->final_function_suffix ?
        data->final_function_suffix : "";

    first_snippet = data->snippets->entries;

    /* First count the number of snippets so we can easily tell when
       we're at the last one */
    for (l = data->snippets->entries; l; l = l->next) {
        snippet = l->data;

        if (snippet->hook == data->hook) {
            /* Don't bother processing any previous snippets if we reach
               one that has a replacement */
            if (snippet->replace) {
                n_snippets = 1;
                first_snippet = l;
            } else
                n_snippets++;
        }
    }

    /* If there weren't any snippets then generate a stub function with
       the final name */
    if (n_snippets == 0) {
        if (data->return_type)
            c_string_append_printf(
                data->source_buf,
                "\n"
                "%s\n"
                "%s%s(%s)\n"
                "{\n"
                "  return %s%s(%s);\n"
                "}\n",
                data->return_type,
                final_func,
                final_func_suffix,
                data->argument_declarations ? data->argument_declarations : "",
                chain_func,
                chain_func_suffix,
                data->arguments ? data->arguments : "");
        else
            c_string_append_printf(
                data->source_buf,
                "\n"
                "void\n"
                "%s%s(%s)\n"
                "{\n"
                "  %s%s(%s);\n"
                "}\n",
                final_func,
                final_func_suffix,
                data->argument_declarations ? data->argument_declarations : "",
                chain_func,
                chain_func_suffix,
                data->arguments ? data->arguments : "");

        return;
    }

    for (l = first_snippet; snippet_num < n_snippets; l = l->next) {
        snippet = l->data;

        if (snippet->hook == data->hook) {
            const char *source;

            if ((source = cg_snippet_get_declarations(snippet)))
                c_string_append(data->source_buf, source);

            c_string_append_printf(data->source_buf,
                                   "\n"
                                   "%s\n",
                                   data->return_type ? data->return_type
                                   : "void");

            if (snippet_num + 1 < n_snippets)
                c_string_append_printf(data->source_buf,
                                       "%s%s_hook%i",
                                       final_func,
                                       final_func_suffix,
                                       snippet_num);
            else {
                c_string_append(data->source_buf, final_func);
                c_string_append(data->source_buf, final_func_suffix);
            }

            c_string_append(data->source_buf, "(");

            if (data->argument_declarations)
                c_string_append(data->source_buf, data->argument_declarations);

            c_string_append(data->source_buf,
                            ")\n"
                            "{\n");

            if (data->return_type && !data->return_variable_is_argument)
                c_string_append_printf(data->source_buf,
                                       "  %s %s;\n"
                                       "\n",
                                       data->return_type,
                                       data->return_variable);

            if ((source = cg_snippet_get_pre(snippet)))
                c_string_append(data->source_buf, source);

            /* Chain on to the next function, or bypass it if there is
               a replace string */
            if ((source = cg_snippet_get_replace(snippet)))
                c_string_append(data->source_buf, source);
            else {
                c_string_append(data->source_buf, "  ");

                if (data->return_type)
                    c_string_append_printf(
                        data->source_buf, "%s = ", data->return_variable);

                if (snippet_num > 0)
                    c_string_append_printf(data->source_buf,
                                           "%s%s_hook%i",
                                           final_func,
                                           final_func_suffix,
                                           snippet_num - 1);
                else {
                    c_string_append(data->source_buf, chain_func);
                    c_string_append(data->source_buf, chain_func_suffix);
                }

                c_string_append(data->source_buf, "(");

                if (data->arguments)
                    c_string_append(data->source_buf, data->arguments);

                c_string_append(data->source_buf, ");\n");
            }

            if ((source = cg_snippet_get_post(snippet)))
                c_string_append(data->source_buf, source);

            if (data->return_type)
                c_string_append_printf(
                    data->source_buf, "  return %s;\n", data->return_variable);

            c_string_append(data->source_buf, "}\n");
            snippet_num++;
        }
    }
}

void
_cg_pipeline_snippet_generate_declarations(c_string_t *declarations_buf,
                                           cg_snippet_hook_t hook,
                                           cg_pipeline_snippet_list_t *snippets)
{
    c_llist_t *l;

    for (l = snippets->entries; l; l = l->next) {
        cg_snippet_t *snippet = l->data;

        if (snippet->hook == hook) {
            const char *source;

            if ((source = cg_snippet_get_declarations(snippet)))
                c_string_append(declarations_buf, source);
        }
    }
}

void
_cg_pipeline_snippet_list_free(cg_pipeline_snippet_list_t *list)
{
    c_llist_t *l, *tmp;

    for (l = list->entries; l; l = tmp) {
        tmp = l->next;

        cg_object_unref(l->data);
        c_llist_free_1(l);
    }
}

void
_cg_pipeline_snippet_list_add(cg_pipeline_snippet_list_t *list,
                              cg_snippet_t *snippet)
{
    list->entries = c_llist_append(list->entries, cg_object_ref(snippet));

    _cg_snippet_make_immutable(snippet);
}

void
_cg_pipeline_snippet_list_copy(cg_pipeline_snippet_list_t *dst,
                               const cg_pipeline_snippet_list_t *src)
{
    c_queue_t queue = C_QUEUE_INIT;
    const c_llist_t *l;

    for (l = src->entries; l; l = l->next)
        c_queue_push_tail(&queue, cg_object_ref(l->data));

    dst->entries = queue.head;
}

void
_cg_pipeline_snippet_list_hash(cg_pipeline_snippet_list_t *list,
                               unsigned int *hash)
{
    c_llist_t *l;

    for (l = list->entries; l; l = l->next) {
        cg_snippet_t *snippet = l->data;

        *hash = _cg_util_one_at_a_time_hash(
            *hash, &snippet, sizeof(cg_snippet_t *));
    }
}

bool
_cg_pipeline_snippet_list_equal(cg_pipeline_snippet_list_t *list0,
                                cg_pipeline_snippet_list_t *list1)
{
    c_llist_t *l0, *l1;

    for (l0 = list0->entries, l1 = list1->entries; l0 && l1;
         l0 = l0->next, l1 = l1->next)
        if (l0->data != l1->data)
            return false;

    return l0 == NULL && l1 == NULL;
}
