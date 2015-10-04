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

#pragma once

extern rut_type_t rig_timeline_type;

typedef struct _rig_timeline_t rig_timeline_t;

rig_timeline_t *rig_timeline_new(rig_engine_t *engine, float length);

void rig_timeline_start(rig_timeline_t *timeline);

void rig_timeline_stop(rig_timeline_t *timeline);

bool rig_timeline_get_running(rut_object_t *timeline);

void rig_timeline_set_running(rut_object_t *timeline, bool running);

bool rig_timeline_is_running(rig_timeline_t *timeline);

double rig_timeline_get_elapsed(rut_object_t *timeline);

void rig_timeline_set_elapsed(rut_object_t *timeline, double elapsed);

double rig_timeline_get_progress(rut_object_t *timeline);

void rig_timeline_set_progress(rut_object_t *timeline, double elapsed);

void rig_timeline_set_length(rut_object_t *timeline, float length);

float rig_timeline_get_length(rut_object_t *timeline);

void rig_timeline_set_loop_enabled(rut_object_t *timeline, bool enabled);

bool rig_timeline_get_loop_enabled(rut_object_t *timeline);

void _rig_timeline_progress(rig_timeline_t *timeline, double delta);
