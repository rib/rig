noinst_LTLIBRARIES = libulib.la libulib-static.la

win_files  = \
	ulib-config.hw \
	udate-win32.c udir-win32.c ufile-win32.c umisc-win32.c \
	umodule-win32.c utimer-win32.c

unix_files = \
	udate-unix.c  udir-unix.c  ufile-unix.c  umisc-unix.c	\
	umodule-unix.c utimer-unix.c

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

libulib_la_SOURCES = \
	sort.frag.h	\
	ulib.h	     	\
	udebugkey.c	\
	uquark.c	\
	uarray.c	\
	ubytearray.c	\
	uerror.c	\
	uhashtable.c 	\
	uiconv.c	\
	umem.c       	\
	umodule.h	\
	uoutput.c    	\
	uqsort.c	\
	ustr.c       	\
	uslist.c     	\
	ustring.c    	\
	uptrarray.c     \
	ulist.c		\
	uhooklist.c	\
	uqueue.c	\
	upath.c		\
	ushell.c	\
	uspawn.c	\
	ufile.c		\
	ufile-posix.c	\
	uutf8.c		\
	uunicode.c	\
	unicode-data.h	\
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

#libulib_la_CFLAGS = -g -Wall -D_FORTIFY_SOURCE=2
#libulib_la_CFLAGS = -g -Wall -D_FORTIFY_SOURCE=2
libulib_static_la_SOURCES=$(libulib_la_SOURCES)
libulib_static_la_CFLAGS = $(libulib_la_CFLAGS)

if OS_WIN32
libulib_la_LIBADD = -lm $(LTLIBICONV) -lpsapi
endif

libulib_static_la_LIBADD = $(libulib_la_LIBADD) $(LTLIBICONV)
libulib_static_la_LDFLAGS = -static

MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST = ulib-config.h.in $(win_files) $(unix_files)
