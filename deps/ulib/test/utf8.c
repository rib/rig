#include <stdlib.h>

#include "test.h"

/*
 * u_utf16_to_utf8
 */

long
compare_strings_utf8_pos (const char *expected, const char *actual, long size)
{
	int i;
	for (i = 0; i < size; i++)
		if (expected [i] != actual [i])
			return i;
	return -1;
}

RESULT
compare_strings_utf8_RESULT (const char *expected, const char *actual, long size)
{
	long ret;

	ret = compare_strings_utf8_pos (expected, actual, size);
	if (ret < 0)
		return OK;
	return FAILED ("Incorrect output: expected '%s' but was '%s', differ at %d\n", expected, actual, ret);
}

void
char_to_uunichar2 (uunichar2 ret[], const char *src)
{
	int i;

	for (i = 0; src [i]; i++)
		ret [i] = src [i];
	ret [i] = 0;
}

RESULT
compare_utf16_to_utf8_explicit (const char *expected, const uunichar2 *utf16, long len_in, long len_out, long size_spec)
{
	UError *error;
	char* ret;
	RESULT result;
	long in_read, out_read;

	result = NULL;

	error = NULL;
	ret = u_utf16_to_utf8 (utf16, size_spec, &in_read, &out_read, &error);
	if (error) {
		result = FAILED ("The error is %d %s\n", (error)->code, (error)->message);
		u_error_free (error);
		if (ret)
			u_free (ret);
		return result;
	}
	if (in_read != len_in)
		result = FAILED ("Read size is incorrect: expected %d but was %d\n", len_in, in_read);
	else if (out_read != len_out)
		result = FAILED ("Converted size is incorrect: expected %d but was %d\n", len_out, out_read);
	else
		result = compare_strings_utf8_RESULT (expected, ret, len_out);

	u_free (ret);
	if (result)
		return result;

	return OK;
}

RESULT
compare_utf16_to_utf8 (const char *expected, const uunichar2 *utf16, long len_in, long len_out)
{
	RESULT result;

	result = compare_utf16_to_utf8_explicit (expected, utf16, len_in, len_out, -1);
	if (result != OK)
		return result;
	return compare_utf16_to_utf8_explicit (expected, utf16, len_in, len_out, len_in);
}

RESULT
test_utf16_to_utf8 ()
{
	const char *src0 = "", *src1 = "ABCDE", *src2 = "\xE5\xB9\xB4\x27", *src3 = "\xEF\xBC\xA1", *src4 = "\xEF\xBD\x81", *src5 = "\xF0\x90\x90\x80";
	uunichar2 str0 [] = {0}, str1 [6], str2 [] = {0x5E74, 39, 0}, str3 [] = {0xFF21, 0}, str4 [] = {0xFF41, 0}, str5 [] = {0xD801, 0xDC00, 0};
	RESULT result;

	char_to_uunichar2 (str1, src1);

	/* empty string */
	result = compare_utf16_to_utf8 (src0, str0, 0, 0);
	if (result != OK)
		return result;

	result = compare_utf16_to_utf8 (src1, str1, 5, 5);
	if (result != OK)
		return result;
	result = compare_utf16_to_utf8 (src2, str2, 2, 4);
	if (result != OK)
		return result;
	result = compare_utf16_to_utf8 (src3, str3, 1, 3);
	if (result != OK)
		return result;
	result = compare_utf16_to_utf8 (src4, str4, 1, 3);
	if (result != OK)
		return result;
	result = compare_utf16_to_utf8 (src5, str5, 2, 4);
	if (result != OK)
		return result;

	return OK;
}

/*
 * u_utf8_to_utf16
 */

long
compare_strings_utf16_pos (const uunichar2 *expected, const uunichar2 *actual, long size)
{
	int i;
	for (i = 0; i < size; i++)
		if (expected [i] != actual [i])
			return i;
	return -1;
}

RESULT
compare_strings_utf16_RESULT (const uunichar2 *expected, const uunichar2 *actual, long size)
{
	long ret;

	ret = compare_strings_utf16_pos (expected, actual, size);
	if (ret < 0)
		return OK;
	return FAILED ("Incorrect output: expected '%s' but was '%s', differ at %d ('%c' x '%c')\n", expected, actual, ret, expected [ret], actual [ret]);
}

#if !defined(ULIB_TESTS)
#define eg_utf8_to_utf16_with_nuls u_utf8_to_utf16
#endif

