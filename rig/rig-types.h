#ifndef _RIG_TYPES_H_
#define _RIG_TYPES_H_

#include "rig-type.h"

typedef struct _RigCamera RigCamera;
#define RIG_CAMERA(X) ((RigCamera *)X)
extern RigType rig_camera_type;

typedef struct _RigTransform RigTransform;
#define RIG_TRANSFORM(X) ((RigTransform *)X)
RigType rig_transform_type;

typedef struct _RigBox
{
  float x1, y1, x2, y2;
} RigBox;

typedef struct _RigVector3
{
  float x, y, z;
} RigVector3;

typedef enum _RigCullResult
{
  RIG_CULL_RESULT_IN,
  RIG_CULL_RESULT_OUT,
  RIG_CULL_RESULT_PARTIAL
} RigCullResult;

#endif /* _RIG_TYPES_H_ */
