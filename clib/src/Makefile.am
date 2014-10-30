EXTRA_DIST =

noinst_LTLIBRARIES = libclib.la libclib-static.la

win_files  = \
	clib-config.hw \
	cdate-win32.c cdir-win32.c cfile-win32.c cmisc-win32.c \
	cmodule-win32.c ctimer-win32.c

unix_files = \
	cdate-unix.c  cdir-unix.c  cfile-unix.c  cmisc-unix.c	\
	cmodule-unix.c ctimer-unix.c

# some unices and windows do not have an implementation of vasprintf
# used by eglib, use provided implementation instead
if NEED_VASPRINTF
vasprintf_files = vasprintf.c
endif

if OS_WIN32
os_files = $(win_files)
else
os_files = $(unix_files)
endif

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
	coutput.c    	\
	cqsort.c	\
	cstr.c       	\
	cslist.c     	\
	cstring.c    	\
	cptrarray.c     \
	clist.c		\
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
	ctls.c

libclib_la_SOURCES = \
	$(clib_common_source) \
	$(sfmt_source) \
	$(os_files) \
	$(vasprintf_files)

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(srcdir) \
	-I$(srcdir)/sfmt \
	-Wall \
	$(RIG_DEP_CFLAGS) \
	$(RIG_EXTRA_CPPFLAGS) \
	-DSFMT_MEXP=19937 \
	$(NULL)

#libclib_la_CFLAGS = -g -Wall -D_FORTIFY_SOURCE=2
#libclib_la_CFLAGS = -g -Wall -D_FORTIFY_SOURCE=2
libclib_static_la_SOURCES=$(libclib_la_SOURCES)
libclib_static_la_CFLAGS = $(libclib_la_CFLAGS)

if OS_WIN32
libclib_la_LIBADD = -lm $(LTLIBICONV) -lpsapi
endif

libclib_static_la_LIBADD = $(libclib_la_LIBADD) $(LTLIBICONV)
libclib_static_la_LDFLAGS = -static

MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST += clib-config.h.in $(win_files) $(unix_files)

android_moduledir = $(datadir)/android_modules/clib
android_module_DATA = \
	$(clib_common_source) \
	$(sfmt_source) \
	$(unix_files) \
	vasprintf.c \
	Android.mk \
	android/clib-config.h
