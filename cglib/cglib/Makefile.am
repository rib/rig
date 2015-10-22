NULL =

SUBDIRS =

BUILT_SOURCES =

EXTRA_DIST =
CLEANFILES =
DISTCLEANFILES =

noinst_LTLIBRARIES =
lib_LTLIBRARIES =

AM_CPPFLAGS = \
	-I$(srcdir)/../				\
	-I$(builddir)/../			\
	-I$(srcdir)/winsys 			\
	-I$(srcdir)/driver/nop 			\
	-I$(srcdir)/driver/gl 			\
	-I$(srcdir)/driver/gl/gl 		\
	-I$(srcdir)/driver/gl/gles 		\
	-I$(top_srcdir)/libuv/include		\
	-I$(top_srcdir)/clib/clib		\
	-I$(top_builddir)/clib/clib		\
	$(NULL)

AM_CPPFLAGS += \
	-DC_LOG_DOMAIN=\"CGlib\" 	\
	-DCG_COMPILATION		\
	-DCG_GL_LIBNAME=\"$(CG_GL_LIBNAME)\" \
	-DCG_GLES2_LIBNAME=\"$(CG_GLES2_LIBNAME)\" \
	-DCG_LOCALEDIR=\""$(localedir)"\" \
	$(NULL)

if HAVE_CG_DEFAULT_DRIVER
AM_CPPFLAGS += -DCG_DEFAULT_DRIVER=\"$(CG_DEFAULT_DRIVER)\"
endif

AM_CFLAGS = $(CG_DEP_CFLAGS) $(CG_EXTRA_CFLAGS)

BUILT_SOURCES += cg-defines.h cg-egl-defines.h cg-gl-header.h
DISTCLEANFILES += cg-defines.h cg-egl-defines.h cg-gl-header.h
EXTRA_DIST += cg-defines.h.in cg-egl-defines.h.in cg-gl-header.h.in

cg_nodist_experimental_h = \
	$(NULL)


include $(srcdir)/Makefile.sources

cg_public_h = $(cg_base_public_h)
cg_sources_c = $(cg_base_sources_c)

if CG_DRIVER_GL_SUPPORTED
cg_sources_c += $(cg_driver_gl_sources)
endif

if CG_DRIVER_GLES_SUPPORTED
cg_sources_c += $(cg_driver_gles_sources)
endif

if CG_USE_GLIB
cg_public_h += $(cg_glib_sources_h)
cg_sources_c += $(cg_glib_sources_c)
endif

if CG_USE_UV
cg_public_h += $(cg_uv_sources_h)
cg_sources_c += $(cg_uv_sources_c)
endif

if CG_SUPPORT_WEBGL
cg_public_h += $(cg_webgl_sources_h)
cg_sources_c += $(cg_webgl_sources_c)
endif

if CG_SUPPORT_XLIB
cg_public_h += $(cg_xlib_sources_h)
cg_sources_c += $(cg_xlib_sources_c)
endif

if CG_SUPPORT_GLX
cg_sources_c += $(cg_glx_sources_c)
endif

if CG_SUPPORT_WGL
cg_public_h += $(cg_wgl_sources_h)
cg_sources_c += $(cg_wgl_sources_c)
endif

if CG_SUPPORT_WAYLAND_EGL_SERVER
cg_public_h += $(cg_egl_wayland_server_sources_h)
endif
if CG_SUPPORT_EGL_PLATFORM_WAYLAND
cg_public_h += $(cg_egl_wayland_sources_h)
cg_sources_c += $(cg_egl_wayland_sources_c)
endif
if CG_SUPPORT_EGL_PLATFORM_KMS
cg_public_h += $(cg_egl_kms_sources_h)
cg_sources_c += $(cg_egl_kms_sources_c)
endif
if CG_SUPPORT_EGL_PLATFORM_XLIB
cg_sources_c += $(cg_egl_xlib_sources_c)
endif
if CG_SUPPORT_EGL_PLATFORM_POWERVR_NULL
cg_sources_c += $(cg_egl_powervr_null_sources_c)
endif
if CG_SUPPORT_EGL_PLATFORM_ANDROID
cg_sources_c += $(cg_egl_android_sources_c)
endif
if CG_SUPPORT_EGL
cg_public_h += $(cg_egl_sources_h)
cg_nodist_experimental_h += $(builddir)/cg-egl-defines.h
cg_sources_c += $(cg_egl_sources_c)
endif
if CG_SUPPORT_SDL2
cg_public_h += $(cg_sdl2_sources_h)
cg_sources_c += $(cg_sdl2_sources_c)
endif

EXTRA_DIST += stb_image.c

if UNIT_TESTS
lib_LTLIBRARIES += libcglib.la
else
noinst_LTLIBRARIES += libcglib.la
endif

libcglib_la_LIBADD = $(LIBM) $(CG_DEP_LIBS) $(CG_EXTRA_LDFLAGS)
libcglib_la_LIBADD += $(top_builddir)/clib/clib/libclib.la
if UNIT_TESTS
libcglib_la_LIBADD += $(top_builddir)/test-fixtures/libtest-fixtures.la
endif
#libcglib_la_LDFLAGS = -static
libcglib_la_SOURCES = $(cg_sources_c)

android_moduledir = $(datadir)/android_modules/cg
nobase_android_module_DATA = $(cg_android_sources) Android.mk

dist_source_dir = $(datadir)/src/cg
dist_source = \
	$(cg_emscripten_sources) \
	cg-emscripten-lib.h \
	cg-emscripten-lib.js \
	stb_image.c \
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
