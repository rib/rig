include $(srcdir)/../Makefile.am.tool

noinst_PROGRAMS = gensprep
gensprep_SOURCES = gensprep.c gensprep.h store.c
gensprep_LDADD = $(tool_ldadd) -lstdc++
EXTRA_DIST += gensprep.8.in
EXTRA_DIST += gensprep.vcxproj gensprep.vcxproj.filters
EXTRA_DIST += filterRFC3454.pl
