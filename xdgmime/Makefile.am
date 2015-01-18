noinst_LTLIBRARIES = libxdgmime.la
noinst_PROGRAMS = xdgmime-query

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(srcdir) \
	-I$(top_srcdir)/clib/src \
	-I$(top_builddir)/clib/src \
	-Wall \
	$(RIG_DEP_CFLAGS) \
	$(RIG_EXTRA_CFLAGS) \
	$(NULL)

libxdgmime_la_SOURCES = \
	glob.h \
	glob.c \
	magic.h \
	magic.c \
	xdgmime.h \
	xdgmime.c

xdgmime_query_SOURCES = xdgmime-query.c
xdgmime_query_LDADD = \
	$(RIG_DEP_LIBS) \
	libxdgmime.la \
	$(top_builddir)/clib/src/libclib.la
