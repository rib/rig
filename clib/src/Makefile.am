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
else
vaprinttf_files = foo.c
endif

if OS_WIN32
os_files = $(win_files)
else
os_files = $(unix_files)
endif

libclib_la_SOURCES = \
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
	cnicode-data.h	\
	$(os_files)	\
	$(vasprintf_files)

AM_CPPFLAGS =			\
	-I$(top_builddir)	\
	-I$(top_srcdir)		\
	-I$(srcdir)		\
	-Wall			\
	$(RIG_DEP_CFLAGS) \
	$(RIG_EXTRA_CPPFLAGS) \
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

EXTRA_DIST = clib-config.h.in $(win_files) $(unix_files)
