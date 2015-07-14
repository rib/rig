/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013,2014  Intel Corporation.
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

#ifndef _RUT_TIMELINE_H_
#define _RUT_TIMELINE_H_

extern rut_type_t rut_timeline_type;

typedef struct _rut_timeline_t rut_timeline_t;

rut_timeline_t *rut_timeline_new(rut_shell_t *shell, float length);

void rut_timeline_start(rut_timeline_t *timeline);

void rut_timeline_stop(rut_timeline_t *timeline);

bool rut_timeline_get_running(rut_object_t *timeline);

void rut_timeline_set_running(rut_object_t *timeline, bool running);

bool rut_timeline_is_running(rut_timeline_t *timeline);

double rut_timeline_get_elapsed(rut_object_t *timeline);

void rut_timeline_set_elapsed(rut_object_t *timeline, double elapsed);

double rut_timeline_get_progress(rut_object_t *timeline);

void rut_timeline_set_progress(rut_object_t *timeline, double elapsed);

void rut_timeline_set_length(rut_object_t *timeline, float length);

float rut_timeline_get_length(rut_object_t *timeline);

void rut_timeline_set_loop_enabled(rut_object_t *timeline, bool enabled);

bool rut_timeline_get_loop_enabled(rut_object_t *timeline);

void _rut_timeline_progress(rut_timeline_t *timeline, double delta);

#endif /* _RUT_TIMELINE_H_ */
