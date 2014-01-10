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
