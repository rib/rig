#ifndef _RUT_TYPES_H_
#define _RUT_TYPES_H_

#include "rut-type.h"

typedef struct _RutContext RutContext;
extern RutType rut_context_type;

typedef struct _RutShell RutShell;
extern RutType rut_shell_type;

typedef struct _RutCamera RutCamera;
#define RUT_CAMERA(X) ((RutCamera *)X)
extern RutType rut_camera_type;

typedef struct _RutInputRegion RutInputRegion;
extern RutType rut_input_region_type;

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

typedef enum _RutAssetType {
  RUT_ASSET_TYPE_BUILTIN,
  RUT_ASSET_TYPE_TEXTURE,
  RUT_ASSET_TYPE_NORMAL_MAP,
  RUT_ASSET_TYPE_ALPHA_MASK,
  RUT_ASSET_TYPE_PLY_MODEL, /* TODO: rename to _TYPE_MESH */
} RutAssetType;

typedef struct _RutPreferredSize
{
  float natural_size;
  float minimum_size;
} RutPreferredSize;

#endif /* _RUT_TYPES_H_ */
