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
	dir.c		\
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

test_ulib_SOURCES = $(SOURCES)

test_ulib_CFLAGS = -DULIB_TESTS=1 -I$(srcdir)/../src -I../src -DDRIVER_NAME=\"EGlib\"
test_ulib_LDADD = ../src/libulib.la $(LTLIBICONV)

run-ulib: all
	srcdir=`readlink -f $(srcdir)` ./test-ulib

noinst_PROGRAMS = test-ulib

run-both: run-ulib

MAINTAINERCLEANFILES = Makefile.in
