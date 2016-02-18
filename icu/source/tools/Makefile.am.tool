EXTRA_DIST=

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/common \
	-I$(top_srcdir)/i18n \
	-I$(top_srcdir)/io \
	-I$(top_srcdir)/tools/toolutil \
	-I$(top_builddir) \
	-I$(top_builddir)/common \
	$(U_CPPFLAGS)

AM_CFLAGS = $(U_CFLAGS)
AM_CXXFLAGS = $(U_CXXFLAGS)

tool_ldadd = $(builddir)/../toolutil/libtool_utils.la -lm
if IO
tool_ldadd += $(top_builddir)/io/libicuio.la
endif
tool_ldadd += $(top_builddir)/i18n/libicui18n.la $(top_builddir)/common/libicuuc.la $(top_builddir)/stubdata/libicudata.la