RESULT
compare_utf8_to_utf16_explicit (const uunichar2 *expected, const char *utf8, long len_in, long len_out, long size_spec, uboolean include_nuls)
{
	UError *error;
	uunichar2* ret;
	RESULT result;
	long in_read, out_read;

	result = NULL;

	error = NULL;
	if (include_nuls)
		ret = eg_utf8_to_utf16_with_nuls (utf8, size_spec, &in_read, &out_read, &error);
	else
		ret = u_utf8_to_utf16 (utf8, size_spec, &in_read, &out_read, &error);

	if (error) {
		result = FAILED ("The error is %d %s\n", (error)->code, (error)->message);
		u_error_free (error);
		if (ret)
			u_free (ret);
		return result;
	}
	if (in_read != len_in)
		result = FAILED ("Read size is incorrect: expected %d but was %d\n", len_in, in_read);
	else if (out_read != len_out)
		result = FAILED ("Converted size is incorrect: expected %d but was %d\n", len_out, out_read);
	else
		result = compare_strings_utf16_RESULT (expected, ret, len_out);

	u_free (ret);
	if (result)
		return result;

	return OK;
}

RESULT
compare_utf8_to_utf16_general (const uunichar2 *expected, const char *utf8, long len_in, long len_out, uboolean include_nuls)
{
	RESULT result;

	result = compare_utf8_to_utf16_explicit (expected, utf8, len_in, len_out, -1, include_nuls);
	if (result != OK)
		return result;
	return compare_utf8_to_utf16_explicit (expected, utf8, len_in, len_out, len_in, include_nuls);
}

RESULT
compare_utf8_to_utf16 (const uunichar2 *expected, const char *utf8, long len_in, long len_out)
{
	return compare_utf8_to_utf16_general (expected, utf8, len_in, len_out, FALSE);
}

RESULT
compare_utf8_to_utf16_with_nuls (const uunichar2 *expected, const char *utf8, long len_in, long len_out)
{
	return compare_utf8_to_utf16_explicit (expected, utf8, len_in, len_out, len_in, TRUE);
}


RESULT
test_utf8_seq ()
{
	const char *src = "\xE5\xB9\xB4\x27";
	long in_read, out_read;
	//uunichar2 expected [6];
	UError *error = NULL;
	uunichar2 *dst;

	//printf ("got: %s\n", src);
	dst = u_utf8_to_utf16 (src, (long)strlen (src), &in_read, &out_read, &error);
	if (error != NULL){
		return error->message;
	}

	if (in_read != 4) {
		return FAILED ("in_read is expected to be 4 but was %d\n", in_read);
	}
	if (out_read != 2) {
		return FAILED ("out_read is expected to be 2 but was %d\n", out_read);
	}
	u_free (dst);

	return OK;
}

RESULT
test_utf8_to_utf16 ()
{
	const char *src0 = "", *src1 = "ABCDE", *src2 = "\xE5\xB9\xB4\x27", *src3 = "\xEF\xBC\xA1", *src4 = "\xEF\xBD\x81";
	uunichar2 str0 [] = {0}, str1 [6], str2 [] = {0x5E74, 39, 0}, str3 [] = {0xFF21, 0}, str4 [] = {0xFF41, 0};
	RESULT result;

	char_to_uunichar2 (str1, src1);

	/* empty string */
	result = compare_utf8_to_utf16 (str0, src0, 0, 0);
	if (result != OK)
		return result;

	result = compare_utf8_to_utf16 (str1, src1, 5, 5);
	if (result != OK)
		return result;
	result = compare_utf8_to_utf16 (str2, src2, 4, 2);
	if (result != OK)
		return result;
	result = compare_utf8_to_utf16 (str3, src3, 3, 1);
	if (result != OK)
		return result;
	result = compare_utf8_to_utf16 (str4, src4, 3, 1);
	if (result != OK)
		return result;

	return OK;
}

RESULT
test_utf8_to_utf16_with_nuls ()
{
	const char *src0 = "", *src1 = "AB\0DE", *src2 = "\xE5\xB9\xB4\x27", *src3 = "\xEF\xBC\xA1", *src4 = "\xEF\xBD\x81";
	uunichar2 str0 [] = {0}, str1 [] = {'A', 'B', 0, 'D', 'E', 0}, str2 [] = {0x5E74, 39, 0}, str3 [] = {0xFF21, 0}, str4 [] = {0xFF41, 0};
	RESULT result;

#if !defined(ULIB_TESTS)
	return OK;
#endif

	/* implicit length is forbidden */
		if (eg_utf8_to_utf16_with_nuls (src1, -1, NULL, NULL, NULL) != NULL)
		return FAILED ("explicit nulls must fail with -1 length\n");

	/* empty string */
	result = compare_utf8_to_utf16_with_nuls (str0, src0, 0, 0);
	if (result != OK)
		return result;

	result = compare_utf8_to_utf16_with_nuls  (str1, src1, 5, 5);
	if (result != OK)
		return result;
	result = compare_utf8_to_utf16_with_nuls  (str2, src2, 4, 2);
	if (result != OK)
		return result;
	result = compare_utf8_to_utf16_with_nuls  (str3, src3, 3, 1);
	if (result != OK)
		return result;
	result = compare_utf8_to_utf16_with_nuls  (str4, src4, 3, 1);
	if (result != OK)
		return result;

	return OK;
}

