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
	-I$(top_srcdir)/deps/clib/src		\
	-I$(top_builddir)/deps/clib/src		\
	$(NULL)

AM_CPPFLAGS += \
	-DG_LOG_DOMAIN=\"Cogl\" 	\
	-DCOGL_COMPILATION		\
	-DCOGL_GL_LIBNAME=\"$(COGL_GL_LIBNAME)\" \
	-DCOGL_GLES2_LIBNAME=\"$(COGL_GLES2_LIBNAME)\" \
	-DCOGL_LOCALEDIR=\""$(localedir)"\" \
	$(NULL)

if HAVE_COGL_DEFAULT_DRIVER
AM_CPPFLAGS += \
	-DCOGL_DEFAULT_DRIVER=\"$(COGL_DEFAULT_DRIVER)\"
endif


AM_CFLAGS = $(COGL_DEP_CFLAGS) $(COGL_EXTRA_CFLAGS) $(MAINTAINER_CFLAGS)

BUILT_SOURCES += cogl-defines.h cogl-egl-defines.h cogl-gl-header.h
DISTCLEANFILES += cogl-defines.h cogl-egl-defines.h cogl-gl-header.h
EXTRA_DIST += cogl-defines.h.in cogl-egl-defines.h.in cogl-gl-header.h.in

pc_files = cogl2.pc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pc_files)

EXTRA_DIST += cogl2.pc.in
DISTCLEANFILES += $(pc_files)

# experimental api headers
# Note: we don't run glib-mkenums or gobject introspection over these headers
cogl_experimental_h = \
	$(NULL)

cogl_nodist_experimental_h = \
	$(NULL)


include $(srcdir)/Makefile.sources

if COGL_DRIVER_GL_SUPPORTED
cogl_sources_c += $(cogl_driver_gl_sources)
endif

if COGL_DRIVER_GLES_SUPPORTED
cogl_sources_c += $(cogl_driver_gles_sources)
endif

if USE_GLIB
cogl_public_h += $(cogl_glib_sources_h)
cogl_sources_c += $(cogl_glib_sources_c)
endif

if SUPPORT_XLIB
cogl_public_h += $(cogl_xlib_sources_h)
cogl_sources_c += $(cogl_xlib_sources_c)
endif

if SUPPORT_GLX
cogl_sources_c += $(cogl_glx_sources_c)
endif

if SUPPORT_WGL
cogl_public_h += $(cogl_wgl_sources_h)
cogl_sources_c += $(cogl_wgl_sources_c)
endif

if SUPPORT_WAYLAND_EGL_SERVER
cogl_experimental_h += $(cogl_egl_wayland_server_sources_h)
endif
if SUPPORT_EGL_PLATFORM_WAYLAND
cogl_experimental_h += $(cogl_egl_wayland_sources_h)
cogl_sources_c += $(cogl_egl_wayland_sources_c)
endif
if SUPPORT_EGL_PLATFORM_KMS
cogl_experimental_h += $(cogl_egl_kms_sources_h)
cogl_sources_c += $(cogl_egl_kms_sources_c)
endif
if SUPPORT_EGL_PLATFORM_XLIB
cogl_sources_c += $(cogl_egl_xlib_sources_c)
endif
if SUPPORT_EGL_PLATFORM_POWERVR_NULL
cogl_sources_c += $(cogl_egl_powervr_null_sources_c)
endif
if SUPPORT_EGL_PLATFORM_GDL
cogl_sources_c += $(cogl_egl_gdl_sources_c)
endif
if SUPPORT_EGL_PLATFORM_ANDROID
cogl_sources_c += $(cogl_egl_android_sources_c)
endif
if SUPPORT_EGL
cogl_public_h += $(cogl_egl_sources_h)
cogl_nodist_experimental_h += $(builddir)/cogl-egl-defines.h
cogl_sources_c += $(cogl_egl_sources_c)
endif
if SUPPORT_SDL
cogl_public_h += $(cogl_sdl_sources_h)
cogl_sources_c += $(cogl_sdl_sources_c)
endif
if SUPPORT_SDL2
cogl_experimental_h += $(cogl_sdl2_sources_h)
cogl_sources_c += $(cogl_sdl2_sources_c)
endif

EXTRA_DIST += stb_image.c

if USE_GLIB
# glib-mkenums rules
glib_enum_h = cogl-enum-types.h
glib_enum_c = cogl-enum-types.c
glib_enum_headers = $(cogl_public_h)
include $(top_srcdir)/build/autotools/Makefile.am.enums
endif

#XXX should we specify a libdir sub-directory now so that
#for the next major version of Cogl we don't need to change
#the library name to safely allow parallel installability?
#E.g. libdir=$(libdir)/
lib_LTLIBRARIES += libcogl2.la

