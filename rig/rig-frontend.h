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

#ifndef _RIG_FRONTEND_H_
#define _RIG_FRONTEND_H_

#include <rut.h>

typedef struct _RigFrontend RigFrontend;

typedef enum _RigFrontendID
{
  RIG_FRONTEND_ID_EDITOR=1,
  RIG_FRONTEND_ID_SLAVE,
  RIG_FRONTEND_ID_DEVICE
} RigFrontendID;


#include "rig-frontend.h"
#include "rig-engine.h"
#include "rig-simulator.h"
#include "rig-pb.h"

#include "rig.pb-c.h"

typedef enum _RigFrontendOpType
{
  RIG_FRONTEND_OP_TYPE_REGISTER_OBJECT=1,
  RIG_FRONTEND_OP_TYPE_SET_PROPERTY,
  RIG_FRONTEND_OP_TYPE_ADD_ENTITY,
  RIG_FRONTEND_OP_TYPE_DELETE_ENTITY,
  RIG_FRONTEND_OP_TYPE_ADD_COMPONENT,
  RIG_FRONTEND_OP_TYPE_DELETE_COMPONENT,
  RIG_FRONTEND_OP_TYPE_ADD_CONTROLLER,
  RIG_FRONTEND_OP_TYPE_DELETE_CONTROLLER,
  RIG_FRONTEND_OP_TYPE_CONTROLLER_SET_CONST,
  RIG_FRONTEND_OP_TYPE_CONTROLLER_PATH_ADD_NODE,
  RIG_FRONTEND_OP_TYPE_CONTROLLER_PATH_DELETE_NODE,
  RIG_FRONTEND_OP_TYPE_CONTROLLER_PATH_SET_NODE,
  RIG_FRONTEND_OP_TYPE_CONTROLLER_ADD_PROPERTY,
  RIG_FRONTEND_OP_TYPE_CONTROLLER_REMOVE_PROPERTY,
  RIG_FRONTEND_OP_TYPE_CONTROLLER_PROPERTY_SET_METHOD,
  RIG_FRONTEND_OP_TYPE_SET_PLAY_MODE,
} RigFrontendOpType;


/* The "frontend" is the main process that controls the running
 * of a Rig UI, either in device mode, as a slave or as an editor.
 *
 * Currently this is also the process where all rendering is handled,
 * though in the future more things may be split out into different
 * threads and processes.
 */
struct _RigFrontend
{
  RutObjectBase _base;

  RigFrontendID id;

  RigEngine *engine;

  pid_t simulator_pid;

  int fd;
  RigRPCPeer *frontend_peer;
  bool connected;

  bool has_resized;
  int pending_width;
  int pending_height;

  RutList ops;
  int n_ops;
};


RigFrontend *
rig_frontend_new (RutShell *shell,
                  RigFrontendID id,
                  const char *ui_filename);

void
rig_frontend_reload_uis (RigFrontend *frontend);

void
rig_frontend_op_register_object (RigFrontend *frontend,
                                 RutObject *object);

void
rig_frontend_op_set_property (RigFrontend *frontend,
                              RutProperty *property,
                              RutBoxed *value);

void
rig_frontend_op_add_entity (RigFrontend *frontend,
                            RutEntity *parent,
                            RutEntity *entity);

void
rig_frontend_op_delete_entity (RigFrontend *frontend,
                               RutEntity *entity);

void
rig_frontend_op_add_component (RigFrontend *frontend,
                               RutEntity *entity,
                               RutComponent *component);

void
rig_frontend_op_delete_component (RigFrontend *frontend,
                                  RutComponent *component);

void
rig_frontend_op_add_controller (RigFrontend *frontend,
                                RigController *controller);

void
rig_frontend_op_delete_controller (RigFrontend *frontend,
                                   RigController *controller);

void
rig_frontend_op_controller_set_const (RigFrontend *frontend,
                                      RigController *controller,
                                      RutProperty *property,
                                      RutBoxed *value);

void
rig_frontend_op_add_path_node (RigFrontend *frontend,
                               RigController *controller,
                               RutProperty *property,
                               float t,
                               RutBoxed *value);

void
rig_frontend_op_controller_path_delete_node (RigFrontend *frontend,
                                             RigController *controller,
                                             RutProperty *property,
                                             float t);

void
rig_frontend_op_controller_path_set_node (RigFrontend *frontend,
                                          RigController *controller,
                                          RutProperty *property,
                                          float t,
                                          RutBoxed *value);

void
rig_frontend_op_controller_add_property (RigFrontend *frontend,
                                         RigController *controller,
                                         RutProperty *property);

void
rig_frontend_op_controller_remove_property (RigFrontend *frontend,
                                            RigController *controller,
                                            RutProperty *property);

void
rig_frontend_op_controller_property_set_method (RigFrontend *frontend,
                                                RigController *controller,
                                                RutProperty *property,
                                                RigControllerMethod method);

void
rig_frontend_op_set_play_mode (RigFrontend *frontend,
                               bool play_mode_enabled);

void
rig_frontend_serialize_ops (RigFrontend *frontend,
                            RigPBSerializer *serializer,
                            Rig__FrameSetup *pb_frame_setup);

void
rig_frontend_start_service (RigFrontend *frontend);

void
rig_frontend_stop_service (RigFrontend *frontend);

#endif /* _RIG_FRONTEND_H_ */
