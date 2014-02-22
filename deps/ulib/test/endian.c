#include "test.h"

RESULT
test_swap ()
{
	uint32_t a = 0xabcdef01, res32;
	uint64_t b = (((uint64_t)a) << 32) | a, res64;
	uint64_t b_expect = (((uint64_t)0x1efcdab) << 32) | 0x01efcdab;
	uint16_t c = 0xabcd, res16;
	
	res32 = UUINT32_SWAP_LE_BE (a);
	if (res32 != 0x01efcdab)
		return FAILED ("UUINT32_SWAP_LE_BE returned 0x%x", res32);
	res32 = UUINT32_SWAP_LE_BE (1);
	if (res32 != 0x1000000)
		return FAILED ("UUINT32_SWAP_LE_BE returned 0x%x", res32);

	res64 = UUINT64_SWAP_LE_BE(b);
	if (res64 != b_expect)
		return FAILED ("UUINT64_SWAP_LE_BE returned 0x%llx (had=0x%llx)", res64, b);
	res16 = UUINT16_SWAP_LE_BE(c);
	if (res16 != 0xcdab)
		return FAILED ("UUINT16_SWAP_LE_BE returned 0x%x", (uint32_t) res16);	
	
	return OK;
}

/*
 * test initialization
 */

static Test endian_tests [] = {
	{"swap", test_swap},
	{NULL, NULL}
};

DEFINE_TEST_GROUP_INIT(endian_tests_init, endian_tests)

