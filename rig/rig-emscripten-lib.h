/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2015  Intel Corporation
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

#ifndef _RIG_EMSCRIPTEN_LIB_H_
#define _RIG_EMSCRIPTEN_LIB_H_

#include <emscripten.h>

typedef int rig_worker_t;

rig_worker_t rig_emscripten_worker_create(const char *url);

typedef void (*rig_worker_callback_func_t)(void *data, int len, void *user_data);

void rig_emscripten_worker_set_main_onmessage(rig_worker_t worker,
                                              rig_worker_callback_func_t callback,
                                              void *user_data);

void rig_emscripten_worker_post(rig_worker_t worker,
                                const char *function_name,
                                void *data,
                                int len);

void rig_emscripten_worker_destroy(rig_worker_t worker);

void rig_emscripten_worker_post_to_main(void *data, int len);

#endif /* _RIG_EMSCRIPTEN_LIB_H_ */
