/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013  Intel Corporation.
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
 */

#include <rut-config.h>

#include <clib.h>

#include "rut-interfaces.h"
#include "rut-graph.h"

struct _rut_graph_t {
    rut_object_base_t _base;

    rut_graphable_props_t graphable;
};

static void
_rut_graph_free(void *object)
{
    rut_graph_t *graph = object;

    rut_graphable_destroy(graph);

    rut_object_free(rut_graph_t, object);
}

rut_type_t rut_graph_type;

static void
_rut_graph_init_type(void)
{
    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child remove */
                                                       NULL, /* child add */
                                                       NULL /* parent changed */
    };

    rut_type_t *type = &rut_graph_type;
#define TYPE rut_graph_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_graph_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_GRAPHABLE,
                       offsetof(TYPE, graphable),
                       &graphable_vtable);

#undef TYPE
}

rut_graph_t *
rut_graph_new(rut_shell_t *shell)
{
    rut_graph_t *graph =
        rut_object_alloc(rut_graph_t, &rut_graph_type, _rut_graph_init_type);

    rut_graphable_init(graph);

    return graph;
}
