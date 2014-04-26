/*
 * Timer
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

#include <config.h>

#include <ulib.h>
#include <windows.h>

struct _UTimer {
	uint64_t start;
	uint64_t stop;
};

UTimer *u_timer_new (void)
{
	UTimer *timer;

	timer = u_new0 (UTimer, 1);
	u_timer_start (timer);
	return timer;
}

void
u_timer_destroy (UTimer *timer)
{
	u_return_if_fail (timer != NULL);
	u_free (timer);
}

void
u_timer_start (UTimer *timer)
{
	u_return_if_fail (timer != NULL);

	QueryPerformanceCounter ((LARGE_INTEGER*)&timer->start);
}

void
u_timer_stop (UTimer *timer)
{
	u_return_if_fail (timer != NULL);

	QueryPerformanceCounter ((LARGE_INTEGER*)&timer->stop);
}

double
u_timer_elapsed (UTimer *timer, unsigned long *microseconds)
{
	static uint64_t freq = 0;
	uint64_t delta, stop;

	if (freq == 0) {
		if (!QueryPerformanceFrequency ((LARGE_INTEGER *)&freq))
			freq = 1;
	}

	if (timer->stop == 0) {
		QueryPerformanceCounter ((LARGE_INTEGER*)&stop);
	}
	else {
		stop = timer->stop;
	}

	delta = stop - timer->start;

	if (microseconds)
		*microseconds = (unsigned long) (delta * (1000000.0 / freq));

	return (double) delta / (double) freq;
}


