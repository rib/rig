#include <config.h>
#include <ulib.h>
#include <string.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef U_OS_UNIX
#include <pthread.h>
#endif
#include "test.h"

#ifdef U_OS_WIN32
#include <direct.h>
#define chdir _chdir
#endif

/* This test is just to be used with valgrind */
RESULT
test_buildpath ()
{
	char *s;
	char *buffer = "var/private";
	char *dir = "/";
	
	s = u_build_path ("/", "hola///", "//mundo", NULL);
	if (strcmp (s, "hola/mundo") != 0)
		return FAILED ("1 Got wrong result, got: %s", s);
	u_free (s);

	s = u_build_path ("/", "hola/", "/mundo", NULL);
	if (strcmp (s, "hola/mundo") != 0)
		return FAILED ("2 Got wrong result, got: %s", s);
	u_free (s);

	s = u_build_path ("/", "hola/", "mundo", NULL);
	if (strcmp (s, "hola/mundo") != 0)
		return FAILED ("3 Got wrong result, got: %s", s);
	u_free (s);

	s = u_build_path ("/", "hola", "/mundo", NULL);
	if (strcmp (s, "hola/mundo") != 0)
		return FAILED ("4 Got wrong result, got: %s", s);
	u_free (s);

	s = u_build_path ("/", "/hello", "world/", NULL);
	if (strcmp (s, "/hello/world/") != 0)
		return FAILED ("5 Got wrong result, got: %s", s);
	u_free (s);
	
	/* Now test multi-char-separators */
	s = u_build_path ("**", "hello", "world", NULL);
	if (strcmp (s, "hello**world") != 0)
		return FAILED ("6 Got wrong result, got: %s", s);
	u_free (s);

	s = u_build_path ("**", "hello**", "world", NULL);
	if (strcmp (s, "hello**world") != 0)
		return FAILED ("7 Got wrong result, got: %s", s);
	u_free (s);

	s = u_build_path ("**", "hello**", "**world", NULL);
	if (strcmp (s, "hello**world") != 0)
		return FAILED ("8 Got wrong result, got: %s", s);
	u_free (s);
	
	s = u_build_path ("**", "hello**", "**world", NULL);
	if (strcmp (s, "hello**world") != 0)
		return FAILED ("9 Got wrong result, got: %s", s);
	u_free (s);

	s = u_build_path ("1234567890", "hello", "world", NULL);
	if (strcmp (s, "hello1234567890world") != 0)
		return FAILED ("10 Got wrong result, got: %s", s);
	u_free (s);

	s = u_build_path ("1234567890", "hello1234567890", "1234567890world", NULL);
	if (strcmp (s, "hello1234567890world") != 0)
		return FAILED ("11 Got wrong result, got: %s", s);
	u_free (s);

	s = u_build_path ("1234567890", "hello12345678901234567890", "1234567890world", NULL);
	if (strcmp (s, "hello1234567890world") != 0)
		return FAILED ("12 Got wrong result, got: %s", s);
	u_free (s);

	/* Multiple */
	s = u_build_path ("/", "a", "b", "c", "d", NULL);
	if (strcmp (s, "a/b/c/d") != 0)
		return FAILED ("13 Got wrong result, got: %s", s);
	u_free (s);

	s = u_build_path ("/", "/a", "", "/c/", NULL);
	if (strcmp (s, "/a/c/") != 0)
		return FAILED ("14 Got wrong result, got: %s", s);
	u_free (s);

	/* Null */
	s = u_build_path ("/", NULL, NULL);
	if (s == NULL)
		return FAILED ("must get a non-NULL return");
	if (s [0] != 0)
		return FAILED ("must get an empty string");

	// This is to test the regression introduced by Levi for the Windows support
	// that code errouneously read below the allowed area (in this case dir [-1]).
	// and caused all kinds of random errors.
	dir = "//";
	dir++;
	s = u_build_filename (dir, buffer, NULL);
	if (s [0] != '/')
		return FAILED ("Must have a '/' at the start");

	u_free (s);
	return OK;
}

RESULT
test_buildfname ()
{
	char *s;
	
	s = u_build_filename ("a", "b", "c", "d", NULL);
#ifdef U_OS_WIN32
	if (strcmp (s, "a\\b\\c\\d") != 0)
#else
	if (strcmp (s, "a/b/c/d") != 0)
#endif
		return FAILED ("1 Got wrong result, got: %s", s);
	u_free (s);

#ifdef U_OS_WIN32
	s = u_build_filename ("C:\\", "a", NULL);
	if (strcmp (s, "C:\\a") != 0)
#else
	s = u_build_filename ("/", "a", NULL);
	if (strcmp (s, "/a") != 0)
#endif
		return FAILED ("1 Got wrong result, got: %s", s);

#ifndef U_OS_WIN32
	s = u_build_filename ("/", "foo", "/bar", "tolo/", "/meo/", NULL);
	if (strcmp (s, "/foo/bar/tolo/meo/") != 0)
		return FAILED ("1 Got wrong result, got: %s", s);
#endif
	
	return OK;
}

