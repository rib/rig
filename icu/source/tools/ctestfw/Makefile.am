include $(srcdir)/../Makefile.am.tool

noinst_LTLIBRARIES = libctestfw.la


libctestfw_la_LIBADD =
libctestfw_la_LDFLAGS = -static
libctestfw_la_SOURCES = \
	unicode/ctest.h \
	ctest.c \
	unicode/datamap.h \
	datamap.cpp \
	unicode/testdata.h \
	testdata.cpp \
	unicode/testlog.h \
	unicode/testtype.h \
	unicode/tstdtmod.h \
	tstdtmod.cpp \
	unicode/uperf.h \
	uperf.cpp \
	unicode/utimer.h \
	ucln_ct.c
libctestfw_la_CPPFLAGS = $(AM_CPPFLAGS) -DT_CTESTFW_IMPLEMENTATION
EXTRA_DIST += ctestfw.vcxproj.filters ctestfw.vcxproj

