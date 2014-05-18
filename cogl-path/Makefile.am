include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL =

CLEANFILES =
DISTCLEANFILES =

EXTRA_DIST =

# tesselator sources
cogl_tesselator_sources = \
	$(srcdir)/tesselator/dict-list.h 	\
	$(srcdir)/tesselator/dict.c 		\
	$(srcdir)/tesselator/dict.h 		\
	$(srcdir)/tesselator/geom.c 		\
	$(srcdir)/tesselator/geom.h 		\
	$(srcdir)/tesselator/gluos.h 		\
	$(srcdir)/tesselator/memalloc.h 	\
	$(srcdir)/tesselator/mesh.c 		\
	$(srcdir)/tesselator/mesh.h 		\
	$(srcdir)/tesselator/normal.c 		\
	$(srcdir)/tesselator/normal.h 		\
	$(srcdir)/tesselator/priorityq-heap.h 	\
	$(srcdir)/tesselator/priorityq-sort.h 	\
	$(srcdir)/tesselator/priorityq.c 	\
	$(srcdir)/tesselator/priorityq.h 	\
	$(srcdir)/tesselator/render.c 		\
	$(srcdir)/tesselator/render.h 		\
	$(srcdir)/tesselator/sweep.c 		\
	$(srcdir)/tesselator/sweep.h 		\
	$(srcdir)/tesselator/tess.c 		\
	$(srcdir)/tesselator/tess.h 		\
	$(srcdir)/tesselator/tesselator.h 	\
	$(srcdir)/tesselator/tessmono.c 	\
	$(srcdir)/tesselator/tessmono.h 	\
	$(srcdir)/tesselator/GL/glu.h 		\
	$(NULL)

source_c = \
	$(cogl_tesselator_sources) \
	cogl-path-private.h \
	cogl-path.c \
	$(NULL)

EXTRA_DIST += \
	$(srcdir)/tesselator/README 		\
	$(srcdir)/tesselator/priorityq-heap.c	\
	$(srcdir)/cogl-path.symbols		\
	$(NULL)


source_h = \
	cogl-path.h \
	$(NULL)

lib_LTLIBRARIES = libcogl-path.la

libcogl_path_la_SOURCES = $(source_c) $(source_h)
libcogl_path_la_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_GST_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS) $(MAINTAINER_CFLAGS)
libcogl_path_la_LIBADD = $(top_builddir)/deps/clib/src/libclib.la
libcogl_path_la_LIBADD += $(top_builddir)/cogl/libcogl2.la
libcogl_path_la_LIBADD += $(COGL_DEP_LIBS) $(COGL_GST_DEP_LIBS) $(COGL_EXTRA_LDFLAGS)
libcogl_path_la_LDFLAGS = \
	-export-dynamic \
	-export-symbols-regex "^cogl_(framebuffer|path)_.*" \
	-no-undefined \
	-version-info @COGL_LT_CURRENT@:@COGL_LT_REVISION@:@COGL_LT_AGE@ \
	-rpath $(libdir)

AM_CPPFLAGS = \
	-DCOGL_COMPILATION		\
	-DG_LOG_DOMAIN=\"CoglPath\"	\
	-I$(top_srcdir)/deps/clib/src	\
	-I$(top_builddir)/deps/clib/src	\
	-I$(srcdir)/tesselator		\
	-I$(top_srcdir)/cogl		\
	-I$(top_builddir)/cogl		\
	-I$(top_srcdir)/cogl/winsys	\
	-I$(top_srcdir)			\
	-I$(top_builddir)

cogl_pathheadersdir = $(includedir)/cogl2/cogl-path
cogl_pathheaders_HEADERS = $(source_h)

pc_files = cogl-path.pc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pc_files)

EXTRA_DIST += cogl-path.pc.in
DISTCLEANFILES += $(pc_files)

dist-hook:	\
	$(top_builddir)/build/win32/vs9/cogl-path.vcproj	\
	$(top_builddir)/build/win32/vs10/cogl-path.vcxproj	\
	$(top_builddir)/build/win32/vs10/cogl-path.vcxproj.filters

MSVC_PROJECT = cogl-path
MSVC_PROJECT_SRCDIR = cogl-path
MSVC_PROJECT_SRCS = $(source_c)
MSVC_PROJECT_EXCLUDES = dummy

include $(top_srcdir)/build/Makefile-msvcproj.mak
