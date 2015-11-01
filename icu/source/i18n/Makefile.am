AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/common \
	-I$(top_builddir) \
	-I$(top_builddir)/common \
	$(U_CPPFLAGS) \
	-DU_I18N_IMPLEMENTATION

AM_CFLAGS = $(U_CFLAGS)
AM_CXXFLAGS = $(U_CXXFLAGS)

include $(srcdir)/Makefile.sources

lib_LTLIBRARIES = libicui18n.la

libicui18n_la_LIBADD =
libicui18n_la_LDFLAGS =
libicui18n_la_SOURCES = $(icu_i18n_sources)

unicode_includedir = $(includedir)/unicode
unicode_include_HEADERS = $(icu_i18n_unicode_headers)
