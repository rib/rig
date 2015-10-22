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
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#include <cglib-config.h>

#include "cg-types.h"
#include "cg-snippet-private.h"
#include "cg-util.h"

static void _cg_snippet_free(cg_snippet_t *snippet);

CG_OBJECT_DEFINE(Snippet, snippet);

cg_snippet_t *
cg_snippet_new(cg_snippet_hook_t hook,
               const char *declarations,
               const char *post)
{
    cg_snippet_t *snippet = c_slice_new0(cg_snippet_t);

    _cg_snippet_object_new(snippet);

    snippet->hook = hook;

    cg_snippet_set_declarations(snippet, declarations);
    cg_snippet_set_post(snippet, post);

    return snippet;
}

cg_snippet_hook_t
cg_snippet_get_hook(cg_snippet_t *snippet)
{
    c_return_val_if_fail(cg_is_snippet(snippet), 0);

    return snippet->hook;
}

static bool
_cg_snippet_modify(cg_snippet_t *snippet)
{
    if (snippet->immutable) {
        c_warning("A cg_snippet_t should not be modified once it has been "
                  "attached to a pipeline. Any modifications after that point "
                  "will be ignored.");

        return false;
    }

    return true;
}

void
cg_snippet_set_declarations(cg_snippet_t *snippet,
                            const char *declarations)
{
    c_return_if_fail(cg_is_snippet(snippet));

    if (!_cg_snippet_modify(snippet))
        return;

    c_free(snippet->declarations);
    snippet->declarations = declarations ? c_strdup(declarations) : NULL;
}

const char *
cg_snippet_get_declarations(cg_snippet_t *snippet)
{
    c_return_val_if_fail(cg_is_snippet(snippet), NULL);

    return snippet->declarations;
}

void
cg_snippet_set_pre(cg_snippet_t *snippet, const char *pre)
{
    c_return_if_fail(cg_is_snippet(snippet));

    if (!_cg_snippet_modify(snippet))
        return;

    c_free(snippet->pre);
    snippet->pre = pre ? c_strdup(pre) : NULL;
}

const char *
cg_snippet_get_pre(cg_snippet_t *snippet)
{
    c_return_val_if_fail(cg_is_snippet(snippet), NULL);

    return snippet->pre;
}

void
cg_snippet_set_replace(cg_snippet_t *snippet, const char *replace)
{
    c_return_if_fail(cg_is_snippet(snippet));

    if (!_cg_snippet_modify(snippet))
        return;

    c_free(snippet->replace);
    snippet->replace = replace ? c_strdup(replace) : NULL;
}

const char *
cg_snippet_get_replace(cg_snippet_t *snippet)
{
    c_return_val_if_fail(cg_is_snippet(snippet), NULL);

    return snippet->replace;
}

void
cg_snippet_set_post(cg_snippet_t *snippet, const char *post)
{
    c_return_if_fail(cg_is_snippet(snippet));

    if (!_cg_snippet_modify(snippet))
        return;

    c_free(snippet->post);
    snippet->post = post ? c_strdup(post) : NULL;
}

const char *
cg_snippet_get_post(cg_snippet_t *snippet)
{
    c_return_val_if_fail(cg_is_snippet(snippet), NULL);

    return snippet->post;
}

void
_cg_snippet_make_immutable(cg_snippet_t *snippet)
{
    snippet->immutable = true;
}

static void
_cg_snippet_free(cg_snippet_t *snippet)
{
    c_free(snippet->declarations);
    c_free(snippet->pre);
    c_free(snippet->replace);
    c_free(snippet->post);
    c_slice_free(cg_snippet_t, snippet);
}
