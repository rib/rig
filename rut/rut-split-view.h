#ifndef _RUT_SPLIT_VIEW_H_
#define _RUT_SPLIT_VIEW_H_

#include <cogl/cogl.h>

#include "rut.h"

extern RutType rut_split_view_type;
typedef struct _RutSplitView RutSplitView;
#define RUT_SPLIT_VIEW(x) ((RutSplitView *) x)

typedef enum _RutSplitViewSplit
{
  RUT_SPLIT_VIEW_SPLIT_NONE,
  RUT_SPLIT_VIEW_SPLIT_VERTICAL = 1,
  RUT_SPLIT_VIEW_SPLIT_HORIZONTAL
} RutSplitViewSplit;

typedef enum _RutSplitViewJoin
{
  RUT_SPLIT_VIEW_JOIN_INTO_O,
  RUT_SPLIT_VIEW_JOIN_INTO_1
} RutSplitViewJoin;

RutSplitView *
rut_split_view_new (RutContext *ctx,
                    RutSplitViewSplit split,
                    float width,
                    float height,
                    ...);

void
rut_split_view_set_size (RutSplitView *split_view,
                         float width,
                         float height);
void
rut_split_view_set_width (RutSplitView *split_view,
                          float width);

void
rut_split_view_set_height (RutSplitView *split_view,
                           float height);

void
rut_split_view_get_size (void *object,
                         float *width,
                         float *height);

void
rut_split_view_split (RutSplitView *split_view, RutSplitViewSplit split);

void
rut_split_view_set_split_offset (RutSplitView *split_view,
                                 int offset);

void
rut_split_view_set_child0 (RutSplitView *split_view,
                           RutObject *child0);

void
rut_split_view_set_child1 (RutSplitView *split_view,
                           RutObject *child1);

#endif /* _RUT_SPLIT_VIEW_H_ */
