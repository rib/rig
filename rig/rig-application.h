/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013  Intel Corporation
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

#ifndef _RIG_APPLICATION_H_
#define _RIG_APPLICATION_H_

#include <clib.h>

#include "rig-editor.h"

C_BEGIN_DECLS

#define RIG_TYPE_APPLICATION (rig_application_get_type())
#define RIG_APPLICATION(obj)                                                   \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), RIG_TYPE_APPLICATION, rig_application_t))
#define RIG_APPLICATION_CLASS(klass)                                           \
    (G_TYPE_CHECK_CLASS_CAST(                                                  \
         (klass), RIG_TYPE_APPLICATION, rig_application_class_t))
#define RIG_IS_APPLICATION(obj)                                                \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), RIG_TYPE_APPLICATION))
#define RIG_IS_APPLICATION_CLASS(klass)                                        \
    (G_TYPE_CHECK_CLASS_TYPE((klass), RIG_TYPE_APPLICATION))
#define RIG_APPLICATION_GET_CLASS(obj)                                         \
    (G_TYPE_INSTANCE_GET_CLASS((obj), RIG_APPLICATION, rig_application_class_t))

typedef struct _rig_application_t rig_application_t;
typedef struct _rig_application_class_t rig_application_class_t;
typedef struct _rig_application_private_t rig_application_private_t;

struct _rig_application_class_t {
    GApplicationClass parent_class;
};

struct _rig_application_t {
    GApplication parent;

    rig_application_private_t *priv;
};

GType rig_application_get_type(void) G_GNUC_CONST;

rig_application_t *rig_application_new(rig_editor_t *editor);

void rig_application_add_onscreen(rig_application_t *app,
                                  rut_shell_onscreen_t *onscreen);

C_END_DECLS

#endif /* _RIG_APPLICATION_H_ */
