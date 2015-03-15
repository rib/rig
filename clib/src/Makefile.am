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

# SIMD-oriented Fast Mersenne Twist Random
# Number Generator...
sfmt_source = \
	sfmt/SFMT.h \
	sfmt/SFMT.c \
	sfmt/SFMT-common.h \
	sfmt/SFMT-params4253.h \
	sfmt/SFMT-params19937.h \
	sfmt/SFMT-params44497.h \
	sfmt/SFMT-params216091.h \
	sfmt/SFMT-params86243.h \
	sfmt/SFMT-params2281.h \
	sfmt/SFMT-params1279.h \
	sfmt/SFMT-params607.h \
	sfmt/SFMT-params.h \
	sfmt/SFMT-params11213.h
EXTRA_DIST += sfmt/README.txt sfmt/LICENSE.txt

clib_common_source = \
	sort.frag.h	\
	clib.h	     	\
	cdebugkey.c	\
	cquark.c	\
	carray.c	\
	cbytearray.c	\
	cerror.c	\
	chashtable.c 	\
	ciconv.c	\
	cmem.c       	\
	cmodule.h	\
	cmodule.c	\
	coutput.c    	\
	cqsort.c	\
	cstr.c       	\
	cslist.c     	\
	cstring.c    	\
	cptrarray.c     \
	clist.c		\
	cllist.c	\
	chooklist.c	\
	cqueue.c	\
	cpath.c		\
	cshell.c	\
	cspawn.c	\
	cfile.c		\
	cfile-posix.c	\
	cutf8.c		\
	cunicode.c	\
	unicode-data.h	\
	crbtree.h	\
	crbtree.c	\
	ascii_snprintf.c \
	crand.c		\
	ctimer.c

libclib_la_SOURCES = \
	$(clib_common_source) \
	$(sfmt_source)

unix_files = cdate-unix.c  cdir-unix.c  cfile-unix.c  cmisc-unix.c \
	     cxdg-unix.c
threads_files = ctls.c cmutex.c

if OS_UNIX
libclib_la_SOURCES += $(unix_files) $(threads_files)
endif

if OS_WIN32
libclib_la_SOURCES += \
	clib-config.hw \
	cdate-win32.c cdir-win32.c cfile-win32.c cmisc-win32.c
libclib_la_SOURCES += $(threads_files)
libclib_la_LIBADD += -lm $(LTLIBICONV) -lpsapi
endif

if OS_WEB
libclib_la_SOURCES += cmisc-emscripten.c
endif

# some unices and windows do not have an implementation of vasprintf
if NEED_VASPRINTF
libclib_la_SOURCES += = vasprintf.c
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
	$(thread_files) \
	vasprintf.c \
	Android.mk \
	android/clib-config.h
