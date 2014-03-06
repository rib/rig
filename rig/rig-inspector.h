/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013,2014  Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


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
