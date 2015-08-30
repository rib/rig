/*
 * Copyright (C) 2015 Intel Corporation.
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

#include <clib.h>

#ifdef C_PLATFORM_UNIX
#include <time.h>
#endif

#ifdef __APPLE__
#include <mach/clock.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif

#ifdef WIN32
static double qpc_scale;
#endif
#ifdef __APPLE__
static double mach_abs_time_scale;
#endif


struct _c_timer_t {
    int64_t start, stop;
};

c_timer_t *
c_timer_new(void)
{
    c_timer_t *timer;

#ifdef WIN32
    if (!qpc_scale) {
        LARGE_INTEGER li;

        QueryPerformanceFrequency(&li);
        qpc_scale = 1e9 / li.QuadPart;
    }
#endif
#ifdef __APPLE__
    if (!mach_abs_time_scale) {
        mach_timebase_info_data_t timebase;

        mach_timebase_info(&timebase);
        mach_abs_time_scale = timebase.numer / timebase.denom;
    }
#endif

    timer = c_slice_new(c_timer_t);
    c_timer_start(timer);

    return timer;
}

void
c_timer_destroy(c_timer_t *timer)
{
    c_return_if_fail(timer != NULL);

    c_slice_free(c_timer_t, timer);
}

int64_t
c_get_monotonic_time(void)
{
    int64_t ret;

#ifdef __APPLE__
    ret = mach_absolute_time() * mach_abs_time_scale;
#elif defined(C_PLATFORM_UNIX)
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    ret = (ts.tv_sec * 1e9) + ts.tv_nsec;
#elif defined(WIN32)
    LARGE_INTEGER li;

    QueryPerformanceCounter(&li);
    ret = li.QuadPart * qpc_scale;
#else
#warning "XXX: platform missing monotonic clock support!"
    struct timeval tv;

    gettimeofday(&tv, NULL);
    ret = (tv.tv_sec * 1e9) + (tv.tv_usec * 1e3);
#endif

    return ret;
}

void
c_timer_start(c_timer_t *timer)
{
    c_return_if_fail(timer != NULL);

    timer->start = c_get_monotonic_time();
    timer->stop = 0;
}

void
c_timer_stop(c_timer_t *timer)
{
    c_return_if_fail(timer != NULL);

    timer->stop = c_get_monotonic_time();
}

double
c_timer_elapsed(c_timer_t *timer,
                unsigned long *unused_must_be_null) /* TODO: remove */
{
    int64_t stop;
    int64_t delta;

    c_return_val_if_fail(unused_must_be_null == NULL, 0);
    c_return_val_if_fail(timer != NULL, 0.0);

    if (timer->stop == 0)
        stop = c_get_monotonic_time();
    else
        stop = timer->stop;

    delta = stop - timer->start;
    return delta / 1e9;
}
