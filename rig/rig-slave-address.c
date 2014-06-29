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

#include <config.h>

#include <clib.h>

#include <rut.h>

#include "rig-slave-address.h"

static void
_rig_slave_address_free(void *object)
{
    rig_slave_address_t *slave_address = object;

    if (slave_address->serial)
        c_free(slave_address->serial);

    c_free(slave_address->name);
    c_free(slave_address->hostname);

    rut_object_free(rig_slave_address_t, slave_address);
}

static rut_type_t rig_slave_address_type;

static void
_rig_slave_address_init_type(void)
{
    rut_type_init(&rig_slave_address_type,
                  "rig_slave_address_t",
                  _rig_slave_address_free);
}

rig_slave_address_t *
rig_slave_address_new_tcp(const char *name, const char *hostname, int port)
{
    rig_slave_address_t *slave_address =
        rut_object_alloc0(rig_slave_address_t,
                          &rig_slave_address_type,
                          _rig_slave_address_init_type);

    slave_address->type = RIG_SLAVE_ADDRESS_TYPE_TCP;

    slave_address->name = c_strdup(name);
    slave_address->hostname = c_strdup(hostname);
    slave_address->port = port;

    return slave_address;
}

rig_slave_address_t *
rig_slave_address_new_adb(const char *name, const char *serial, int port)
{
    rig_slave_address_t *slave_address =
        rut_object_alloc0(rig_slave_address_t,
                          &rig_slave_address_type,
                          _rig_slave_address_init_type);

    slave_address->type = RIG_SLAVE_ADDRESS_TYPE_ADB_SERIAL;

    slave_address->name = c_strdup(name);
    slave_address->serial = c_strdup(serial);
    slave_address->hostname = c_strdup("127.0.0.1");
    slave_address->port = port;

    return slave_address;
}
