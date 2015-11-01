
AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(U_CPPFLAGS) \
	-DU_COMMON_IMPLEMENTATION

AM_CFLAGS = $(U_CFLAGS)
AM_CXXFLAGS = $(U_CXXFLAGS)

include $(srcdir)/Makefile.sources

lib_LTLIBRARIES = libicuuc.la

libicuuc_la_LIBADD =
libicuuc_la_LDFLAGS =
libicuuc_la_SOURCES = $(icu_common_sources)

unicode_includedir = $(includedir)/unicode
unicode_include_HEADERS = $(icu_unicode_headers)
