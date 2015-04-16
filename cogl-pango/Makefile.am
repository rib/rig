include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

CLEANFILES =
DISTCLEANFILES =

EXTRA_DIST =

source_c = \
	cogl-pango-display-list.c   \
	cogl-pango-fontmap.c        \
	cogl-pango-render.c         \
	cogl-pango-glyph-cache.c    \
	cogl-pango-pipeline-cache.c \
	$(NULL)

source_h = cogl-pango.h

source_h_priv = \
	cogl-pango-display-list.h   \
	cogl-pango-private.h        \
	cogl-pango-glyph-cache.h    \
	cogl-pango-pipeline-cache.h \
	$(NULL)

lib_LTLIBRARIES = libcogl-pango2.la

libcogl_pango2_la_SOURCES = $(source_c) $(source_h) $(source_h_priv)
libcogl_pango2_la_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_PANGO_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS) $(MAINTAINER_CFLAGS)
libcogl_pango2_la_LIBADD = $(top_builddir)/clib/src/libclib.la
libcogl_pango2_la_LIBADD += $(top_builddir)/cogl/libcogl2.la
libcogl_pango2_la_LIBADD += $(COGL_DEP_LIBS) $(COGL_PANGO_DEP_LIBS) $(COGL_EXTRA_LDFLAGS)
libcogl_pango2_la_LDFLAGS = -static

AM_CPPFLAGS = \
	-DCG_COMPILATION \
	-DG_LOG_DOMAIN=\"CoglPango\" \
	-I$(top_srcdir)/libuv/include \
	-I$(top_srcdir)/clib/src \
	-I$(top_builddir)/clib/src \
	-I$(top_srcdir)/cogl \
	-I$(top_builddir)/cogl \
	-I$(top_srcdir)/cogl/winsys \
	-I$(top_srcdir) \
	-I$(top_builddir)