typedef struct {
	char *content;
	size_t length;
} convert_result_t;

RESULT
test_convert ()
{
	static const char *charsets[] = { "UTF-8", "UTF-16LE", "UTF-16BE", "UTF-32LE", "UTF-32BE" };
	size_t length, converted_length, n;
	char *content, *converted, *path;
	convert_result_t **expected;
	UError *err = NULL;
	const char *srcdir;
	uboolean loaded;
	unsigned int i, j, k;
	char c;

	if (!(srcdir = getenv ("srcdir")) && !(srcdir = getenv ("PWD")))
		return FAILED ("srcdir not defined!");

	expected = u_malloc (sizeof (convert_result_t *) * U_N_ELEMENTS (charsets));

	/* first load all our test samples... */
	for (i = 0; i < U_N_ELEMENTS (charsets); i++) {
		path = u_strdup_printf ("%s%c%s.txt", srcdir, U_DIR_SEPARATOR, charsets[i]);
		loaded = u_file_get_contents (path, &content, &length, &err);
		u_free (path);

		if (!loaded) {
			for (j = 0; j < i; j++) {
				u_free (expected[j]->content);
				u_free (expected[j]);
			}

			u_free (expected);

			return FAILED ("Failed to load content for %s: %s", charsets[i], err->message);
		}

		expected[i] = u_malloc (sizeof (convert_result_t));
		expected[i]->content = content;
		expected[i]->length = length;
	}

	/* test conversion from every charset to every other charset */
	for (i = 0; i < U_N_ELEMENTS (charsets); i++) {
		for (j = 0; j < U_N_ELEMENTS (charsets); j++) {
			converted = u_convert (expected[i]->content, expected[i]->length, charsets[j],
					       charsets[i], NULL, &converted_length, NULL);

			if (converted == NULL) {
				for (k = 0; k < U_N_ELEMENTS (charsets); k++) {
					u_free (expected[k]->content);
					u_free (expected[k]);
				}

				u_free (expected);

				return FAILED ("Failed to convert from %s to %s: NULL", charsets[i], charsets[j]);
			}

			if (converted_length != expected[j]->length) {
				length = expected[j]->length;

				for (k = 0; k < U_N_ELEMENTS (charsets); k++) {
					u_free (expected[k]->content);
					u_free (expected[k]);
				}

				u_free (converted);
				u_free (expected);

				return FAILED ("Failed to convert from %s to %s: expected %u bytes, got %u",
					       charsets[i], charsets[j], length, converted_length);
			}

			for (n = 0; n < converted_length; n++) {
				if (converted[n] != expected[j]->content[n]) {
					c = expected[j]->content[n];

					for (k = 0; k < U_N_ELEMENTS (charsets); k++) {
						u_free (expected[k]->content);
						u_free (expected[k]);
					}

					u_free (converted);
					u_free (expected);

					return FAILED ("Failed to convert from %s to %s: expected 0x%x at offset %u, got 0x%x",
						       charsets[i], charsets[j], c, n, converted[n]);
				}
			}

			u_free (converted);
		}
	}

	for (k = 0; k < U_N_ELEMENTS (charsets); k++) {
		u_free (expected[k]->content);
		u_free (expected[k]);
	}

	u_free (expected);

	return OK;
}


RESULT
test_xdigit ()
{
	static char test_chars[] = {
		'0', '1', '2', '3', '4',
		'5', '6', '7', '8', '9',
		'a', 'b', 'c', 'd', 'e', 'f', 'g',
		'A', 'B', 'C', 'D', 'E', 'F', 'U'};
	static int32_t test_values[] = {
		0, 1, 2, 3, 4,
		5, 6, 7, 8, 9,
		10, 11, 12, 13, 14, 15, -1,
		10, 11, 12, 13, 14, 15, -1};

		int i =0;

		for (i = 0; i < sizeof(test_chars); i++)
			if (u_unichar_xdigit_value ((uunichar)test_chars[i]) != test_values[i])
				return FAILED("Incorrect value %d at index %d", test_values[i], i);

		return OK;
}

