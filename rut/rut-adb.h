/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2014 Intel Corporation.
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

#ifndef _RUT_ADB_H_
#define _RUT_ADB_H_

#include "rut-object.h"
#include "rut-shell.h"
#include "rut-exception.h"

typedef enum _rut_adb_exception_t {
    RUT_ADB_EXCEPTION_IO = 1
} rut_adb_exception_t;

typedef struct _rut_adb_device_tracker_t {
    rut_object_base_t _base;

    rut_shell_t *shell;
    int fd;

    void (*devices_update_callback)(const char **serials,
                                    int n_devices,
                                    void *user_data);
    void *devices_update_data;
} rut_adb_device_tracker_t;

rut_adb_device_tracker_t *
rut_adb_device_tracker_new(rut_shell_t *shell,
                           void (*devices_update)(const char **serials,
                                                  int n_devices,
                                                  void *user_data),
                           void *user_data);

bool rut_adb_command(const char *serial,
                     rut_exception_t **e,
                     const char *format,
                     ...);

char *
rut_adb_query(const char *serial, rut_exception_t **e, const char *format, ...);

char *rut_adb_run_shell_cmd(const char *serial,
                            rut_exception_t **e,
                            const char *format,
                            ...);

char *
rut_adb_getprop(const char *serial, const char *property, rut_exception_t **e);

#endif /* _RUT_ADB_H_ */
