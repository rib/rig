#ifndef _RIG_TIMELINE_H_
#define _RIG_TIMELINE_H_

void
_rig_timeline_init_type (void);

typedef struct _RigTimeline RigTimeline;
#define RIG_TIMELINE(x) ((RigTimeline *)X)

RigTimeline *
rig_timeline_new (RigContext *ctx,
                  float length);

void
rig_timeline_start (RigTimeline *timeline);

void
rig_timeline_stop (RigTimeline *timeline);

CoglBool
rig_timeline_is_running (RigTimeline *timeline);

double
rig_timeline_get_elapsed (RigTimeline *timeline);

void
rig_timeline_set_elapsed (RigTimeline *timeline,
                          double elapsed);

double
rig_timeline_get_progress (RigTimeline *timeline);

void
rig_timeline_set_progress (RigTimeline *timeline,
                           double elapsed);

void
rig_timeline_set_loop_enabled (RigTimeline *timeline, CoglBool enabled);

/* PRIVATE */
void
_rig_timeline_update (RigTimeline *timeline);

#endif /* _RIG_TIMELINE_H_ */
