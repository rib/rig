#ifndef _RIG_TRANSFORM_PRIVATE_H_
#define _RIG_TRANSFORM_PRIVATE_H_

#include <cogl/cogl.h>

#include "rig-object.h"
#include "rig-interfaces.h"

struct _RigTransform
{
  RigObjectProps _parent;
  int ref_count;

  RigGraphableProps graphable;

  CoglMatrix matrix;
};

#endif /* _RIG_TRANSFORM_PRIVATE_H_ */
