#include <config.h>

#include <stdlib.h>

#include <clib.h>

#include "test.h"
#include "test-fixtures.h"

bool _test_is_verbose;

static bool
is_boolean_env_set(const char *variable)
{
    char *val = getenv(variable);
    bool ret;

    if (!val)
        return false;

    if (c_ascii_strcasecmp(val, "1") == 0 ||
        c_ascii_strcasecmp(val, "on") == 0 ||
        c_ascii_strcasecmp(val, "true") == 0)
        ret = true;
    else if (c_ascii_strcasecmp(val, "0") == 0 ||
             c_ascii_strcasecmp(val, "off") == 0 ||
             c_ascii_strcasecmp(val, "false") == 0)
        ret = false;
    else {
        c_critical("Spurious boolean environment variable value (%s=%s)",
                   variable,
                   val);
        ret = true;
    }

    return ret;
}

void
test_init(void)
{
    static int counter = 0;

    if (counter != 0)
        c_critical("We don't support running more than one test at a time\n"
                   "in a single test run due to the state leakage that can\n"
                   "cause subsequent tests to fail.\n"
                   "\n"
                   "If you want to run all the tests you should run\n"
                   "$ make check");
    counter++;

    if (is_boolean_env_set("V"))
        _test_is_verbose = true;
}

void
test_fini(void)
{
}

bool
test_verbose(void)
{
    return _test_is_verbose;
}

void
test_allow_failure(void)
{
    c_print("WARNING: Test is known to fail\n");
}

