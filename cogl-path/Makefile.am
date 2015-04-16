CLEANFILES =
DISTCLEANFILES =

EXTRA_DIST =

# tesselator sources
cogl_tesselator_sources = \
	tesselator/dict-list.h 	\
	tesselator/dict.c \
	tesselator/dict.h \
	tesselator/geom.c \
	tesselator/geom.h \
	tesselator/gluos.h \
	tesselator/memalloc.h \
	tesselator/mesh.c \
	tesselator/mesh.h \
	tesselator/normal.c \
	tesselator/normal.h \
	tesselator/priorityq-heap.h \
	tesselator/priorityq-sort.h \
	tesselator/priorityq.c \
	tesselator/priorityq.h \
	tesselator/render.c \
	tesselator/render.h \
	tesselator/sweep.c \
	tesselator/sweep.h \
	tesselator/tess.c \
	tesselator/tess.h \
	tesselator/tesselator.h \
	tesselator/tessmono.c \
	tesselator/tessmono.h \
	tesselator/GL/glu.h

source_c = \
	$(cogl_tesselator_sources) \
	cogl-path-private.h \
	cogl-path.c

EXTRA_DIST += \
	tesselator/README \
	tesselator/priorityq-heap.c \
	cogl-path.symbols


source_h = cogl-path.h

noinst_LTLIBRARIES = libcogl-path.la

libcogl_path_la_SOURCES = $(source_c) $(source_h)
libcogl_path_la_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_GST_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS) $(MAINTAINER_CFLAGS)
libcogl_path_la_LIBADD = $(top_builddir)/clib/src/libclib.la
libcogl_path_la_LIBADD += $(top_builddir)/cogl/libcogl2.la
libcogl_path_la_LIBADD += $(COGL_DEP_LIBS) $(COGL_GST_DEP_LIBS) $(COGL_EXTRA_LDFLAGS)
libcogl_path_la_LDFLAGS = -static

AM_CPPFLAGS = \
	-DCG_COMPILATION \
	-DG_LOG_DOMAIN=\"CoglPath\" \
	-I$(top_srcdir)/libuv/include \
	-I$(top_srcdir)/clib/src \
	-I$(top_builddir)/clib/src \
	-I$(srcdir)/tesselator \
	-I$(top_srcdir)/cogl \
	-I$(top_builddir)/cogl \
	-I$(top_srcdir)/cogl/winsys \
	-I$(top_srcdir) \
	-I$(top_builddir)
