
noinst_LTLIBRARIES = libtest-fixtures.la

libtest_fixtures_la_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_builddir)/cglib \
	-I$(top_srcdir)/clib/clib \
	-I$(top_builddir)/clib/clib \
	-Wall \
	$(NULL)

libtest_fixtures_la_CPPFLAGS += \
	-DTESTS_DATADIR=\""$(top_srcdir)/tests/data"\" \
	-DCG_COMPILATION

libtest_fixtures_la_CFLAGS = -g3 -O0 $(CG_DEP_CFLAGS) $(CG_EXTRA_CFLAGS)

libtest_fixtures_la_SOURCES = \
	test.h \
	test-fixtures.h \
	test-fixtures.c \
	test-cg-fixtures.h \
	test-cg-fixtures.c

android_modulesdir = $(datadir)/android_modules/test-fixtures/
android_modules_DATA = $(libtest_fixtures_la_SOURCES)

dist_source_dir = $(datadir)/src/test-fixtures
dist_source = \
	$(libtest_fixtures_la_SOURCES)

# Use a custom install-data-local to install the source so that
# we can preserve the directory structure...
install-data-local:
	for f in $(dist_source); do \
	d=`dirname $$f`; \
	$(mkinstalldirs) $(DESTDIR)$(dist_source_dir)/$$d; \
	$(INSTALL_DATA) $(srcdir)/$$f $(DESTDIR)$(dist_source_dir)/$$f; \
	done
