#include <config.h>
#include <ulib.h>
#include <string.h>
#include <math.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <stdio.h>

#ifdef U_OS_WIN32
#include <windows.h>
#define sleep(t)                 Sleep((t) * 1000)
#endif

#include "test.h"

RESULT
test_timer ()
{
	UTimer *timer;
	double elapsed1, elapsed2;
	unsigned long usec = 0;

	timer = u_timer_new ();
	sleep (1);
	elapsed1 = u_timer_elapsed (timer, NULL);
	if ((elapsed1 + 0.1) < 1.0)
		return FAILED ("Elapsed time should be around 1s and was %f", elapsed1);

	u_timer_stop (timer);
	elapsed1 = u_timer_elapsed (timer, NULL);
	elapsed2 = u_timer_elapsed (timer, &usec);
	if (fabs (elapsed1 - elapsed2) > 0.000001)
		return FAILED ("The elapsed times are not equal %f - %f.", elapsed1, elapsed2);

	elapsed2 *= 1000000;
	while (elapsed2 > 1000000)
		elapsed2 -= 1000000;

	if (fabs (usec - elapsed2) > 100.0)
		return FAILED ("usecs are wrong.");

	u_timer_destroy (timer);
	return OK;
}

static Test timer_tests [] = {
	{"u_timer", test_timer},
	{NULL, NULL}
};

DEFINE_TEST_GROUP_INIT(timer_tests_init, timer_tests)


