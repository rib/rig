
lib_LTLIBRARIES = librut.la

include Makefile.sources

runtime_headersdir = $(prefix)/share/rig/codegen_includes
runtime_headers_DATA = $(runtime_headers)

librut_la_SOURCES = \
    $(rut_common_sources)

if USE_SDL
librut_la_SOURCES += \
	$(rut_sdl_sources)
endif

if COGL_SUPPORT_GLX
librut_la_SOURCES += \
	rut-x11-shell.h \
	rut-x11-shell.c
endif

if EMSCRIPTEN
librut_la_SOURCES += \
	rut-emscripten-shell.h \
	rut-emscripten-shell.c
endif

if EDITOR_ENABLED
librut_la_SOURCES += \
	$(rut_editor_sources)
endif

if USE_GSTREAMER
librut_la_SOURCES += \
    gstmemsrc.h \
    gstmemsrc.c
endif

librut_la_LIBADD = \
    $(RIG_DEP_LIBS) \
    $(RIG_EXTRA_LDFLAGS)
librut_la_LDFLAGS = \
    -no-undefined \
    -export-dynamic

if !EMSCRIPTEN
librut_la_LDFLAGS += \
    -version-info @RIG_LT_CURRENT@:@RIG_LT_REVISION@:@RIG_LT_AGE@ \
    -export-symbols-regex "(^rut.*)|(^_rut.*)"
endif

librut_la_CFLAGS = \
    $(RIG_DEP_CFLAGS) \
    $(RIG_EXTRA_CFLAGS)
librut_la_CPPFLAGS = \
    -I$(top_srcdir) \
    -I$(top_builddir) \
    -I$(top_srcdir)/clib/src \
    -I$(top_builddir)/clib/src \
    -I$(top_srcdir)/rut

# rut_headersdir =  $(includedir)/rut
# rut_headers_DATA = $(source_h)

rut_android_moduledir = $(datadir)/android_modules/rut/
rut_android_module_DATA = \
	$(rut_common_sources) \
	rut-android-shell.h \
	rut-android-shell.c \
	$(runtime_headers) \
	fmemopen.h \
	fmemopen.c \
	Android.mk
