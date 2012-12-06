#ifndef _RUT_CAMERA_PRIVATE_H_
#define _RUT_CAMERA_PRIVATE_H_

#include <glib.h>

#include <cogl/cogl.h>

#include "rut-object.h"
#include "rut-interfaces.h"
#include "rut-context.h"
#include "rut-entity.h"

enum {
  RUT_CAMERA_PROP_MODE,
  RUT_CAMERA_PROP_VIEWPORT_X,
  RUT_CAMERA_PROP_VIEWPORT_Y,
  RUT_CAMERA_PROP_VIEWPORT_WIDTH,
  RUT_CAMERA_PROP_VIEWPORT_HEIGHT,
  RUT_CAMERA_PROP_FOV,
  RUT_CAMERA_PROP_NEAR,
  RUT_CAMERA_PROP_FAR,
  RUT_CAMERA_PROP_ZOOM,
  RUT_CAMERA_PROP_BG_COLOR,
  RUT_CAMERA_PROP_FOCAL_DISTANCE,
  RUT_CAMERA_PROP_DEPTH_OF_FIELD,
  RUT_CAMERA_N_PROPS
};

/* TODO: Make internals private */
struct _RutCamera
{
  RutObjectProps _parent;

  RutComponentableProps component;

  int ref_count;

  RutContext *ctx;

  RutCamera *child_camera;

  CoglColor bg_color;
  CoglBool clear_fb;

  float viewport[4];

  float near, far;

  float fov; /* perspective */

  float x1, y1, x2, y2; /* orthographic */

  float zoom;

  float focal_distance;
  float depth_of_field;

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
  unsigned int at_suspend_transform_age;

  CoglFramebuffer *fb;

  RutGraphableProps graphable;

  CoglMatrix input_transform;
  GList *input_regions;

  RutSimpleIntrospectableProps introspectable;
  RutProperty properties[RUT_CAMERA_N_PROPS];

  unsigned int orthographic:1;
  unsigned int in_frame:1;
  unsigned int suspended:1;
};

#endif /* _RUT_CAMERA_PRIVATE_H_ */
