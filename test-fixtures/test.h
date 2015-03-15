#ifndef _TEST_UNIT_H_
#define _TEST_UNIT_H_

#include <test-fixtures/test-fixtures.h>

#ifdef ENABLE_UNIT_TESTS

/* We don't really care about functions that are defined without a
   header for the unit tests so we can just disable it here */
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#endif

typedef struct _test_t {
    const char *name;
    void (*run)(void);
} test_t;

#define TEST(NAME)                      \
    static void NAME(void);             \
                                        \
    const test_t test_state_##NAME = {  \
        #NAME, NAME                     \
    };                                  \
                                        \
    static void NAME(void)

#else /* ENABLE_UNIT_TESTS */

#define TEST(NAME) \
    static inline void NAME(void)

#endif /* ENABLE_UNIT_TESTS */

#endif /* _TEST_UNIT_H_ */