static RESULT
ucs4_to_utf16_check_result (const uunichar2 *result_str, const uunichar2 *expected_str,
			    long result_items_read, long expected_items_read,
			    long result_items_written, long expected_items_written,
			    UError* result_error, uboolean expect_error)
{
	long i;
	if (result_items_read != expected_items_read)
		return FAILED("Incorrect number of items read; expected %d, got %d", expected_items_read, result_items_read);
	if (result_items_written != expected_items_written)
		return FAILED("Incorrect number of items written; expected %d, got %d", expected_items_written, result_items_written);
	if (result_error && !expect_error)
		return FAILED("There should not be an error code.");
	if (!result_error && expect_error)
		return FAILED("Unexpected error object.");
	if (expect_error && result_str)
		return FAILED("NULL should be returned when an error occurs.");
	if (!expect_error && !result_str)
		return FAILED("When no error occurs NULL should not be returned.");
	for (i=0; i<expected_items_written;i++) {
		if (result_str [i] != expected_str [i])
			return FAILED("Incorrect value %d at index %d", result_str [i], i);
	}
	if (result_str && result_str[expected_items_written] != '\0')
		return FAILED("Null termination not found at the end of the string.");

	return OK;
}

RESULT
test_ucs4_to_utf16 ()
{
	static uunichar str1[12] = {'H','e','l','l','o',' ','W','o','r','l','d','\0'};
	static uunichar2 exp1[12] = {'H','e','l','l','o',' ','W','o','r','l','d','\0'};
	static uunichar str2[3] = {'h',0x80000000,'\0'};
	static uunichar2 exp2[2] = {'h','\0'};
	static uunichar str3[3] = {'h',0xDA00,'\0'};
	static uunichar str4[3] = {'h',0x10FFFF,'\0'};
	static uunichar2 exp4[4] = {'h',0xdbff,0xdfff,'\0'};
	static uunichar str5[7] = {0xD7FF,0xD800,0xDFFF,0xE000,0x110000,0x10FFFF,'\0'};
	static uunichar2 exp5[5] = {0xD7FF,0xE000,0xdbff,0xdfff,'\0'};
	static uunichar str6[2] = {0x10400, '\0'};
	static uunichar2 exp6[3] = {0xD801, 0xDC00, '\0'};
	static long read_write[12] = {1,1,0,0,0,0,1,1,0,0,1,2};
	uunichar2* res;
	long items_read, items_written, current_write_index;
	UError* err=0;
	RESULT check_result;
	long i;

	res = u_ucs4_to_utf16 (str1, 12, &items_read, &items_written, &err);
	check_result = ucs4_to_utf16_check_result (res, exp1, items_read, 11, items_written, 11, err, FALSE);
	if (check_result) return check_result;
	u_free (res);

	items_read = items_written = 0;
	res = u_ucs4_to_utf16 (str2, 0, &items_read, &items_written, &err);
	check_result = ucs4_to_utf16_check_result (res, exp2, items_read, 0, items_written, 0, err, FALSE);
	if (check_result) return check_result;
	u_free (res);

	items_read = items_written = 0;
	res = u_ucs4_to_utf16 (str2, 1, &items_read, &items_written, &err);
	check_result = ucs4_to_utf16_check_result (res, exp2, items_read, 1, items_written, 1, err, FALSE);
	if (check_result) return check_result;
	u_free (res);

	items_read = items_written = 0;
	res = u_ucs4_to_utf16 (str2, 2, &items_read, &items_written, &err);
	check_result = ucs4_to_utf16_check_result (res, 0, items_read, 1, items_written, 0, err, TRUE);
	u_free (res);
	if (check_result) return check_result;

	items_read = items_written = 0;
	err = 0;
	res = u_ucs4_to_utf16 (str3, 2, &items_read, &items_written, &err);
	check_result = ucs4_to_utf16_check_result (res, 0, items_read, 1, items_written, 0, err, TRUE);
	if (check_result) return check_result;
	u_free (res);

	items_read = items_written = 0;
	err = 0;
	res = u_ucs4_to_utf16 (str4, 5, &items_read, &items_written, &err);
	check_result = ucs4_to_utf16_check_result (res, exp4, items_read, 2, items_written, 3, err, FALSE);
	if (check_result) return check_result;
	u_free (res);

	// This loop tests the bounds of the conversion algorithm
	current_write_index = 0;
	for (i=0;i<6;i++) {
		items_read = items_written = 0;
		err = 0;
		res = u_ucs4_to_utf16 (&str5[i], 1, &items_read, &items_written, &err);
		check_result = ucs4_to_utf16_check_result (res, &exp5[current_write_index],
					items_read, read_write[i*2], items_written, read_write[(i*2)+1], err, !read_write[(i*2)+1]);
		if (check_result) return check_result;
		u_free (res);
		current_write_index += items_written;
	}

	items_read = items_written = 0;
	err = 0;
	res = u_ucs4_to_utf16 (str6, 1, &items_read, &items_written, &err);
	check_result = ucs4_to_utf16_check_result (res, exp6, items_read, 1, items_written, 2, err, FALSE);
	if (check_result) return check_result;
	u_free (res);

	return OK;
}