libcogl2_la_LIBADD = $(LIBM) $(COGL_DEP_LIBS) $(COGL_EXTRA_LDFLAGS)
libcogl2_la_LIBADD += $(top_builddir)/deps/clib/src/libclib.la
if UNIT_TESTS
libcogl2_la_LIBADD += $(top_builddir)/test-fixtures/libtest-fixtures.la
endif
# XXX: The aim is to eventually get rid of all private API exports
# for cogl-pango.
libcogl2_la_LDFLAGS = \
	-no-undefined \
	-version-info @COGL_LT_CURRENT@:@COGL_LT_REVISION@:@COGL_LT_AGE@ \
	-export-dynamic \
	-export-symbols-regex "^(cogl|_cogl_list_remove|_cogl_list_insert|_cogl_list_init|_cogl_get_atlas_set|_cogl_debug_flags|_cogl_atlas_new|_cogl_atlas_add_reorganize_callback|_cogl_atlas_reserve_space|_cogl_callback|_cogl_util_get_eye_planes_for_screen_poly|_cogl_atlas_texture_remove_reorganize_callback|_cogl_atlas_texture_add_reorganize_callback|_cogl_texture_get_format|_cogl_texture_foreach_sub_texture_in_region|_cogl_profile_trace_message|_cogl_context_get_default|_cogl_framebuffer_get_stencil_bits|_cogl_clip_stack_push_rectangle|_cogl_framebuffer_get_modelview_stack|_cogl_object_default_unref|_cogl_pipeline_foreach_layer_internal|_cogl_clip_stack_push_primitive|_cogl_buffer_unmap_for_fill_or_fallback|_cogl_primitive_draw|_cogl_debug_instances|_cogl_framebuffer_get_projection_stack|_cogl_pipeline_layer_get_texture|_cogl_buffer_map_for_fill_or_fallback|_cogl_texture_can_hardware_repeat|_cogl_pipeline_prune_to_n_layers|test_|unit_test_).*"

libcogl2_la_SOURCES = $(cogl_sources_c)
nodist_libcogl2_la_SOURCES = $(BUILT_SOURCES)

coglincludedir = $(includedir)/cogl2/cogl
coglinclude_HEADERS = $(cogl_public_h) $(cogl_experimental_h)
nodist_coglinclude_HEADERS = $(cogl_nodist_experimental_h) cogl-defines.h
if USE_GLIB
nodist_coglinclude_HEADERS += cogl-enum-types.h
endif

cogl_proto_includedir = $(includedir)/cogl2/cogl/gl-prototypes
cogl_proto_include_HEADERS = $(cogl_gl_prototypes_h)

dist-hook: ../build/win32/vs9/cogl.vcproj ../build/win32/vs10/cogl.vcxproj ../build/win32/vs10/cogl.vcxproj.filters ../build/win32/gen-enums.bat

# I know those filters below don't look nice, but this is to ensure the right files are in the Project files only *once*
../build/win32/vs9/cogl.vcproj: $(top_srcdir)/build/win32/vs9/cogl.vcprojin
	for F in `echo $(cogl_sources_c) $(BUILT_SOURCES) | tr '/' '\\'`; do \
		case $$F in \
		*-egl.c|*-egl-*.c|*-glx.c|*-xlib*.c|*-x11.c|*.wgl.c|*-gl.c|*-gles.c|*-kms.c|*-sdl.c|*.h) ;; \
		*.c)	echo '   <File RelativePath="..\..\..\cogl\'$$F'" />' \
			;; \
		esac; \
	done >cogl.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs9/cogl.vcprojin >$@
	rm cogl.sourcefiles

../build/win32/vs10/cogl.vcxproj: $(top_srcdir)/build/win32/vs10/cogl.vcxprojin
	for F in `echo $(cogl_sources_c) $(BUILT_SOURCES) | tr '/' '\\'`; do \
		case $$F in \
		*-egl.c|*-egl-*.c|*-glx.c|*-xlib*.c|*-x11.c|*.wgl.c|*-gl.c|*-gles.c|*-kms.c|*-sdl.c|*.h) ;; \
		*.c)	echo '    <ClCompile Include="..\..\..\cogl\'$$F'" />' \
			;; \
		esac; \
	done >cogl.vs10.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/cogl.vcxprojin >$@
	rm cogl.vs10.sourcefiles

