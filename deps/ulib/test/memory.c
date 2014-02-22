
#include <ulib.h>
#include "test.h"

RESULT
test_memory_zero_size_allocations ()
{
	void * p;

	p = u_malloc (0);
        if (p)
                return FAILED ("Calling u_malloc with size zero should return NULL.");

	p = u_malloc0 (0);
        if (p)
                return FAILED ("Calling u_malloc0 with size zero should return NULL.");

	p = u_realloc (NULL, 0);
        if (p)
                return FAILED ("Calling u_realloc with size zero should return NULL.");

	p = u_new (int, 0);
        if (p)
                return FAILED ("Calling u_new with size zero should return NULL.");

	p = u_new0 (int, 0);
        if (p)
                return FAILED ("Calling u_new0 with size zero should return NULL.");

        return OK;
}


static Test memory_tests [] = {
        {       "zero_size_allocations", test_memory_zero_size_allocations},
        {NULL, NULL}
};

DEFINE_TEST_GROUP_INIT(memory_tests_init, memory_tests)

