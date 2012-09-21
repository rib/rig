#ifndef _RUT_TYPES_H_
#define _RUT_TYPES_H_

#include "rut-type.h"

typedef struct _RutContext RutContext;
#define RUT_CONTEXT(X) ((RutContext *)X)
extern RutType rut_context_type;

typedef struct _RutShell RutShell;
extern RutType rut_shell_type;
#define RUT_SHELL(X) ((RutShell *)X)

typedef struct _RutCamera RutCamera;
#define RUT_CAMERA(X) ((RutCamera *)X)
extern RutType rut_camera_type;

typedef struct _RutTransform RutTransform;
#define RUT_TRANSFORM(X) ((RutTransform *)X)
RutType rut_transform_type;

typedef struct _RutUIEnumValue
{
  int value;
  const char *nick;
  const char *blurb;
} RutUIEnumValue;

typedef struct _RutUIEnum
{
  const char *nick;
  const char *blurb;
  RutUIEnumValue values[];
} RutUIEnum;

typedef enum
{
  RUT_PROJECTION_PERSPECTIVE,
  RUT_PROJECTION_ORTHOGRAPHIC
} RutProjection;

/* XXX: Update this in rig.c if RutProjection is changed! */
extern RutUIEnum _rut_projection_ui_enum;

typedef struct _RutBox
{
  float x1, y1, x2, y2;
} RutBox;

typedef struct _RutRectangleInt
{
  int x;
  int y;
  int width;
  int height;
} RutRectangleInt;

typedef struct _RutVector3
{
  float x, y, z;
} RutVector3;

typedef enum _RutCullResult
{
  RUT_CULL_RESULT_IN,
  RUT_CULL_RESULT_OUT,
  RUT_CULL_RESULT_PARTIAL
} RutCullResult;

typedef enum _RutAxis
{
  RUT_AXIS_X,
  RUT_AXIS_Y,
  RUT_AXIS_Z
} RutAxis;

typedef struct _RutColor
{
  float red, green, blue, alpha;
} RutColor;

#endif /* _RUT_TYPES_H_ */
