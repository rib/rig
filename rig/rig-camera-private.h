#ifndef _RIG_CAMERA_PRIVATE_H_
#define _RIG_CAMERA_PRIVATE_H_

#include <glib.h>

#include <cogl/cogl.h>

#include "rig-object.h"
#include "rig-interfaces.h"
#include "rig.h"

/* TODO: Make internals private */
struct _RigCamera
{
  RigObjectProps _parent;

  int ref_count;

  RigContext *ctx;

  float viewport[4];

  CoglMatrix projection;
  CoglMatrix inverse_projection;
  CoglBool inverse_projection_cached;

  CoglMatrix view;
  CoglMatrix inverse_view;
  CoglBool inverse_view_cached;

  CoglFramebuffer *fb;

  int age;

  RigGraphableProps graphable;

  CoglMatrix input_transform;
  GList *input_regions;

  GList *input_callbacks;

  int frame;
  GTimer *timer;
};

#endif /* _RIG_CAMERA_PRIVATE_H_ */
