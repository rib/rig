/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013  Intel Corporation.
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
 *
 */

#ifndef __RIG_SLAVE_ADDRESS_H__
#define __RIG_SLAVE_ADDRESS_H__

typedef enum _RigSlaveAddressType
{
  RIG_SLAVE_ADDRESS_TYPE_TCP = 1,
  RIG_SLAVE_ADDRESS_TYPE_ADB_SERIAL
} RigSlaveAddressType;

typedef struct _RigSlaveAddress
{
  RutObjectBase _base;

  RigSlaveAddressType type;

  char *name;

  /* adb serial */
  char *serial;

  char *hostname;
  int port;
} RigSlaveAddress;

RigSlaveAddress *
rig_slave_address_new_tcp (const char *name,
                           const char *hostname,
                           int port);

RigSlaveAddress *
rig_slave_address_new_adb (const char *name,
                           const char *serial,
                           int port);

#endif /* __RIG_SLAVE_ADDRESS_H__ */
