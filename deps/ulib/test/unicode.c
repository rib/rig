#include "test.h"

/*
 * u_unichar_type
 */
RESULT
test_g_unichar_type ()
{
	if (u_unichar_type ('A') != U_UNICODE_UPPERCASE_LETTER)
		return FAILED ("#1");
	if (u_unichar_type ('a') != U_UNICODE_LOWERCASE_LETTER)
		return FAILED ("#2");
	if (u_unichar_type ('1') != U_UNICODE_DECIMAL_NUMBER)
		return FAILED ("#3");
	if (u_unichar_type (0xA3) != U_UNICODE_CURRENCY_SYMBOL)
		return FAILED ("#4");
	return NULL;
}

/*
 * u_unichar_toupper
 */
RESULT
test_g_unichar_toupper ()
{
	if (u_unichar_toupper (0) != 0)
		return FAILED ("#0");
	if (u_unichar_toupper ('a') != 'A')
		return FAILED ("#1");
	if (u_unichar_toupper ('1') != '1')
		return FAILED ("#2");
	if (u_unichar_toupper (0x1C4) != 0x1C4)
		return FAILED ("#3");
	if (u_unichar_toupper (0x1F2) != 0x1F1)
		return FAILED ("#4");
	if (u_unichar_toupper (0x1F3) != 0x1F1)
		return FAILED ("#5");
	if (u_unichar_toupper (0xFFFF) != 0xFFFF)
		return FAILED ("#6");
	if (u_unichar_toupper (0x10428) != 0x10400)
		return FAILED ("#7");
	return NULL;
}

/*
 * u_unichar_tolower
 */
RESULT
test_g_unichar_tolower ()
{
	if (u_unichar_tolower (0) != 0)
		return FAILED ("#0");
	if (u_unichar_tolower ('A') != 'a')
		return FAILED ("#1");
	if (u_unichar_tolower ('1') != '1')
		return FAILED ("#2");
	if (u_unichar_tolower (0x1C5) != 0x1C6)
		return FAILED ("#3");
	if (u_unichar_tolower (0x1F1) != 0x1F3)
		return FAILED ("#4");
	if (u_unichar_tolower (0x1F2) != 0x1F3)
		return FAILED ("#5");
	if (u_unichar_tolower (0xFFFF) != 0xFFFF)
		return FAILED ("#6");
	return NULL;
}

/*
 * u_unichar_totitle
 */
RESULT
test_g_unichar_totitle ()
{
	if (u_unichar_toupper (0) != 0)
		return FAILED ("#0");
	if (u_unichar_totitle ('a') != 'A')
		return FAILED ("#1");
	if (u_unichar_totitle ('1') != '1')
		return FAILED ("#2");
	if (u_unichar_totitle (0x1C4) != 0x1C5)
		return FAILED ("#3");
	if (u_unichar_totitle (0x1F2) != 0x1F2)
		return FAILED ("#4");
	if (u_unichar_totitle (0x1F3) != 0x1F2)
		return FAILED ("#5");
	if (u_unichar_toupper (0xFFFF) != 0xFFFF)
		return FAILED ("#6");
	return NULL;
}

static Test unicode_tests [] = {
	{"u_unichar_type", test_g_unichar_type},
	{"u_unichar_toupper", test_g_unichar_toupper},
	{"u_unichar_tolower", test_g_unichar_tolower},
	{"u_unichar_totitle", test_g_unichar_totitle},
	{NULL, NULL}
};

DEFINE_TEST_GROUP_INIT(unicode_tests_init, unicode_tests)
