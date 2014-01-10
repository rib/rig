#ifndef _RUT_TYPE_H_
#define _RUT_TYPE_H_

#include <sys/types.h>

#include "rut-bitmask.h"
#include "rut-list.h"
#include "rut-magazine.h"

/* Indices for builtin traits */
typedef enum _RutTraitID
{
  RUT_TRAIT_ID_GRAPHABLE = 1,
  RUT_TRAIT_ID_INTROSPECTABLE,
  RUT_TRAIT_ID_SIMPLE_INTROSPECTABLE,
  RUT_TRAIT_ID_PAINTABLE,
  RUT_TRAIT_ID_TRANSFORMABLE,
  RUT_TRAIT_ID_COMPONENTABLE,
  RUT_TRAIT_ID_SIZABLE,
  RUT_TRAIT_ID_COMPOSITE_SIZABLE,
  RUT_TRAIT_ID_PRIMABLE,
  RUT_TRAIT_ID_MESHABLE,
  RUT_TRAIT_ID_INPUTABLE,
  RUT_TRAIT_ID_PICKABLE,
  RUT_TRAIT_ID_IMAGE_SIZE_DEPENDENT,
  RUT_TRAIT_ID_RENDERER,
  RUT_TRAIT_ID_SELECTABLE,
  RUT_TRAIT_ID_MIMABLE,
} RutTraitID;

typedef void (*RutTraitDestructor) (void *object);

/* A trait implementation optionally defines:
 * - An offset into an instance for some arbitrary data
 * - A table of function pointers specific to that trait
 * - A destructor that can be used to clean up data when
 *   an instance associated with the trait is freed.
 */
typedef struct _RutTraitImplementation
{
  /* Interfaces with a destructor get linked into RutType::destructors */
  RutList destructor_link;

  size_t props_offset;
  void *vtable;
  RutTraitDestructor destructor;

} RutTraitImplementation;

/* The pointer to a RutType variable serves as a unique identifier for
 * a type and the RutType itself contains a bitmask of traits associated
 * with the type.
 *
 * A trait may imply a set of functions and/or a set of properties are
 * associated with the type.
 *
 * vtables is an array of pointers to trait function vtables where
 * each index corresponds to a bit set in the traits mask.
 */
typedef struct _RutType
{
  RutBitmask traits_mask;
  RutTraitImplementation *traits;
  RutMagazine *magazine;
  void (*free) (void *object);
  RutList destructors;

  const char *name;
} RutType;

/* Note: the type destructor is called before any trait destructors */
typedef void (*RutTypeDestructor) (void *object);

void
rut_type_init (RutType *type,
               const char *name,
               RutTypeDestructor type_destructor);

void
rut_type_set_magazine (RutType *type, RutMagazine *magazine);

void
rut_type_add_trait (RutType *type,
                    RutTraitID id,
                    size_t instance_priv_offset,
                    void *interface_vtable);

void
rut_trait_set_destructor (RutType *type,
                          RutTraitID id,
                          RutTraitDestructor trait_destructor);

#endif /* _RUT_TYPE_H_ */