char *
test_dirname ()
{
	char *s;

#ifdef U_OS_WIN32
	s = u_path_get_dirname ("c:\\home\\miguel");
	if (strcmp (s, "c:\\home") != 0)
		return FAILED ("Expected c:\\home, got %s", s);
	u_free (s);

	s = u_path_get_dirname ("c:/home/miguel");
	if (strcmp (s, "c:/home") != 0)
		return FAILED ("Expected c:/home, got %s", s);
	u_free (s);

	s = u_path_get_dirname ("c:\\home\\dingus\\");
	if (strcmp (s, "c:\\home\\dingus") != 0)
		return FAILED ("Expected c:\\home\\dingus, got %s", s);
	u_free (s);

	s = u_path_get_dirname ("dir.c");
	if (strcmp (s, ".") != 0)
		return FAILED ("Expected `.', got %s", s);
	u_free (s);

	s = u_path_get_dirname ("c:\\index.html");
	if (strcmp (s, "c:") != 0)
		return FAILED ("Expected [c:], got [%s]", s);
#else
	s = u_path_get_dirname ("/home/miguel");
	if (strcmp (s, "/home") != 0)
		return FAILED ("Expected /home, got %s", s);
	u_free (s);

	s = u_path_get_dirname ("/home/dingus/");
	if (strcmp (s, "/home/dingus") != 0)
		return FAILED ("Expected /home/dingus, got %s", s);
	u_free (s);

	s = u_path_get_dirname ("dir.c");
	if (strcmp (s, ".") != 0)
		return FAILED ("Expected `.', got %s", s);
	u_free (s);

	s = u_path_get_dirname ("/index.html");
	if (strcmp (s, "/") != 0)
		return FAILED ("Expected [/], got [%s]", s);
#endif	
	return OK;
}

char *
test_basename ()
{
	char *s;

#ifdef U_OS_WIN32
	s = u_path_get_basename ("");
	if (strcmp (s, ".") != 0)
		return FAILED ("Expected `.', got %s", s);
	u_free (s);

	s = u_path_get_basename ("c:\\home\\dingus\\");
	if (strcmp (s, "dingus") != 0)
		return FAILED ("1 Expected dingus, got %s", s);
	u_free (s);

	s = u_path_get_basename ("c:/home/dingus/");
	if (strcmp (s, "dingus") != 0)
		return FAILED ("1 Expected dingus, got %s", s);
	u_free (s);

	s = u_path_get_basename ("c:\\home\\dingus");
	if (strcmp (s, "dingus") != 0)
		return FAILED ("2 Expected dingus, got %s", s);
	u_free (s);

	s = u_path_get_basename ("c:/home/dingus");
	if (strcmp (s, "dingus") != 0)
		return FAILED ("2 Expected dingus, got %s", s);
	u_free (s);
#else
	s = u_path_get_basename ("");
	if (strcmp (s, ".") != 0)
		return FAILED ("Expected `.', got %s", s);
	u_free (s);

	s = u_path_get_basename ("/home/dingus/");
	if (strcmp (s, "dingus") != 0)
		return FAILED ("1 Expected dingus, got %s", s);
	u_free (s);

	s = u_path_get_basename ("/home/dingus");
	if (strcmp (s, "dingus") != 0)
		return FAILED ("2 Expected dingus, got %s", s);
	u_free (s);
#endif
	return OK;
}

char *
test_ppath ()
{
	char *s;
#ifdef U_OS_WIN32
	const char *searchfor = "explorer.exe";
#else
	const char *searchfor = "ls";
#endif
	s = u_find_program_in_path (searchfor);
	if (s == NULL)
		return FAILED ("No %s on this system?", searchfor);
	u_free (s);
	return OK;
}

char *
test_ppath2 ()
{
	char *s;
	const char *path = u_getenv ("PATH");
#ifdef U_OS_WIN32
	const char *searchfor = "test_eglib.exe";
#else
	const char *searchfor = "test-glib";
#endif
	
	u_setenv ("PATH", "", TRUE);
	s = u_find_program_in_path ("ls");
	if (s != NULL) {
		u_setenv ("PATH", path, TRUE);
		return FAILED ("Found something interesting here: %s", s);
	}
	u_free (s);
	s = u_find_program_in_path (searchfor);
	if (s == NULL) {
		u_setenv ("PATH", path, TRUE);
		return FAILED ("It should find '%s' in the current directory.", searchfor);
	}
	u_free (s);
	u_setenv ("PATH", path, TRUE);
	return OK;
}

#ifndef DISABLE_FILESYSTEM_TESTS
char *
test_cwd ()
{
	char *dir = u_get_current_dir ();
#ifdef U_OS_WIN32
	const char *newdir = "C:\\Windows";
#else
	const char *newdir = "/bin";
#endif

	if (dir == NULL)
		return FAILED ("No current directory?");
	u_free (dir);
	
	if (chdir (newdir) == -1)
		return FAILED ("No %s?", newdir);
	
	dir = u_get_current_dir ();
	if (strcmp (dir, newdir) != 0)
		return FAILED("Did not go to %s?", newdir);
	u_free (dir);
	
	return OK;
}
#else
char *
test_cwd ()
{
	return OK;
}
#endif

char *
test_misc ()
{
	const char *home = u_get_home_dir ();
	const char *tmp = u_get_tmp_dir ();
	
	if (home == NULL)
		return FAILED ("Where did my home go?");

	if (tmp == NULL)
		return FAILED ("Where did my /tmp go?");

	return OK;
}

static Test path_tests [] = {
	{"u_build_filename", test_buildfname},
	{"u_buildpath", test_buildpath},
	{"u_path_get_dirname", test_dirname},
	{"u_path_get_basename", test_basename},
	{"u_find_program_in_path", test_ppath},
	{"u_find_program_in_path2", test_ppath2},
	{"test_cwd", test_cwd },
	{"test_misc", test_misc },
	{NULL, NULL}
};

DEFINE_TEST_GROUP_INIT(path_tests_init, path_tests)


