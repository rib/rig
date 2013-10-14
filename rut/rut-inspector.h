#ifndef _RUT_INSPECTOR_H_
#define _RUT_INSPECTOR_H_

#include <cogl/cogl.h>

#include "rut.h"

extern RutType rut_inspector_type;

typedef struct _RutInspector RutInspector;

/* This is called whenever one of the properties changes */
typedef void
(* RutInspectorCallback) (RutProperty *target_property,
                          RutProperty *source_property,
                          bool mergable,
                          void *user_data);

/* This is called whenever the 'controlled' state changes */
typedef void
(* RutInspectorControlledCallback) (RutProperty *property,
                                    CoglBool value,
                                    void *user_data);

#define RUT_INSPECTOR(x) ((RutInspector *) x)

RutInspector *
rut_inspector_new (RutContext *ctx,
                   GList *objects,
                   RutInspectorCallback property_changed_cb,
                   RutInspectorControlledCallback controlled_changed_cb,
                   void *user_data);

void
rut_inspector_reload_property (RutInspector *inspector,
                               RutProperty *property);

void
rut_inspector_reload_properties (RutInspector *inspector);

void
rut_inspector_set_property_controlled (RutInspector *inspector,
                                       RutProperty *property,
                                       CoglBool controlled);

#endif /* _RUT_INSPECTOR_H_ */
