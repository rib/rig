include Makefile.sources

EXTRA_DIST =

if UNIT_TESTS
lib_LTLIBRARIES = libclib.la
else
noinst_LTLIBRARIES = libclib.la
endif

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(srcdir)/../ \
	-I$(srcdir) \
	-I$(srcdir)/sfmt \
	$(CLIB_EXTRA_CFLAGS) \
	-DSFMT_MEXP=19937 \
	-D_C_COMPILATION \
	$(NULL)

libclib_la_LIBADD =
libclib_la_LDFLAGS =

libclib_la_SOURCES = \
	$(clib_common_source) \
	$(sfmt_source)

unix_files = cdate-unix.c cdir-unix.c cmisc-unix.c
threads_files = ctls.c

if OS_UNIX
libclib_la_SOURCES += $(unix_files) $(threads_files)
endif

if OS_LINUX
libclib_la_SOURCES += cxdg-unix.c
endif

if OS_DARWIN
libclib_la_SOURCES += \
	fmemopen.c
endif

if OS_WIN32
libclib_la_SOURCES += \
	cdate-win32.c cdir-win32.c cmisc-win32.c \
	mkstemp.c \
	vasprintf.c \
	fmemopen.c
libclib_la_SOURCES += $(threads_files)
libclib_la_LIBADD += -lm $(LTLIBICONV) -lpsapi
libclib_la_LDFLAGS += -no-undefined
endif

if USE_SYSTEM_UV
AM_CPPFLAGS += $(LIBUV_DEP_CFLAGS)
libclib_la_LIBADD += $(LIBUV_DEP_LIBS)
else
AM_CPPFLAGS += -I$(top_srcdir)/libuv/include
libclib_la_LIBADD += $(top_builddir)/libuv/libuv.la
endif

EXTRA_DIST += clib-config.h.in
EXTRA_DIST += sfmt/README.txt sfmt/LICENSE.txt

android_moduledir = $(datadir)/android_modules/clib
android_module_DATA = \
	$(clib_common_source) \
	$(sfmt_source) \
	$(unix_files) \
	$(threads_files) \
	vasprintf.c \
	Android.mk

dist_source_dir = $(datadir)/src/clib
dist_source = \
	$(clib_common_source) \
	$(sfmt_source) \
	cmisc-emscripten.c \
	clib-web.js \
	clib-web.h \
	Makefile.sources \
	Makefile.emscripten

# Use a custom install-data-local to install the source so that
# we can preserve the directory structure...
install-data-local:
	for f in $(dist_source); do \
	d=`dirname $$f`; \
	$(mkinstalldirs) $(DESTDIR)$(dist_source_dir)/$$d; \
	$(INSTALL_DATA) $(srcdir)/$$f $(DESTDIR)$(dist_source_dir)/$$f; \
	done
