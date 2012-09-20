#include <cogl/cogl.h>

#include "rig-context.h"
#include "rig-interfaces.h"
#include "rig-timeline.h"

enum {
  RIG_TIMELINE_PROP_ELAPSED,
  RIG_TIMELINE_PROP_PROGRESS,
  RIG_TIMELINE_N_PROPS
};

struct _RigTimeline
{
  RigObjectProps _parent;

  RigContext *ctx;

  int ref_count;

  float length;

  GTimer *gtimer;

  double offset;
  int direction;
  CoglBool loop_enabled;
  CoglBool running;
  double elapsed;

  RigSimpleIntrospectableProps introspectable;
  RigProperty properties[RIG_TIMELINE_N_PROPS];
};

static RigPropertySpec _rig_timeline_prop_specs[] = {
  {
    .name = "elapsed",
    .type = RIG_PROPERTY_TYPE_DOUBLE,
    .data_offset = offsetof (RigTimeline, elapsed),
    .setter = rig_timeline_set_elapsed
  },
  {
    .name = "progress",
    .type = RIG_PROPERTY_TYPE_DOUBLE,
    .getter = rig_timeline_get_progress,
    .setter = rig_timeline_set_progress
  },
  { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rig_timeline_free (void *object)
{
  RigTimeline *timeline = object;

  timeline->ctx->timelines =
    g_slist_remove (timeline->ctx->timelines, timeline);
  rig_ref_countable_unref (timeline->ctx);

  g_timer_destroy (timeline->gtimer);

  rig_simple_introspectable_destroy (RIG_OBJECT (timeline));

  g_slice_free (RigTimeline, timeline);
}

static RigRefCountableVTable _rig_timeline_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_timeline_free
};

static RigIntrospectableVTable _rig_timeline_introspectable_vtable = {
  rig_simple_introspectable_lookup_property,
  rig_simple_introspectable_foreach_property
};

static RigType _rig_timeline_type;

void
_rig_timeline_init_type (void)
{
  rig_type_init (&_rig_timeline_type);
  rig_type_add_interface (&_rig_timeline_type,
                          RIG_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigTimeline, ref_count),
                          &_rig_timeline_ref_countable_vtable);
  rig_type_add_interface (&_rig_timeline_type,
                          RIG_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rig_timeline_introspectable_vtable);
  rig_type_add_interface (&_rig_timeline_type,
                          RIG_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RigTimeline, introspectable),
                          NULL); /* no implied vtable */
}

RigTimeline *
rig_timeline_new (RigContext *ctx,
                  float length)
{
  RigTimeline *timeline = g_slice_new0 (RigTimeline);

  rig_object_init (&timeline->_parent, &_rig_timeline_type);

  timeline->ref_count = 1;

  timeline->length = length;
  timeline->gtimer = g_timer_new ();
  timeline->offset = 0;
  timeline->direction = 1;
  timeline->running = TRUE;

  timeline->elapsed = 0;

  rig_simple_introspectable_init (RIG_OBJECT (timeline),
                                  _rig_timeline_prop_specs,
                                  timeline->properties);

  timeline->ctx = rig_ref_countable_ref (ctx);
  ctx->timelines = g_slist_prepend (ctx->timelines, timeline);

  return timeline;
}

void
rig_timeline_start (RigTimeline *timeline)
{
  g_timer_start (timeline->gtimer);

  rig_timeline_set_elapsed (timeline, 0);

  if (!timeline->running)
    timeline->running = TRUE;
}

void
rig_timeline_stop (RigTimeline *timeline)
{
  g_timer_stop (timeline->gtimer);
  timeline->running = FALSE;
}

CoglBool
rig_timeline_is_running (RigTimeline *timeline)
{
  return timeline->running;
}

double
rig_timeline_get_elapsed (RigTimeline *timeline)
{
  return timeline->elapsed;
}

/* Considering an out of range elapsed value should wrap around
 * this returns an equivalent in-range value. */
static double
_rig_timeline_normalize (RigTimeline *timeline,
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
 * timeline so it could be reused by rig_timeline_set_elapsed
 * but in the end it looks like we could have used the code
 * that directly modified the timeline. Perhaps simplify this
 * code again and just directly modify the timeline.
 */
static double
_rig_timeline_validate_elapsed (RigTimeline *timeline,
                                double elapsed,
                                CoglBool *should_stop,
                                CoglBool *should_restart_with_offset)
{
  *should_stop = FALSE;
  *should_restart_with_offset = FALSE;

  if (elapsed > timeline->length)
    {
      if (timeline->loop_enabled)
        {
          elapsed = _rig_timeline_normalize (timeline, elapsed);
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
          elapsed = _rig_timeline_normalize (timeline, elapsed);
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
rig_timeline_set_elapsed (RigTimeline *timeline,
                          double elapsed)
{
  CoglBool should_stop;
  CoglBool should_restart_with_offset;

  elapsed = _rig_timeline_validate_elapsed (timeline, elapsed,
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
      rig_property_dirty (&timeline->ctx->property_ctx,
                          &timeline->properties[RIG_TIMELINE_PROP_ELAPSED]);
      rig_property_dirty (&timeline->ctx->property_ctx,
                          &timeline->properties[RIG_TIMELINE_PROP_PROGRESS]);
    }
}

double
rig_timeline_get_progress (RigTimeline *timeline)
{
  return timeline->elapsed / timeline->length;
}

void
rig_timeline_set_progress (RigTimeline *timeline,
                           double progress)
{
  double elapsed = timeline->length * progress;
  rig_timeline_set_elapsed (timeline, elapsed);
}

void
rig_timeline_set_loop_enabled (RigTimeline *timeline, CoglBool enabled)
{
  timeline->loop_enabled = enabled;
}

void
_rig_timeline_update (RigTimeline *timeline)
{
  double elapsed;
  CoglBool should_stop;
  CoglBool should_restart_with_offset;

  if (!timeline->running)
    return;

  elapsed = timeline->offset +
    g_timer_elapsed (timeline->gtimer, NULL) * timeline->direction;

  elapsed = _rig_timeline_validate_elapsed (timeline, elapsed,
                                            &should_stop,
                                            &should_restart_with_offset);

  g_print ("elapsed = %f\n", elapsed);
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
      rig_property_dirty (&timeline->ctx->property_ctx,
                          &timeline->properties[RIG_TIMELINE_PROP_ELAPSED]);
      rig_property_dirty (&timeline->ctx->property_ctx,
                          &timeline->properties[RIG_TIMELINE_PROP_PROGRESS]);
    }
}
