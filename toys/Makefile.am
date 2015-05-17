include $(top_srcdir)/build/autotools/Makefile.am.silent

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/rut \
	-I$(top_builddir)/rut \
	-I$(top_srcdir)/libuv/include \
	-I$(top_srcdir)/clib/src \
	-I$(top_builddir)/clib/src \
	-I$(top_srcdir)/LibOVR/Src

if USE_UV
AM_CPPFLAGS += -I$(top_srcdir)/h2o/include
AM_CPPFLAGS += -I$(top_srcdir)/h2o
endif

if OCULUS_RIFT
AM_CPPFLAGS += -I$(top_srcdir)/LibOVR/Src
endif

AM_CFLAGS = \
	$(COGL_DEP_CFLAGS) \
	$(COGL_EXTRA_CFLAGS) \
	$(COGL_PANGO_DEP_CFLAGS) \
	$(COGL_GST_DEP_CFLAGS)

AM_CFLAGS += -DCG_EXAMPLES_DATA=\""$(abs_top_srcdir)/toys/"\"

common_ldadd = \
	$(RIG_EXTRA_LDFLAGS) \
	$(top_builddir)/rut/librut.la \
	$(top_builddir)/cogl/libcg.la \
	$(top_builddir)/cogl-path/libcogl-path.la \
	$(RIG_DEP_LIBS) \
	$(COGL_DEP_LIBS) \
	$(top_builddir)/clib/src/libclib.la \
	-lstdc++ \
	$(LIBM)

if EDITOR_ENABLED
common_ldadd += $(top_builddir)/cogl-pango/libcogl-pango2.la
endif

if USE_GSTREAMER
common_ldadd += $(top_builddir)/cogl-gst/libcogl-gst.la
endif

if OCULUS_RIFT
common_ldadd += $(top_builddir)/LibOVR/libOVR.la
endif

programs = cogl-info
emscripten_programs = cogl-info

cogl_info_SOURCES = cogl-info.c
cogl_info_LDADD = $(common_ldadd)

if USE_UV
programs += cogl-hello cogl-gles2-context cogl-point-sprites
endif
examples_datadir = $(pkgdatadir)/examples-data
examples_data_DATA =

cogl_hello_SOURCES = cogl-hello.c
cogl_hello_LDADD = $(common_ldadd)

if USE_GLIB
programs += cogl-msaa
cogl_msaa_SOURCES = cogl-msaa.c
cogl_msaa_LDADD = $(common_ldadd)
endif

cogl_point_sprites_SOURCES = cogl-point-sprites.c
cogl_point_sprites_LDADD = $(common_ldadd)

if BUILD_COGL_PANGO
programs += cogl-crate
cogl_crate_SOURCES = cogl-crate.c
cogl_crate_LDADD = $(common_ldadd)
examples_data_DATA += crate.jpg
endif

if BUILD_COGL_GST
programs += cogl-basic-video-player
cogl_basic_video_player_SOURCES = cogl-basic-video-player.c
cogl_basic_video_player_LDADD = $(common_ldadd)
endif

if USE_GLIB
if X11_TESTS
programs += cogl-x11-foreign cogl-x11-tfp
cogl_x11_foreign_SOURCES = cogl-x11-foreign.c
cogl_x11_foreign_LDADD = $(common_ldadd)
cogl_x11_tfp_SOURCES = cogl-x11-tfp.c
cogl_x11_tfp_LDADD = $(common_ldadd)
endif
endif

if COGL_SUPPORT_WAYLAND_EGL_SERVER
programs += cogland
cogland_SOURCES = cogland.c
cogland_LDADD = $(common_ldadd)
endif

cogl_gles2_context_SOURCES = cogl-gles2-context.c
cogl_gles2_context_LDADD = $(common_ldadd)

if COGL_SUPPORT_SDL2
programs += cogl-sdl2-hello
cogl_sdl2_hello_SOURCES = cogl-sdl2-hello.c
cogl_sdl2_hello_LDADD = $(common_ldadd)
endif

if OCULUS_RIFT
programs += libovr-test
libovr_test_SOURCES = libovr-test.c
livovr_test_LDADD = $(common_ldadd)
endif

programs += shell-hello
emscripten_programs += shell-hello
shell_hello_SOURCES = shell-hello.c
shell_hello_LDADD = $(common_ldadd)

emscripten_programs += cogl-emscripten-hello
cogl_emscripten_hello_SOURCES = cogl-emscripten-hello.c
cogl_emscripten_hello_LDADD = $(common_ldadd)

if USE_UV
programs += websocket
websocket_SOURCES = websocket.c
websocket_LDADD = $(common_ldadd) $(top_builddir)/h2o/libh2o.la -lssl -lcrypto
endif

noinst_PROGRAMS = $(programs)

EXTRA_DIST = \
	crate.jpg \
	android/hello/AndroidManifest.xml \
	android/hello/jni/Application.mk \
	android/hello/jni/Android.mk \
	android/hello/jni/main.c \
	android/hello/res/values/strings.xml \
	android/hello/.gitignore \
	android/hello/build.xml

