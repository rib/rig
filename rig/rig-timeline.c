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

#include <rig-config.h>

#include <cglib/cglib.h>

#include <rut.h>

#include "rig-engine.h"
#include "rig-timeline.h"

enum {
    RUT_TIMELINE_PROP_LENGTH,
    RUT_TIMELINE_PROP_ELAPSED,
    RUT_TIMELINE_PROP_PROGRESS,
    RUT_TIMELINE_PROP_LOOP,
    RUT_TIMELINE_PROP_RUNNING,
    RUT_TIMELINE_N_PROPS
};

struct _rig_timeline_t {
    rut_object_base_t _base;

    rig_engine_t *engine;

    double length;

    int direction;
    bool loop_enabled;
    bool running;
    double elapsed;

    rut_introspectable_props_t introspectable;
    rut_property_t properties[RUT_TIMELINE_N_PROPS];
};

static rut_property_spec_t _rig_timeline_prop_specs[] = {
    { .name = "length",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rig_timeline_t, length),
      .setter.float_type = rig_timeline_set_length },
    { .name = "elapsed",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_DOUBLE,
      .data_offset = offsetof(rig_timeline_t, elapsed),
      .setter.double_type = rig_timeline_set_elapsed },
    { .name = "progress",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_DOUBLE,
      .getter.double_type = rig_timeline_get_progress,
      .setter.double_type = rig_timeline_set_progress },
    { .name = "loop",
      .nick = "Loop",
      .blurb = "Whether the timeline loops",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .getter.boolean_type = rig_timeline_get_loop_enabled,
      .setter.boolean_type = rig_timeline_set_loop_enabled,
      .flags = RUT_PROPERTY_FLAG_READWRITE, },
    { .name = "running",
      .nick = "Running",
      .blurb = "The timeline progressing over time",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .getter.boolean_type = rig_timeline_get_running,
      .setter.boolean_type = rig_timeline_set_running,
      .flags = RUT_PROPERTY_FLAG_READWRITE, },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rig_timeline_free(void *object)
{
    rig_timeline_t *timeline = object;

    timeline->engine->timelines =
        c_sllist_remove(timeline->engine->timelines, timeline);
    rut_object_unref(timeline->engine);

    rut_introspectable_destroy(timeline);

    rut_object_free(rig_timeline_t, timeline);
}

rut_type_t rig_timeline_type;

static void
_rig_timeline_init_type(void)
{

    rut_type_t *type = &rig_timeline_type;
#define TYPE rig_timeline_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_timeline_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */

#undef TYPE
}

rig_timeline_t *
rig_timeline_new(rig_engine_t *engine, float length)
{
    rig_timeline_t *timeline = rut_object_alloc0(
        rig_timeline_t, &rig_timeline_type, _rig_timeline_init_type);

    timeline->length = length;
    timeline->direction = 1;
    timeline->running = true;

    timeline->elapsed = 0;

    rut_introspectable_init(
        timeline, _rig_timeline_prop_specs, timeline->properties);

    timeline->engine = rut_object_ref(engine);
    engine->timelines = c_sllist_prepend(engine->timelines, timeline);

    return timeline;
}

bool
rig_timeline_get_running(rut_object_t *object)
{
    rig_timeline_t *timeline = object;
    return timeline->running;
}

void
rig_timeline_set_running(rut_object_t *object, bool running)
{
    rig_timeline_t *timeline = object;

    if (timeline->running == running)
        return;

    timeline->running = running;

    rut_property_dirty(timeline->engine->property_ctx,
                       &timeline->properties[RUT_TIMELINE_PROP_RUNNING]);
}

void
rig_timeline_start(rig_timeline_t *timeline)
{
    rig_timeline_set_elapsed(timeline, 0);

    rig_timeline_set_running(timeline, true);
}

void
rig_timeline_stop(rig_timeline_t *timeline)
{
    rig_timeline_set_running(timeline, false);
}

bool
rig_timeline_is_running(rig_timeline_t *timeline)
{
    return timeline->running;
}

