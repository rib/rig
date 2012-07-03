#ifndef _RIG_CAMERA_PRIVATE_H_
#define _RIG_CAMERA_PRIVATE_H_

#include <glib.h>

#include <cogl/cogl.h>

#include "rig-object.h"
#include "rig-interfaces.h"
#include "rig-context.h"
#include "rig-entity.h"

/* TODO: Make internals private */
struct _RigCamera
{
  RigObjectProps _parent;

  RigComponentableProps component;

  int ref_count;

  RigContext *ctx;

  CoglColor bg_color;
  CoglBool clear_fb;

  float viewport[4];

  float near, far;

  float fov; /* perspective */

  float x1, y1, x2, y2; /* orthographic */

  CoglMatrix projection;
  unsigned int projection_age;
  unsigned int projection_cache_age;

  CoglMatrix inverse_projection;
  unsigned int inverse_projection_age;

  CoglMatrix view;
  unsigned int view_age;

  CoglMatrix inverse_view;
  unsigned int inverse_view_age;

  unsigned int transform_age;

  CoglFramebuffer *fb;

  RigGraphableProps graphable;

  CoglMatrix input_transform;
  GList *input_regions;

  GList *input_callbacks;

  int frame;
  GTimer *timer;

  unsigned int orthographic:1;
};

#endif /* _RIG_CAMERA_PRIVATE_H_ */
