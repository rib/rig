#include <ulib.h>
#include <string.h>
#include <stdio.h>
#include "test.h"

/* This test is just to be used with valgrind */
RESULT
test_strfreev ()
{
	char **array = u_new (char *, 4);
	array [0] = u_strdup ("one");
	array [1] = u_strdup ("two");
	array [2] = u_strdup ("three");
	array [3] = NULL;
	
	u_strfreev (array);
	u_strfreev (NULL);

	return OK;
}

RESULT
test_concat ()
{
	char *x = u_strconcat ("Hello", ", ", "world", NULL);
	if (strcmp (x, "Hello, world") != 0)
		return FAILED("concat failed, got: %s", x);
	u_free (x);
	return OK;
}

RESULT
test_split ()
{
	const char *to_split = "Hello world, how are we doing today?";
	int i;
	char **v;
	
	v= u_strsplit(to_split, " ", 0);
	
	if(v == NULL) {
		return FAILED("split failed, got NULL vector (1)");
	}
	
	for(i = 0; v[i] != NULL; i++);
	if(i != 7) {
		return FAILED("split failed, expected 7 tokens, got %d", i);
	}
	
	u_strfreev(v);

	v = u_strsplit(to_split, ":", -1);
	if(v == NULL) {
		return FAILED("split failed, got NULL vector (2)");
	}

	for(i = 0; v[i] != NULL; i++);
	if(i != 1) {
		return FAILED("split failed, expected 1 token, got %d", i);
	}

	if(strcmp(v[0], to_split) != 0) {
		return FAILED("expected vector[0] to be '%s' but it was '%s'",
			to_split, v[0]);
	}
	u_strfreev(v);

	v = u_strsplit ("", ":", 0);
	if (v == NULL)
		return FAILED ("u_strsplit returned NULL");
	u_strfreev (v);

	v = u_strsplit ("/home/miguel/dingus", "/", 0);
	if (v [0][0] != 0)
		return FAILED ("Got a non-empty first element");
	u_strfreev (v);

	v = u_strsplit ("appdomain1, Version=0.0.0.0, Culture=neutral", ",", 4);
	if (strcmp (v [0], "appdomain1") != 0)
		return FAILED ("Invalid value");
	
	if (strcmp (v [1], " Version=0.0.0.0") != 0)
		return FAILED ("Invalid value");
	
	if (strcmp (v [2], " Culture=neutral") != 0)
		return FAILED ("Invalid value");

	if (v [3] != NULL)
		return FAILED ("Expected only 3 elements");
	
	u_strfreev (v);

	v = u_strsplit ("abcXYdefXghiXYjklYmno", "XY", 4);
	if (strcmp (v [0], "abc") != 0)
		return FAILED ("Invalid value 0");
	
	if (strcmp (v [1], "defXghi") != 0)
		return FAILED ("Invalid value 1");

	if (strcmp (v [2], "jklYmno") != 0)
		return FAILED ("Invalid value 2");

	if (v [3] != NULL)
		return FAILED ("Expected only 3 elements (1)");
	
	u_strfreev (v);

	v = u_strsplit ("abcXYdefXghiXYjklYmno", "XY", 2);
	if (strcmp (v [0], "abc") != 0)
		return FAILED ("Invalid value 3");
	
	if (strcmp (v [1], "defXghiXYjklYmno") != 0)
		return FAILED ("Invalid value 4");

	if (v [2] != NULL)
		return FAILED ("Expected only 2 elements (2)");
	
	u_strfreev (v);

	v = u_strsplit ("abcXYdefXghiXYjklYmnoXY", "XY", 3);
	if (strcmp (v [0], "abc") != 0)
		return FAILED ("Invalid value 5");
	
	if (strcmp (v [1], "defXghi") != 0)
		return FAILED ("Invalid value 6");

	if (strcmp (v [2], "jklYmnoXY") != 0)
		return FAILED ("Invalid value 7");

	if (v [3] != NULL)
		return FAILED ("Expected only 3 elements (3)");
	
	u_strfreev (v);

	v = u_strsplit ("abcXYXYXYdefXY", "XY", -1);
	if (strcmp (v [0], "abc") != 0)
		return FAILED ("Invalid value 8");

	if (strcmp (v [1], "") != 0)
		return FAILED ("Invalid value 9");

	if (strcmp (v [2], "") != 0)
		return FAILED ("Invalid value 10");
	
	if (strcmp (v [3], "def") != 0)
		return FAILED ("Invalid value 11");

	if (strcmp (v [4], "") != 0)
		return FAILED ("Invalid value 12");

	if (v [5] != NULL)
		return FAILED ("Expected only 5 elements (4)");
	
	u_strfreev (v);

	v = u_strsplit ("XYXYXYabcXYdef", "XY", -1);
	if (strcmp (v [0], "") != 0)
		return FAILED ("Invalid value 13");
	
	if (strcmp (v [1], "") != 0)
		return FAILED ("Invalid value 14");
	
	if (strcmp (v [2], "") != 0)
		return FAILED ("Invalid value 15");
	
	if (strcmp (v [3], "abc") != 0)
		return FAILED ("Invalid value 16");
	
	if (strcmp (v [4], "def") != 0)
		return FAILED ("Invalid value 17");

	if (v [5] != NULL)
		return FAILED ("Expected only 5 elements (5)");
	
	u_strfreev (v);

	v = u_strsplit ("value=", "=", 2);
	if (strcmp (v [0], "value") != 0)
		return FAILED ("Invalid value 18; expected 'value', got '%s'", v [0]);
	if (strcmp (v [1], "") != 0)
		return FAILED ("Invalid value 19; expected '', got '%s'", v [1]);
	if (v [2] != NULL)
		return FAILED ("Expected only 2 elements (6)");

	u_strfreev (v);

	return OK;
}