double
rig_timeline_get_elapsed(rut_object_t *obj)
{
    rig_timeline_t *timeline = obj;

    return timeline->elapsed;
}

/* Considering an out of range elapsed value should wrap around
 * this returns an equivalent in-range value. */
static double
_rig_timeline_normalize(rig_timeline_t *timeline, double elapsed)
{
    if (elapsed > timeline->length) {
        int n = elapsed / timeline->length;
        elapsed -= n * timeline->length;
    } else if (elapsed < 0) {
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
 * in the timeline being stopped
 *
 * XXX: this was generalized to not directly manipulate the
 * timeline so it could be reused by rig_timeline_set_elapsed
 * but in the end it looks like we could have used the code
 * that directly modified the timeline. Perhaps simplify this
 * code again and just directly modify the timeline.
 */
static double
_rig_timeline_validate_elapsed(rig_timeline_t *timeline,
                               double elapsed,
                               bool *should_stop)
{
    *should_stop = false;

    if (elapsed > timeline->length) {
        if (timeline->loop_enabled) {
            elapsed = _rig_timeline_normalize(timeline, elapsed);
        } else {
            elapsed = timeline->length;
            *should_stop = true;
        }
    } else if (elapsed < 0) {
        if (timeline->loop_enabled) {
            elapsed = _rig_timeline_normalize(timeline, elapsed);
        } else {
            elapsed = 0;
            *should_stop = true;
        }
    }

    return elapsed;
}

void
rig_timeline_set_elapsed(rut_object_t *obj, double elapsed)
{
    rig_timeline_t *timeline = obj;

    bool should_stop;

    elapsed = _rig_timeline_validate_elapsed(timeline, elapsed, &should_stop);

    if (should_stop) {
        rig_timeline_set_running(timeline, false);
    }

    if (elapsed != timeline->elapsed) {
        timeline->elapsed = elapsed;
        rut_property_dirty(timeline->engine->property_ctx,
                           &timeline->properties[RUT_TIMELINE_PROP_ELAPSED]);
        rut_property_dirty(timeline->engine->property_ctx,
                           &timeline->properties[RUT_TIMELINE_PROP_PROGRESS]);
    }
}

double
rig_timeline_get_progress(rut_object_t *obj)
{
    rig_timeline_t *timeline = obj;

    if (timeline->length)
        return timeline->elapsed / timeline->length;
    else
        return 0;
}

void
rig_timeline_set_progress(rut_object_t *obj, double progress)
{
    rig_timeline_t *timeline = obj;

    double elapsed = timeline->length * progress;
    rig_timeline_set_elapsed(timeline, elapsed);
}

void
rig_timeline_set_length(rut_object_t *obj, float length)
{
    rig_timeline_t *timeline = obj;

    if (timeline->length == length)
        return;

    timeline->length = length;

    rut_property_dirty(timeline->engine->property_ctx,
                       &timeline->properties[RUT_TIMELINE_PROP_LENGTH]);

    rig_timeline_set_elapsed(timeline, timeline->elapsed);
}

float
rig_timeline_get_length(rut_object_t *obj)
{
    rig_timeline_t *timeline = obj;

    return timeline->length;
}

void
rig_timeline_set_loop_enabled(rut_object_t *object, bool enabled)
{
    rig_timeline_t *timeline = object;

    if (timeline->loop_enabled == enabled)
        return;

    timeline->loop_enabled = enabled;

    rut_property_dirty(timeline->engine->property_ctx,
                       &timeline->properties[RUT_TIMELINE_PROP_LOOP]);
}

bool
rig_timeline_get_loop_enabled(rut_object_t *object)
{
    rig_timeline_t *timeline = object;
    return timeline->loop_enabled;
}

void
_rig_timeline_progress(rig_timeline_t *timeline, double delta)
{
    double elapsed;

    if (!timeline->running)
        return;

    elapsed = rig_timeline_get_elapsed(timeline);
    elapsed += (delta * timeline->direction);

    rig_timeline_set_elapsed(timeline, elapsed);
}