static RESULT
utf16_to_ucs4_check_result (const uunichar *result_str, const uunichar *expected_str,
			    long result_items_read, long expected_items_read,
			    long result_items_written, long expected_items_written,
			    UError* result_error, uboolean expect_error)
{
	long i;
	if (result_items_read != expected_items_read)
		return FAILED("Incorrect number of items read; expected %d, got %d", expected_items_read, result_items_read);
	if (result_items_written != expected_items_written)
		return FAILED("Incorrect number of items written; expected %d, got %d", expected_items_written, result_items_written);
	if (result_error && !expect_error)
		return FAILED("There should not be an error code.");
	if (!result_error && expect_error)
		return FAILED("Unexpected error object.");
	if (expect_error && result_str)
		return FAILED("NULL should be returned when an error occurs.");
	if (!expect_error && !result_str)
		return FAILED("When no error occurs NULL should not be returned.");
	for (i=0; i<expected_items_written;i++) {
		if (result_str [i] != expected_str [i])
			return FAILED("Incorrect value %d at index %d", result_str [i], i);
	}
	if (result_str && result_str[expected_items_written] != '\0')
		return FAILED("Null termination not found at the end of the string.");

	return OK;
}

RESULT
test_utf16_to_ucs4 ()
{
	static uunichar2 str1[12] = {'H','e','l','l','o',' ','W','o','r','l','d','\0'};
	static uunichar exp1[12] = {'H','e','l','l','o',' ','W','o','r','l','d','\0'};
	static uunichar2 str2[7] = {'H', 0xD800, 0xDC01,0xD800,0xDBFF,'l','\0'};
	static uunichar exp2[3] = {'H',0x00010001,'\0'};
	static uunichar2 str3[4] = {'H', 0xDC00 ,'l','\0'};
	static uunichar exp3[2] = {'H','\0'};
	static uunichar2 str4[20] = {0xDC00,0xDFFF,0xDFF,0xD800,0xDBFF,0xD800,0xDC00,0xD800,0xDFFF,
				     0xD800,0xE000,0xDBFF,0xDBFF,0xDBFF,0xDC00,0xDBFF,0xDFFF,0xDBFF,0xE000,'\0'};
	static uunichar exp4[6] = {0xDFF,0x10000,0x103ff,0x10fc00,0x10FFFF,'\0'};
	static uunichar2 str5[3] = {0xD801, 0xDC00, 0};
	static uunichar exp5[2] = {0x10400, 0};
	static long read_write[33] = {1,0,0,1,0,0,1,1,1,2,1,0,2,2,1,2,2,1,2,1,0,2,1,0,2,2,1,2,2,1,2,1,0};
	uunichar* res;
	long items_read, items_written, current_read_index,current_write_index;
	UError* err=0;
	RESULT check_result;
	long i;

	res = u_utf16_to_ucs4 (str1, 12, &items_read, &items_written, &err);
	check_result = utf16_to_ucs4_check_result (res, exp1, items_read, 11, items_written, 11, err, FALSE);
	if (check_result) return check_result;
	u_free (res);

	items_read = items_written = 0;
	res = u_utf16_to_ucs4 (str2, 0, &items_read, &items_written, &err);
	check_result = utf16_to_ucs4_check_result (res, exp2, items_read, 0, items_written, 0, err, FALSE);
	if (check_result) return check_result;
	u_free (res);

	items_read = items_written = 0;
	res = u_utf16_to_ucs4 (str2, 1, &items_read, &items_written, &err);
	check_result = utf16_to_ucs4_check_result (res, exp2, items_read, 1, items_written, 1, err, FALSE);
	if (check_result) return check_result;
	u_free (res);

	items_read = items_written = 0;
	res = u_utf16_to_ucs4 (str2, 2, &items_read, &items_written, &err);
	check_result = utf16_to_ucs4_check_result (res, exp2, items_read, 1, items_written, 1, err, FALSE);
	if (check_result) return check_result;
	u_free (res);

	items_read = items_written = 0;
	res = u_utf16_to_ucs4 (str2, 3, &items_read, &items_written, &err);
	check_result = utf16_to_ucs4_check_result (res, exp2, items_read, 3, items_written, 2, err, FALSE);
	if (check_result) return check_result;
	u_free (res);

	items_read = items_written = 0;
	res = u_utf16_to_ucs4 (str2, 4, &items_read, &items_written, &err);
	check_result = utf16_to_ucs4_check_result (res, exp2, items_read, 3, items_written, 2, err, FALSE);
	if (check_result) return check_result;
	u_free (res);

	items_read = items_written = 0;
	res = u_utf16_to_ucs4 (str2, 5, &items_read, &items_written, &err);
	check_result = utf16_to_ucs4_check_result (res, exp2, items_read, 4, items_written, 0, err, TRUE);
	if (check_result) return check_result;
	u_free (res);

	items_read = items_written = 0;
	err = 0;
	res = u_utf16_to_ucs4 (str3, 5, &items_read, &items_written, &err);
	check_result = utf16_to_ucs4_check_result (res, exp3, items_read, 1, items_written, 0, err, TRUE);
	if (check_result) return check_result;
	u_free (res);

	// This loop tests the bounds of the conversion algorithm
	current_read_index = current_write_index = 0;
	for (i=0;i<11;i++) {
		items_read = items_written = 0;
		err = 0;
		res = u_utf16_to_ucs4 (&str4[current_read_index], read_write[i*3], &items_read, &items_written, &err);
		check_result = utf16_to_ucs4_check_result (res, &exp4[current_write_index], items_read,
					     read_write[(i*3)+1], items_written, read_write[(i*3)+2], err,
					     !read_write[(i*3)+2]);
		if (check_result) return check_result;
		u_free (res);
		current_read_index += read_write[i*3];
		current_write_index += items_written;
	}

	items_read = items_written = 0;
	err = 0;
	res = u_utf16_to_ucs4 (str5, 2, &items_read, &items_written, &err);
	check_result = utf16_to_ucs4_check_result (res, exp5, items_read, 2, items_written, 1, err, FALSE);
	if (check_result) return check_result;
	u_free (res);

	return OK;
}
RESULT
test_utf8_strlen ()
{
	char word1 [] = {0xC2, 0x82,0x45,0xE1, 0x81, 0x83,0x58,0xF1, 0x82, 0x82, 0x82,'\0'};//Valid, len = 5
	char word2 [] = {0xF1, 0x82, 0x82, 0x82,0xC2, 0x82,0x45,0xE1, 0x81, 0x83,0x58,'\0'};//Valid, len = 5
	char word3 [] = {'h','e',0xC2, 0x82,0x45,'\0'};										//Valid, len = 4
	char word4 [] = {0x62,0xC2, 0x82,0x45,0xE1, 0x81, 0x83,0x58,'\0'}; 					//Valid, len = 5

	long len = 0;

	//Test word1
	len = u_utf8_strlen (word1,-1);
	if (len != 5)
		return FAILED ("Word1 expected length of 5, but was %i", len);
	//Do tests with different values for max parameter.
	len = u_utf8_strlen (word1,1);
	if (len != 0)
		return FAILED ("Word1, max = 1, expected length of 0, but was %i", len);
	len = u_utf8_strlen (word1,2);
	if (len != 1)
		return FAILED ("Word1, max = 1, expected length of 1, but was %i", len);
	len = u_utf8_strlen (word1,3);
	if (len != 2)
		return FAILED ("Word1, max = 2, expected length of 2, but was %i", len);

	//Test word2
	len = u_utf8_strlen (word2,-1);
	if (len != 5)
		return FAILED ("Word2 expected length of 5, but was %i", len);

	//Test word3
	len = u_utf8_strlen (word3,-1);
	if (len != 4)
		return FAILED ("Word3 expected length of 4, but was %i", len);

	//Test word4
	len = u_utf8_strlen (word4,-1);
	if (len != 5)
		return FAILED ("Word4 expected length of 5, but was %i", len);

	//Test null case
	len = u_utf8_strlen(NULL,0);
	if (len != 0)
		return FAILED ("Expected passing null to result in a length of 0");
	return OK;
}

