/*
 * Date and time utility functions.
 *
 * Author:
 *   Gonzalo Paniagua Javier (gonzalo@novell.com
 *
 * (C) 2006 Novell, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <clib-config.h>

#include <stdio.h>
#include <clib.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>

void
c_get_current_time(c_timeval_t *result)
{
    struct timeval tv;

    c_return_if_fail(result != NULL);
    gettimeofday(&tv, NULL);
    result->tv_sec = tv.tv_sec;
    result->tv_usec = tv.tv_usec;
}

void
c_usleep(unsigned long microseconds)
{
    struct timespec req, rem;

    req.tv_sec = microseconds / 1000000;
    req.tv_nsec = (microseconds % 1000000) * 1000;

    while (nanosleep(&req, &rem) == -1 && errno == EINTR)
        req = rem;
}
