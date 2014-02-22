/*
 * Tests to ensure that our type definitions are correct
 *
 * These depend on -Werror, -Wall being set to catch the build error.
 */
#include <stdio.h>
#ifndef _MSC_VER
#include <stdint.h>
#endif
#include <string.h>
#include <ulib.h>
#include "test.h"

RESULT
test_ptrconv ()
{
	int iv, iv2;
	unsigned int uv, uv2;
	void * ptr;

	iv = U_MAXINT32;
	ptr = U_INT_TO_POINTER (iv);
	iv2 = U_POINTER_TO_INT (ptr);
	if (iv != iv2)
		return FAILED ("int to pointer and back conversions fail %d != %d", iv, iv2);

	iv = U_MININT32;
	ptr = U_INT_TO_POINTER (iv);
	iv2 = U_POINTER_TO_INT (ptr);
	if (iv != iv2)
		return FAILED ("int to pointer and back conversions fail %d != %d", iv, iv2);

	iv = 1;
	ptr = U_INT_TO_POINTER (iv);
	iv2 = U_POINTER_TO_INT (ptr);
	if (iv != iv2)
		return FAILED ("int to pointer and back conversions fail %d != %d", iv, iv2);

	iv = -1;
	ptr = U_INT_TO_POINTER (iv);
	iv2 = U_POINTER_TO_INT (ptr);
	if (iv != iv2)
		return FAILED ("int to pointer and back conversions fail %d != %d", iv, iv2);

	iv = 0;
	ptr = U_INT_TO_POINTER (iv);
	iv2 = U_POINTER_TO_INT (ptr);
	if (iv != iv2)
		return FAILED ("int to pointer and back conversions fail %d != %d", iv, iv2);

	uv = 0;
	ptr = U_UINT_TO_POINTER (uv);
	uv2 = U_POINTER_TO_UINT (ptr);
	if (uv != uv2)
		return FAILED ("uint to pointer and back conversions fail %u != %d", uv, uv2);

	uv = 1;
	ptr = U_UINT_TO_POINTER (uv);
	uv2 = U_POINTER_TO_UINT (ptr);
	if (uv != uv2)
		return FAILED ("uint to pointer and back conversions fail %u != %d", uv, uv2);

	uv = UINT32_MAX;
	ptr = U_UINT_TO_POINTER (uv);
	uv2 = U_POINTER_TO_UINT (ptr);
	if (uv != uv2)
		return FAILED ("uint to pointer and back conversions fail %u != %d", uv, uv2);

	return NULL;

}

typedef struct {
	int a;
	int b;
} my_struct;

RESULT
test_offset ()
{
	if (U_STRUCT_OFFSET (my_struct, a) != 0)
		return FAILED ("offset of a is not zero");

	if (U_STRUCT_OFFSET (my_struct, b) != 4 && U_STRUCT_OFFSET (my_struct, b) != 8)
		return FAILED ("offset of b is 4 or 8, macro might be busted");

	return OK;
}

static Test size_tests [] = {
	{"ptrconv", test_ptrconv},
	{"u_struct_offset", test_offset},
	{NULL, NULL}
};

DEFINE_TEST_GROUP_INIT(size_tests_init, size_tests)
