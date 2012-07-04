#ifndef _RIG_CAMERA_PRIVATE_H_
#define _RIG_CAMERA_PRIVATE_H_

#include <glib.h>

#include <cogl/cogl.h>

#include "rig-object.h"
#include "rig-interfaces.h"
#include "rig-context.h"
#include "rig-entity.h"

enum {
  RIG_CAMERA_PROP_MODE,
  RIG_CAMERA_PROP_FOV,
  RIG_CAMERA_PROP_NEAR,
  RIG_CAMERA_PROP_FAR,
  RIG_CAMERA_PROP_BG_COLOR,
  RIG_CAMERA_N_PROPS
};

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

  RigSimpleIntrospectableProps introspectable;
  RigProperty properties[RIG_CAMERA_N_PROPS];

  unsigned int orthographic:1;
};

#endif /* _RIG_CAMERA_PRIVATE_H_ */
