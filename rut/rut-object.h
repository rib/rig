#ifndef _RUT_OBJECT_H_
#define _RUT_OBJECT_H_

#include "rut-type.h"

/* We largely give up having compile time type safety for RutObjects since
 * the type system is conceptually intended to be dynamic and for most
 * apis dealing with RutObjects, they care about interfaces more than
 * about object types and so using a strong types would require us to
 * always be casting objects between different interface types which
 * would make code more verbose and by explicitly casting we'd lose
 * the compile time type safety anyway.
 */
typedef void RutObject;
#define RUT_OBJECT(X) ((RutObject *)X)

/* For struct inheritance purposes this struct can be placed as the
 * first member of another struct so that it can be used with the
 * Rut type system.
 *
 * The struct should be initialized using rut_object_init() when
 * constructing an instance that inherits from this.
 */
typedef struct _RutObjectProps
{
  RutType *type;
} RutObjectProps;

void
rut_object_init (RutObjectProps *object_properties, RutType *type);

static inline const RutType *
rut_object_get_type (RutObject *object)
{
  RutObjectProps *obj = object;
  return obj->type;
}

static inline void *
rut_object_get_properties (RutObject *object, RutInterfaceID interface)
{
  RutObjectProps *obj = object;
  size_t props_offset = obj->type->interfaces[interface].props_offset;
  return (uint8_t *)obj + props_offset;
}

static inline void *
rut_object_get_vtable (void *object, RutInterfaceID interface)
{
  RutObjectProps *obj = object;
  return obj->type->interfaces[interface].vtable;
}

static inline CoglBool
rut_object_is (void *object, RutInterfaceID interface)
{
  RutObjectProps *obj = object;
  return _rut_bitmask_get (&obj->type->interfaces_mask, interface);
}

#endif /* _RUT_OBJECT_H_ */
