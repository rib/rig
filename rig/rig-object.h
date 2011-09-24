#ifndef _RIG_OBJECT_H_
#define _RIG_OBJECT_H_

#include "rig-type.h"

/* We largely give up having compile time type safety for RigObjects since
 * the type system is conceptually intended to be dynamic and for most
 * apis dealing with RigObjects, they care about interfaces more than
 * about object types and so using a strong types would require us to
 * always be casting objects between different interface types which
 * would make code more verbose and by explicitly casting we'd lose
 * the compile time type safety anyway.
 */
typedef void RigObject;
#define RIG_OBJECT(X) ((RigObject *)X)

/* For struct inheritance purposes this struct can be placed as the
 * first member of another struct so that it can be used with the
 * Rig type system.
 *
 * The struct should be initialized using rig_object_init() when
 * constructing an instance that inherits from this.
 */
typedef struct _RigObjectProps
{
  RigType *type;
} RigObjectProps;

void
rig_object_init (RigObjectProps *object_properties, RigType *type);

static inline const RigType *
rig_object_get_type (RigObject *object)
{
  RigObjectProps *obj = object;
  return obj->type;
}

static inline void *
rig_object_get_properties (RigObject *object, RigInterfaceID interface)
{
  RigObjectProps *obj = object;
  size_t props_offset = obj->type->interfaces[interface].props_offset;
  return (uint8_t *)obj + props_offset;
}

static inline void *
rig_object_get_vtable (void *object, RigInterfaceID interface)
{
  RigObjectProps *obj = object;
  return obj->type->interfaces[interface].vtable;
}

static inline CoglBool
rig_object_is (void *object, RigInterfaceID interface)
{
  RigObjectProps *obj = object;
  return _rig_bitmask_get (&obj->type->interfaces_mask, interface);
}

#endif /* _RIG_OBJECT_H_ */
