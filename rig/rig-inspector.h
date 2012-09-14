#ifndef _RIG_INSPECTOR_H_
#define _RIG_INSPECTOR_H_

#include <cogl/cogl.h>

#include "rig.h"

extern RigType rig_inspector_type;

typedef struct _RigInspector RigInspector;

/* This is called whenever one of the properties changes */
typedef void
(* RigInspectorCallback) (RigProperty *target_property,
                          RigProperty *source_property,
                          void *user_data);

#define RIG_INSPECTOR(x) ((RigInspector *) x)

RigInspector *
rig_inspector_new (RigContext *ctx,
                   RigObject *object,
                   RigInspectorCallback property_changed_cb,
                   void *user_data);

void
rig_inspector_reload_property (RigInspector *inspector,
                               RigProperty *property);

void
rig_inspector_reload_properties (RigInspector *inspector);

#endif /* _RIG_INSPECTOR_H_ */
