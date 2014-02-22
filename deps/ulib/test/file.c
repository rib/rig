#include <config.h>
#include <ulib.h>
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include "test.h"

#ifdef U_OS_WIN32
#include <io.h>
#define close _close
#endif

RESULT
test_file_get_contents ()
{
	UError *error;
	char *content;
	uboolean ret;
	size_t length;
#ifdef U_OS_WIN32
	const char *filename = "c:\\Windows\\system.ini";
#else
	const char *filename = "/etc/hosts";
#endif

	/*
	filename != NULL
	ret = u_file_get_contents (NULL, NULL, NULL, NULL);
	contents != NULL
	ret = u_file_get_contents ("", NULL, NULL, NULL);
	error no such file and fails for 'error' not being null too
	ret = u_file_get_contents ("", &content, NULL, &error);
	*/

	error = NULL;
	ret = u_file_get_contents ("", &content, NULL, &error);
	if (ret)
		return FAILED ("HAH!");
	if (error == NULL)
		return FAILED ("Got nothing as error.");
	if (content != NULL)
		return FAILED ("Content is uninitialized");

	u_error_free (error);
	error = NULL;
	ret = u_file_get_contents (filename, &content, &length, &error);
	if (!ret)
		return FAILED ("The error is %d %s\n", error->code, error->message);
	if (error != NULL)
		return FAILED ("Got an error returning TRUE");
	if (content == NULL)
		return FAILED ("Content is NULL");
	if (strlen (content) != length)
		return FAILED ("length is %d but the string is %d", length, strlen (content));
	u_free (content);

	return OK;
}

RESULT
test_open_tmp ()
{
	UError *error;
	int fd;
	char *name = U_INT_TO_POINTER (-1);

	/*
	 * Okay, this works, but creates a .xxx file in /tmp on every run. Disabled.
	 * fd = u_file_open_tmp (NULL, NULL, NULL);
	 * if (fd < 0)
	 *	return FAILED ("Default failed.");
	 * close (fd);
	*/
	error = NULL;
	fd = u_file_open_tmp ("invalidtemplate", NULL, &error);
	if (fd != -1)
		return FAILED ("The template was invalid and accepted");
	if (error == NULL)
		return FAILED ("No error returned.");
	u_error_free (error);

	error = NULL;
	fd = u_file_open_tmp ("i/nvalidtemplate", &name, &error);
	if (fd != -1)
		return FAILED ("The template was invalid and accepted");
	if (error == NULL)
		return FAILED ("No error returned.");
	if (name == NULL)
		return FAILED ("'name' is not reset");
	u_error_free (error);

	error = NULL;
	fd = u_file_open_tmp ("valid-XXXXXX", &name, &error);
	if (fd == -1)
		return FAILED ("This should be valid");
	if (error != NULL)
		return FAILED ("No error returned.");
	if (name == NULL)
		return FAILED ("No name returned.");
	close (fd);
	unlink (name);
	u_free (name);
	return OK;
}

