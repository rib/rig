include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

CLEANFILES =
DISTCLEANFILES =

EXTRA_DIST =

source_c = \
	cogl-gst-video-sink.c \
	$(NULL)

source_h = \
	cogl-gst.h \
	cogl-gst-video-sink.h \
	$(NULL)

lib_LTLIBRARIES = libcogl-gst.la

libcogl_gst_la_SOURCES = $(source_c) $(source_h)
libcogl_gst_la_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_GST_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS) $(MAINTAINER_CFLAGS)
libcogl_gst_la_LIBADD = $(top_builddir)/clib/clib/libclib.la
libcogl_gst_la_LIBADD += $(top_builddir)/cogl/libcg.la
libcogl_gst_la_LIBADD += $(COGL_DEP_LIBS) $(COGL_GST_DEP_LIBS) $(COGL_EXTRA_LDFLAGS)
libcogl_gst_la_LDFLAGS = -static

AM_CPPFLAGS = \
	-DCG_COMPILATION \
	-DG_LOG_DOMAIN=\"CoglGst\" \
	-I$(top_srcdir)/clib/clib \
	-I$(top_builddir)/clib/clib \
	-I$(top_srcdir)/cogl \
	-I$(top_builddir)/cogl \
	-I$(top_srcdir)/cogl/winsys \
	-I$(top_srcdir)	\
	-I$(top_builddir)

plugin_source_c = \
	$(srcdir)/cogl-gst-plugin.c \
	$(NULL)

libgstcogl_la_SOURCES =	\
	$(plugin_source_c) \
	$(NULL)

plugin_LTLIBRARIES = libgstcogl.la

libgstcogl_la_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_GST_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS) $(MAINTAINER_CFLAGS)
libgstcogl_la_LIBADD = \
	$(top_builddir)/cogl/libcg.la \
	$(builddir)/libcogl-gst.la
libgstcogl_la_LIBADD += $(COGL_DEP_LIBS) $(COGL_GST_DEP_LIBS) $(COGL_EXTRA_LDFLAGS)
libgstcogl_la_LDFLAGS =	\
	-avoid-version -no-undefined		\
	$(NULL)