../build/win32/vs10/cogl.vcxproj.filters: $(top_srcdir)/build/win32/vs10/cogl.vcxproj.filtersin
	for F in `echo $(cogl_sources_c) $(BUILT_SOURCES) | tr '/' '\\'`; do \
		case $$F in \
		*-egl.c|*-egl-*.c|*-glx.c|*-xlib*.c|*-x11.c|*.wgl.c|*-gl.c|*-gles.c|*-kms.c|*-sdl.c|*.h) ;; \
		*.c)	echo '    <ClCompile Include="..\..\..\cogl\'$$F'"><Filter>Sources</Filter></ClCompile>' \
			;; \
		esac; \
	done >cogl.vs10.sourcefiles.filters
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/cogl.vcxproj.filtersin >$@
	rm cogl.vs10.sourcefiles.filters

cogl.vsenums_h:
	echo 'perl %1\bin\glib-mkenums --symbol-prefix cogl_gtype --template ../../cogl/cogl-enum-types.h.in ' >vsenums_h.temp1
	for F in `echo $(cogl_public_h) $(srcdir)/cogl-win32-renderer.h`; do \
		case $$F in \
		*xlib*.h|*wayland*.h) ;; \
		*.h) echo '../../cogl'$$F' '	\
			;;	\
		esac;	\
	done >>vsenums_h.temp1
	cat vsenums_h.temp1 | sed 's_/cogl./_/cogl/_' >vsenums_h.temp
	cat vsenums_h.temp | tr -d '\n' >>$@
	echo '> ..\..\cogl\cogl-enum-types.h' >>$@
	rm vsenums_h.temp1
	rm vsenums_h.temp

cogl.vsenums_c:
	echo 'perl %1\bin\glib-mkenums --symbol-prefix cogl_gtype --template ../../cogl/cogl-enum-types.c.in ' >vsenums_c.temp1
	for F in `echo $(cogl_public_h) $(srcdir)/cogl-win32-renderer.h`; do \
		case $$F in \
		*xlib*.h|*wayland*.h) ;; \
		*.h) echo '../../cogl'$$F' '	\
			;;	\
		esac;	\
	done >>vsenums_c.temp1
	cat vsenums_c.temp1 | sed 's_/cogl./_/cogl/_' >vsenums_c.temp
	cat vsenums_c.temp | tr -d '\n' >>$@
	echo '> ..\..\cogl\cogl-enum-types.c' >>$@
	rm vsenums_c.temp1
	rm vsenums_c.temp

../build/win32/gen-enums.bat: cogl.vsenums_h cogl.vsenums_c
	echo 'if exist ..\..\cogl\cogl-enum-types.h goto DONE_COGL_ENUMS_H' >$@
	cat cogl.vsenums_h >>$@
	echo ':DONE_COGL_ENUMS_H' >>$@
	echo 'if exist ..\..\cogl\cogl-enum-types.c goto DONE_COGL_ENUMS_C' >>$@
	cat cogl.vsenums_c >>$@
	echo ':DONE_COGL_ENUMS_C' >>$@
	rm cogl.vsenums_h
	rm cogl.vsenums_c

EXTRA_DIST += \
	cogl-defines.h.win32.in	\
	cogl-defines.h.win32	\
	cogl-defines.h.win32_SDL.in	\
	cogl-defines.h.win32_SDL	\
	cogl-gl-header.h.win32	\
	cogl.symbols	\
	cogl.rc.in	\
	cogl.rc

DISTCLEANFILES += ../build/win32/vs9/cogl.vcproj ../build/win32/vs10/cogl.vcxproj ../build/win32/vs10/cogl.vcxproj.filters ../build/win32/gen-enums.bat cogl.rc

-include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS =

if HAVE_INTROSPECTION
Cogl-2.0.gir: libcogl2.la Makefile

Cogl_2_0_gir_NAMESPACE = Cogl
Cogl_2_0_gir_VERSION = 2.0
Cogl_2_0_gir_LIBS = libcogl2.la
if UNIT_TESTS
Cogl_2_0_gir_LIBS += $(top_builddir)/test-fixtures/libtest-fixtures.la
endif
Cogl_2_0_gir_FILES = $(cogl_public_h) cogl-enum-types.h
Cogl_2_0_gir_CFLAGS = $(AM_CPPFLAGS) $(COGL_DEP_CFLAGS) -UCOGL_ENABLE_EXPERIMENTAL_API -UCOGL_ENABLE_EXPERIMENTAL_2_0_API -UCOGL_COMPILATION -D__COGL_H_INSIDE__ -D__COGL_XLIB_H_INSIDE__
Cogl_2_0_gir_INCLUDES = GL-1.0 GObject-2.0
Cogl_2_0_gir_EXPORT_PACKAGES = cogl2
Cogl_2_0_gir_SCANNERFLAGS = --warn-all --c-include='cogl/cogl.h'

INTROSPECTION_GIRS += Cogl-2.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif
