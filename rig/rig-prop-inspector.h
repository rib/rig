#ifndef _RIG_PROP_INSPECTOR_H_
#define _RIG_PROP_INSPECTOR_H_

#include "rig.h"

extern RigType rig_prop_inspector_type;

typedef struct _RigPropInspector RigPropInspector;

#define RIG_PROP_INSPECTOR(x) ((RigPropInspector *) x)

/* This is called whenever the properties changes */
typedef void
(* RigPropInspectorCallback) (RigProperty *target_property,
                              RigProperty *source_property,
                              void *user_data);

RigPropInspector *
rig_prop_inspector_new (RigContext *ctx,
                        RigProperty *property,
                        RigPropInspectorCallback property_changed_cb,
                        void *user_data);

#endif /* _RIG_PROP_INSPECTOR_H_ */
