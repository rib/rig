/*
 * Rig
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef _RIG_RPC_NETWORK_H_
#define _RIG_RPC_NETWORK_H_

void
rig_start_rpc_server (RigData *data);

void
rig_stop_rpc_server (RigData *data);

void
rig_start_rpc_client (RigData *data,
                      const char *hostname,
                      int port,
                      void (*connected) (RigData *data));

#endif /* _RIG_RPC_NETWORK_H_ */
