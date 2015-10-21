include $(top_srcdir)/build/autotools/Makefile.am.silent

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/rut \
	-I$(top_builddir)/rut \
	-I$(top_srcdir)/clib/clib \
	-I$(top_builddir)/clib/clib \
	-I$(top_srcdir)/cglib \
	-I$(top_builddir)/cglib

AM_CFLAGS = \
	$(CG_DEP_CFLAGS) \
	$(CG_EXTRA_CFLAGS) \
	$(RIG_DEP_CFLAGS) \
	$(RIG_EXTRA_CFLAGS) \
	-DRIG_EXAMPLES_DATA=\""$(abs_top_srcdir)/examples/"\" \
	-DRIG_SHARE_DIR=\"$(datadir)/\"

common_ldadd = \
	$(RIG_DEP_LIBS) \
	$(CG_DEP_LIBS) \
	$(top_builddir)/cglib/cglib/libcglib.la \
	$(top_builddir)/clib/clib/libclib.la

bin_SCRIPTS=rig-package-html5

if USE_GLIB
bin_PROGRAMS = rig-bump-map-gen
endif

rig_bump_map_gen_SOURCES = bump-map-gen.c
rig_bump_map_gen_LDADD = $(common_ldadd)

noinst_PROGRAMS =

if USE_GLIB
if HAVE_LIBCRYPTO
noinst_PROGRAMS += rig-check-signature
endif
endif

rig_check_signature_SOURCES = \
	check-signature.c
rig_check_signature_LDADD = \
	$(GLIB_LIBS) \
	$(LIBCRYPTO_LIBS)
rig_check_signatures_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(LIBCRYPTO_CFLAGS)
