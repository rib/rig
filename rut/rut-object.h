#ifndef _RUT_OBJECT_H_
#define _RUT_OBJECT_H_

#include "rut-type.h"
#include "rut-refcount-debug.h"

G_BEGIN_DECLS

/* We largely give up having compile time type safety for RutObjects
 * since the type system is intended to be dynamic and most apis
 * dealing with RutObjects, care more about traits than about object
 * types. Using strong types would require us to always be casting
 * objects between different trait types which would make code more
 * verbose and by explicitly casting we'd lose the compile time type
 * safety anyway.
 */
typedef void RutObject;

/* Put one of these structs as the first member of your own struct
 * so that it can be used with the Rut type system.
 *
 * Allocate instances of your struct using rut_object_alloc() or
 * zeroed using rut_object_alloc0().
 */
typedef struct _RutObjectBase
{
  RutType *type;
  int ref_count;
} RutObjectBase;

typedef void (*RutTypeInit) (void);

RutObject *
_rut_object_alloc (size_t bytes, RutType *type, RutTypeInit type_init);

RutObject *
_rut_object_alloc0 (size_t bytes, RutType *type, RutTypeInit type_init);

#define rut_object_alloc(TYPE, TYPE_PTR, TYPE_INIT_FUNC) \
  _rut_object_alloc (sizeof (TYPE), TYPE_PTR, TYPE_INIT_FUNC)

#define rut_object_alloc0(TYPE, TYPE_PTR, TYPE_INIT_FUNC) \
  _rut_object_alloc0 (sizeof (TYPE), TYPE_PTR, TYPE_INIT_FUNC)

void
_rut_object_free (size_t bytes, void *object);

#define rut_object_free(TYPE, MEM) \
  _rut_object_free (sizeof (TYPE), MEM);

void
rut_object_init (RutObjectBase *object_properties, RutType *type);

static inline const RutType *
rut_object_get_type (RutObject *object)
{
  RutObjectBase *obj = (RutObjectBase *)object;
  return obj->type;
}

static inline void *
rut_object_get_properties (RutObject *object, RutTraitID trait)
{
  RutObjectBase *obj = (RutObjectBase *)object;
  size_t props_offset = obj->type->traits[trait].props_offset;
  return (uint8_t *)obj + props_offset;
}

static inline void *
rut_object_get_vtable (void *object, RutTraitID trait)
{
  RutObjectBase *obj = (RutObjectBase *)object;
  return obj->type->traits[trait].vtable;
}

static inline bool
rut_object_is (void *object, RutTraitID trait)
{
  RutObjectBase *obj = (RutObjectBase *)object;
  return _rut_bitmask_get (&obj->type->traits_mask, trait);
}

static inline const char *
rut_object_get_type_name (void *object)
{
  RutObjectBase *obj = (RutObjectBase *)object;
  return obj->type->name;
}

static inline void *
rut_object_ref (void *object)
{
  RutObjectBase *base = (RutObjectBase *)object;

  _rut_refcount_debug_ref (object);

  base->ref_count++;
  return object;
}

static inline void
rut_object_unref (void *object)
{
  RutObjectBase *base = (RutObjectBase *)object;

  _rut_refcount_debug_unref (object);

  if (--(base->ref_count) < 1)
    base->type->free (object);
}

/* Wherever possible it is recommended that you use rut_object_claim()
 * and rut_object_release() instead of rut_object_ref()/_unref()
 * because it allows for much more powerfull debugging.
 *
 * This will track relationships between objects which is extremely
 * useful when tracking down leaks and we should even be able to use
 * the graph for automatically detecting leaks in the future by
 * detecting disconnected sub-graphs of objects.
 */
static inline void *
rut_object_claim (void *object, void *owner)
{
  RutObjectBase *base = (RutObjectBase *)object;

  _rut_refcount_debug_claim (object, owner);

  base->ref_count++;
  return object;
}

static inline void
rut_object_release (void *object, void *owner)
{
  RutObjectBase *base = (RutObjectBase *)object;

  _rut_refcount_debug_release (object, owner);

  if (--(base->ref_count) < 1)
    base->type->free (object);
}

G_END_DECLS

#endif /* _RUT_OBJECT_H_ */
