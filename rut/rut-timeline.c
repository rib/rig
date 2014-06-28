/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012,2013  Intel Corporation
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

#include <config.h>

#include <cogl/cogl.h>

#include "rut-context.h"
#include "rut-introspectable.h"
#include "rut-timeline.h"

enum {
  RUT_TIMELINE_PROP_LENGTH,
  RUT_TIMELINE_PROP_ELAPSED,
  RUT_TIMELINE_PROP_PROGRESS,
  RUT_TIMELINE_PROP_LOOP,
  RUT_TIMELINE_PROP_RUNNING,
  RUT_TIMELINE_N_PROPS
};

struct _RutTimeline
{
  RutObjectBase _base;

  RutContext *ctx;


  float length;

  GTimer *gtimer;

  double offset;
  int direction;
  bool loop_enabled;
  bool running;
  double elapsed;

  RutIntrospectableProps introspectable;
  RutProperty properties[RUT_TIMELINE_N_PROPS];
};

static RutPropertySpec _rut_timeline_prop_specs[] = {
  {
    .name = "length",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutTimeline, length),
    .setter.float_type = rut_timeline_set_length
  },
  {
    .name = "elapsed",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_DOUBLE,
    .data_offset = offsetof (RutTimeline, elapsed),
    .setter.double_type = rut_timeline_set_elapsed
  },
  {
    .name = "progress",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_DOUBLE,
    .getter.double_type = rut_timeline_get_progress,
    .setter.double_type = rut_timeline_set_progress
  },
  {
    .name = "loop",
    .nick = "Loop",
    .blurb = "Whether the timeline loops",
    .type = RUT_PROPERTY_TYPE_BOOLEAN,
    .getter.boolean_type = rut_timeline_get_loop_enabled,
    .setter.boolean_type = rut_timeline_set_loop_enabled,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
  },
  {
    .name = "running",
    .nick = "Running",
    .blurb = "The timeline progressing over time",
    .type = RUT_PROPERTY_TYPE_BOOLEAN,
    .getter.boolean_type = rut_timeline_get_running,
    .setter.boolean_type = rut_timeline_set_running,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
  },

  { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rut_timeline_free (void *object)
{
  RutTimeline *timeline = object;

  timeline->ctx->timelines =
    g_slist_remove (timeline->ctx->timelines, timeline);
  rut_object_unref (timeline->ctx);

  g_timer_destroy (timeline->gtimer);

  rut_introspectable_destroy (timeline);

  rut_object_free (RutTimeline, timeline);
}

RutType rut_timeline_type;

static void
_rut_timeline_init_type (void)
{

  RutType *type = &rut_timeline_type;
#define TYPE RutTimeline

  rut_type_init (type, C_STRINGIFY (TYPE), _rut_timeline_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_INTROSPECTABLE,
                      offsetof (TYPE, introspectable),
                      NULL); /* no implied vtable */

#undef TYPE
}

RutTimeline *
rut_timeline_new (RutContext *ctx,
                  float length)
{
  RutTimeline *timeline =
    rut_object_alloc0 (RutTimeline, &rut_timeline_type, _rut_timeline_init_type);



  timeline->length = length;
  timeline->gtimer = g_timer_new ();
  timeline->offset = 0;
  timeline->direction = 1;
  timeline->running = TRUE;

  timeline->elapsed = 0;

  rut_introspectable_init (timeline,
                           _rut_timeline_prop_specs,
                           timeline->properties);

  timeline->ctx = rut_object_ref (ctx);
  ctx->timelines = g_slist_prepend (ctx->timelines, timeline);

  return timeline;
}

bool
rut_timeline_get_running (RutObject *object)
{
  RutTimeline *timeline = object;
  return timeline->running;
}

void
rut_timeline_set_running (RutObject *object,
                          bool running)
{
  RutTimeline *timeline = object;

  if (timeline->running == running)
    return;

  timeline->running = running;

  rut_property_dirty (&timeline->ctx->property_ctx,
                      &timeline->properties[RUT_TIMELINE_PROP_RUNNING]);
}

void
rut_timeline_start (RutTimeline *timeline)
{
  g_timer_start (timeline->gtimer);

  rut_timeline_set_elapsed (timeline, 0);

  rut_timeline_set_running (timeline, true);
}

void
rut_timeline_stop (RutTimeline *timeline)
{
  g_timer_stop (timeline->gtimer);
  rut_timeline_set_running (timeline, false);
}

bool
rut_timeline_is_running (RutTimeline *timeline)
{
  return timeline->running;
}

double
rut_timeline_get_elapsed (RutObject *obj)
{
  RutTimeline *timeline = obj;

  return timeline->elapsed;
}

/* Considering an out of range elapsed value should wrap around
 * this returns an equivalent in-range value. */
static double
_rut_timeline_normalize (RutTimeline *timeline,
                         double elapsed)
{
  if (elapsed > timeline->length)
    {
      int n = elapsed / timeline->length;
      elapsed -= n * timeline->length;
    }
  else if (elapsed < 0)
    {
      int n;
      elapsed = -elapsed;
      n = elapsed / timeline->length;
      elapsed -= n * timeline->length;
      elapsed = timeline->length - elapsed;
    }

  return elapsed;
}

/* For any given elapsed value, if the value is out of range it
 * clamps it if the timeline is non looping or normalizes the
 * value to be in-range if the time line is looping.
 *
 * It also returns whether such an elapsed value should result
 * in the timeline being stopped or restarted using the
 * modified elapsed value as an offset.
 *
 * XXX: this was generalized to not directly manipulate the
 * timeline so it could be reused by rut_timeline_set_elapsed
 * but in the end it looks like we could have used the code
 * that directly modified the timeline. Perhaps simplify this
 * code again and just directly modify the timeline.
 */
static double
_rut_timeline_validate_elapsed (RutTimeline *timeline,
                                double elapsed,
                                bool *should_stop,
                                bool *should_restart_with_offset)
{
  *should_stop = FALSE;
  *should_restart_with_offset = FALSE;

  if (elapsed > timeline->length)
    {
      if (timeline->loop_enabled)
        {
          elapsed = _rut_timeline_normalize (timeline, elapsed);
          *should_restart_with_offset = TRUE;
        }
      else
        {
          elapsed = timeline->length;
          *should_stop = TRUE;
        }
    }
  else if (elapsed < 0)
    {
      if (timeline->loop_enabled)
        {
          elapsed = _rut_timeline_normalize (timeline, elapsed);
          *should_restart_with_offset = TRUE;
        }
      else
        {
          elapsed = 0;
          *should_stop = TRUE;
        }
    }

  return elapsed;
}

void
rut_timeline_set_elapsed (RutObject *obj,
                          double elapsed)
{
  RutTimeline *timeline = obj;

  bool should_stop;
  bool should_restart_with_offset;

  elapsed = _rut_timeline_validate_elapsed (timeline, elapsed,
                                            &should_stop,
                                            &should_restart_with_offset);

  if (should_stop)
    g_timer_stop (timeline->gtimer);
  else
    {
      timeline->offset = elapsed;
      g_timer_start (timeline->gtimer);
    }

  if (elapsed != timeline->elapsed)
    {
      timeline->elapsed = elapsed;
      rut_property_dirty (&timeline->ctx->property_ctx,
                          &timeline->properties[RUT_TIMELINE_PROP_ELAPSED]);
      rut_property_dirty (&timeline->ctx->property_ctx,
                          &timeline->properties[RUT_TIMELINE_PROP_PROGRESS]);
    }
}

double
rut_timeline_get_progress (RutObject *obj)
{
  RutTimeline *timeline = obj;

  if (timeline->length)
    return timeline->elapsed / timeline->length;
  else
    return 0;
}

void
rut_timeline_set_progress (RutObject *obj,
                           double progress)
{
  RutTimeline *timeline = obj;

  double elapsed = timeline->length * progress;
  rut_timeline_set_elapsed (timeline, elapsed);
}

void
rut_timeline_set_length (RutObject *obj,
                         float length)
{
  RutTimeline *timeline = obj;

  if (timeline->length == length)
    return;

  timeline->length = length;

  rut_property_dirty (&timeline->ctx->property_ctx,
                      &timeline->properties[RUT_TIMELINE_PROP_LENGTH]);

  rut_timeline_set_elapsed (timeline, timeline->elapsed);
}

float
rut_timeline_get_length (RutObject *obj)
{
  RutTimeline *timeline = obj;

  return timeline->length;
}

void
rut_timeline_set_loop_enabled (RutObject *object, bool enabled)
{
  RutTimeline *timeline = object;

  if (timeline->loop_enabled == enabled)
    return;

  timeline->loop_enabled = enabled;

  rut_property_dirty (&timeline->ctx->property_ctx,
                      &timeline->properties[RUT_TIMELINE_PROP_LOOP]);
}

bool
rut_timeline_get_loop_enabled (RutObject *object)
{
  RutTimeline *timeline = object;
  return timeline->loop_enabled;
}

void
_rut_timeline_update (RutTimeline *timeline)
{
  double elapsed;
  bool should_stop;
  bool should_restart_with_offset;

  if (!timeline->running)
    return;

  elapsed = timeline->offset +
    g_timer_elapsed (timeline->gtimer, NULL) * timeline->direction;

  elapsed = _rut_timeline_validate_elapsed (timeline, elapsed,
                                            &should_stop,
                                            &should_restart_with_offset);

  c_print ("elapsed = %f\n", elapsed);
  if (should_stop)
    g_timer_stop (timeline->gtimer);
  else if (should_restart_with_offset)
    {
      timeline->offset = elapsed;
      g_timer_start (timeline->gtimer);
    }

  if (elapsed != timeline->elapsed)
    {
      timeline->elapsed = elapsed;
      rut_property_dirty (&timeline->ctx->property_ctx,
                          &timeline->properties[RUT_TIMELINE_PROP_ELAPSED]);
      rut_property_dirty (&timeline->ctx->property_ctx,
                          &timeline->properties[RUT_TIMELINE_PROP_PROGRESS]);
    }
}
