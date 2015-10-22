AC_DEFUN([AM_CGLIB],
[
  AC_REQUIRE([AC_USE_SYSTEM_EXTENSIONS])
  AC_REQUIRE([AC_CANONICAL_SYSTEM])
  AC_REQUIRE([AC_CANONICAL_HOST])
  AC_REQUIRE([AM_EMSCRIPTEN])
  AC_REQUIRE([AM_CLIB])
  AC_REQUIRE([AC_PATH_X])

  CG_EXTRA_CFLAGS="$CG_EXTRA_CFLAGS $CLIB_EXTRA_CFLAGS"
  CG_EXTRA_LDFLAGS="$CG_EXTRA_CFLAGS $CLIB_EXTRA_LDFLAGS"

  dnl ================================================================
  dnl Required versions for dependencies
  dnl ================================================================
  m4_define([gdk_pixbuf_req_version],     [2.0])
  m4_define([xfixes_req_version],         [3])
  m4_define([xcomposite_req_version],     [0.4])
  m4_define([xrandr_req_version],         [1.2])
  m4_define([cairo_req_version],          [1.10])
  m4_define([wayland_req_version],        [1.0.0])
  m4_define([wayland_server_req_version], [1.1.90])


  dnl ================================================================
  dnl Export the API versioning
  dnl ================================================================
  AC_SUBST([CG_MAJOR_VERSION],[2])
  AC_SUBST([CG_MINOR_VERSION],[0])
  AC_SUBST([CG_MICRO_VERSION],[0])
  AC_SUBST([CG_VERSION],[2.0.0])
  AC_SUBST([CG_API_VERSION],[2.0])


  dnl ================================================================
  dnl Export the source code release status
  dnl ================================================================
  AC_SUBST([CG_RELEASE_STATUS], [release_status])


  dnl ================================================================
  dnl Find an appropriate libm, for sin() etc.
  dnl ================================================================
  LT_LIB_M
  AC_SUBST(LIBM)


  dnl ================================================================
  dnl Handle extra configure options
  dnl ================================================================

  dnl     ============================================================
  dnl     Enable debugging
  dnl     ============================================================
  AS_IF([test "x$enable_debug" = "xyes"],
    [ CG_EXTRA_CFLAGS="$CG_EXTRA_CFLAGS -DCG_GL_DEBUG -DCG_OBJECT_DEBUG -DCG_ENABLE_DEBUG" ],
    [ CG_EXTRA_CFLAGS="$CG_EXTRA_CFLAGS -DG_DISABLE_CHECKS -DG_DISABLE_CAST_CHECKS" ]
  )

  dnl     ============================================================
  dnl     Enable cairo usage for debugging
  dnl       (debugging code can use cairo to dump the atlas)
  dnl     ============================================================

  PKG_CHECK_EXISTS([CAIRO], [cairo >= cairo_req_version], [have_cairo=yes])
  AS_IF([test "x$enable_cairo" = "xyes" && test "x$enable_debug" = "xyes"],
        [
          AS_IF([test "x$have_cairo" != "xyes"],
                [AC_MSG_ERROR([Could not find Cairo])])

          CG_PKG_REQUIRES="$CG_PKG_REQUIRES cairo >= cairo_req_version"
          AC_DEFINE([HAVE_CAIRO], [1], [Whether we have cairo or not])
        ])



  dnl     ============================================================
  dnl     Enable strict compiler flags
  dnl     ============================================================
  # strip leading spaces
  CG_EXTRA_CFLAGS="$CG_EXTRA_CFLAGS ${MAINTAINER_CFLAGS#*  }"


  dnl ================================================================
  dnl Check for dependency packages.
  dnl ================================================================

  AM_CONDITIONAL([CG_USE_GLIB], [test "x$enable_glib" = "xyes"])

  AS_IF([test "x$enable_glib" = "xyes"],
        [
          CG_DEFINES_SYMBOLS="$CG_DEFINES_SYMBOLS CG_HAS_GLIB_SUPPORT"
          CG_PKG_REQUIRES="$CG_PKG_REQUIRES glib-2.0"
        ])

  AM_CONDITIONAL([CG_USE_UV], [test "x$enable_uv" = "xyes"])

  AS_IF([test "x$enable_uv" = "xyes"],
        [
          CG_DEFINES_SYMBOLS="$CG_DEFINES_SYMBOLS CG_HAS_UV_SUPPORT"
          CG_PKG_REQUIRES="$CG_PKG_REQUIRES libuv"
        ])

  dnl     ============================================================
  dnl     Choose image loading backend
  dnl     ============================================================
  AC_ARG_ENABLE(
    [gdk-pixbuf],
    [AC_HELP_STRING([--enable-gdk-pixbuf=@<:@no/yes@:>@], [Enable image loading via gdk-pixbuf @<:@default=yes@:>@])],
    [],
    [AS_IF([test "x$enable_glib" = "xyes"],
           [PKG_CHECK_EXISTS([gdk-pixbuf-2.0 >= gdk_pixbuf_req_version],
                             [enable_gdk_pixbuf=yes],
                             [enable_gdk_pixbuf=no])])]
  )

  AC_ARG_ENABLE(
    [quartz-image],
    [AC_HELP_STRING([--enable-quartz-image=@<:@no/yes@:>@], [Enable image loading via quartz @<:@default=no@:>@])],
    [],
    enable_quartz_image=no
  )

  AS_IF(
    [test "x$enable_gdk_pixbuf" = "xyes"],
    [
      AS_IF([test "x$enable_glib" != "xyes"],
            [AC_MSG_ERROR([--disable-glib conflicts with --enable-gdk-pixbuf])])
      AC_DEFINE([USE_GDKPIXBUF], 1, [Use GdkPixbuf for loading image data])
      CG_PKG_REQUIRES="$CG_PKG_REQUIRES gdk-pixbuf-2.0 >= gdk_pixbuf_req_version"
      CG_IMAGE_BACKEND="gdk-pixbuf"
    ],
    [test "x$enable_quartz_image" = "xyes"],
    [
      EXPERIMENTAL_CONFIG=yes
      EXPERIMENTAL_OPTIONS="$EXPERIMENTAL_OPTIONS Quartz Core Graphics,"
      AC_DEFINE([USE_QUARTZ], 1,
                [Use Core Graphics (Quartz) for loading image data])
      CG_EXTRA_LDFLAGS="$CG_EXTRA_LDFLAGS -framework ApplicationServices"
      CG_IMAGE_BACKEND="quartz"
    ],
    [
      EXPERIMENTAL_CONFIG=yes
      EXPERIMENTAL_OPTIONS="$EXPERIMENTAL_OPTIONS fallback image decoding (stb_image),"
      AC_DEFINE([USE_INTERNAL], 1,
                [Use internal image decoding for loading image data])
      CG_IMAGE_BACKEND="stb_image"
    ]
  )

  dnl     ============================================================
  dnl     Determine which drivers and window systems we can support
  dnl     ============================================================

  dnl         ========================================================
  dnl         Drivers first...
  dnl         ========================================================
  EGL_CHECKED=no

  dnl This gets set to yes if CGlib directly links to the GL library API
  dnl so it doesn't need to be dlopened. This currently happens on OSX
  dnl and WGL where it's not clear if window system API can be separated
  dnl from the GL API.
  GL_LIBRARY_DIRECTLY_LINKED=no

  enabled_drivers=""

  HAVE_GLES2=0
  AS_IF([test "x$enable_gles2" = "xyes"],
        [
          AS_IF([test "x$platform_win32" = "xyes"],
                [AC_MSG_ERROR([GLES 2 not available for win32])])

          enabled_drivers="$enabled_drivers gles2"

          AC_DEFINE([HAVE_CG_GLES2], 1, [Have GLES 2.0 for rendering])
          HAVE_GLES2=1

          CG_DEFINES_SYMBOLS="$CG_DEFINES_SYMBOLS CG_HAS_GLES2_SUPPORT"

          AS_IF([test "x$emscripten_compiler" = "xyes"],
                [
                  GL_LIBRARY_DIRECTLY_LINKED=yes
                  CG_GLES2_LIBNAME=""
                  AC_DEFINE([HAVE_CG_WEBGL], 1, [Have WebGL for rendering])
                ],

                [
                  PKG_CHECK_EXISTS([glesv2],
                    [CG_PKG_REQUIRES_GL="$CG_PKG_REQUIRES_GL glesv2"
                     CG_GLES2_LIBNAME="libGLESv2.so"
                    ],
                    [
                      # We have to check the two headers independently as GLES2/gl2ext.h
                      # needs to include GLES2/gl2.h to have the GL types defined (eg.
                      # GLenum).
                      AC_CHECK_HEADER([GLES2/gl2.h],
                                      [],
                                      [AC_MSG_ERROR([Unable to locate GLES2/gl2.h])])
                      AC_CHECK_HEADER([GLES2/gl2ext.h],
                                      [],
                                      [AC_MSG_ERROR([Unable to locate GLES2/gl2ext.h])],
                                      [#include <GLES2/gl2.h>])

                      CG_GLES2_LIBNAME="libGLESv2.so"
                    ])
                ])
        ])

  HAVE_GL=0
  AS_IF([test "x$enable_gl" = "xyes"],
        [
          enabled_drivers="$enabled_drivers gl"

          PKG_CHECK_EXISTS([x11], [ALLOW_GLX=yes])

          AS_IF([test "x$platform_darwin" = "xyes"],
                [
                  CG_EXTRA_LDFLAGS="$CG_EXTRA_LDFLAGS -framework OpenGL"
                  dnl The GL API is being directly linked in so there is
                  dnl no need to dlopen it separately
                  GL_LIBRARY_DIRECTLY_LINKED=yes
                  CG_GL_LIBNAME=""
                ],

                [test "x$platform_win32" = "xyes"],
                [
                  CG_EXTRA_LDFLAGS="$CG_EXTRA_LDFLAGS -lopengl32 -lgdi32 -lwinmm"
                  CG_EXTRA_CFLAGS="$CG_EXTRA_CFLAGS -D_WIN32_WINNT=0x0500"
                  ALLOW_WGL=yes
                  dnl The GL API is being directly linked in so there is
                  dnl no need to dlopen it separately
                  GL_LIBRARY_DIRECTLY_LINKED=yes
                  CG_GL_LIBNAME=""
                ],

                [
                  PKG_CHECK_EXISTS([gl],
                    dnl We don't want to use CG_PKG_REQUIRES here because we don't want to
                    dnl directly link against libGL
                    [CG_PKG_REQUIRES_GL="$CG_PKG_REQUIRES_GL gl"],
                    [AC_CHECK_LIB(GL, [glGetString],
                                  ,
                                  [AC_MSG_ERROR([Unable to locate required GL library])])
                    ])
                  CG_GL_LIBNAME="libGL.so.1"
                ])

          AC_DEFINE([HAVE_CG_GL], [1], [Have GL for rendering])

          CG_DEFINES_SYMBOLS="$CG_DEFINES_SYMBOLS CG_HAS_GL_SUPPORT"
          HAVE_GL=1
        ])

  dnl Allow the GL library names and default driver to be overridden with configure options
  AC_ARG_WITH([gl-libname],
              [AS_HELP_STRING([--with-gl-libname],
                              override the name of the GL library to dlopen)],
              [CG_GL_LIBNAME="$withval"])
  AC_ARG_WITH([gles2-libname],
              [AS_HELP_STRING([--with-gles2-libname],
                              override the name of the GLESv2 library to dlopen)],
              [CG_GLES2_LIBNAME="$withval"])
  AC_ARG_WITH([default-driver],
              [AS_HELP_STRING([--with-default-driver],
                              specify a default cg driver)],
              [CG_DEFAULT_DRIVER="${withval}"],
              [CG_DEFAULT_DRIVER="" ])

  AM_CONDITIONAL(HAVE_CG_DEFAULT_DRIVER,
    [ test "x$CG_DEFAULT_DRIVER" != "x" ])


  AS_IF([test "x$emscripten_compiler" = "xyes"],
        [
          enabled_drivers="$enabled_drivers webgl"
          enable_webgl=yes
          CG_DEFINES_SYMBOLS="$CG_DEFINES_SYMBOLS CG_HAS_WEBGL_SUPPORT"

          GL_LIBRARY_DIRECTLY_LINKED=yes
          CG_GLES2_LIBNAME=""
          AC_DEFINE([HAVE_CG_WEBGL], 1, [Have WebGL for rendering])

          AC_DEFINE([HAVE_CG_GLES2], 1, [Have GLES 2.0 for rendering])
          HAVE_GLES2=1
        ])
  AM_CONDITIONAL(CG_SUPPORT_WEBGL, [test "x$enable_webgl" = "xyes" ])

  AM_CONDITIONAL([CG_DRIVER_GL_SUPPORTED], [test "x$enable_gl" = "xyes"])
  AM_CONDITIONAL([CG_DRIVER_GLES_SUPPORTED], [test "x$enable_gles2" = "xyes" -o "x$enable_webgl" = "xyes"])

  dnl         ========================================================
  dnl         Check window system integration libraries...
  dnl         ========================================================

  AS_IF([test "x$enable_glx" = "xyes"],
        [
          AS_IF([test "x$ALLOW_GLX" != "xyes"],
                [AC_MSG_ERROR([GLX not supported with this configuration])])

          NEED_XLIB=yes
          SUPPORT_GLX=yes
          GL_WINSYS_APIS="$GL_WINSYS_APIS glx"

          CG_DEFINES_SYMBOLS="$CG_DEFINES_SYMBOLS CG_HAS_GLX_SUPPORT"
        ])
  AM_CONDITIONAL(CG_SUPPORT_GLX, [test "x$SUPPORT_GLX" = "xyes"])

  AS_IF([test "x$enable_wgl" = "xyes"],
        [
          AS_IF([test "x$ALLOW_WGL" != "xyes"],
                [AC_MSG_ERROR([WGL not supported with this configuration])])

          SUPPORT_WGL=yes
          GL_WINSYS_APIS="$GL_WINSYS_APIS wgl"

          AC_DEFINE([CG_HAS_WGL_SUPPORT], [1], [CGlib supports OpenGL using the WGL API])
          CG_DEFINES_SYMBOLS="$CG_DEFINES_SYMBOLS CG_HAS_WIN32_SUPPORT"
        ])
  AM_CONDITIONAL(CG_SUPPORT_WGL, [test "x$SUPPORT_WGL" = "xyes"])

  AS_IF([test "x$enable_sdl2" = "xyes"],
        [
          PKG_CHECK_MODULES([SDL2],
                            [sdl2-rig],
                            [],
                            [AC_MSG_ERROR([SDL2 support requested but SDL2 not found])])

          SUPPORT_SDL2=yes
          GL_WINSYS_APIS="$GL_WINSYS_APIS sdl2"
          CG_PKG_REQUIRES="$CG_PKG_REQUIRES sdl2-rig"

          CG_DEFINES_SYMBOLS="$CG_DEFINES_SYMBOLS CG_HAS_SDL_SUPPORT"
        ],
        [SUPPORT_SDL2=no])
  AM_CONDITIONAL(CG_SUPPORT_SDL2, [test "x$SUPPORT_SDL2" = "xyes"])

  AS_IF([test "x$SUPPORT_SDL2" = "xyes" -a "x$SUPPORT_SDL" = "xyes"],
        [AC_MSG_ERROR([The SDL1 and SDL2 winsyses are currently mutually exclusive])])

  EGL_PLATFORM_COUNT=0

  AC_ARG_ENABLE(
    [null-egl-platform],
    [AC_HELP_STRING([--enable-null-egl-platform=@<:@no/yes@:>@], [Enable support for the NULL egl platform @<:@default=no@:>@])],
    [],
    enable_null_egl_platform=no
  )
  AS_IF([test "x$enable_null_egl_platform" = "xyes"],
        [
          EGL_PLATFORM_COUNT=$((EGL_PLATFORM_COUNT+1))
          NEED_EGL=yes
          EGL_PLATFORMS="$EGL_PLATFORMS null"

          CG_DEFINES_SYMBOLS="$CG_DEFINES_SYMBOLS CG_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT"
        ])
  AM_CONDITIONAL(CG_SUPPORT_EGL_PLATFORM_POWERVR_NULL,
                 [test "x$enable_null_egl_platform" = "xyes"])

  AC_ARG_ENABLE(
    [wayland-egl-platform],
    [AC_HELP_STRING([--enable-wayland-egl-platform=@<:@no/yes@:>@], [Enable support for the Wayland egl platform @<:@default=no@:>@])],
    [],
    [
        enable_wayland_egl_platform=no
        PKG_CHECK_EXISTS([wayland-egl >= wayland_req_version wayland-client >= wayland_req_version],
                         [enable_wayland_egl_platform=yes])
    ]
  )
  AS_IF([test "x$enable_wayland_egl_platform" = "xyes"],
        [
          EGL_PLATFORM_COUNT=$((EGL_PLATFORM_COUNT+1))
          NEED_EGL=yes
          EGL_PLATFORMS="$EGL_PLATFORMS wayland"

          PKG_CHECK_MODULES(WAYLAND_CLIENT,
                            [wayland-egl >= wayland_req_version wayland-client >= wayland_req_version])
          CG_PKG_REQUIRES="$CG_PKG_REQUIRES wayland-egl >= wayland_req_version"
          CG_PKG_REQUIRES="$CG_PKG_REQUIRES wayland-client >= wayland_req_version"

          CG_DEFINES_SYMBOLS="$CG_DEFINES_SYMBOLS CG_HAS_EGL_PLATFORM_WAYLAND_SUPPORT"
        ])
  AM_CONDITIONAL(CG_SUPPORT_EGL_PLATFORM_WAYLAND,
                 [test "x$enable_wayland_egl_platform" = "xyes"])

  AC_ARG_ENABLE(
    [kms-egl-platform],
    [AC_HELP_STRING([--enable-kms-egl-platform=@<:@no/yes@:>@], [Enable support for the KMS egl platform @<:@default=no@:>@])],
    [],
    [
        enable_kms_egl_platform=no
        PKG_CHECK_EXISTS([egl gbm libdrm], [enable_kms_egl_platform=yes])
    ]
  )
  AS_IF([test "x$enable_kms_egl_platform" = "xyes"],
        [
          EGL_PLATFORM_COUNT=$((EGL_PLATFORM_COUNT+1))
          NEED_EGL=yes
          EGL_PLATFORMS="$EGL_PLATFORMS kms"

          PKG_CHECK_EXISTS([gbm],
                           [
                             CG_PKG_REQUIRES="$CG_PKG_REQUIRES gbm"
                             CG_PKG_REQUIRES="$CG_PKG_REQUIRES libdrm"
                           ],
                           [AC_MSG_ERROR([Unable to locate required libgbm library for the KMS egl platform])])

          GBM_VERSION=`$PKG_CONFIG --modversion gbm`
          GBM_MAJOR=`echo $GBM_VERSION | cut -d'.' -f1`
          GBM_MINOR=`echo $GBM_VERSION | cut -d'.' -f2`
          GBM_MICRO=`echo $GBM_VERSION | cut -d'.' -f3 | sed 's/-.*//'`

          AC_DEFINE_UNQUOTED([CG_GBM_MAJOR], [$GBM_MAJOR], [The major version for libgbm])
          AC_DEFINE_UNQUOTED([CG_GBM_MINOR], [$GBM_MINOR], [The minor version for libgbm])
          AC_DEFINE_UNQUOTED([CG_GBM_MICRO], [$GBM_MICRO], [The micro version for libgbm])

          CG_DEFINES_SYMBOLS="$CG_DEFINES_SYMBOLS CG_HAS_EGL_PLATFORM_KMS_SUPPORT"
          CG_PKG_REQUIRES="$CG_PKG_REQUIRES egl gbm libdrm"
        ])
  AM_CONDITIONAL(CG_SUPPORT_EGL_PLATFORM_KMS, [test "x$enable_kms_egl_platform" = "xyes"])

  AC_ARG_ENABLE(
    [wayland-egl-server],
    [AC_HELP_STRING([--enable-wayland-egl-server=@<:@no/yes@:>@], [Enable server side wayland support @<:@default=no@:>@])],
    [],
    [
        enable_wayland_egl_server=no
        PKG_CHECK_EXISTS([egl wayland-server >= wayland_server_req_version],
                         [enable_wayland_egl_server=yes])
    ]
  )
  AS_IF([test "x$enable_wayland_egl_server" = "xyes"],
        [
          NEED_EGL=yes

          PKG_CHECK_MODULES(WAYLAND_SERVER,
                            [wayland-server >= wayland_server_req_version])
          CG_PKG_REQUIRES="$CG_PKG_REQUIRES wayland-server >= wayland_server_req_version"

          CG_DEFINES_SYMBOLS="$CG_DEFINES_SYMBOLS CG_HAS_WAYLAND_EGL_SERVER_SUPPORT"
        ])
  AM_CONDITIONAL(CG_SUPPORT_WAYLAND_EGL_SERVER,
                 [test "x$enable_wayland_egl_server" = "xyes"])

  dnl Android EGL platform
  AC_ARG_ENABLE(
    [android-egl-platform],
    [AC_HELP_STRING([--enable-android-egl-platform=@<:@no/yes@:>@], [Enable support for the Android egl platform @<:@default=no@:>@])],
    [],
    enable_android_egl_platform=no
  )
  AS_IF([test "x$enable_android_egl_platform" = "xyes"],
        [
          EGL_PLATFORM_COUNT=$((EGL_PLATFORM_COUNT+1))
          NEED_EGL=yes
          EGL_PLATFORMS="$EGL_PLATFORMS android"

          AC_CHECK_HEADER([android/native_window.h],
                          [],
                          [AC_MSG_ERROR([Unable to locate android/native_window.h])])

          CG_DEFINES_SYMBOLS="$CG_DEFINES_SYMBOLS CG_HAS_EGL_PLATFORM_ANDROID_SUPPORT"
        ])
  AM_CONDITIONAL(CG_SUPPORT_EGL_PLATFORM_ANDROID,
                 [test "x$enable_android_egl_platform" = "xyes"])

  dnl This should go last, since it's the default fallback and we need
  dnl to check the value of $EGL_PLATFORM_COUNT here.
  AC_ARG_ENABLE(
    [xlib-egl-platform],
    [AC_HELP_STRING([--enable-xlib-egl-platform=@<:@no/yes@:>@], [Enable support for the Xlib egl platform @<:@default=auto@:>@])],
    [],
    AS_IF([test "x$enable_gles2" = "xyes" && \
           test "x$emscripten_compiler" != "xyes" && \
           test "x$SUPPORT_SDL_WEBGL" != "xyes" && \
           test "x$SUPPORT_SDL2" != "xyes" && \
           test $EGL_PLATFORM_COUNT -eq 0],
          [enable_xlib_egl_platform=yes], [enable_xlib_egl_platform=no])
  )
  AS_IF([test "x$enable_xlib_egl_platform" = "xyes"],
        [
          EGL_PLATFORM_COUNT=$((EGL_PLATFORM_COUNT+1))
          NEED_EGL=yes
          NEED_XLIB=yes
          EGL_PLATFORMS="$EGL_PLATFORMS xlib"

          CG_DEFINES_SYMBOLS="$CG_DEFINES_SYMBOLS CG_HAS_EGL_PLATFORM_XLIB_SUPPORT"
        ])
  AM_CONDITIONAL(CG_SUPPORT_EGL_PLATFORM_XLIB,
                 [test "x$enable_xlib_egl_platform" = "xyes"])

  AS_IF([test "x$NEED_EGL" = "xyes" && test "x$EGL_CHECKED" != "xyes"],
        [
          PKG_CHECK_EXISTS([egl],
            [CG_PKG_REQUIRES="$CG_PKG_REQUIRES egl"],
            [
              AC_CHECK_HEADERS(
                [EGL/egl.h],
                [],
                [AC_MSG_ERROR([Unable to locate required EGL headers])])
              AC_CHECK_HEADERS(
                [EGL/eglext.h],
                [],
                [AC_MSG_ERROR([Unable to locate required EGL headers])],
                [#include <EGL/egl.h>])

              AC_CHECK_LIB(EGL, [eglInitialize],
                [CG_EXTRA_LDFLAGS="$CG_EXTRA_LDFLAGS -lEGL"],
                [AC_MSG_ERROR([Unable to locate required EGL library])])

              CG_EXTRA_LDFLAGS="$CG_EXTRA_LDFLAGS -lEGL"
            ]
            )

          CG_EGL_INCLUDES="#include <EGL/egl.h>
  #include <EGL/eglext.h>"
          AC_SUBST([CG_EGL_INCLUDES])
        ])

  AS_IF([test "x$NEED_EGL" = "xyes"],
        [
          SUPPORT_EGL=yes
          GL_WINSYS_APIS="$GL_WINSYS_APIS egl"
          CG_DEFINES_SYMBOLS="$CG_DEFINES_SYMBOLS CG_HAS_EGL_SUPPORT"
        ])

  AM_CONDITIONAL(CG_SUPPORT_EGL, [test "x$SUPPORT_EGL" = "xyes"])

  dnl         ========================================================
  dnl         Check X11 dependencies if required
  dnl         ========================================================
  AS_IF([test "x$NEED_XLIB" = "xyes"],
        [
          X11_MODULES="x11 xext xfixes >= xfixes_req_version xdamage xcomposite >= xcomposite_req_version xrandr >= xrandr_req_version"
          PKG_CHECK_MODULES(DUMMY, [$X11_MODULES],
                            [CG_PKG_REQUIRES="$CG_PKG_REQUIRES $X11_MODULES"])
          SUPPORT_X11=yes
          SUPPORT_XLIB=yes

          CG_DEFINES_SYMBOLS="$CG_DEFINES_SYMBOLS CG_HAS_X11_SUPPORT"
          CG_DEFINES_SYMBOLS="$CG_DEFINES_SYMBOLS CG_HAS_XLIB_SUPPORT"
        ])

  AM_CONDITIONAL(X11_TESTS, [test "x$SUPPORT_X11" = "xyes"])
  AM_CONDITIONAL(CG_SUPPORT_X11, [test "x$SUPPORT_X11" = "xyes"])
  AM_CONDITIONAL(CG_SUPPORT_XLIB, [test "x$SUPPORT_XLIB" = "xyes"])

  AC_SUBST(CG_PKG_REQUIRES)
  if test -n "$CG_PKG_REQUIRES"; then
    PKG_CHECK_MODULES(CG_DEP, [$CG_PKG_REQUIRES])

    if test -n "$CG_PKG_REQUIRES_GL"; then
      PKG_CHECK_MODULES(CG_DEP_GL, [$CG_PKG_REQUIRES_GL])

      dnl Strip out the GL libraries from the GL pkg-config files so we can
      dnl dynamically load them instead
      gl_libs=""
      for x in $CG_DEP_GL_LIBS; do
        AS_CASE([$x],
                [-lGL], [],
                [-lGLESv2], [],
                [-lGLESv1_CM], [],
                [*], [gl_libs="$gl_libs $x"])
      done
      CG_DEP_CFLAGS="$CG_DEP_CFLAGS $CG_DEP_CFLAGS_GL"
      CG_DEP_LIBS="$CG_DEP_LIBS $gl_libs"
    fi
  fi

  dnl ================================================================
  dnl Checks for library functions.
  dnl ================================================================

  dnl The 'ffs' function is part of C99 so it isn't always
  dnl available. CGlib has a fallback if needed.
  dnl
  dnl XXX: ffs isnt available with the emscripten toolchain currently
  dnl but the check passes so we manually skip the check in this case
  AS_IF([test "x$emscripten_compiler" = "xno"],
        [AC_CHECK_FUNCS([ffs])])

  dnl ================================================================
  dnl What needs to be substituted in other files
  dnl ================================================================

  AC_SUBST([CG_GL_LIBNAME])
  AC_SUBST([HAVE_GL])
  AC_SUBST([CG_GLES2_LIBNAME])
  AC_SUBST([HAVE_GLES2])
  AC_SUBST([CG_DEFAULT_DRIVER])

  CG_DEFINES="$CG_DEFINES_EXTRA"
  for x in $CG_DEFINES_SYMBOLS; do
    CG_DEFINES="$CG_DEFINES
  #define $x 1"
  done;
  AC_SUBST(CG_DEFINES)
  AM_SUBST_NOTMAKE(CG_DEFINES)

  AC_SUBST(CG_DEP_CFLAGS)
  AC_SUBST(CG_DEP_LIBS)
  AC_SUBST(CG_EXTRA_CFLAGS)
  AC_SUBST(CG_EXTRA_LDFLAGS)

  dnl ================================================================
  dnl Dah Da!
  dnl ================================================================
  echo ""
  echo "CGlib - $CG_VERSION (${CG_RELEASE_STATUS})"

  # Global flags
  echo ""
  echo " • Global:"
  echo "        Prefix: ${prefix}"
  if test "x$CG_DEFAULT_DRIVER" != "x"; then
  echo "        Default driver: ${CG_DEFAULT_DRIVER}"
  fi

  echo ""
  # Features
  echo " • Features:"
  echo "        Drivers: ${enabled_drivers}"
  AS_IF([test "x$GL_LIBRARY_DIRECTLY_LINKED" != xyes],
        [for driver in $enabled_drivers; do
           driver=`echo $driver | tr "[gles]" "[GLES]"`
           libname=`eval echo \\$CG_${driver}_LIBNAME`
           echo "        Library name for $driver: $libname"
         done])
  echo "        GL Window System APIs:${GL_WINSYS_APIS}"
  if test "x$SUPPORT_EGL" = "xyes"; then
  echo "        EGL Platforms:${EGL_PLATFORMS}"
  echo "        Wayland compositor support: ${enable_wayland_egl_server}"
  fi
  echo "        Building for emscripten environment: $emscripten_compiler"
  echo "        Image backend: ${CG_IMAGE_BACKEND}"

  # Compiler/Debug related flags
  echo ""
  echo " • Build options:"
  echo "        Debugging: ${enable_debug}"
  echo "        Profiling: ${enable_profile}"
  echo "        Enable deprecated symbols: ${enable_deprecated}"
  echo "        Compiler flags: ${CFLAGS} ${CG_EXTRA_CFLAGS}"
  echo "        Linker flags: ${LDFLAGS} ${CG_EXTRA_LDFLAGS}"

  echo ""

  # General warning about experimental features
  if test "x$EXPERIMENTAL_CONFIG" = "xyes"; then
  echo ""
  echo "☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠"
  echo " *WARNING*"
  echo ""
  echo "  The stability of your build might be affected by one or more"
  echo "  experimental configuration options."
  echo
  echo "  experimental options: $EXPERIMENTAL_OPTIONS"
  echo "☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠"
  echo ""
  fi
])