RESULT
test_utf8_get_char()
{
	char word1 [] = {0xC2, 0x82,0x45,0xE1, 0x81, 0x83,0x58,0xF1, 0x82, 0x82, 0x82,'\0'}; //Valid, len = 5

	uunichar value = u_utf8_get_char (&word1 [0]);
	if (value != 0x82UL)
		return FAILED ("Expected value of 0x82, but was %x", value);
	value = u_utf8_get_char (&word1 [2]);
	if (value != 0x45UL)
		return FAILED ("Expected value of 0x45, but was %x", value);
	value = u_utf8_get_char (&word1 [3]);
	if (value != 0x1043UL)
		return FAILED ("Expected value of 0x1043, but was %x", value);
	value = u_utf8_get_char (&word1 [6]);
	if (value != 0x58UL)
		return FAILED ("Expected value of 0x58, but was %x", value);
	value = u_utf8_get_char (&word1 [7]);
	if (value != 0x42082UL)
		return FAILED ("Expected value of 0x42082, but was %x", value);

	return OK;
}

RESULT
test_utf8_next_char()
{
	char word1 [] = {0xC2, 0x82,0x45,0xE1, 0x81, 0x83,0x58,0xF1, 0x82, 0x82, 0x82,'\0'}; //Valid, len = 5
	char word2 [] = {0xF1, 0x82, 0x82, 0x82,0xC2, 0x82,0x45,0xE1, 0x81, 0x83,0x58,'\0'}; //Valid, len = 5
	char word1ExpectedValues [] = {0xC2, 0x45,0xE1, 0x58, 0xF1};
	char word2ExpectedValues [] = {0xF1, 0xC2, 0x45, 0xE1, 0x58};

	char* ptr = word1;
	int count = 0;
	//Test word1
	while (*ptr != 0) {
		if (count > 4)
			return FAILED ("Word1 has gone past its expected length");
		if (*ptr != word1ExpectedValues[count])
			return FAILED ("Word1 has an incorrect next_char at index %i", count);
		ptr = u_utf8_next_char (ptr);
		count++;
	}

	//Test word2
	count = 0;
	ptr = word2;
	while (*ptr != 0) {
		if (count > 4)
			return FAILED ("Word2 has gone past its expected length");
		if (*ptr != word2ExpectedValues[count])
			return FAILED ("Word2 has an incorrect next_char at index %i", count);
		ptr = u_utf8_next_char (ptr);
		count++;
	}

	return OK;
}

