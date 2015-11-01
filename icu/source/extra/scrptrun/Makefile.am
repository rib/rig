EXTRA_DIST =

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/common \
	-I$(top_srcdir)/i18n \
	-I$(top_builddir) \
	-I$(top_builddir)/common \
	$(U_CPPFLAGS)

AM_CFLAGS = $(U_CFLAGS)
AM_CXXFLAGS = $(U_CXXFLAGS)


noinst_PROGRAMS = srtest

srtest_SOURCES = scrptrun.h scrptrun.cpp srtest.cpp
srtest_LDADD = $(top_builddir)/i18n/libicui18n.la \
	       $(top_builddir)/common/libicuuc.la \
	       $(top_builddir)/stubdata/libicudata.la
EXTRA_DIST += readme.html
EXTRA_DIST += scrptrun.1.in
EXTRA_DIST += scrptrun.vcxproj scrptrun.vcxproj.filters
EXTRA_DIST += srtest.dsp srtest.dsw

