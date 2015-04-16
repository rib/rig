
noinst_LTLIBRARIES = libtest-fixtures.la

libtest_fixtures_la_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/libuv/include \
	-I$(top_builddir)/cogl \
	-I$(top_srcdir)/clib/src \
	-I$(top_builddir)/clib/src \
	-Wall \
	$(NULL)

libtest_fixtures_la_CPPFLAGS += \
	-DCG_DISABLE_DEPRECATED \
	-DTESTS_DATADIR=\""$(top_srcdir)/tests/data"\" \
	-DCG_COMPILATION

libtest_fixtures_la_CFLAGS = -g3 -O0 $(COGL_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS)

libtest_fixtures_la_SOURCES = \
	test.h \
	test-fixtures.h \
	test-fixtures.c \
	test-cg-fixtures.h \
	test-cg-fixtures.c

android_modulesdir = $(datadir)/android_modules/test-fixtures/
android_modules_DATA = $(libtest_fixtures_la_SOURCES)
