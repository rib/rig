/* Copyright (C) 2015 Robert Bragg
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

#include "xdgmime-config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <err.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include <clib.h>
#include <uv.h>

#include "xdgmime.h"
#include "glob.h"
#include "magic.h"

static bool initialized = false;

typedef void (*dir_callback_t)(const char *dir, void *data);

static void
foreach_xdg_mime_dir(dir_callback_t callback,
                     void *data)
{
    const char *home = c_get_xdg_data_home();

    if (home)
        callback(home, data);

    c_foreach_xdg_data_dir(callback, data);
}

static void
add_glob_path_cb(const char *path, void *data)
{
    char *glob_path = c_build_filename(path, "/mime/globs2", NULL);
    bool *status = data;

    if (glob_add_file(glob_path))
        *status = true;

    c_free(glob_path);
}

static void
add_magic_path_cb(const char *path, void *data)
{
    char *magic_path = c_build_filename(path, "/mime/magic", NULL);
    bool *status = data;

    if (magic_add_file(magic_path))
        *status = true;

    c_free(magic_path);
}

static void
xdgmime_init(void)
{
    bool have_globs = false, have_magic = false;

    if (initialized)
        return;

    foreach_xdg_mime_dir(add_glob_path_cb, &have_globs);
    foreach_xdg_mime_dir(add_magic_path_cb, &have_magic);

    if (!have_globs)
        c_warning("Could not find any xdg shared-mime globs2 files");
    if (!have_magic)
        c_warning("Could not find any xdg shared-mime magic files");

    initialized = true;
}

void
xdgmime_request_init(uv_loop_t *loop, xdgmime_request_t *req)
{
    if (!initialized)
        xdgmime_init();

    req->loop = loop;
    req->work_req.data = req;
}

static void
work_cb(uv_work_t *work_req)
{
    xdgmime_request_t *req = work_req->data;
    int fd = open(req->filename, O_RDONLY|O_CLOEXEC);
    ssize_t size;
    bool needs_magic = false;
    const char *mime;

    if (fd < 0) {
        req->mime_type = NULL;
        return;
    }

    size = fgetxattr(fd, "user.mime_type",
                     req->type_buf, sizeof(req->type_buf) - 1);
    if (size > 0) {
        req->type_buf[size - 1] = '\0';
        req->mime_type = req->type_buf;
        close(fd);
        return;
    }

    mime = glob_lookup_mime_type(req->filename, &needs_magic);
    if (mime && !needs_magic)
        req->mime_type = mime;
    else
        req->mime_type = magic_lookup_mime_type(fd);

    close(fd);
}

static void
done_cb(uv_work_t *work_req, int status)
{
    xdgmime_request_t *req = work_req->data;

    if (status == 0)
        req->callback(req, req->mime_type);

    c_free(req->filename);
    req->filename = NULL;
    req->callback = NULL;
}

void
xdgmime_request_start(xdgmime_request_t *req,
                      const char *filename,
                      xdgmime_result_cb callback)
{
    req->filename = c_strdup(filename);
    req->callback = callback;

    uv_queue_work(req->loop, &req->work_req, work_cb, done_cb);
}

void
xdgmime_request_cancel(xdgmime_request_t *req)
{
    if (req->callback) {
        done_cb(&req->work_req, -1);
        uv_cancel((uv_req_t *)&req->work_req);
    }
}

void
xdgmime_cleanup(void)
{
    if (!initialized)
        return;

    magic_cleanup();
    glob_cleanup();
}
