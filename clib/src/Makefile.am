include $(top_srcdir)/build/autotools/Makefile.am.silent

include Makefile.sources

EXTRA_DIST =

if EMSCRIPTEN
lib_LTLIBRARIES = libclib.la
else
if UNIT_TESTS
lib_LTLIBRARIES = libclib.la
else
noinst_LTLIBRARIES = libclib.la
endif
endif #EMSCRIPTEN


libclib_la_LIBADD = $(CLIB_DEP_LIBS)

EXTRA_DIST += sfmt/README.txt sfmt/LICENSE.txt

libclib_la_SOURCES = \
	$(clib_common_source) \
	$(sfmt_source)

unix_files = cdate-unix.c cdir-unix.c cmisc-unix.c \
	     cxdg-unix.c
threads_files = ctls.c

if OS_UNIX
libclib_la_SOURCES += $(unix_files) $(threads_files)
endif

if OS_WIN32
libclib_la_SOURCES += \
	clib-config.hw \
	cdate-win32.c cdir-win32.c cmisc-win32.c \
	mkstemp.c \
	vasprintf.c \
	fmemopen.h \
	fmemopen.c
libclib_la_SOURCES += $(threads_files)
libclib_la_LIBADD += -lm $(LTLIBICONV) -lpsapi
endif

if OS_DARWIN
libclib_la_SOURCES += \
	fmemopen.h \
	fmemopen.c
endif

if EMSCRIPTEN
libclib_la_SOURCES += cmisc-emscripten.c
endif

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(srcdir) \
	-I$(srcdir)/sfmt \
	-Wall \
	$(CLIB_DEP_CFLAGS) \
	$(RIG_EXTRA_CFLAGS) \
	-DSFMT_MEXP=19937 \
	$(NULL)

#libclib_la_LIBADD += $(LTLIBICONV)
#libclib_la_LDFLAGS = -static

#MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST += clib-config.h.in

android_moduledir = $(datadir)/android_modules/clib
android_module_DATA = \
	$(clib_common_source) \
	$(sfmt_source) \
	$(unix_files) \
	$(threads_files) \
	vasprintf.c \
	Android.mk \
	android/clib-config.h
