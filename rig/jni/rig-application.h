/*
 * Rut
 *
 * Copyright (C) 2013  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _RIG_APPLICATION_H_
#define _RIG_APPLICATION_H_

#include <glib.h>

#include "rig-engine.h"

G_BEGIN_DECLS

#define RIG_TYPE_APPLICATION                                            \
  (rig_application_get_type())
#define RIG_APPLICATION(obj)                                            \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                   \
                               RIG_TYPE_APPLICATION,                    \
                               RigApplication))
#define RIG_APPLICATION_CLASS(klass)                                    \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                                    \
                            RIG_TYPE_APPLICATION,                       \
                            RigApplicationClass))
#define RIG_IS_APPLICATION(obj)                                         \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                   \
                               RIG_TYPE_APPLICATION))
#define RIG_IS_APPLICATION_CLASS(klass)                                 \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                                    \
                            RIG_TYPE_APPLICATION))
#define RIG_APPLICATION_GET_CLASS(obj)                                  \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                                    \
                              RIG_APPLICATION,                          \
                              RigApplicationClass))

typedef struct _RigApplication RigApplication;
typedef struct _RigApplicationClass RigApplicationClass;
typedef struct _RigApplicationPrivate RigApplicationPrivate;

struct _RigApplicationClass
{
  GApplicationClass parent_class;
};

struct _RigApplication
{
  GApplication parent;

  RigApplicationPrivate *priv;
};

GType
rig_application_get_type (void) G_GNUC_CONST;

RigApplication *
rig_application_new (RigEngine *engine);

void
rig_application_add_onscreen (RigApplication *app,
                              CoglOnscreen *onscreen);

G_END_DECLS

#endif /* _RIG_APPLICATION_H_ */