RESULT
test_split_set ()
{
	char **v;
	
	v = u_strsplit_set ("abcXYdefXghiXYjklYmno", "XY", 6);
	if (strcmp (v [0], "abc") != 0)
		return FAILED ("Invalid value 0");

	if (strcmp (v [1], "") != 0)
		return FAILED ("Invalid value 1");
	
	if (strcmp (v [2], "def") != 0)
		return FAILED ("Invalid value 2");

	if (strcmp (v [3], "ghi") != 0)
		return FAILED ("Invalid value 3");

	if (strcmp (v [4], "") != 0)
		return FAILED ("Invalid value 4");

	if (strcmp (v [5], "jklYmno") != 0)
		return FAILED ("Invalid value 5");

	if (v [6] != NULL)
		return FAILED ("Expected only 6 elements (1)");

	u_strfreev (v);

	v = u_strsplit_set ("abcXYdefXghiXYjklYmno", "XY", 3);
	if (strcmp (v [0], "abc") != 0)
		return FAILED ("Invalid value 6");

	if (strcmp (v [1], "") != 0)
		return FAILED ("Invalid value 7");
	
	if (strcmp (v [2], "defXghiXYjklYmno") != 0)
		return FAILED ("Invalid value 8");

	if (v [3] != NULL)
		return FAILED ("Expected only 3 elements (2)");
	
	u_strfreev (v);

	v = u_strsplit_set ("abcXdefYghiXjklYmnoX", "XY", 5);
	if (strcmp (v [0], "abc") != 0)
		return FAILED ("Invalid value 9");
	
	if (strcmp (v [1], "def") != 0)
		return FAILED ("Invalid value 10");

	if (strcmp (v [2], "ghi") != 0)
		return FAILED ("Invalid value 11");

	if (strcmp (v [3], "jkl") != 0)
		return FAILED ("Invalid value 12");

	if (strcmp (v [4], "mnoX") != 0)
		return FAILED ("Invalid value 13");

	if (v [5] != NULL)
		return FAILED ("Expected only 5 elements (5)");
	
	u_strfreev (v);

	v = u_strsplit_set ("abcXYXdefXY", "XY", -1);
	if (strcmp (v [0], "abc") != 0)
		return FAILED ("Invalid value 14");

	if (strcmp (v [1], "") != 0)
		return FAILED ("Invalid value 15");

	if (strcmp (v [2], "") != 0)
		return FAILED ("Invalid value 16");
	
	if (strcmp (v [3], "def") != 0)
		return FAILED ("Invalid value 17");

	if (strcmp (v [4], "") != 0)
		return FAILED ("Invalid value 18");

	if (strcmp (v [5], "") != 0)
		return FAILED ("Invalid value 19");

	if (v [6] != NULL)
		return FAILED ("Expected only 6 elements (4)");
	
	u_strfreev (v);

	v = u_strsplit_set ("XYXabcXYdef", "XY", -1);
	if (strcmp (v [0], "") != 0)
		return FAILED ("Invalid value 20");
	
	if (strcmp (v [1], "") != 0)
		return FAILED ("Invalid value 21");
	
	if (strcmp (v [2], "") != 0)
		return FAILED ("Invalid value 22");
	
	if (strcmp (v [3], "abc") != 0)
		return FAILED ("Invalid value 23");

	if (strcmp (v [4], "") != 0)
		return FAILED ("Invalid value 24");
	
	if (strcmp (v [5], "def") != 0)
		return FAILED ("Invalid value 25");

	if (v [6] != NULL)
		return FAILED ("Expected only 6 elements (5)");
	
	u_strfreev (v);

	return OK;
}

