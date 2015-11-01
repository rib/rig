include $(srcdir)/../Makefile.am.tool

noinst_PROGRAMS = makeconv

makeconv_SOURCES = \
    gencnvex.c \
    genmbcs.cpp \
    genmbcs.h \
    makeconv.c \
    makeconv.h \
    ucnvstat.c
makeconv_LDADD = $(tool_ldadd)
EXTRA_DIST += makeconv/makeconv.1.in
EXTRA_DIST += makeconv/makeconv.vcxproj makeconv/makeconv.vcxproj.filters
