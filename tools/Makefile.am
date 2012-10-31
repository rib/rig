include $(top_srcdir)/build/autotools/Makefile.am.silent

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/rut \
	-I$(top_builddir)/rut

AM_CFLAGS = \
	$(RIG_DEP_CFLAGS) \
	$(RIG_EXTRA_CFLAGS) \
	-DRIG_EXAMPLES_DATA=\""$(abs_top_srcdir)/examples/"\" \
	-DRIG_SHARE_DIR=\"$(datadir)/\"

AM_CPPFLAGS = \
	-DG_DISABLE_SINGLE_INCLUDES \
	-DCOGL_DISABLE_DEPRECATED

common_ldadd = \
	$(RIG_DEP_LIBS)

bin_PROGRAMS = rig-bump-map-gen

rig_bump_map_gen_SOURCES = bump-map-gen.c
rig_bump_map_gen_LDADD = $(common_ldadd)
