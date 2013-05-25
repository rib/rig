#ifndef __RIG_CODEGEN_H__
#define __RIG_CODEGEN_H__

//#define NULL ((void *)0)

typedef struct _RutType RutType;

typedef void RutObject;

typedef struct _RutContext RutContext;

typedef struct _RutUIEnum RutUIEnum;

typedef struct _RutPropertyContext RutPropertyContext;

typedef void RutAsset;
typedef int RutAssetType;

typedef struct _RutMemoryStack RutMemoryStack;

/* We want to minimize the number of headers we include during the
 * runtime compilation of UI logic and so we just duplicate the
 * GLib and Cogl typedefs that are required in rut-property-bare.h...
 */

typedef struct _GSList GSList;
#define g_return_if_fail(X)
#define g_return_val_if_fail(X, Y)

typedef int CoglBool;

typedef struct _CoglColor
{
  float r, g, b, a;
} CoglColor;

typedef struct _CoglQuaternion
{
  float w, x, y, z;
} CoglQuaternion;

#endif /* __RIG_CODEGEN_H__ */
