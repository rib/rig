include $(top_srcdir)/build/autotools/Makefile.am.silent

include Makefile.sources

EXTRA_DIST =

if EMSCRIPTEN
lib_LTLIBRARIES = libclib.la
all-local: clib.bc
else
if UNIT_TESTS
lib_LTLIBRARIES = libclib.la
else
noinst_LTLIBRARIES = libclib.la
endif
endif


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
libclib_la_LIBADD += $(top_builddir)/libuv/libuv.la
endif

if OS_WIN32
libclib_la_SOURCES += \
	clib-config.hw \
	cdate-win32.c cdir-win32.c cmisc-win32.c \
	mkstemp.c \
	vasprintf.c \
	fmemopen.c \
	memmem.c
libclib_la_SOURCES += $(threads_files)
libclib_la_LIBADD += -lm $(LTLIBICONV) -lpsapi
libclib_la_LIBADD += $(top_builddir)/libuv/libuv.la
endif

if OS_DARWIN
libclib_la_SOURCES += \
	fmemopen.c \
	memmem.c
libclib_la_LIBADD += $(top_builddir)/libuv/libuv.la
endif

if EMSCRIPTEN
libclib_la_SOURCES += cmisc-emscripten.c
endif

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(srcdir) \
	-I$(srcdir)/sfmt \
	-I$(top_srcdir)/libuv/include \
	-Wall \
	$(CLIB_DEP_CFLAGS) \
	$(RIG_EXTRA_CFLAGS) \
	-DSFMT_MEXP=19937 \
	$(NULL)

#libclib_la_LIBADD += $(LTLIBICONV)
#libclib_la_LDFLAGS = -static

if EMSCRIPTEN
clib.bc: $(libclib_la_OBJECTS)
	emcc -o $(builddir)/clib.bc $(patsubst %.lo,.libs/%.o,$(libclib_la_OBJECTS)) $(AM_CPPFLAGS) $(CFLAGS) $(RIG_EXTRA_EMCC_BCFLAGS)
endif

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
