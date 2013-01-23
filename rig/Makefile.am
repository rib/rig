include $(top_srcdir)/build/autotools/Makefile.am.silent

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/rut \
	-I$(top_builddir)/rut \
	-I$(top_builddir)/rig/jni \
	-DG_DISABLE_SINGLE_INCLUDES \
	-DCOGL_DISABLE_DEPRECATED \
	$(RIG_DEP_CFLAGS) \
	$(RIG_EXTRA_CPPFLAGS)

AM_CFLAGS = \
	$(RIG_EXTRA_CFLAGS)

bin_PROGRAMS = rig

rig_SOURCES = \
	jni/main.c \
	jni/rig-node.c \
	jni/rig-node.h \
	jni/rig-path.c \
	jni/rig-path.h \
	jni/rig-transition.c \
	jni/rig-transition.h \
	jni/rig-load-save.h \
	jni/rig-load-save.c \
	jni/rig-transition-view.c \
	jni/rig-transition-view.h \
	jni/rig-types.h \
	jni/rig-renderer.h \
	jni/rig-renderer.c \
	jni/rig-data.h \
	jni/rig-osx.h

if EDITOR_ENABLED
rig_SOURCES += \
	jni/rig-undo-journal.h \
	jni/rig-undo-journal.c
endif

if HAVE_OSX
rig_SOURCES += \
	jni/rig-osx.m
endif

if HAVE_GTK
rig_SOURCES += \
	jni/rig-application.h \
	jni/rig-application.c
endif

rig_LDADD = $(common_ldadd)
