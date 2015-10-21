include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/clib/clib \
	-I$(top_builddir)/clib/clib \
	-I$(top_srcdir)/cglib \
	-I$(top_builddir)/cglib

test_conformance_CPPFLAGS = \
	-DTESTS_DATADIR=\""$(top_srcdir)/tests/data"\"


noinst_PROGRAMS =

if USE_GLIB
noinst_PROGRAMS += test-journal
endif

noinst_PROGRAMS += test-instancing

AM_CFLAGS = $(CG_DEP_CFLAGS) $(RIG_EXTRA_CFLAGS)

common_ldadd = \
	$(CG_DEP_LIBS) \
	$(top_builddir)/clib/clib/libclib.la \
	$(top_builddir)/cglib/cglib/libcglib.la \
	$(LIBM)

test_journal_SOURCES = test-journal.c
test_journal_LDADD = $(common_ldadd)

test_instancing_SOURCES = test-instancing.c
test_instancing_LDADD = $(common_ldadd)
