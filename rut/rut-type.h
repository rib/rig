#ifndef _RUT_TYPE_H_
#define _RUT_TYPE_H_

#include <sys/types.h>

#include <cogl/cogl.h>

#include "rut-bitmask.h"

/* Indices for builtin interfaces */
typedef enum _RutInterfaceID
{
  RUT_INTERFACE_ID_REF_COUNTABLE = 1,
  RUT_INTERFACE_ID_GRAPHABLE,
  RUT_INTERFACE_ID_PAINT_BATCHABLE,
  RUT_INTERFACE_ID_INTROSPECTABLE,
  RUT_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
  RUT_INTERFACE_ID_PAINTABLE,
  RUT_INTERFACE_ID_TRANSFORMABLE,
  RUT_INTERFACE_ID_COMPONENTABLE,
  RUT_INTERFACE_ID_SIZABLE,
  RUT_INTERFACE_ID_COMPOSITE_SIZABLE,
  RUT_INTERFACE_ID_PRIMABLE,
  RUT_INTERFACE_ID_PICKABLE,
  RUT_INTERFACE_ID_INPUTABLE
} RutInterfaceID;

/* An interface defines an offset into an instance for some arbitrary
 * private data + a table of function pointers specific to that
 * interface. */
typedef struct _RutInterface
{
  /* PRIVATE */
  size_t props_offset;
  void *vtable;
  /* TODO
   * void (*v8_prototype_append) (RutType *type); */
} RutInterface;

/* The pointer to a RutType variable serves as a unique identifier for
 * a type and the RutType itself contains a bitmask of supported
 * interfaces and vtables is an array of pointers to interface vtables
 * where each index corresponds to a bit set in the interfaces mask.
 *
 * In JavaScript parlance this could be considered a prototype.
 */
typedef struct _RutType
{
  /* PRIVATE */
  const char *name;
  RutBitmask interfaces_mask;
  RutInterface *interfaces;
} RutType;

void
rut_type_init (RutType *type, const char *name);

void
rut_type_add_interface (RutType *type,
                        RutInterfaceID id,
                        size_t instance_priv_offset,
                        void *interface_vtable);

#endif /* _RUT_TYPE_H_ */
