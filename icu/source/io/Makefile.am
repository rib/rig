
AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/common \
	-I$(top_builddir) \
	-I$(top_builddir)/common \
	-I$(top_srcdir)/i18n \
	$(U_CPPFLAGS) \
	-DU_IO_IMPLEMENTATION

AM_CFLAGS = $(U_CFLAGS)
AM_CXXFLAGS = $(U_CXXFLAGS)

include $(srcdir)/Makefile.sources

lib_LTLIBRARIES = libicuio.la

libicuio_la_LIBADD =
libicuio_la_LDFLAGS =
libicuio_la_SOURCES = $(icu_io_sources)
