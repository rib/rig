include $(top_srcdir)/build/autotools/Makefile.am.silent

# preamble

NULL =

SUBDIRS =

BUILT_SOURCES =

EXTRA_DIST =
CLEANFILES =
DISTCLEANFILES =

noinst_LTLIBRARIES =
lib_LTLIBRARIES =

AM_CPPFLAGS = \
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	-I$(srcdir)/winsys 			\
	-I$(srcdir)/driver/nop 			\
	-I$(srcdir)/driver/gl 			\
	-I$(srcdir)/driver/gl/gl 		\
	-I$(srcdir)/driver/gl/gles 		\
	-I$(top_srcdir)/clib/src		\
	-I$(top_builddir)/clib/src		\
	$(NULL)

AM_CPPFLAGS += \
	-DG_LOG_DOMAIN=\"Cogl\" 	\
	-DCG_COMPILATION		\
	-DCG_GL_LIBNAME=\"$(COGL_GL_LIBNAME)\" \
	-DCG_GLES2_LIBNAME=\"$(COGL_GLES2_LIBNAME)\" \
	-DCG_LOCALEDIR=\""$(localedir)"\" \
	$(NULL)

if HAVE_COGL_DEFAULT_DRIVER
AM_CPPFLAGS += -DCG_DEFAULT_DRIVER=\"$(COGL_DEFAULT_DRIVER)\"
endif

AM_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS)

BUILT_SOURCES += cogl-defines.h cogl-egl-defines.h cogl-gl-header.h
DISTCLEANFILES += cogl-defines.h cogl-egl-defines.h cogl-gl-header.h
EXTRA_DIST += cogl-defines.h.in cogl-egl-defines.h.in cogl-gl-header.h.in

# experimental api headers
# Note: we don't run glib-mkenums or gobject introspection over these headers
cogl_experimental_h = \
	$(NULL)

cogl_nodist_experimental_h = \
	$(NULL)


include $(srcdir)/Makefile.sources

cogl_public_h = $(cogl_base_public_h)
cogl_sources_c = $(cogl_base_sources_c)

if COGL_DRIVER_GL_SUPPORTED
cogl_sources_c += $(cogl_driver_gl_sources)
endif

if COGL_DRIVER_GLES_SUPPORTED
cogl_sources_c += $(cogl_driver_gles_sources)
endif

if COGL_USE_GLIB
cogl_public_h += $(cogl_glib_sources_h)
cogl_sources_c += $(cogl_glib_sources_c)
endif

if COGL_USE_UV
cogl_public_h += $(cogl_uv_sources_h)
cogl_sources_c += $(cogl_uv_sources_c)
endif

if COGL_SUPPORT_XLIB
cogl_public_h += $(cogl_xlib_sources_h)
cogl_sources_c += $(cogl_xlib_sources_c)
endif

if COGL_SUPPORT_GLX
cogl_sources_c += $(cogl_glx_sources_c)
endif

if COGL_SUPPORT_WGL
cogl_public_h += $(cogl_wgl_sources_h)
cogl_sources_c += $(cogl_wgl_sources_c)
endif

if COGL_SUPPORT_WAYLAND_EGL_SERVER
cogl_experimental_h += $(cogl_egl_wayland_server_sources_h)
endif
if COGL_SUPPORT_EGL_PLATFORM_WAYLAND
cogl_experimental_h += $(cogl_egl_wayland_sources_h)
cogl_sources_c += $(cogl_egl_wayland_sources_c)
endif
if COGL_SUPPORT_EGL_PLATFORM_KMS
cogl_experimental_h += $(cogl_egl_kms_sources_h)
cogl_sources_c += $(cogl_egl_kms_sources_c)
endif
if COGL_SUPPORT_EGL_PLATFORM_XLIB
cogl_sources_c += $(cogl_egl_xlib_sources_c)
endif
if COGL_SUPPORT_EGL_PLATFORM_POWERVR_NULL
cogl_sources_c += $(cogl_egl_powervr_null_sources_c)
endif
if COGL_SUPPORT_EGL_PLATFORM_GDL
cogl_sources_c += $(cogl_egl_gdl_sources_c)
endif
if COGL_SUPPORT_EGL_PLATFORM_ANDROID
cogl_sources_c += $(cogl_egl_android_sources_c)
endif
if COGL_SUPPORT_EGL
cogl_public_h += $(cogl_egl_sources_h)
cogl_nodist_experimental_h += $(builddir)/cogl-egl-defines.h
cogl_sources_c += $(cogl_egl_sources_c)
endif
if COGL_SUPPORT_SDL2
cogl_experimental_h += $(cogl_sdl2_sources_h)
cogl_sources_c += $(cogl_sdl2_sources_c)
endif

EXTRA_DIST += stb_image.c

if UNIT_TESTS
lib_LTLIBRARIES += libcogl2.la
else
noinst_LTLIBRARIES += libcogl2.la
endif

libcogl2_la_LIBADD = $(LIBM) $(COGL_DEP_LIBS) $(COGL_EXTRA_LDFLAGS)
libcogl2_la_LIBADD += $(top_builddir)/clib/src/libclib.la
if UNIT_TESTS
libcogl2_la_LIBADD += $(top_builddir)/test-fixtures/libtest-fixtures.la
endif
libcogl2_la_LDFLAGS = -static
libcogl2_la_SOURCES = $(cogl_sources_c)

android_moduledir = $(datadir)/android_modules/cogl
nobase_android_module_DATA = $(cogl_android_sources) Android.mk