RESULT
test_utf8_validate()
{
	char invalidWord1 [] = {0xC3, 0x82, 0xC1,0x90,'\0'}; //Invalid, 1nd oct Can't be 0xC0 or 0xC1
	char invalidWord2 [] = {0xC1, 0x89, 0x60, '\0'}; //Invalid, 1st oct can not be 0xC1
	char invalidWord3 [] = {0xC2, 0x45,0xE1, 0x81, 0x83,0x58,'\0'}; //Invalid, oct after 0xC2 must be > 0x80

	char validWord1 [] = {0xC2, 0x82, 0xC3,0xA0,'\0'}; //Valid
	char validWord2 [] = {0xC2, 0x82,0x45,0xE1, 0x81, 0x83,0x58,0xF1, 0x82, 0x82, 0x82,'\0'}; //Valid

	const char* end;
	uboolean retVal = u_utf8_validate (invalidWord1, -1, &end);
	if (retVal != FALSE)
		return FAILED ("Expected invalidWord1 to be invalid");
	if (end != &invalidWord1 [2])
		return FAILED ("Expected end parameter to be pointing to invalidWord1[2]");

	end = NULL;
	retVal = u_utf8_validate (invalidWord2, -1, &end);
	if (retVal != FALSE)
		return FAILED ("Expected invalidWord2 to be invalid");
	if (end != &invalidWord2 [0])
		return FAILED ("Expected end parameter to be pointing to invalidWord2[0]");

	end = NULL;
	retVal = u_utf8_validate (invalidWord3, -1, &end);
	if (retVal != FALSE)
		return FAILED ("Expected invalidWord3 to be invalid");
	if (end != &invalidWord3 [0])
		return FAILED ("Expected end parameter to be pointing to invalidWord3[1]");

	end = NULL;
	retVal = u_utf8_validate (validWord1, -1, &end);
	if (retVal != TRUE)
		return FAILED ("Expected validWord1 to be valid");
	if (end != &validWord1 [4])
		return FAILED ("Expected end parameter to be pointing to validWord1[4]");

	end = NULL;
	retVal = u_utf8_validate (validWord2, -1, &end);
	if (retVal != TRUE)
		return FAILED ("Expected validWord2 to be valid");
	if (end != &validWord2 [11])
		return FAILED ("Expected end parameter to be pointing to validWord2[11]");
	return OK;
}

