include $(top_srcdir)/build/autotools/Makefile.am.silent

include Makefile.sources

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/libuv/include \
	-I$(top_srcdir)/clib/clib \
	-I$(top_builddir)/clib/clib \
	-I$(top_srcdir)/cglib \
	-I$(top_builddir)/cglib \
	-I$(top_srcdir)/xdgmime \
	-I$(top_srcdir)/rut \
	-I$(top_builddir)/rut \
	-I$(top_builddir)/rig \
	-I$(top_builddir)/rig/protobuf-c-rpc \
	-DG_DISABLE_SINGLE_INCLUDES \
	$(RIG_DEP_CFLAGS) \
	$(CG_DEP_CFLAGS) \
	-DRIG_BIN_DIR=\"$(bindir)/\" \
	$(RIG_CLANG_CFLAGS) \
	-DRIG_LLVM_PREFIX=\"@RIG_LLVM_PREFIX@\" \
	-DRIG_LLVM_VERSION=\"@RIG_LLVM_VERSION@\" \
	-DICU_DATA_DIR=\""$(ICU_PREFIX)/share"\"

if USE_UV
AM_CPPFLAGS += -I$(top_srcdir)/h2o/include
AM_CPPFLAGS += -I$(top_srcdir)/h2o
AM_CPPFLAGS += -I$(top_srcdir)/wslay/lib
AM_CPPFLAGS += -I$(top_srcdir)/wslay/lib/includes
endif

if OCULUS_RIFT
AM_CPPFLAGS += -I$(top_srcdir)/LibOVR/Src
endif

AM_CFLAGS = \
	$(RIG_EXTRA_CFLAGS)

lib_LTLIBRARIES =
noinst_LTLIBRARIES =
bin_PROGRAMS =
htmlfiles =
worker_js_files =
js_files =

if USE_LLVM
lib_LTLIBRARIES += librig_clang.la
endif

noinst_LTLIBRARIES += librig.la
#noinst_LTLIBRARIES += librig-device-archive.la librig-simulator-archive.la librig-simulator-server-archive.la
noinst_LTLIBRARIES += librig-simulator-archive.la
#bin_PROGRAMS += rig-device rig-simulator rig-simulator-server
bin_PROGRAMS += rig-simulator

rig_includedir=$(includedir)/rig
rig_include_SOURCE=rig-c.h

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
librig_clang_la_LIBADD += $(top_builddir)/clib/clib/libclib.la
librig_clang_la_LIBADD += $(top_builddir)/rut/librut.la
librig_clang_la_LDFLAGS = -export-dynamic -version-info 0:0:0
if !HAVE_OSX
librig_clang_la_LDFLAGS += -Wl,--unresolved-symbols=ignore-all
endif
endif

#-fuse-ld=gold


librig_la_SOURCES = \
	$(rig_common_sources) \
	$(PROTOBUF_C_FILES)

if USE_UV
librig_la_SOURCES += \
	components/rig-native-module.h \
	components/rig-native-module.c
endif

if USE_MOZJS
librig_la_SOURCES += \
	rig-js.h \
	rig-js.cpp
endif

if RIG_DEBUG
librig_la_SOURCES += $(rig_debug_sources)
endif

if USE_NCURSES
librig_la_SOURCES += \
	rig-curses-debug.h \
	rig-curses-debug.c
endif

common_ldadd = \
	$(RIG_EXTRA_LDFLAGS) \
	librig.la \
	$(top_builddir)/rut/librut.la \
	$(top_builddir)/cglib/cglib/libcglib.la \
	$(RIG_DEP_LIBS) \
	$(CG_DEP_LIBS)

if USE_UV
#common_ldadd += $(top_builddir)/xdgmime/libxdgmime.la
#common_ldadd += $(top_builddir)/h2o/libh2o.la -lssl -lcrypto
endif

if OCULUS_RIFT
common_ldadd += $(top_builddir)/LibOVR/libOVR.la -lstdc++
endif

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

#htmlfiles += rig-device.html
#librig_device_archive_la_SOURCES = \
#	$(rig_device_sources) \
#	rig-device-main.c
#librig_device_archive_la_LIBADD = $(common_ldadd)

worker_js_files += rig-simulator-worker.js
librig_simulator_archive_la_SOURCES = \
	$(rig_simulator_sources) \
	rig-simulator-main.c
librig_simulator_archive_la_LIBADD = $(common_ldadd)

#librig_simulator_server_archive_la_SOURCES = \
#	$(rig_simulator_sources) \
#	rig-simulator-server.c
#librig_simulator_server_archive_la_LIBADD = $(common_ldadd)

#rig_device_SOURCES =
#rig_device_LDADD = $(common_ldadd) librig-device-archive.la

rig_simulator_SOURCES =
rig_simulator_LDADD = $(common_ldadd) librig-simulator-archive.la
rig_simulator_LDFLAGS = -export-dynamic

#rig_simulator_server_SOURCES =
#rig_simulator_server_LDADD = $(common_ldadd) librig-simulator-server-archive.la
#rig_simulator_server_LDFLAGS = -export-dynamic

rig_android_moduledir = $(datadir)/android_modules/rig/
nobase_rig_android_module_DATA = $(rig_common_sources) $(rig_slave_sources) Android.mk

dist_source_dir = $(datadir)/src/rig
dist_source = \
	$(rig_common_sources) \
	$(rig_debug_sources) \
	rig.h \
	rig-js.h \
	rig-c.h \
	$(runtime_headers) \
	$(librig_simulator_archive_la_SOURCES) \
	$(librig_device_archive_la_SOURCES) \
	$(js_files) \
	$(worker_js_files) \
	$(rig_slave_sources) \
	rig-slave-main.c \
	rig.proto \
	rig-emscripten-lib.js \
	rig-emscripten-lib.h \
	Makefile.emscripten \
	Makefile.sources

# Use a custom install-data-local to install the source so that
# we can preserve the directory structure...
install-data-local:
	for f in $(dist_source); do \
	d=`dirname $$f`; \
	$(mkinstalldirs) $(DESTDIR)$(dist_source_dir)/$$d; \
	$(INSTALL_DATA) $(srcdir)/$$f $(DESTDIR)$(dist_source_dir)/$$f; \
	done
