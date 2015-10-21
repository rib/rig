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

#include <cglib/cglib.h>

#include <rut.h>

extern rut_type_t rig_inspector_type;

typedef struct _rig_inspector_t rig_inspector_t;

/* This is called whenever one of the properties changes */
typedef void (*rig_inspector_callback_t)(rut_property_t *target_property,
                                        rut_property_t *source_property,
                                        bool mergable,
                                        void *user_data);

/* This is called whenever the 'controlled' state changes */
typedef void (*rig_inspector_controlled_callback_t)(rut_property_t *property,
                                                  bool value,
                                                  void *user_data);

rig_inspector_t *
rig_inspector_new(rut_shell_t *shell,
                  c_llist_t *objects,
                  rig_inspector_callback_t property_changed_cb,
                  rig_inspector_controlled_callback_t controlled_changed_cb,
                  void *user_data);

void rig_inspector_reload_property(rig_inspector_t *inspector,
                                   rut_property_t *property);

void rig_inspector_reload_properties(rig_inspector_t *inspector);

void rig_inspector_set_property_controlled(rig_inspector_t *inspector,
                                           rut_property_t *property,
                                           bool controlled);

#endif /* _RIG_INSPECTOR_H_ */
