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
	$(RIG_DEP_LIBS) \
	$(top_builddir)/rut/librut.la

bin_PROGRAMS = rig

rig_SOURCES = \
	jni/main.c \
	jni/rig-node.c \
	jni/rig-node.h \
	jni/rig-path.c \
	jni/rig-path.h \
	jni/rig-transition.c \
	jni/rig-transition.h \
	jni/rig-types-private.h \
	jni/rig-load-save.h \
	jni/rig-load-save.c

if EDITOR_ENABLED
rig_SOURCES += \
	jni/rig-undo-journal.h \
	jni/rig-undo-journal.c
endif

rig_LDADD = $(common_ldadd)
