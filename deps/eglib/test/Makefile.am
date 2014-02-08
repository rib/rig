EXTRA_DIST = UTF-8.txt UTF-16BE.txt UTF-16LE.txt UTF-32BE.txt UTF-32LE.txt

SOURCES = \
	test.c 		\
	test.h 		\
	tests.h 	\
	driver.c 	\
	hashtable.c 	\
	string-util.c 	\
	string.c 	\
	slist.c 	\
	sizes.c		\
	ptrarray.c 	\
	list.c		\
	array.c		\
	fake.c		\
	path.c		\
	queue.c		\
	shell.c		\
	spawn.c		\
	timer.c		\
	file.c		\
	pattern.c	\
	dir.c		\
	markup.c	\
	unicode.c	\
	utf8.c		\
	endian.c	\
	module.c	\
	memory.c

AM_CPPFLAGS =			\
	-I$(top_builddir)	\
	-I$(top_srcdir)		\
	-I$(srcdir)		\
	-Wall			\
	$(NULL)

test_eglib_SOURCES = $(SOURCES)

test_eglib_CFLAGS = -DEGLIB_TESTS=1 -I$(srcdir)/../src -I../src -DDRIVER_NAME=\"EGlib\"
test_eglib_LDADD = ../src/libeglib.la $(LIBICONV)

run-eglib: all
	srcdir=`readlink -f $(srcdir)` ./test-eglib

noinst_PROGRAMS = test-eglib

run-both: run-eglib

MAINTAINERCLEANFILES = Makefile.in
