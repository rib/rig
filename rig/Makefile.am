include $(top_srcdir)/build/autotools/Makefile.am.silent

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/rut \
	-I$(top_builddir)/rut \
	-I$(top_builddir)/rig/jni \
	-I$(top_builddir)/rig/jni/protobuf-c \
	-DG_DISABLE_SINGLE_INCLUDES \
	-DCOGL_DISABLE_DEPRECATED \
	$(RIG_DEP_CFLAGS) \
	$(RIG_EXTRA_CPPFLAGS)

AM_CFLAGS = \
	$(RIG_EXTRA_CFLAGS)

noinst_LTLIBRARIES = librig.la
bin_PROGRAMS = rig rig-slave rig-device

%.pb-c.c %.pb-c.h: %.proto
	protoc-c --c_out=$(top_builddir)/rig $(srcdir)/$(*).proto

PROTOBUF_C_FILES = jni/rig.pb-c.c jni/rig.pb-c.h

BUILT_SOURCES = $(PROTOBUF_C_FILES)
DISTCLEANFILES = $(PROTOBUF_C_FILES)
EXTRA_DIST = $(PROTOBUF_C_FILES)

rig_protobuf_c_sources = \
	jni/protobuf-c/rig-protobuf-c-data-buffer.h \
	jni/protobuf-c/rig-protobuf-c-data-buffer.c \
	jni/protobuf-c/rig-protobuf-c-rpc.h \
	jni/protobuf-c/rig-protobuf-c-rpc.c \
	jni/protobuf-c/gsklistmacros.h
EXTRA_DIST += jni/protobuf-c/README

librig_la_SOURCES = \
	$(rig_protobuf_c_sources) \
	jni/rig-engine.c \
	jni/rig-node.c \
	jni/rig-node.h \
	jni/rig-path.c \
	jni/rig-path.h \
	jni/rig-controller.c \
	jni/rig-controller.h \
	jni/rig-pb.h \
	jni/rig-pb.c \
	jni/rig-load-save.h \
	jni/rig-load-save.c \
	jni/rig-controller-view.c \
	jni/rig-controller-view.h \
    	jni/rig-split-view.h \
    	jni/rig-split-view.c \
    	jni/rig-view.h \
    	jni/rig-view.c \
    	jni/rig-camera-view.h \
    	jni/rig-camera-view.c \
	jni/rig-types.h \
	jni/rig-renderer.h \
	jni/rig-renderer.c \
	jni/rig-engine.h \
	jni/rig-osx.h \
	jni/rig-avahi.h \
	jni/rig-avahi.c \
	jni/rig-rpc-network.h \
	jni/rig-rpc-network.c \
	jni/rig-slave-address.h \
	jni/rig-slave-address.c \
	jni/rig-slave-master.h \
	jni/rig-slave-master.c \
	jni/rig.pb-c.c

if EDITOR_ENABLED
librig_la_SOURCES += \
	jni/rig-undo-journal.h \
	jni/rig-undo-journal.c \
	jni/rig-rotation-tool.h \
	jni/rig-rotation-tool.c
endif

if HAVE_OSX
librig_la_SOURCES += \
	jni/rig-osx.m
endif

if HAVE_GTK
librig_la_SOURCES += \
	jni/rig-application.h \
	jni/rig-application.c
endif

common_ldadd = \
	$(RIG_DEP_LIBS) \
	$(RIG_EXTRA_LDFLAGS) \
	librig.la \
	$(top_builddir)/rut/librut.la

librig_la_LIBS = $(common_ldadd)

rig_SOURCES = \
	jni/rig-editor.c
rig_LDADD = $(common_ldadd)

rig_slave_SOURCES = \
	jni/rig-slave.c
rig_slave_LDADD = $(common_ldadd)

rig_device_SOURCES = \
	jni/rig-device.c
rig_device_LDADD = $(common_ldadd)
