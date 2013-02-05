#ifndef _RIG_SPLIT_VIEW_H_
#define _RIG_SPLIT_VIEW_H_

#include <cogl/cogl.h>

#include "rut.h"

extern RutType rig_split_view_type;
typedef struct _RigSplitView RigSplitView;
#define RIG_SPLIT_VIEW(x) ((RigSplitView *) x)

typedef enum _RigSplitViewSplit
{
  RIG_SPLIT_VIEW_SPLIT_NONE,
  RIG_SPLIT_VIEW_SPLIT_VERTICAL = 1,
  RIG_SPLIT_VIEW_SPLIT_HORIZONTAL
} RigSplitViewSplit;

typedef enum _RigSplitViewJoin
{
  RIG_SPLIT_VIEW_JOIN_INTO_O,
  RIG_SPLIT_VIEW_JOIN_INTO_1
} RigSplitViewJoin;

RigSplitView *
rig_split_view_new (RigData *data,
                    RigSplitViewSplit split,
                    float width,
                    float height);

void
rig_split_view_set_size (RigSplitView *split_view,
                         float width,
                         float height);
void
rig_split_view_set_width (RutObject *split_view,
                          float width);

void
rig_split_view_set_height (RutObject *split_view,
                           float height);

void
rig_split_view_get_size (void *object,
                         float *width,
                         float *height);

void
rig_split_view_split (RigSplitView *split_view, RigSplitViewSplit split);

void
rig_split_view_set_split_offset (RigSplitView *split_view,
                                 int offset);

void
rig_split_view_set_child0 (RigSplitView *split_view,
                           RutObject *child0);

void
rig_split_view_set_child1 (RigSplitView *split_view,
                           RutObject *child1);

#endif /* _RIG_SPLIT_VIEW_H_ */
