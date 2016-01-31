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

#ifndef _XDGMIME_H_
#define _XDGMIME_H_

#include <uv.h>

typedef struct xdgmime_request xdgmime_request_t;

typedef void (*xdgmime_result_cb)(xdgmime_request_t *req, const char *type);

struct xdgmime_request
{
    /* PUBLIC */
    uv_loop_t *loop;
    void *data;
    char *filename;
    const char *mime_type;

    /* PRIVATE */
    uv_work_t work_req;
    xdgmime_result_cb callback;
    char type_buf[256];
};

void xdgmime_request_init(uv_loop_t *loop, xdgmime_request_t *req);
void xdgmime_request_start(xdgmime_request_t *req,
                           const char *filename,
                           xdgmime_result_cb callback);
void xdgmime_request_cancel(xdgmime_request_t *req);

void xdgmime_cleanup(void);

#endif	/* _XDGMIME_H_ */
