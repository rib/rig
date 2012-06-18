include $(top_srcdir)/build/autotools/Makefile.am.silent

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/rig \
	-I$(top_builddir)/rig

AM_CFLAGS = \
	$(RIG_DEP_CFLAGS) \
	$(RIG_EXTRA_CFLAGS) \
	-DRIG_EXAMPLES_DATA=\""$(abs_top_srcdir)/examples/"\"

AM_CPPFLAGS = \
	-DG_DISABLE_SINGLE_INCLUDES \
	-DCOGL_DISABLE_DEPRECATED

common_ldadd = \
	$(RIG_DEP_LIBS) \
	$(top_builddir)/rig/librig.la

bin_PROGRAMS = rig-editor

rig_editor_SOURCES = main.c
rig_editor_LDADD = $(common_ldadd)
