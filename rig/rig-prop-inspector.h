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
