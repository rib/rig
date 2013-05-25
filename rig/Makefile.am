include $(top_srcdir)/build/autotools/Makefile.am.silent

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/rut \
	-I$(top_builddir)/rut \
	-I$(top_builddir)/rig \
	-I$(top_builddir)/rig/protobuf-c-rpc \
	-DG_DISABLE_SINGLE_INCLUDES \
	-DCOGL_DISABLE_DEPRECATED \
	$(RIG_DEP_CFLAGS) \
	$(RIG_EXTRA_CPPFLAGS) \
	-DRIG_BIN_DIR=\"$(bindir)/\" \
	$(RIG_CLANG_CFLAGS) \
	$(RIG_EXTRA_CPPFLAGS) \
	-DRIG_LLVM_PREFIX=\"@RIG_LLVM_PREFIX@\" \
	-DRIG_LLVM_VERSION=\"@RIG_LLVM_VERSION@\"

AM_CFLAGS = \
	$(RIG_EXTRA_CFLAGS)

lib_LTLIBRARIES = librig_clang.la
noinst_LTLIBRARIES = librig.la
bin_PROGRAMS = rig rig-slave rig-device rig-simulator

%.pb-c.c %.pb-c.h: %.proto
	protoc-c --c_out=$(top_builddir)/rig $(srcdir)/$(*).proto

PROTOBUF_C_FILES = rig.pb-c.c rig.pb-c.h

BUILT_SOURCES = $(PROTOBUF_C_FILES)
DISTCLEANFILES = $(PROTOBUF_C_FILES)
EXTRA_DIST = $(PROTOBUF_C_FILES)

rig_protobuf_c_rpc_sources = \
	protobuf-c-rpc/rig-protobuf-c-dispatch.h \
	protobuf-c-rpc/rig-protobuf-c-dispatch.c \
	protobuf-c-rpc/rig-protobuf-c-data-buffer.h \
	protobuf-c-rpc/rig-protobuf-c-data-buffer.c \
	protobuf-c-rpc/rig-protobuf-c-rpc.h \
	protobuf-c-rpc/rig-protobuf-c-rpc.c \
	protobuf-c-rpc/gsklistmacros.h
EXTRA_DIST += protobuf-c-rpc/README

# Headers that will need to be available for compiling code at runtime
# and so need to be installed...
runtime_headers = \
    rig-codegen.h

runtime_headersdir = $(prefix)/share/rig/codegen_includes
runtime_headers_DATA = $(runtime_headers)

librig_clang_la_SOURCES = rig-llvm.cpp rig-llvm.h
librig_clang_la_LIBADD = $(RIG_CLANG_LIBS) -lstdc++ $(top_builddir)/rut/librut.la
librig_clang_la_LDFLAGS = -export-dynamic -version-info 0:0:0 -Wl,--unresolved-symbols=ignore-all
#-fuse-ld=gold

librig_la_SOURCES = \
	$(rig_protobuf_c_rpc_sources) \
	rig-engine.c \
	rig-engine-op.h \
	rig-engine-op.c \
	rig-node.c \
	rig-node.h \
	rig-path.c \
	rig-path.h \
	rig-controller.c \
	rig-controller.h \
	rig-binding.h \
	rig-binding.c \
	rig-pb.h \
	rig-pb.c \
	rig-load-save.h \
	rig-load-save.c \
	rig-controller-view.c \
	rig-controller-view.h \
    	rig-split-view.h \
    	rig-split-view.c \
    	rig-view.h \
    	rig-view.c \
    	rig-camera-view.h \
    	rig-camera-view.c \
	rig-types.h \
	rig-renderer.h \
	rig-renderer.c \
	rig-engine.h \
	rig-osx.h \
	rig-avahi.h \
	rig-avahi.c \
	rig-rpc-network.h \
	rig-rpc-network.c \
	rig-slave-address.h \
	rig-slave-address.c \
	rig-slave-master.h \
	rig-slave-master.c \
	rig.pb-c.c \
	rig-simulator.h \
	rig-simulator.c \
	rig-frontend.c \
	rig-frontend.h \
	rig-ui.h \
	rig-ui.c \
	rig-binding.h \
	rig-binding.c

if EDITOR_ENABLED
librig_la_SOURCES += \
	rig-editor.h \
	rig-editor.c \
	rig-undo-journal.h \
	rig-undo-journal.c \
	rig-rotation-tool.h \
	rig-rotation-tool.c \
	rig-selection-tool.h \
	rig-selection-tool.c \
	rig-binding-view.h \
	rig-binding-view.c \
	rig-code.h \
	rig-code.c
endif

if HAVE_OSX
librig_la_SOURCES += \
	rig-osx.m
endif

if HAVE_GTK
librig_la_SOURCES += \
	rig-application.h \
	rig-application.c
endif

common_ldadd = \
	$(RIG_EXTRA_LDFLAGS) \
	librig.la \
	-lrig_clang \
	$(top_builddir)/rut/librut.la \
	$(RIG_DEP_LIBS)

librig_la_LIBS = $(common_ldadd)

rig_SOURCES = \
	rig-editor-main.c
rig_LDADD = $(common_ldadd)

rig_slave_SOURCES = \
	rig-slave.c
rig_slave_LDADD = $(common_ldadd)

rig_device_SOURCES = \
	rig-device.c
rig_device_LDADD = $(common_ldadd)

#TODO: Avoid linking with Cogl and SDL in the simulator...
rig_simulator_SOURCES = \
	rig-simulator-main.c
rig_simulator_LDADD = $(common_ldadd)
