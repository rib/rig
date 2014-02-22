#include <config.h>
#include <ulib.h>
#include <umodule.h>
#include <string.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "test.h"

#if defined (U_OS_WIN32)
#define EXTERNAL_SYMBOL "UetProcAddress"
#else
#define EXTERNAL_SYMBOL "system"
#endif

void U_MODULE_EXPORT
dummy_test_export ()
{
}

/* test for u_module_open (NULL, ...) */
RESULT
test_module_symbol_null ()
{
	void * proc = U_INT_TO_POINTER (42);

	UModule *m = u_module_open (NULL, U_MODULE_BIND_LAZY);

	if (m == NULL)
		return FAILED ("bind to main module failed. #0");

	if (u_module_symbol (m, "__unlikely_\nexistent__", &proc))
		return FAILED ("non-existent symbol lookup failed. #1");

	if (proc)
		return FAILED ("non-existent symbol lookup failed. #2");

	if (!u_module_symbol (m, EXTERNAL_SYMBOL, &proc))
		return FAILED ("external lookup failed. #3");

	if (!proc)
		return FAILED ("external lookup failed. #4");

	if (!u_module_symbol (m, "dummy_test_export", &proc))
		return FAILED ("in-proc lookup failed. #5");

	if (!proc)
		return FAILED ("in-proc lookup failed. #6");

	if (!u_module_close (m))
		return FAILED ("close failed. #7");

	return OK;
}

static Test module_tests [] = {
	{"u_module_symbol_null", test_module_symbol_null},
	{NULL, NULL}
};

DEFINE_TEST_GROUP_INIT(module_tests_init, module_tests)


