#include <ulib.h>
#include <string.h>
#include <stdio.h>
#include "test.h"

#define sfail(k,p) if (s->str [p] != k) { u_string_free (s,TRUE); return FAILED("Got %s, Failed at %d, expected '%c'", s->str, p, k);}

RESULT
test_append_speed()
{
	UString *s = u_string_new("");
	int i;
	
	for(i = 0; i < 1024; i++) {
		u_string_append(s, "x");
	}
	
	if(strlen (s->str) != 1024) {
		return FAILED("Incorrect string size, got: %s %d", 
			s->str, strlen(s->str));
	}
	
	u_string_free (s, TRUE);

	return OK;
}

RESULT
test_append_c_speed()
{
	UString *s = u_string_new("");
	int i;
	
	for(i = 0; i < 1024; i++) {
		u_string_append_c(s, 'x');
	}
	
	if(strlen(s->str) != 1024) {
		return FAILED("Incorrect string size, got: %s %d", s->str, 
			strlen(s->str));
	}
	
	u_string_free(s, TRUE);

	return OK;
}

RESULT
test_gstring ()
{
	UString *s = u_string_new_len ("My stuff", 2);
	char *ret;
	int i;

	if (strcmp (s->str, "My") != 0)
		return "Expected only 'My' on the string";
	u_string_free (s, TRUE);

	s = u_string_new_len ("My\0\0Rest", 6);
	if (s->str [2] != 0)
		return "Null was not copied";
	if (strcmp (s->str+4, "Re") != 0){
		return "Did not find the 'Re' part";
	}

	u_string_append (s, "lalalalalalalalalalalalalalalalalalalalalalal");
	if (s->str [2] != 0)
		return "Null as not copied";
	if (strncmp (s->str+4, "Relala", 6) != 0){
		return FAILED("Did not copy correctly, got: %s", s->str+4);
	}

	u_string_free (s, TRUE);

	s = u_string_new ("");
	for (i = 0; i < 1024; i++){
		u_string_append_c (s, 'x');
	}
	if (strlen (s->str) != 1024){
		return FAILED("Incorrect string size, got: %s %d\n", s->str, strlen (s->str));
	}
	u_string_free (s, TRUE);

	s = u_string_new ("hola");
	u_string_sprintfa (s, "%s%d", ", bola", 5);
	if (strcmp (s->str, "hola, bola5") != 0){
		return FAILED("Incorrect data, got: %s\n", s->str);
	}
	u_string_free (s, TRUE);

	s = u_string_new ("Hola");
	u_string_printf (s, "Dingus");
	
	/* Test that it does not release it */
	ret = u_string_free (s, FALSE);
	u_free (ret);

 	s = u_string_new_len ("H" "\000" "H", 3);
	u_string_append_len (s, "1" "\000" "2", 3);
	sfail ('H', 0);
	sfail ( 0, 1);
	sfail ('H', 2);
	sfail ('1', 3);
	sfail ( 0, 4);
	sfail ('2', 5);
	u_string_free (s, TRUE);
	
	return OK;
}

RESULT
test_sized ()
{
	UString *s = u_string_sized_new (20);

	if (s->str [0] != 0)
		return FAILED ("Expected an empty string");
	if (s->len != 0)
		return FAILED ("Expected an empty len");

	u_string_free (s, TRUE);
	
	return NULL;
}

RESULT
test_truncate ()
{
	UString *s = u_string_new ("0123456789");
	u_string_truncate (s, 3);

	if (strlen (s->str) != 3)
		return FAILED ("size of string should have been 3, instead it is [%s]\n", s->str);
	u_string_free (s, TRUE);
	
	s = u_string_new ("a");
	s = u_string_truncate (s, 10);
	if (strlen (s->str) != 1)
		return FAILED ("The size is not 1");
	u_string_truncate (s, (size_t)-1);
	if (strlen (s->str) != 1)
		return FAILED ("The size is not 1");
	u_string_truncate (s, 0);
	if (strlen (s->str) != 0)
		return FAILED ("The size is not 0");
	
	u_string_free (s, TRUE);

	return NULL;
}

RESULT
test_prepend ()
{
	UString *s = u_string_new ("dingus");
	u_string_prepend (s, "one");

	if (strcmp (s->str, "onedingus") != 0)
		return FAILED ("Failed, expected onedingus, got [%s]", s->str);

	u_string_free (s, TRUE);

	/* This is to force the code that where stuff does not fit in the allocated block */
	s = u_string_sized_new (1);
	u_string_prepend (s, "one");
	if (strcmp (s->str, "one") != 0)
		return FAILED ("Got erroneous result, expected [one] got [%s]", s->str);
	u_string_free (s, TRUE);

	/* This is to force the path where things fit */
	s = u_string_new ("123123123123123123123123");
	u_string_truncate (s, 1);
	if (strcmp (s->str, "1") != 0)
		return FAILED ("Expected [1] string, got [%s]", s->str);

	u_string_prepend (s, "pre");
	if (strcmp (s->str, "pre1") != 0)
		return FAILED ("Expected [pre1], got [%s]", s->str);
	u_string_free (s, TRUE);
	
	return NULL;
}

RESULT
test_appendlen ()
{
	UString *s = u_string_new ("");

	u_string_append_len (s, "boo\000x", 0);
	if (s->len != 0)
		return FAILED ("The length is not zero %d", s->len);
	u_string_append_len (s, "boo\000x", 5);
	if (s->len != 5)
		return FAILED ("The length is not five %d", s->len);
	u_string_append_len (s, "ha", -1);
	if (s->len != 7)
		return FAILED ("The length is not seven %d", s->len);
		
	u_string_free (s, TRUE);

	return NULL;
}

RESULT
test_macros ()
{
	char *s = u_strdup (U_STRLOC);
	char *p = strchr (s + 2, ':');
	int n;
	
	if (p == NULL)
		return FAILED ("Did not find a separator");
	n = atoi (p+1);
	if (n <= 0)
		return FAILED ("did not find a valid line number");

	*p = 0;
	if (strcmp (s + strlen(s) - 8 , "string.c") != 0)
		return FAILED ("This did not store the filename on U_STRLOC");
	
	u_free (s);
	return NULL;
}

static Test string_tests [] = {
	{"append-speed", test_append_speed},
	{"append_c-speed", test_append_c_speed},
	{"ctor+append", test_gstring },
	{"ctor+sized", test_sized },
	{"truncate", test_truncate },
	{"prepend", test_prepend },
	{"append_len", test_appendlen },
	{"macros", test_macros },
	{NULL, NULL}
};

DEFINE_TEST_GROUP_INIT(string_tests_init, string_tests)
