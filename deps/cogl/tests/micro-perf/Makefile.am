include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/clib/src \
	-I$(top_builddir)/clib/src \
	-I$(top_srcdir)/deps/cogl \
	-I$(top_builddir)/deps/cogl

test_conformance_CPPFLAGS = \
	-DCOGL_ENABLE_EXPERIMENTAL_API \
	-DCOGL_DISABLE_DEPRECATED \
	-DTESTS_DATADIR=\""$(top_srcdir)/deps/cogl/tests/data"\"


noinst_PROGRAMS =

if COGL_USE_GLIB
noinst_PROGRAMS += test-journal
endif

AM_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS)

common_ldadd = \
	$(COGL_DEP_LIBS) \
	$(top_builddir)/clib/src/libclib.la \
	$(top_builddir)/deps/cogl/cogl/libcogl2.la \
	$(LIBM)

test_journal_SOURCES = test-journal.c
test_journal_LDADD = $(common_ldadd)
