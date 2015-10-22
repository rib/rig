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
 */

#ifndef __CG_RECTANGLE_MAP_H
#define __CG_RECTANGLE_MAP_H

#include <clib.h>
#include "cg-types.h"

typedef struct _cg_rectangle_map_t cg_rectangle_map_t;
typedef struct _cg_rectangle_map_entry_t cg_rectangle_map_entry_t;

typedef void (*cg_rectangle_map_callback_t)(
    const cg_rectangle_map_entry_t *entry,
    void *rectangle_data,
    void *user_data);

struct _cg_rectangle_map_entry_t {
    unsigned int x, y;
    unsigned int width, height;
};

cg_rectangle_map_t *_cg_rectangle_map_new(unsigned int width,
                                          unsigned int height,
                                          c_destroy_func_t value_destroy_func);

bool _cg_rectangle_map_add(cg_rectangle_map_t *map,
                           unsigned int width,
                           unsigned int height,
                           void *data,
                           cg_rectangle_map_entry_t *rectangle);

void _cg_rectangle_map_remove(cg_rectangle_map_t *map,
                              const cg_rectangle_map_entry_t *rectangle);

unsigned int _cg_rectangle_map_get_width(cg_rectangle_map_t *map);

unsigned int _cg_rectangle_map_get_height(cg_rectangle_map_t *map);

unsigned int _cg_rectangle_map_get_remaining_space(cg_rectangle_map_t *map);

unsigned int _cg_rectangle_map_get_n_rectangles(cg_rectangle_map_t *map);

void _cg_rectangle_map_foreach(cg_rectangle_map_t *map,
                               cg_rectangle_map_callback_t callback,
                               void *data);

void _cg_rectangle_map_free(cg_rectangle_map_t *map);

#endif /* __CG_RECTANGLE_MAP_H */
