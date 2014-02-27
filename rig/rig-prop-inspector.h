#ifndef _RIG_PROP_INSPECTOR_H_
#define _RIG_PROP_INSPECTOR_H_

#include <rut.h>

extern RutType rig_prop_inspector_type;

typedef struct _RigPropInspector RigPropInspector;

/* This is called whenever the properties changes */
typedef void
(* RigPropInspectorCallback) (RutProperty *target_property,
                              RutProperty *source_property,
                              void *user_data);

/* This is called whenever the 'controlled' state changes */
typedef void
(* RigPropInspectorControlledCallback) (RutProperty *property,
                                        bool value,
                                        void *user_data);

RigPropInspector *
rig_prop_inspector_new (RutContext *ctx,
                        RutProperty *property,
                        RigPropInspectorCallback property_changed_cb,
                        RigPropInspectorControlledCallback controlled_changed_cb,
                        bool with_label,
                        void *user_data);

void
rig_prop_inspector_reload_property (RigPropInspector *inspector);

void
rig_prop_inspector_set_controlled (RigPropInspector *inspector,
                                   bool controlled);

RutProperty *
rig_prop_inspector_get_property (RigPropInspector *inspector);

#endif /* _RIG_PROP_INSPECTOR_H_ */
