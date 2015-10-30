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
#include <stdlib.h>

#include <uv.h>

#include <xdgmime.h>

static uv_loop_t *loop;

static void
mime_type_cb(xdgmime_request_t *req, const char *mime_type)
{
    printf("%s", mime_type ? mime_type : "not found");
}

int
main(int argc, char *argv[])
{
    xdgmime_request_t req;

    loop = uv_default_loop();

    if (argc < 2) {
        printf("Usage: test file\n");
        return (EXIT_FAILURE);
    }

    xdgmime_request_init(loop, &req);
    xdgmime_request_start(&req, argv[1], mime_type_cb);

    uv_run(loop, UV_RUN_DEFAULT);

    return EXIT_SUCCESS;
}
