#ifndef _RIG_TYPE_H_
#define _RIG_TYPE_H_

#include <sys/types.h>

#include <cogl/cogl.h>

#include "rig-bitmask.h"

/* Indices for builtin interfaces */
typedef enum _RigInterfaceID
{
  RIG_INTERFACE_ID_REF_COUNTABLE = 1,
  RIG_INTERFACE_ID_GRAPHABLE,
  RIG_INTERFACE_ID_PAINT_BATCHABLE,
  RIG_INTERFACE_ID_SIMPLE_WIDGET,
  RIG_INTERFACE_ID_INTROSPECTABLE,
  RIG_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
  RIG_INTERFACE_ID_PAINTABLE,
  RIG_INTERFACE_ID_TRANSFORMABLE,
} RigInterfaceID;

/* An interface defines an offset into an instance for some arbitrary
 * private data + a table of function pointers specific to that
 * interface. */
typedef struct _RigInterface
{
  /* PRIVATE */
  size_t props_offset;
  void *vtable;
  /* TODO
   * void (*v8_prototype_append) (RigType *type); */
} RigInterface;

/* The pointer to a RigType variable serves as a unique identifier for
 * a type and the RigType itself contains a bitmask of supported
 * interfaces and vtables is an array of pointers to interface vtables
 * where each index corresponds to a bit set in the interfaces mask.
 *
 * In JavaScript parlance this could be considered a prototype.
 */
typedef struct _RigType
{
  /* PRIVATE */
  RigBitmask interfaces_mask;
  RigInterface *interfaces;
} RigType;

void
rig_type_init (RigType *type);

void
rig_type_add_interface (RigType *type,
                        RigInterfaceID id,
                        size_t instance_priv_offset,
                        void *interface_vtable);

#endif /* _RIG_TYPE_H_ */
