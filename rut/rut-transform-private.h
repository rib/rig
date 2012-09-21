#ifndef _RUT_TRANSFORM_PRIVATE_H_
#define _RUT_TRANSFORM_PRIVATE_H_

#include <cogl/cogl.h>

#include "rut-object.h"
#include "rut-interfaces.h"

struct _RutTransform
{
  RutObjectProps _parent;
  int ref_count;

  RutGraphableProps graphable;

  CoglMatrix matrix;
};

#endif /* _RUT_TRANSFORM_PRIVATE_H_ */
