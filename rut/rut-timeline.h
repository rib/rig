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

extern RutType rut_timeline_type;

typedef struct _RutTimeline RutTimeline;

RutTimeline *
rut_timeline_new (RutContext *ctx,
                  float length);

void
rut_timeline_start (RutTimeline *timeline);

void
rut_timeline_stop (RutTimeline *timeline);

bool
rut_timeline_get_running (RutObject *timeline);

void
rut_timeline_set_running (RutObject *timeline,
                          bool running);

bool
rut_timeline_is_running (RutTimeline *timeline);

double
rut_timeline_get_elapsed (RutObject *timeline);

void
rut_timeline_set_elapsed (RutObject *timeline,
                          double elapsed);

double
rut_timeline_get_progress (RutObject *timeline);

void
rut_timeline_set_progress (RutObject *timeline,
                           double elapsed);

void
rut_timeline_set_length (RutObject *timeline,
                         float length);

float
rut_timeline_get_length (RutObject *timeline);

void
rut_timeline_set_loop_enabled (RutObject *timeline, bool enabled);

bool
rut_timeline_get_loop_enabled (RutObject *timeline);

/* PRIVATE */
void
_rut_timeline_update (RutTimeline *timeline);

#endif /* _RUT_TIMELINE_H_ */
