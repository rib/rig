include $(top_srcdir)/build/autotools/Makefile.am.silent

include Makefile.sources

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/deps/ulib/src \
	-I$(top_builddir)/deps/ulib/src \
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
	-DRIG_LLVM_PREFIX=\"@RIG_LLVM_PREFIX@\" \
	-DRIG_LLVM_VERSION=\"@RIG_LLVM_VERSION@\"

AM_CFLAGS = \
	$(RIG_EXTRA_CFLAGS)

lib_LTLIBRARIES = librig_clang.la
noinst_LTLIBRARIES = librig.la
bin_PROGRAMS = rig rig-slave rig-device rig-simulator

%.pb-c.c %.pb-c.h: %.proto
	protoc-c --c_out=$(top_builddir)/rig $(srcdir)/$(*).proto

%.pb.cc %.pb.h: %.proto
	protoc --cpp_out=$(top_builddir)/rig --proto_path=$(srcdir)/ $(srcdir)/$(*).proto

PROTOBUF_C_FILES = rig.pb-c.c rig.pb-c.h
if RIG_DEBUG
PROTOBUF_C_FILES += rig.pb.cc rig.pb.h
endif

BUILT_SOURCES = $(PROTOBUF_C_FILES)
DISTCLEANFILES = $(PROTOBUF_C_FILES)
EXTRA_DIST = $(PROTOBUF_C_FILES)

EXTRA_DIST += protobuf-c-rpc/README

# Headers that will need to be available for compiling code at runtime
# and so need to be installed...
runtime_headers = \
    rig-codegen.h

runtime_headersdir = $(prefix)/share/rig/codegen_includes
runtime_headers_DATA = $(runtime_headers)

librig_clang_la_SOURCES = rig-llvm.cpp rig-llvm.h
librig_clang_la_LIBADD = $(RIG_CLANG_LIBS) -lstdc++ $(top_builddir)/deps/ulib/src/libulib.la $(top_builddir)/rut/librut.la
librig_clang_la_LDFLAGS = -export-dynamic -version-info 0:0:0
if !HAVE_OSX
librig_clang_la_LDFLAGS += -Wl,--unresolved-symbols=ignore-all
endif

#-fuse-ld=gold


librig_la_SOURCES = \
	$(rig_common_sources)

if RIG_DEBUG
librig_la_SOURCES += \
	rig.pb.h \
	rig.pb.cc \
	rig-pb-debug.h \
	rig-pb-debug.cpp
endif

if HAVE_AVAHI
librig_la_SOURCES += \
	rig-avahi.h \
	rig-avahi.c
endif

if EDITOR_ENABLED
librig_la_SOURCES += \
	$(rig_editor_sources)
endif

if HAVE_OSX
librig_la_SOURCES += \
	rig-osx.h \
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
	$(rig_slave_sources) \
	rig-slave-main.c
rig_slave_LDADD = $(common_ldadd)

rig_device_SOURCES = \
	rig-device.c
rig_device_LDADD = $(common_ldadd)

#TODO: Avoid linking with Cogl and SDL in the simulator...
rig_simulator_SOURCES = \
	rig-simulator-main.c
rig_simulator_LDADD = $(common_ldadd)
