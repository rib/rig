#include <cogl/cogl.h>

#include "rut-context.h"
#include "rut-interfaces.h"
#include "rut-timeline.h"

enum {
  RUT_TIMELINE_PROP_ELAPSED,
  RUT_TIMELINE_PROP_PROGRESS,
  RUT_TIMELINE_N_PROPS
};

struct _RutTimeline
{
  RutObjectProps _parent;

  RutContext *ctx;

  int ref_count;

  float length;

  GTimer *gtimer;

  double offset;
  int direction;
  CoglBool loop_enabled;
  CoglBool running;
  double elapsed;

  RutSimpleIntrospectableProps introspectable;
  RutProperty properties[RUT_TIMELINE_N_PROPS];
};

static RutPropertySpec _rut_timeline_prop_specs[] = {
  {
    .name = "elapsed",
    .type = RUT_PROPERTY_TYPE_DOUBLE,
    .data_offset = offsetof (RutTimeline, elapsed),
    .setter = rut_timeline_set_elapsed
  },
  {
    .name = "progress",
    .type = RUT_PROPERTY_TYPE_DOUBLE,
    .getter = rut_timeline_get_progress,
    .setter = rut_timeline_set_progress
  },
  { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rut_timeline_free (void *object)
{
  RutTimeline *timeline = object;

  timeline->ctx->timelines =
    g_slist_remove (timeline->ctx->timelines, timeline);
  rut_refable_unref (timeline->ctx);

  g_timer_destroy (timeline->gtimer);

  rut_simple_introspectable_destroy (RUT_OBJECT (timeline));

  g_slice_free (RutTimeline, timeline);
}

static RutRefCountableVTable _rut_timeline_ref_countable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_timeline_free
};

static RutIntrospectableVTable _rut_timeline_introspectable_vtable = {
  rut_simple_introspectable_lookup_property,
  rut_simple_introspectable_foreach_property
};

static RutType _rut_timeline_type;

void
_rut_timeline_init_type (void)
{
  rut_type_init (&_rut_timeline_type);
  rut_type_add_interface (&_rut_timeline_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutTimeline, ref_count),
                          &_rut_timeline_ref_countable_vtable);
  rut_type_add_interface (&_rut_timeline_type,
                          RUT_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rut_timeline_introspectable_vtable);
  rut_type_add_interface (&_rut_timeline_type,
                          RUT_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RutTimeline, introspectable),
                          NULL); /* no implied vtable */
}

RutTimeline *
rut_timeline_new (RutContext *ctx,
                  float length)
{
  RutTimeline *timeline = g_slice_new0 (RutTimeline);

  rut_object_init (&timeline->_parent, &_rut_timeline_type);

  timeline->ref_count = 1;

  timeline->length = length;
  timeline->gtimer = g_timer_new ();
  timeline->offset = 0;
  timeline->direction = 1;
  timeline->running = TRUE;

  timeline->elapsed = 0;

  rut_simple_introspectable_init (RUT_OBJECT (timeline),
                                  _rut_timeline_prop_specs,
                                  timeline->properties);

  timeline->ctx = rut_refable_ref (ctx);
  ctx->timelines = g_slist_prepend (ctx->timelines, timeline);

  return timeline;
}

void
rut_timeline_start (RutTimeline *timeline)
{
  g_timer_start (timeline->gtimer);

  rut_timeline_set_elapsed (timeline, 0);

  if (!timeline->running)
    timeline->running = TRUE;
}

void
rut_timeline_stop (RutTimeline *timeline)
{
  g_timer_stop (timeline->gtimer);
  timeline->running = FALSE;
}

CoglBool
rut_timeline_is_running (RutTimeline *timeline)
{
  return timeline->running;
}

double
rut_timeline_get_elapsed (RutTimeline *timeline)
{
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
                                CoglBool *should_stop,
                                CoglBool *should_restart_with_offset)
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
rut_timeline_set_elapsed (RutTimeline *timeline,
                          double elapsed)
{
  CoglBool should_stop;
  CoglBool should_restart_with_offset;

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
rut_timeline_get_progress (RutTimeline *timeline)
{
  return timeline->elapsed / timeline->length;
}

void
rut_timeline_set_progress (RutTimeline *timeline,
                           double progress)
{
  double elapsed = timeline->length * progress;
  rut_timeline_set_elapsed (timeline, elapsed);
}

void
rut_timeline_set_loop_enabled (RutTimeline *timeline, CoglBool enabled)
{
  timeline->loop_enabled = enabled;
}

void
_rut_timeline_update (RutTimeline *timeline)
{
  double elapsed;
  CoglBool should_stop;
  CoglBool should_restart_with_offset;

  if (!timeline->running)
    return;

  elapsed = timeline->offset +
    g_timer_elapsed (timeline->gtimer, NULL) * timeline->direction;

  elapsed = _rut_timeline_validate_elapsed (timeline, elapsed,
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
      rut_property_dirty (&timeline->ctx->property_ctx,
                          &timeline->properties[RUT_TIMELINE_PROP_ELAPSED]);
      rut_property_dirty (&timeline->ctx->property_ctx,
                          &timeline->properties[RUT_TIMELINE_PROP_PROGRESS]);
    }
}
