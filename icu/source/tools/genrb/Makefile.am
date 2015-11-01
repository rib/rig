include $(srcdir)/../Makefile.am.tool

noinst_PROGRAMS = genrb derb

genrb_SOURCES = \
    errmsg.c \
    errmsg.h \
    genrb.1.in \
    genrb.c \
    genrb.h \
    parse.cpp \
    parse.h \
    prscmnts.cpp \
    prscmnts.h \
    rbutil.c \
    rbutil.h \
    read.c \
    read.h \
    reslist.c \
    reslist.h \
    rle.c \
    rle.h \
    ustr.c \
    ustr.h \
    wrtjava.c \
    wrtxml.cpp
genrb_LDADD = $(tool_ldadd)
EXTRA_DIST += genrb.vcxproj.filters

if IO
derb_SOURCES = derb.c
derb_LDADD = $(tool_ldadd) -lstdc++
EXTRA_DIST += derb.1.in
EXTRA_DIST += derb.vcxproj derb.vcxproj.filters genrb.vcxproj
endif
