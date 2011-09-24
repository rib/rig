#include "rig-type.h"
#include "rig-bitmask.h"

static CoglBool
find_max_id (int bit_num, void *user_data)
{
  int *max_id = user_data;
  if (bit_num > *max_id)
    *max_id = bit_num;

  return TRUE;
}

void
rig_type_add_interface (RigType *type,
                        RigInterfaceID id,
                        size_t instance_priv_offset,
                        void *interface_vtable)
{
  int max_id = 0;

  _rig_bitmask_foreach (&type->interfaces_mask,
                        find_max_id,
                        &max_id);

  if (id > max_id)
    {
      type->interfaces = g_realloc (type->interfaces,
                                    sizeof (RigInterface) * (id + 1));
    }

  _rig_bitmask_set (&type->interfaces_mask, id, TRUE);
  type->interfaces[id].props_offset = instance_priv_offset;
  type->interfaces[id].vtable = interface_vtable;

  /* TODO
   * type->interfaces[interface_id].v8_prototype_append (type);
   */
}

void
rig_type_init (RigType *type)
{
  _rig_bitmask_init (&type->interfaces_mask);

  type->interfaces = NULL;

  /* TODO
   * type->v8_template = FunctionTemplate::New();
   */
}


