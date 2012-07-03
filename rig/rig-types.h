#ifndef _RIG_TYPES_H_
#define _RIG_TYPES_H_

#include "rig-type.h"

typedef struct _RigContext RigContext;
#define RIG_CONTEXT(X) ((RigContext *)X)
extern RigType rig_context_type;

typedef struct _RigShell RigShell;
extern RigType rig_shell_type;
#define RIG_SHELL(X) ((RigShell *)X)

typedef struct _RigCamera RigCamera;
#define RIG_CAMERA(X) ((RigCamera *)X)
extern RigType rig_camera_type;

typedef struct _RigTransform RigTransform;
#define RIG_TRANSFORM(X) ((RigTransform *)X)
RigType rig_transform_type;

typedef enum
{
  RIG_PROJECTION_PERSPECTIVE,
  RIG_PROJECTION_ORTHOGRAPHIC
} RigProjection;

typedef struct _RigBox
{
  float x1, y1, x2, y2;
} RigBox;

typedef struct _RigRectangleInt
{
  int x;
  int y;
  int width;
  int height;
} RigRectangleInt;

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

typedef enum _RigAxis
{
  RIG_AXIS_X,
  RIG_AXIS_Y,
  RIG_AXIS_Z
} RigAxis;

typedef struct _RigColor
{
  float red, green, blue, alpha;
} RigColor;

#endif /* _RIG_TYPES_H_ */