RESULT
test_strreverse ()
{
	RESULT res = OK;
	char *a = u_strdup ("onetwothree");
	char *a_target = "eerhtowteno";
	char *b = u_strdup ("onetwothre");
	char *b_target = "erhtowteno";
	char *c = u_strdup ("");
	char *c_target = "";

	u_strreverse (a);
	if (strcmp (a, a_target)) {
		res = FAILED("strreverse failed. Expecting: '%s' and got '%s'\n", a, a_target);
		goto cleanup;
	}

	u_strreverse (b);
	if (strcmp (b, b_target)) {
		res = FAILED("strreverse failed. Expecting: '%s' and got '%s'\n", b, b_target);
		goto cleanup;
	}

	u_strreverse (c);
	if (strcmp (c, c_target)) {
		res = FAILED("strreverse failed. Expecting: '%s' and got '%s'\n", b, b_target);
		goto cleanup;
	}

cleanup:
	u_free (c);
	u_free (b);
	u_free (a);
	return res;
}

RESULT
test_strjoin ()
{
	char *s;
	
	s = u_strjoin (NULL, "a", "b", NULL);
	if (strcmp (s, "ab") != 0)
		return FAILED ("Join of two strings with no separator fails");
	u_free (s);

	s = u_strjoin ("", "a", "b", NULL);
	if (strcmp (s, "ab") != 0)
		return FAILED ("Join of two strings with empty separator fails");
	u_free (s);

	s = u_strjoin ("-", "a", "b", NULL);
	if (strcmp (s, "a-b") != 0)
		return FAILED ("Join of two strings with separator fails");
	u_free (s);

	s = u_strjoin ("-", "aaaa", "bbbb", "cccc", "dddd", NULL);
	if (strcmp (s, "aaaa-bbbb-cccc-dddd") != 0)
		return FAILED ("Join of multiple strings fails");
	u_free (s);

	s = u_strjoin ("-", NULL);
	if (s == NULL || (strcmp (s, "") != 0))
		return FAILED ("Failed to join empty arguments");
	u_free (s);

	return OK;
}

RESULT
test_strchug ()
{
	char *str = u_strdup (" \t\n hola");

	u_strchug (str);
	if (strcmp ("hola", str)) {
		fprintf (stderr, "%s\n", str);
		u_free (str);
		return FAILED ("Failed.");
	}
	u_free (str);
	return OK;
}

RESULT
test_strchomp ()
{
	char *str = u_strdup ("hola  \t");

	u_strchomp (str);
	if (strcmp ("hola", str)) {
		fprintf (stderr, "%s\n", str);
		u_free (str);
		return FAILED ("Failed.");
	}
	u_free (str);
	return OK;
}

RESULT
test_strstrip ()
{
	char *str = u_strdup (" \t hola   ");

	u_strstrip (str);
	if (strcmp ("hola", str)) {
		fprintf (stderr, "%s\n", str);
		u_free (str);
		return FAILED ("Failed.");
	}
	u_free (str);
	return OK;
}

#define urit(so,j) do { s = u_filename_to_uri (so, NULL, NULL); if (strcmp (s, j) != 0) return FAILED("Got %s expected %s", s, j); u_free (s); } while (0);

#define errit(so) do { s = u_filename_to_uri (so, NULL, NULL); if (s != NULL) return FAILED ("got %s, expected NULL", s); } while (0);