long
utf8_byteslen (const char *src)
{
	int i = 0;
	do {
		if (src [i] == '\0')
			return i;
		i++;
	} while (TRUE);
}

RESULT
test_utf8_strcase_each (const char *src, const char *expected, uboolean strup)
{
	char *tmp;
	long len, len2;
	RESULT r;

	len = utf8_byteslen (src);
	tmp = strup ? u_utf8_strup (src, len) : u_utf8_strdown (src, len);
	len2 = utf8_byteslen (tmp);
	r = compare_strings_utf8_RESULT (expected, tmp, len < len2 ? len2 : len);
	u_free (tmp);
	return r;
}

RESULT
test_utf8_strup_each (const char *src, const char *expected)
{
	return test_utf8_strcase_each (src, expected, TRUE);
}

RESULT
test_utf8_strdown_each (const char *src, const char *expected)
{
	return test_utf8_strcase_each (src, expected, FALSE);
}

/*
 * u_utf8_strup
 */
RESULT
test_utf8_strup ()
{
	RESULT r;

	if ((r = test_utf8_strup_each ("aBc", "ABC")) != OK)
		return r;
	if ((r = test_utf8_strup_each ("x86-64", "X86-64")) != OK)
		return r;
	// U+3B1 U+392 -> U+391 U+392
	if ((r = test_utf8_strup_each ("\xCE\xB1\xCE\x92", "\xCE\x91\xCE\x92")) != OK)
		return r;
	// U+FF21 -> U+FF21
	if ((r = test_utf8_strup_each ("\xEF\xBC\xA1", "\xEF\xBC\xA1")) != OK)
		return r;
	// U+FF41 -> U+FF21
	if ((r = test_utf8_strup_each ("\xEF\xBD\x81", "\xEF\xBC\xA1")) != OK)
		return r;
	// U+10428 -> U+10400
	if ((r = test_utf8_strup_each ("\xF0\x90\x90\xA8", "\xF0\x90\x90\x80")) != OK)
		return r;

	return OK;
}

/*
 * u_utf8_strdown
 */
RESULT
test_utf8_strdown ()
{
	RESULT r;

	if ((r = test_utf8_strdown_each ("aBc", "abc")) != OK)
		return r;
	if ((r = test_utf8_strdown_each ("X86-64", "x86-64")) != OK)
		return r;
	// U+391 U+3B2 -> U+3B1 U+3B2
	if ((r = test_utf8_strdown_each ("\xCE\x91\xCE\xB2", "\xCE\xB1\xCE\xB2")) != OK)
		return r;
/*
	// U+FF41 -> U+FF41
	if ((r = test_utf8_strdown_each ("\xEF\xBC\x81", "\xEF\xBC\x81")) != OK)
		return r;
	// U+FF21 -> U+FF41
	if ((r = test_utf8_strdown_each ("\xEF\xBC\xA1", "\xEF\xBD\x81")) != OK)
		return r;
	// U+10400 -> U+10428
	if ((r = test_utf8_strdown_each ("\xF0\x90\x90\x80", "\xF0\x90\x90\xA8")) != OK)
		return r;
*/
	return OK;
}

/*
 * test initialization
 */

static Test utf8_tests [] = {
	{"u_utf16_to_utf8", test_utf16_to_utf8},
	{"u_utf8_to_utf16", test_utf8_to_utf16},
	{"u_utf8_to_utf16_with_nuls", test_utf8_to_utf16_with_nuls},
	{"u_utf8_seq", test_utf8_seq},
	{"u_convert", test_convert },
	{"u_unichar_xdigit_value", test_xdigit },
	{"u_ucs4_to_utf16", test_ucs4_to_utf16 },
	{"u_utf16_to_ucs4", test_utf16_to_ucs4 },
	{"u_utf8_strlen", test_utf8_strlen },
	{"u_utf8_get_char", test_utf8_get_char },
	{"u_utf8_next_char", test_utf8_next_char },
	{"u_utf8_validate", test_utf8_validate },
	{"u_utf8_strup", test_utf8_strup},
	{"u_utf8_strdown", test_utf8_strdown},
	{NULL, NULL}
};

DEFINE_TEST_GROUP_INIT(utf8_tests_init, utf8_tests)


