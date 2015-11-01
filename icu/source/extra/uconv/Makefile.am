EXTRA_DIST =

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/common \
	-I$(top_srcdir)/i18n \
	-I$(top_builddir) \
	-I$(top_builddir)/common \
	$(U_CPPFLAGS)

AM_CFLAGS = $(U_CFLAGS)
AM_CXXFLAGS = $(U_CXXFLAGS)


noinst_PROGRAMS = uconv

uconv_SOURCES = uconv.cpp unicode/uwmsg.h uwmsg.c
uconv_LDADD = $(top_builddir)/i18n/libicui18n.la \
	      $(top_builddir)/common/libicuuc.la \
	      $(top_builddir)/stubdata/libicudata.la
EXTRA_DIST += uconv.1.in
EXTRA_DIST += uconv.vcxproj uconv.vcxproj.filters
EXTRA_DIST += resources/fr.txt resources/root.txt
EXTRA_DIST += \
	samples/danish-ISO-8859-1.txt \
	samples/eucJP.txt \
	samples/hangul-eucKR.txt \
	samples/hania-eucKR.txt \
	samples/ibm-37-test.txt \
	samples/iso8859-1.txt \
	samples/ISO-8859-2.txt \
	samples/ISO-8859-3.txt \
	samples/koi8r.txt

EXTRA_DIST += \
	samples/utf8/armenian.txt \
	samples/utf8/chinese-ulysses.txt \
	samples/utf8/danish.txt \
	samples/utf8/hangul.txt \
	samples/utf8/jap.txt \
	samples/utf8/linji.txt \
	samples/utf8/maopoem.txt \
	samples/utf8/simplechinese.txt \
	samples/utf8/utf-8-demo.txt \
	samples/utf8/banviet.txt \
	samples/utf8/croat.txt \
	samples/utf8/greek.txt \
	samples/utf8/hania.txt \
	samples/utf8/korean.txt \
	samples/utf8/many.txt \
	samples/utf8/russian.txt \
	samples/utf8/turkish.txt