RESULT
test_filename_to_uri ()
{
#ifdef U_OS_WIN32
#else
	char *s;

	urit ("/a", "file:///a");
	urit ("/home/miguel", "file:///home/miguel");
	urit ("/home/mig uel", "file:///home/mig%20uel");
	urit ("/\303\241", "file:///%C3%A1");
	urit ("/\303\241/octal", "file:///%C3%A1/octal");
	urit ("/%", "file:///%25");
	urit ("/\001\002\003\004\005\006\007\010\011\012\013\014\015\016\017\020\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037\040", "file:///%01%02%03%04%05%06%07%08%09%0A%0B%0C%0D%0E%0F%10%11%12%13%14%15%16%17%18%19%1A%1B%1C%1D%1E%1F%20");
	urit ("/!$&'()*+,-./", "file:///!$&'()*+,-./");
	urit ("/\042\043\045", "file:///%22%23%25");
	urit ("/0123456789:=", "file:///0123456789:=");
	urit ("/\073\074\076\077", "file:///%3B%3C%3E%3F");
	urit ("/\133\134\135\136_\140\173\174\175", "file:///%5B%5C%5D%5E_%60%7B%7C%7D");
	urit ("/\173\174\175\176\177\200", "file:///%7B%7C%7D~%7F%80");
	urit ("/@ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", "file:///@ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
	errit ("a");
	errit ("./hola");
#endif
	
	return OK;
}

#define fileit(so,j) do { s = u_filename_from_uri (so, NULL, NULL); if (strcmp (s, j) != 0) return FAILED("Got %s expected %s", s, j); u_free (s); } while (0);

#define ferrit(so) do { s = u_filename_from_uri (so, NULL, NULL); if (s != NULL) return FAILED ("got %s, expected NULL", s); } while (0);

RESULT
test_filename_from_uri ()
{
#ifdef U_OS_WIN32
#else
	char *s;

	fileit ("file:///a", "/a");
	fileit ("file:///%41", "/A");
	fileit ("file:///home/miguel", "/home/miguel");
	fileit ("file:///home/mig%20uel", "/home/mig uel");
	ferrit ("/a");
	ferrit ("a");
	ferrit ("file://a");
	ferrit ("file:a");
	ferrit ("file:///%");
	ferrit ("file:///%0");
	ferrit ("file:///%jj");
#endif
	
	return OK;
}

RESULT
test_ascii_xdigit_value ()
{
	int i;
	char j;

	i = u_ascii_xdigit_value ('9' + 1);
	if (i != -1)
		return FAILED ("'9' + 1");
	i = u_ascii_xdigit_value ('0' - 1);
	if (i != -1)
		return FAILED ("'0' - 1");
	i = u_ascii_xdigit_value ('a' - 1);
	if (i != -1)
		return FAILED ("'a' - 1");
	i = u_ascii_xdigit_value ('f' + 1);
	if (i != -1)
		return FAILED ("'f' + 1");
	i = u_ascii_xdigit_value ('A' - 1);
	if (i != -1)
		return FAILED ("'A' - 1");
	i = u_ascii_xdigit_value ('F' + 1);
	if (i != -1)
		return FAILED ("'F' + 1");

	for (j = '0'; j < '9'; j++) {
		int c = u_ascii_xdigit_value (j);
		if (c  != (j - '0'))
			return FAILED ("Digits %c -> %d", j, c);
	}
	for (j = 'a'; j < 'f'; j++) {
		int c = u_ascii_xdigit_value (j);
		if (c  != (j - 'a' + 10))
			return FAILED ("Lower %c -> %d", j, c);
	}
	for (j = 'A'; j < 'F'; j++) {
		int c = u_ascii_xdigit_value (j);
		if (c  != (j - 'A' + 10))
			return FAILED ("Upper %c -> %d", j, c);
	}
	return OK;
}

RESULT
test_strdelimit ()
{
	char *str;

	str = u_strdup (U_STR_DELIMITERS);
	str = u_strdelimit (str, NULL, 'a');
	if (0 != strcmp ("aaaaaaa", str))
		return FAILED ("All delimiters: '%s'", str);
	u_free (str);
	str = u_strdup ("hola");
	str = u_strdelimit (str, "ha", '+');
	if (0 != strcmp ("+ol+", str))
		return FAILED ("2 delimiters: '%s'", str);
	u_free (str);
	return OK;
}

#define NUMBERS "0123456789"

RESULT
test_strlcpy ()
{
	const char *src = "onetwothree";
	char *dest;
	size_t i;

	dest = u_malloc (strlen (src) + 1);
	memset (dest, 0, strlen (src) + 1);
	i = u_strlcpy (dest, src, (size_t)-1);
	if (i != strlen (src))
		return FAILED ("Test1 got %d", i);

	if (0 != strcmp (dest, src))
		return FAILED ("Src and dest not equal");

	i = u_strlcpy (dest, src, 3);
	if (i != strlen (src))
		return FAILED ("Test1 got %d", i);
	if (0 != strcmp (dest, "on"))
		return FAILED ("Test2");

	i = u_strlcpy (dest, src, 1);
	if (i != strlen (src))
		return FAILED ("Test3 got %d", i);
	if (*dest != '\0')
		return FAILED ("Test4");

	i = u_strlcpy (dest, src, 12345);
	if (i != strlen (src))
		return FAILED ("Test4 got %d", i);
	if (0 != strcmp (dest, src))
		return FAILED ("Src and dest not equal 2");
	u_free (dest);

	/* This is a test for u_filename_from_utf8, even if it does not look like it */
	dest = u_filename_from_utf8 (NUMBERS, strlen (NUMBERS), NULL, NULL, NULL);
	if (0 != strcmp (dest, NUMBERS))
		return FAILED ("problem [%s] and [%s]", dest, NUMBERS);
	u_free (dest);
	
	return OK;
}

RESULT
test_strescape ()
{
	char *str;

	str = u_strescape ("abc", NULL);
	if (strcmp ("abc", str))
		return FAILED ("#1");
	str = u_strescape ("\t\b\f\n\r\\\"abc", NULL);
	if (strcmp ("\\t\\b\\f\\n\\r\\\\\\\"abc", str))
		return FAILED ("#2 %s", str);
	str = u_strescape ("\001abc", NULL);
	if (strcmp ("\\001abc", str))
		return FAILED ("#3 %s", str);
	str = u_strescape ("\001abc", "\001");
	if (strcmp ("\001abc", str))
		return FAILED ("#3 %s", str);
	return OK;
}

RESULT
test_ascii_strncasecmp ()
{
	int n;

	n = u_ascii_strncasecmp ("123", "123", 1);
	if (n != 0)
		return FAILED ("Should have been 0");
	
	n = u_ascii_strncasecmp ("423", "123", 1);
	if (n != 3)
		return FAILED ("Should have been 3, got %d", n);

	n = u_ascii_strncasecmp ("123", "423", 1);
	if (n != -3)
		return FAILED ("Should have been -3, got %d", n);

	n = u_ascii_strncasecmp ("1", "1", 10);
	if (n != 0)
		return FAILED ("Should have been 0, got %d", n);
	return OK;
}

RESULT
test_ascii_strdown ()
{
	const char *a = "~09+AaBcDeFzZ$0909EmPAbCdEEEEEZZZZAAA";
	const char *b = "~09+aabcdefzz$0909empabcdeeeeezzzzaaa";
	char *c;
	int n, l;

	l = (int)strlen (b);
	c = u_ascii_strdown (a, l);
	n = u_ascii_strncasecmp (b, c, l);

	if (n != 0) {
		u_free (c);
		return FAILED ("Should have been 0, got %d", n);
	}

	u_free (c);
	return OK;
}

RESULT
test_strdupv ()
{
	char **one;
	char **two;
	int len;

	one = u_strdupv (NULL);
	if (one)
		return FAILED ("Should have been NULL");

	one = u_malloc (sizeof (char *));
	*one = NULL;
	two = u_strdupv (one);
	if (!two)
		FAILED ("Should have been not NULL");
	len = u_strv_length (two);
	if (len)
		FAILED ("Should have been 0");
	u_strfreev (two);
	u_strfreev (one);
	return NULL;
}

static Test strutil_tests [] = {
	{"u_strfreev", test_strfreev},
	{"u_strconcat", test_concat},
	{"u_strsplit", test_split},
	{"u_strsplit_set", test_split_set},
	{"u_strreverse", test_strreverse},
	{"u_strjoin", test_strjoin},
	{"u_strchug", test_strchug},
	{"u_strchomp", test_strchomp},
	{"u_strstrip", test_strstrip},
	{"u_filename_to_uri", test_filename_to_uri},
	{"u_filename_from_uri", test_filename_from_uri},
	{"u_ascii_xdigit_value", test_ascii_xdigit_value},
	{"u_strdelimit", test_strdelimit},
	{"u_strlcpy", test_strlcpy},
	{"u_strescape", test_strescape},
	{"u_ascii_strncasecmp", test_ascii_strncasecmp },
	{"u_ascii_strdown", test_ascii_strdown },
	{"u_strdupv", test_strdupv },
	{NULL, NULL}
};

DEFINE_TEST_GROUP_INIT(strutil_tests_init, strutil_tests)


