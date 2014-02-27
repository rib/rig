#ifndef _RIG_INSPECTOR_H_
#define _RIG_INSPECTOR_H_

#include <cogl/cogl.h>

#include <rut.h>

extern RutType rig_inspector_type;

typedef struct _RigInspector RigInspector;

/* This is called whenever one of the properties changes */
typedef void
(* RigInspectorCallback) (RutProperty *target_property,
                          RutProperty *source_property,
                          bool mergable,
                          void *user_data);

/* This is called whenever the 'controlled' state changes */
typedef void
(* RigInspectorControlledCallback) (RutProperty *property,
                                    bool value,
                                    void *user_data);

RigInspector *
rig_inspector_new (RutContext *ctx,
                   GList *objects,
                   RigInspectorCallback property_changed_cb,
                   RigInspectorControlledCallback controlled_changed_cb,
                   void *user_data);

void
rig_inspector_reload_property (RigInspector *inspector,
                               RutProperty *property);

void
rig_inspector_reload_properties (RigInspector *inspector);

void
rig_inspector_set_property_controlled (RigInspector *inspector,
                                       RutProperty *property,
                                       bool controlled);

#endif /* _RIG_INSPECTOR_H_ */
