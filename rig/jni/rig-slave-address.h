/*
 * Rut.
 *
 * Copyright (C) 2013  Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __RIG_SLAVE_ADDRESS_H__
#define __RIG_SLAVE_ADDRESS_H__

typedef struct _RigSlaveAddress
{
  RutObjectProps _parent;
  int ref_count;

  char *name;
  char *hostname;
  int port;
} RigSlaveAddress;

RigSlaveAddress *
rig_slave_address_new (const char *name,
                       const char *hostname,
                       int port);

#endif /* __RIG_SLAVE_ADDRESS_H__ */
