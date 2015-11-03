include $(top_srcdir)/build/autotools/Makefile.am.silent

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/rut \
	-I$(top_builddir)/rut \
	-I$(top_srcdir)/rig \
	-I$(top_builddir)/rig/protobuf-c-rpc \
	-I$(top_builddir)/rig \
	-I$(top_srcdir)/libuv/include \
	-I$(top_srcdir)/clib/clib \
	-I$(top_builddir)/clib/clib \
	-I$(top_srcdir)/cglib \
	-I$(top_builddir)/cglib \
	-I$(top_srcdir)/LibOVR/Src \
	$(RIG_DEP_CFLAGS)

if USE_UV
#AM_CPPFLAGS += -I$(top_srcdir)/h2o/include
#AM_CPPFLAGS += -I$(top_srcdir)/h2o
endif

if OCULUS_RIFT
AM_CPPFLAGS += -I$(top_srcdir)/LibOVR/Src
endif

AM_CFLAGS = \
	$(CG_DEP_CFLAGS) \
	$(CG_EXTRA_CFLAGS) \
	$(CG_PANGO_DEP_CFLAGS) \
	$(CG_GST_DEP_CFLAGS) \
	$(RIG_EXTRA_CFLAGS)

AM_CFLAGS += -DCG_EXAMPLES_DATA=\""$(abs_top_srcdir)/toys/"\"

common_ldadd = \
	$(RIG_EXTRA_LDFLAGS) \
	$(top_builddir)/rig/librig.la \
	$(top_builddir)/rut/librut.la \
	$(top_builddir)/cglib/cglib/libcglib.la \
	$(RIG_DEP_LIBS) \
	$(CG_DEP_LIBS) \
	$(top_builddir)/clib/clib/libclib.la \
	-lstdc++ \
	$(LIBM)

if USE_NCURSES
common_ldadd += -lncursesw
endif

if USE_UV
#common_ldadd += $(top_builddir)/xdgmime/libxdgmime.la
#common_ldadd += $(top_builddir)/h2o/libh2o.la -lssl -lcrypto
endif

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

if USE_CGLIB_PANGO
programs += cogl-crate
cogl_crate_SOURCES = cogl-crate.c
cogl_crate_LDADD = $(common_ldadd)
examples_data_DATA += crate.jpg
endif

if USE_CGLIB_GST
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

if CG_SUPPORT_WAYLAND_EGL_SERVER
programs += cogland
cogland_SOURCES = cogland.c
cogland_LDADD = $(common_ldadd)
endif

cogl_gles2_context_SOURCES = cogl-gles2-context.c
cogl_gles2_context_LDADD = $(common_ldadd)

if CG_SUPPORT_SDL2
programs += cogl-sdl2-hello
cogl_sdl2_hello_SOURCES = cogl-sdl2-hello.c
cogl_sdl2_hello_LDADD = $(common_ldadd)
endif

if OCULUS_RIFT
programs += libovr-test
libovr_test_SOURCES = libovr-test.c
libovr_test_LDADD = $(common_ldadd)
endif

programs += shell-hello
shell_hello_SOURCES = shell-hello.c
shell_hello_LDADD = $(common_ldadd)

programs += rig-hello
rig_hello_SOURCES = rig-hello.c
rig_hello_LDADD = $(common_ldadd)
rig_hello_LDFLAGS = -export-dynamic

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

