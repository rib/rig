include $(top_srcdir)/build/autotools/Makefile.am.silent

#include Makefile.sources

EXTRA_DIST =

noinst_LTLIBRARIES = libyuv.la

#libyuv_la_LIBADD =
libyuv_la_SOURCES = \
	row_any.cc \
	row_common.cc \
	convert_argb.cc \
	row.h \
	convert_from_argb.cc \
	row_neon.cc \
	row_mips.cc \
	compare_win.cc \
	scale_gcc.cc \
	compare_common.cc \
	scale_argb.h \
	compare_gcc.cc \
	rotate_mips.cc \
	scale_neon.cc \
	scale_argb.cc \
	libyuv.h \
	rotate_argb.cc \
	video_common.cc \
	convert.h \
	compare_neon.cc \
	rotate_neon.cc \
	version.h \
	convert.cc \
	scale_mips.cc \
	scale.cc \
	scale_any.cc \
	convert_from.h \
	convert_from_argb.h \
	compare.h \
	scale_neon64.cc \
	scale_win.cc \
	mjpeg_decoder.cc \
	convert_from.cc \
	mjpeg_decoder.h \
	row_win.cc \
	scale.h \
	rotate_neon64.cc \
	convert_argb.h \
	cpu_id.h \
	compare.cc \
	cpu_id.cc \
	video_common.h \
	scale_common.cc \
	mjpeg_validate.cc \
	compare_neon64.cc \
	planar_functions.cc \
	rotate.cc \
	basic_types.h \
	convert_jpeg.cc \
	row_neon64.cc \
	row_gcc.cc \
	rotate.h \
	planar_functions.h \
	scale_row.h \
	rotate_argb.h \
	convert_to_i420.cc \
	convert_to_argb.cc

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(srcdir) \
	-Wall \
	$(RIG_EXTRA_CFLAGS) \
	$(NULL)

android_moduledir = $(datadir)/android_modules/libyuv
android_module_DATA = \
	$(libyuv_la_SOURCES)

#	android/config.h
#	Android.mk

dist_source_dir = $(datadir)/src/clib
dist_source = \
	$(libyuv_la_SOURCES)

#	Makefile.emscripten

# Use a custom install-data-local to install the source so that
# we can preserve the directory structure...
install-data-local:
	for f in $(dist_source); do \
	d=`dirname $$f`; \
	$(mkinstalldirs) $(DESTDIR)$(dist_source_dir)/$$d; \
	$(INSTALL_DATA) $(srcdir)/$$f $(DESTDIR)$(dist_source_dir)/$$f; \
	done