RESULT
test_file ()
{
	uboolean res;
	const char *tmp;
	char *path;

#ifndef U_OS_WIN32 /* FIXME */
	char *sympath;
	int ignored;
#endif

	res = u_file_test (NULL, 0);
	if (res)
		return FAILED ("Should return FALSE HERE");

	res = u_file_test ("file.c", 0);
	if (res)
		return FAILED ("Should return FALSE HERE");

	tmp = u_get_tmp_dir ();
	res = u_file_test (tmp, U_FILE_TEST_EXISTS);
	if (!res)
		return FAILED ("tmp does not exist.");
	res = u_file_test (tmp, U_FILE_TEST_IS_REGULAR);
	if (res)
		return FAILED ("tmp is regular");

	res = u_file_test (tmp, U_FILE_TEST_IS_DIR);
	if (!res)
		return FAILED ("tmp is not a directory");
	res = u_file_test (tmp, U_FILE_TEST_IS_EXECUTABLE);
	if (!res)
		return FAILED ("tmp is not a executable");

	res = u_file_test (tmp, U_FILE_TEST_EXISTS | U_FILE_TEST_IS_SYMLINK);
	if (!res)
		return FAILED ("2 tmp does not exist.");
	res = u_file_test (tmp, U_FILE_TEST_IS_REGULAR | U_FILE_TEST_IS_SYMLINK);
	if (res)
		return FAILED ("2 tmp is regular");

	res = u_file_test (tmp, U_FILE_TEST_IS_DIR | U_FILE_TEST_IS_SYMLINK);
	if (!res)
		return FAILED ("2 tmp is not a directory");
	res = u_file_test (tmp, U_FILE_TEST_IS_EXECUTABLE | U_FILE_TEST_IS_SYMLINK);
	if (!res)
		return FAILED ("2 tmp is not a executable");

	close (u_file_open_tmp (NULL, &path, NULL)); /* create an empty file */
	res = u_file_test (path, U_FILE_TEST_EXISTS);
	if (!res)
		return FAILED ("3 %s should exist", path);
	res = u_file_test (path, U_FILE_TEST_IS_REGULAR);
	/* This is strange. Empty file is reported as not existing! */
	if (!res)
		return FAILED ("3 %s IS_REGULAR", path);
	res = u_file_test (path, U_FILE_TEST_IS_DIR);
	if (res)
		return FAILED ("3 %s should not be a directory", path);
	res = u_file_test (path, U_FILE_TEST_IS_EXECUTABLE);
	if (res)
		return FAILED ("3 %s should not be executable", path);
	res = u_file_test (path, U_FILE_TEST_IS_SYMLINK);
	if (res)
		return FAILED ("3 %s should not be a symlink", path);

#ifndef U_OS_WIN32 /* FIXME */
	sympath = u_strconcat (path, "-link", NULL);
	ignored = symlink (path, sympath);
	res = u_file_test (sympath, U_FILE_TEST_EXISTS);
	if (!res)
		return FAILED ("4 %s should not exist", sympath);
	res = u_file_test (sympath, U_FILE_TEST_IS_REGULAR);
	if (!res)
		return FAILED ("4 %s should not be a regular file", sympath);
	res = u_file_test (sympath, U_FILE_TEST_IS_DIR);
	if (res)
		return FAILED ("4 %s should not be a directory", sympath);
	res = u_file_test (sympath, U_FILE_TEST_IS_EXECUTABLE);
	if (res)
		return FAILED ("4 %s should not be executable", sympath);
	res = u_file_test (sympath, U_FILE_TEST_IS_SYMLINK);
	if (!res)
		return FAILED ("4 %s should be a symlink", sympath);

	unlink (path);

	res = u_file_test (sympath, U_FILE_TEST_EXISTS);
	if (res)
		return FAILED ("5 %s should exist", sympath);
	res = u_file_test (sympath, U_FILE_TEST_IS_REGULAR);
	if (res)
		return FAILED ("5 %s should be a regular file", sympath);
	res = u_file_test (sympath, U_FILE_TEST_IS_DIR);
	if (res)
		return FAILED ("5 %s should not be a directory", sympath);
	res = u_file_test (sympath, U_FILE_TEST_IS_EXECUTABLE);
	if (res)
		return FAILED ("5 %s should not be executable", sympath);
	res = u_file_test (sympath, U_FILE_TEST_IS_SYMLINK);
	if (!res)
		return FAILED ("5 %s should be a symlink", sympath);
	unlink (sympath);
	u_free (sympath);
#endif
	u_free (path);
	return OK;
}

static Test file_tests [] = {
	{"u_file_get_contents", test_file_get_contents},
	{"u_file_open_tmp", test_open_tmp},
	{"u_file_test", test_file},
	{NULL, NULL}
};

DEFINE_TEST_GROUP_INIT(file_tests_init, file_tests)


