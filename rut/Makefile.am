
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

if CG_SUPPORT_GLX
librut_la_SOURCES += \
	rut-x11-shell.h \
	rut-x11-shell.c
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
    $(CG_DEP_LIBS) \
    $(RIG_EXTRA_LDFLAGS)
librut_la_LDFLAGS = \
    -no-undefined \
    -export-dynamic

librut_la_LDFLAGS += \
    -export-symbols-regex "(^rut.*)|(^_rut.*)"

librut_la_CFLAGS = \
    $(RIG_DEP_CFLAGS) \
    $(CG_DEP_CFLAGS) \
    $(RIG_EXTRA_CFLAGS)
librut_la_CPPFLAGS = \
    -I$(top_srcdir) \
    -I$(top_builddir) \
    -I$(top_srcdir)/libuv/include \
    -I$(top_srcdir)/clib/clib \
    -I$(top_builddir)/clib/clib \
    -I$(top_srcdir)/cglib \
    -I$(top_builddir)/cglib \
    -I$(top_srcdir)/rut

# rut_headersdir =  $(includedir)/rut
# rut_headers_DATA = $(source_h)

rut_android_moduledir = $(datadir)/android_modules/rut/
rut_android_module_DATA = \
	$(rut_common_sources) \
	rut-android-shell.h \
	rut-android-shell.c \
	$(runtime_headers) \
	Android.mk

dist_source_dir = $(datadir)/src/rut
dist_source = \
	$(rut_emscripten_sources) \
	$(runtime_headers) \
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
