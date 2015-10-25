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

#include <rut-config.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#include <clib.h>

#include "rut-adb.h"
#include "rut-exception.h"
#include "rut-os.h"
#include "rut-poll.h"

static bool send_adb_command(
    int fd, const char *serial, rut_exception_t **e, const char *format, ...);

static void
_rut_adb_device_tracker_free(void *object)
{
    rut_adb_device_tracker_t *tracker = object;

    rut_object_free(rut_adb_device_tracker_t, tracker);
}

rut_type_t rut_adb_device_tracker_type;

void
_rut_adb_device_tracker_init_type(void)
{
    rut_type_init(&rut_adb_device_tracker_type,
                  "rut_adb_device_tracker_t",
                  _rut_adb_device_tracker_free);
}

static int
connect_to_adb(rut_exception_t **e)
{
    struct sockaddr_in server;
    int fd;

    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_port = htons(5037);
    server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (connect(fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        rut_throw(e,
                  RUT_IO_EXCEPTION,
                  RUT_IO_EXCEPTION_IO,
                  "Could not connect to ADB daemon: %s",
                  strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

static bool
send_adb_vcommand(int fd,
                  const char *serial,
                  rut_exception_t **e,
                  const char *format,
                  va_list args)
{
    char *command;
    rut_exception_t *catch = NULL;
    char buffer[5];
    char *request;
    bool status;

    if (serial &&
        !send_adb_command(fd, NULL, &catch, "host:transport:%s", serial)) {
        rut_throw(e,
                  RUT_ADB_EXCEPTION,
                  RUT_ADB_EXCEPTION_IO,
                  "Failed to redirect ADB IO to device with serial %s: %s",
                  serial,
                  catch->message);
        rut_exception_free(catch);
        return false;
    }

    command = c_strdup_vprintf(format, args);
    request = c_strdup_printf("%04x%s", strlen(command), command);
    status = rut_os_write(fd, request, strlen(request), &catch);
    c_free(request);
    c_free(command);

    if (!status) {
        rut_throw(e,
                  RUT_ADB_EXCEPTION,
                  RUT_ADB_EXCEPTION_IO,
                  "Failed to send command to ADB daemon: %s",
                  catch->message);
        rut_exception_free(catch);
        return false;
    }

    if (!rut_os_read_len(fd, buffer, 4, &catch)) {
        rut_throw(e,
                  RUT_ADB_EXCEPTION,
                  RUT_ADB_EXCEPTION_IO,
                  "Failed to read ADB daemon response: %s",
                  catch->message);
        rut_exception_free(catch);
        return false;
    }

    if (strncmp(buffer, "OKAY", 4) != 0) {
        rut_throw(e,
                  RUT_ADB_EXCEPTION,
                  RUT_ADB_EXCEPTION_IO,
                  "Didn't receive \"OKAY\" response from ADB daemon");
        return false;
    }

    return true;
}

static bool
send_adb_command(
    int fd, const char *serial, rut_exception_t **e, const char *format, ...)
{
    va_list args;
    bool status;

    va_start(args, format);
    status = send_adb_vcommand(fd, serial, e, format, args);
    va_end(args);

    return status;
}

static char *
read_reply(int fd, rut_exception_t **e)
{
    char len_buf[5] = "0000";
    char *buffer;
    int len = sizeof(buffer);
    rut_exception_t *catch = NULL;

    if (!rut_os_read_len(fd, len_buf, 4, &catch)) {
        rut_throw(e,
                  RUT_ADB_EXCEPTION,
                  RUT_ADB_EXCEPTION_IO,
                  "Spurious ADB daemon IO error: %s",
                  catch->message);
        rut_exception_free(catch);
        return NULL;
    }

    if (sscanf(len_buf, "%04x", &len) != 1) {
        rut_throw(e,
                  RUT_ADB_EXCEPTION,
                  RUT_ADB_EXCEPTION_IO,
                  "Read invalid length from ADB daemon");
        return NULL;
    }

    buffer = c_malloc(len);

    if (!rut_os_read_len(fd, buffer, len, &catch)) {
        rut_throw(e,
                  RUT_ADB_EXCEPTION,
                  RUT_ADB_EXCEPTION_IO,
                  "Failed to read reply from ADB daemon: %s",
                  catch->message);
        rut_exception_free(catch);
        c_free(buffer);
        return NULL;
    }

    return buffer;
}

bool
rut_adb_command(const char *serial,
                rut_exception_t **e,
                const char *format,
                ...)
{
    va_list args;
    bool status;
    int fd;

    fd = connect_to_adb(e);
    if (fd == -1)
        return false;

    va_start(args, format);
    status = send_adb_vcommand(fd, serial, e, format, args);
    va_end(args);

    close(fd);

    return status;
}

char *
rut_adb_query(const char *serial, rut_exception_t **e, const char *format, ...)
{
    va_list args;
    bool status;
    char *reply;
    int fd;

    fd = connect_to_adb(e);
    if (fd == -1)
        return NULL;

    va_start(args, format);
    status = send_adb_vcommand(fd, serial, e, format, args);
    va_end(args);

    if (!status)
        return NULL;

    reply = read_reply(fd, e);

    close(fd);

    return reply;
}

static char *
read_until_eof(int fd, rut_exception_t **e)
{
    char buffer[4096];
    c_string_t *data = c_string_new("");

    do {
        int len = sizeof(buffer);
        if (!rut_os_read(fd, buffer, &len, e)) {
            c_string_free(data, true);
            return NULL;
        }
        c_string_append_len(data, buffer, len);
        if (len < sizeof(buffer))
            break;
    } while (1);

    return c_string_free(data, false);
}

char *
rut_adb_run_shell_cmd(const char *serial,
                      rut_exception_t **e,
                      const char *format,
                      ...)
{
    va_list args;
    bool status;
    char *reply;
    int fd;

    fd = connect_to_adb(e);
    if (fd == -1)
        return NULL;

    va_start(args, format);
    status = send_adb_vcommand(fd, serial, e, format, args);
    va_end(args);

    if (!status)
        return NULL;

    reply = read_until_eof(fd, e);

    close(fd);

    return reply;
}

char *
rut_adb_getprop(const char *serial, const char *property, rut_exception_t **e)
{
    char *command = c_strconcat("shell:getprop ", property, NULL);
    char *result = rut_adb_run_shell_cmd(serial, e, command);
    c_free(command);

    if (!result)
        return NULL;

    return g_strchomp(result);
}

static void
handle_devices_update_cb(void *user_data, int fd, int revents)
{
    rut_adb_device_tracker_t *tracker = user_data;
    rut_exception_t *catch = NULL;
    char *reply;
    char **lines;
    const char **serials_vec;
    int i;

    reply = read_reply(tracker->fd, &catch);
    if (!reply)
        return;

    lines = c_strsplit(reply, "\n", -1);

    for (i = 0; lines[i]; i++)
        ;

    serials_vec = alloca(sizeof(void *) * i + 1);
    for (i = 0; lines[i]; i++) {
        char *p = strstr(lines[i], "\t");
        if (!p)
            break;
        p[0] = '\0';
        serials_vec[i] = lines[i];
    }
    serials_vec[i] = NULL;

    if (tracker->devices_update_callback) {
        tracker->devices_update_callback(
            serials_vec, i, tracker->devices_update_data);
    }

    c_strfreev(lines);

    /* XXX: We only rely on host:track-devices for notifications of
     * device changes, but since we want the device qualifier info
     * we follow up with a devices-l query... */
    c_free(reply);
}

rut_adb_device_tracker_t *
rut_adb_device_tracker_new(rut_shell_t *shell,
                           void (*devices_update)(const char **serials,
                                                  int n_devices,
                                                  void *user_data),
                           void *user_data)
{
    rut_adb_device_tracker_t *tracker =
        rut_object_alloc0(rut_adb_device_tracker_t,
                          &rut_adb_device_tracker_type,
                          _rut_adb_device_tracker_init_type);
    rut_exception_t *catch = NULL;
    int fd;

    fd = connect_to_adb(&catch);
    if (fd == -1) {
        c_warning("%s", catch->message);
        rut_exception_free(catch);
        goto error;
    }

    if (!send_adb_command(fd,
                          NULL, /* serial */
                          &catch,
                          "host:track-devices")) {
        c_warning("Failed to start tracking Android devices via ADB daemon: %s",
                  catch->message);
        rut_exception_free(catch);
        goto error;
    }

    tracker->shell = shell;
    tracker->fd = fd;
    tracker->devices_update_callback = devices_update;
    tracker->devices_update_data = user_data;

    rut_poll_shell_add_fd(shell,
                          fd,
                          RUT_POLL_FD_EVENT_IN,
                          NULL, /* prepare */
                          handle_devices_update_cb,
                          tracker);

    return tracker;

error:
    if (fd != -1)
        close(fd);

    rut_object_free(rut_adb_device_tracker_t, tracker);

    return NULL;
}
