include $(top_srcdir)/build/autotools/Makefile.am.silent

include Makefile.sources

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/libuv/include \
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
	-DRIG_LLVM_VERSION=\"@RIG_LLVM_VERSION@\" \
	-DICU_DATA_DIR=\""$(ICU_PREFIX)/share"\"

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

if EMSCRIPTEN
lib_LTLIBRARIES += librig.la
lib_LTLIBRARIES += librig-device-archive.la librig-simulator-archive.la
else
noinst_LTLIBRARIES += librig.la
noinst_LTLIBRARIES += librig-device-archive.la librig-simulator-archive.la
bin_PROGRAMS += rig-device rig-simulator
endif

if EDITOR_ENABLED
noinst_LTLIBRARIES += librig-slave-archive.la
bin_PROGRAMS += rig rig-slave
lib_LTLIBRARIES += rigeditor2.la
endif

rig_includedir=$(includedir)/rig
rig_include_SOURCE=rig-c.h

%.pb-c.c %.pb-c.h: %.proto
	protoc-c --c_out=$(top_builddir)/rig $(srcdir)/$(*).proto

%.pb.cc %.pb.h: %.proto
	protoc --cpp_out=$(top_builddir)/rig --proto_path=$(srcdir)/ $(srcdir)/$(*).proto

PROTOBUF_C_FILES = rig.pb-c.c rig.pb-c.h
if RIG_DEBUG
if !EMSCRIPTEN
PROTOBUF_C_FILES += rig.pb.cc rig.pb.h
endif
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
if !EMSCRIPTEN
librig_la_SOURCES += $(rig_debug_sources)
endif
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
	$(top_builddir)/rut/librut.la \
	$(top_builddir)/cogl/libcg.la \
	$(top_builddir)/cogl-path/libcogl-path.la \
	$(RIG_DEP_LIBS)

if USE_UV
common_ldadd += $(top_builddir)/xdgmime/libxdgmime.la
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

if EDITOR_ENABLED
rig_SOURCES = rig-editor-main.c
rig_LDADD = $(common_ldadd)

rigeditor2_la_SOURCES=rig-editor2.c
rigeditor2_la_LDFLAGS=-module -shared -export-dynamic -avoid-version
#rigeditor2_la_LIBADD

librig_slave_archive_la_SOURCES = \
	$(rig_slave_sources) \
	rig-slave-main.c
librig_slave_archive_la_LIBADD = $(common_ldadd)
endif

htmlfiles += rig-device.html
librig_device_archive_la_SOURCES = \
	$(rig_device_sources) \
	rig-device-main.c
librig_device_archive_la_LIBADD = $(common_ldadd)

worker_js_files += rig-simulator-worker.js
librig_simulator_archive_la_SOURCES = \
	$(rig_simulator_sources) \
	rig-simulator-main.c
librig_simulator_archive_la_LIBADD = $(common_ldadd)

if EMSCRIPTEN

emscripten_libs = \
	--js-library $(top_srcdir)/cogl/cogl-emscripten-lib.js \
	--js-library $(top_srcdir)/rig/rig-emscripten-lib.js

%.html: $(top_builddir)/rig/.libs/lib%-archive.so $(top_builddir)/rig/.libs/librig.so $(top_builddir)/rut/.libs/librut.so $(top_builddir)/cogl/.libs/libcg.so $(top_builddir)/cogl-path/.libs/libcogl-path.so $(top_builddir)/clib/src/.libs/libclib.so
	$(CC) $(AM_CFLAGS) $(CFLAGS) $(RIG_EXTRA_EMCC_JSFLAGS) -Os $(emscripten_libs) --js-library $(top_srcdir)/toys/emscripten-example-js-library.js -o $@ $^ -L/home/bob/local/freetype-emcc/lib -lfreetype  -L/home/bob/local/harfbuzz-emcc/lib -lharfbuzz -lharfbuzz-icu -L/home/bob/local/fontconfig-emcc/lib -lfontconfig -L/home/bob/local/icu-emcc/lib -licuuc -licudata -L/home/bob/local/protobuf-c-emcc/lib -lprotobuf-c -L/home/bob/local/libxml2-emcc/lib -lxml2

%-worker.js: $(top_builddir)/rig/.libs/lib%-archive.so $(top_builddir)/rig/.libs/librig.so $(top_builddir)/rut/.libs/librut.so $(top_builddir)/cogl/.libs/libcg.so $(top_builddir)/cogl-path/.libs/libcogl-path.so $(top_builddir)/clib/src/.libs/libclib.so
	$(CC) $(AM_CFLAGS) $(CFLAGS) $(RIG_EXTRA_EMCC_JSFLAGS) -Os -s ASSERTIONS=2 $(emscripten_libs) --js-library $(top_srcdir)/toys/emscripten-example-js-library.js -s BUILD_AS_WORKER=1 -o $@ $^ -L/home/bob/local/freetype-emcc/lib -lfreetype  -L/home/bob/local/harfbuzz-emcc/lib -lharfbuzz -lharfbuzz-icu -L/home/bob/local/fontconfig-emcc/lib -lfontconfig -L/home/bob/local/icu-emcc/lib -licuuc -licudata -L/home/bob/local/protobuf-c-emcc/lib -lprotobuf-c -L/home/bob/local/libxml2-emcc/lib -lxml2

all-local: $(js_files) $(worker_js_files) $(htmlfiles)
else

if EDITOR_ENABLED
rig_slave_SOURCES =
rig_slave_LDADD = $(common_ldadd) librig-slave-archive.la
endif

rig_device_SOURCES =
rig_device_LDADD = $(common_ldadd) librig-device-archive.la
#TODO: Avoid linking with Cogl and SDL in the simulator...
rig_simulator_SOURCES =
rig_simulator_LDADD = $(common_ldadd) librig-simulator-archive.la
rig_simulator_LDFLAGS = -export-dynamic
endif

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
