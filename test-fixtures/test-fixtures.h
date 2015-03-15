#ifndef _TEST_FIXTURES_H_
#define _TEST_FIXTURES_H_

#include <clib.h>

#include <test-fixtures/test.h>

extern bool _test_is_verbose;

void test_init(void);
void test_fini(void);

/* test_verbose:
 *
 * Queries if the user asked for verbose test output or not.
 */
bool test_verbose(void);

/* test_allow_failure:
 *
 * Mark that this test is expected to fail
 */
void test_allow_failure(void);

#endif /* _TEST_FIXTURES_H_ */
