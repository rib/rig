include $(top_srcdir)/build/autotools/Makefile.am.silent

include Makefile.sources

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/clib/src \
	-I$(top_builddir)/clib/src \
	-I$(top_srcdir)/xdgmime \
	-I$(top_srcdir)/rut \
	-I$(top_builddir)/rut \
	-I$(top_builddir)/rig \
	-I$(top_builddir)/rig/protobuf-c-rpc \
	-DG_DISABLE_SINGLE_INCLUDES \
	-DCOGL_DISABLE_DEPRECATED \
	$(RIG_DEP_CFLAGS) \
	-DRIG_BIN_DIR=\"$(bindir)/\" \
	$(RIG_CLANG_CFLAGS) \
	-DRIG_LLVM_PREFIX=\"@RIG_LLVM_PREFIX@\" \
	-DRIG_LLVM_VERSION=\"@RIG_LLVM_VERSION@\"

AM_CFLAGS = \
	$(RIG_EXTRA_CFLAGS)

lib_LTLIBRARIES = librig_clang.la
noinst_LTLIBRARIES = librig.la
bin_PROGRAMS = rig-device rig-simulator
if EDITOR_ENABLED
bin_PROGRAMS += rig rig-slave
endif

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

if USE_LLVM
librig_clang_la_SOURCES = rig-llvm.cpp rig-llvm.h
librig_clang_la_LIBADD = $(RIG_CLANG_LIBS) -lstdc++
librig_clang_la_LIBADD += $(top_builddir)/clib/src/libclib.la
librig_clang_la_LIBADD += $(top_builddir)/rut/librut.la
librig_clang_la_LDFLAGS = -export-dynamic -version-info 0:0:0
if !HAVE_OSX
librig_clang_la_LDFLAGS += -Wl,--unresolved-symbols=ignore-all
endif
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

if USE_NCURSES
librig_la_SOURCES += \
	rig-curses-debug.h \
	rig-curses-debug.c
endif

common_ldadd = \
	$(RIG_EXTRA_LDFLAGS) \
	librig.la \
	$(top_builddir)/xdgmime/libxdgmime.la \
	$(top_builddir)/rut/librut.la \
	$(top_builddir)/cogl/libcogl2.la \
	$(top_builddir)/cogl-path/libcogl-path.la \
	$(RIG_DEP_LIBS)

if EDITOR_ENABLED
common_ldadd += $(top_builddir)/cogl-pango/libcogl-pango2.la
endif

if USE_GSTREAMER
common_ldadd += $(top_builddir)/cogl-gst/libcogl-gst.la
endif

if USE_LLVM
common_ldadd += -lrig_clang
endif

if USE_NCURSES
common_ldadd += -lncursesw
endif

librig_la_LIBS = $(common_ldadd)

if EDITOR_ENABLED
rig_SOURCES = \
	rig-editor-main.c
rig_LDADD = $(common_ldadd)
endif

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

rig_android_moduledir = $(datadir)/android_modules/rig/
nobase_rig_android_module_DATA = $(rig_common_sources) $(rig_slave_sources) Android.mk
