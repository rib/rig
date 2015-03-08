dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)

dnl ================================================================
dnl Package version (i.e. the pretty number that users see)
dnl ================================================================
m4_define([rig_version], 2)

dnl ================================================================
dnl Interface version details for libtool
dnl ================================================================
# NB: I didn't copy the widely used glib/gtk style of dealing with libtool
# versioning since I found the different (compared to libtool's documentation)
# ideas for what the interface age should represent; and the addition of a
# binary age confusing. I couldn't find any documentation/mailing list archives
# etc explaing the ideas nor any code that makes use of those values. They also
# seem to go against the advice of not basing the libtool versioning on package
# versioning.
#
# For reference on how the various numbers should be updated at release time
# these rules are adapted from the libtool info pages:
#
#  1. Update the version information only immediately before a public
#     release of your software.
#
#  2. If the library source code has changed at all since the last
#     update, then increment REVISION (`C:R:A' becomes `C:r+1:A').
#
#  3. If any interfaces have been added, removed, or changed since the
#     last update, increment CURRENT, and set REVISION to 0.
#
#  4. If any interfaces have been added since the last public release,
#     then increment AGE.
#
#  5. If any interfaces have been removed since the last public release,
#     then set AGE to 0.
m4_define([rig_lt_current], 0)
m4_define([rig_lt_revision], 0)
m4_define([rig_lt_age], 0)
# We do also tell libtool the pretty version also:
m4_define([rig_lt_release], [rig_version])


dnl ================================================================
dnl Source code release status
dnl ================================================================
# Finally we explicitly track when we are building development source
# from Git vs building source corresponding to a release. As with the
# libtool version info we don't automatically derive this from the
# pretty version number because we want to test the results of
# updating the version number in advance of a release.
m4_define([release_status], [git])

AC_INIT(rig, [rig_version])
AC_CONFIG_SRCDIR(rut/rut-shell.c)
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([build/autotools])
AC_CONFIG_HEADERS(config.h)

AC_CANONICAL_SYSTEM
AC_CANONICAL_HOST

# Save this value here, since automake will set cflags later and we
# want to know if the user specified custom cflags or not.
cflags_set=${CFLAGS+set}

AM_INIT_AUTOMAKE([1.11 foreign -Wno-portability no-define no-dist-gzip
                  dist-bzip2 subdir-objects])
AM_SILENT_RULES([yes])

AH_BOTTOM([#include "config-custom.h"])

dnl ================================================================
dnl More version number stuff
dnl ================================================================
RIG_VERSION=rig_version

AC_SUBST(RIG_VERSION)


dnl ================================================================
dnl libtool versioning
dnl ================================================================
RIG_LT_CURRENT=rig_lt_current
RIG_LT_REVISION=rig_lt_revision
RIG_LT_AGE=rig_lt_age
RIG_LT_RELEASE=rig_lt_release
AC_SUBST(RIG_LT_CURRENT)
AC_SUBST(RIG_LT_REVISION)
AC_SUBST(RIG_LT_AGE)
AC_SUBST(RIG_LT_RELEASE)


dnl ================================================================
dnl Export the source code release status
dnl ================================================================
AC_SUBST([RIG_RELEASE_STATUS], [release_status])

dnl ================================================================
dnl Compiler stuff.
dnl ================================================================
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AC_PROG_OBJC
AC_PROG_CXX

AM_EMSCRIPTEN

dnl ================================================================
dnl Libtool stuff.
dnl ================================================================
LT_PREREQ([2.2.6])
LT_INIT([disable-static])
dnl when using libtool 2.x create libtool early, because it's used in the
dnl internal glib configure (as-glibconfig.m4)
m4_ifdef([LT_OUTPUT], [LT_OUTPUT])


dnl ================================================================
dnl I18n stuff.
dnl ================================================================
AM_GNU_GETTEXT_VERSION([0.17])
AM_GNU_GETTEXT([external])

GETTEXT_PACKAGE="Rig"
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,
                   "$GETTEXT_PACKAGE",
                   [The prefix for our gettext translation domains.])
AS_ALL_LINGUAS

dnl ================================================================
dnl Handle extra configure options
dnl ================================================================

m4_define([default], [m4_if(release_status, [git], [yes], [no])])
AC_ARG_ENABLE(
  [debug],
  [AC_HELP_STRING([--enable-debug=@<:@no/yes@:>@],
                  [Control debugging level @<:@default=]default[@:>@])],
  [enable_debug="$enableval"],
  [enable_debug=default]
)
AS_CASE(
  [$enable_debug],
  [yes],
  [
    test "$cflags_set" = set || CFLAGS="$CFLAGS -g -O0"
    RIG_EXTRA_CFLAGS="$RIG_EXTRA_CFLAGS -DRIG_ENABLE_DEBUG"

    AS_IF([test "x$emscripten_compiler" != xyes],
          [
            # We can use libprotobuf for pretty printing protocol buffer streams
            RIG_PKG_REQUIRES="$RIG_PKG_REQUIRES protobuf"
            RIG_EXTRA_LDFLAGS="$RIG_EXTRA_LDFLAGS -lstdc++"
          ],
          [
            dnl RIG_EXTRA_EMCC_JSFLAGS="$RIG_EXTRA_EMCC_JSFLAGS -s ASSERTIONS=2 -g4"
            RIG_EXTRA_EMCC_JSFLAGS="$RIG_EXTRA_EMCC_JSFLAGS -g4"
            RIG_EXTRA_EMCC_BCFLAGS="$RIG_EXTRA_EMCC_BCFLAGS -g4"
          ])
  ],
  [no],
  [
    RIG_EXTRA_CFLAGS="$RIG_EXTRA_CFLAGS -DG_DISABLE_CHECKS -DG_DISABLE_CAST_CHECKS"
  ],
  [AC_MSG_ERROR([Unknown argument for --enable-debug])]
)
AM_CONDITIONAL(RIG_DEBUG, [test "$enable_debug" = "yes"])

AC_CHECK_HEADER([execinfo.h], [have_backtrace=yes], [have_backtrace=no])
AC_CHECK_FUNC([backtrace], [], [have_backtrace=no])
AC_CHECK_FUNC([backtrace_symbols], [], [have_backtrace=no])

AS_IF([test "x$have_backtrace" = "xyes"],
      [AC_DEFINE([HAVE_BACKTRACE],
                 [1],
                 [Define if backtracing is available])])

AC_ARG_ENABLE(
  [refcount_debug],
  [AC_HELP_STRING([--enable-refcount-debug=@<:@no/yes@:>@],
                  [Enable tracking refcounts @<:@default=]default[@:>@])],
  [enable_refcount_debug="$enableval"],
  [enable_refcount_debug="$have_backtrace"]
)
AS_CASE(
  [$enable_refcount_debug],
  [yes],
  [
    AC_DEFINE([RUT_ENABLE_REFCOUNT_DEBUG], [1],
              [Define to enable refcount debugging])
  ],
  [no], [],
  [AC_MSG_ERROR([Unknown argument for --enable-refcount-debug])]
)

# use strict compiler flags only when building from git; the rules for
# distcheck will take care of turning this on when making a release
m4_define([default], [m4_if(release_status, [git], [yes], [no])])
AC_ARG_ENABLE(
  [maintainer-flags],
  [AC_HELP_STRING([--enable-maintainer-flags=@<:@no/yes/error@:>@], [Use strict compiler flags @<:@default=]default[@:>@])],
  [],
  enable_maintainer_flags=default
)

MAINTAINER_COMPILER_FLAGS="-Wall -Wuninitialized
                           -Wno-strict-aliasing -Wempty-body -Wformat
                           -Wformat-security -Winit-self
                           -Wpointer-arith"

AS_CASE(
  [$enable_maintainer_flags],
  [yes],
  [
    AS_COMPILER_FLAGS([MAINTAINER_CFLAGS], [$MAINTAINER_COMPILER_FLAGS])
  ],
  [no],
  [
  ],
  [error],
  [
    MAINTAINER_COMPILER_FLAGS="$MAINTAINER_COMPILER_FLAGS -Werror"
    AS_COMPILER_FLAGS([MAINTAINER_CFLAGS], [$MAINTAINER_COMPILER_FLAGS])
  ],
  [*],
  [AC_MSG_ERROR([Invalid option for --enable-maintainer-flags])]
)

# strip leading spaces
RIG_EXTRA_CFLAGS="$RIG_EXTRA_CFLAGS ${MAINTAINER_CFLAGS/#  }"


AC_ARG_ENABLE(profile,
              [AC_HELP_STRING([--enable-profile=@<:@no/yes@:>@],
                              [Turn on uprof profiling support. yes; All UProf profiling probe points are compiled in and may be runtime enabled. no; No profiling support will built into cogl.  @<:@default=no@:>@])],
              [],
              [enable_profile=no])
AS_IF([test "x$enable_profile" = "xyes"],
      [
        AS_IF([test "x$GCC" = "xyes"],
              [
                RIG_PKG_REQUIRES="$RIG_PKG_REQUIRES uprof-0.3"
                RIG_EXTRA_CFLAGS="$RIG_EXTRA_CFLAGS -DENABLE_PROFILE"
                AS_IF([test "x$enable_debug" = "xyes"], [RIG_EXTRA_CFLAGS="$RIG_EXTRA_CFLAGS -DUPROF_DEBUG"])
              ],
              [
                AC_MSG_ERROR([--enable-profile is currently only supported if using GCC])
              ])
      ])
AM_CONDITIONAL(PROFILE, test "x$enable_profile" != "xno")


AC_ARG_ENABLE(
  [unit-tests],
  [AC_HELP_STRING([--enable-unit-tests=@<:@no/yes@:>@], [Build unit tests @<:@default=yes@:>@])],
  [],
  enable_unit_tests=yes
)
AS_IF([test "x$enable_unit_tests" = "xyes"],
      [
        AC_DEFINE([ENABLE_UNIT_TESTS], [1], [Whether to enable building unit tests])
      ]
)
AS_IF([test "x$emscripten_compiler" = xyes], [enable_unit_tests=no])
AM_CONDITIONAL(UNIT_TESTS, test "x$enable_unit_tests" = "xyes")


AC_ARG_ENABLE(installed_tests,
              AS_HELP_STRING([--enable-installed-tests],
                             [Install test programs (default: no)]),,
              [enable_installed_tests=no])
AM_CONDITIONAL(ENABLE_INSTALLED_TESTS, test x$enable_installed_tests = xyes)


AC_ARG_ENABLE(uninstalled-build,
              AC_HELP_STRING([--enable-uninstalled-build],
			     [Search for files in build directory as well]),
	      enable_uninstalled=yes,)
if test "x$enable_uninstalled" = xyes; then
	AC_DEFINE_UNQUOTED(
        SHARE_UNINSTALLED_DIR,"`pwd`/rig/data",[path to source data dir])
fi

AC_ARG_ENABLE(editor,
              AC_HELP_STRING([--disable-editor],
			     [Disable building the interactive editor]),
              [],
	      [enable_editor=yes])
AS_IF([test "x$emscripten_compiler" = xyes], [enable_editor=no])
AS_IF([test "x$enable_editor" = xyes],
      [
	AC_DEFINE(RIG_EDITOR_ENABLED, [1], [Whether to build the editor or not])
	AC_DEFINE(RUT_EDITOR_ENABLED, [1], [Whether to build the editor or not])
      ])
AM_CONDITIONAL(EDITOR_ENABLED, test "x$enable_editor" = "xyes")

AC_ARG_ENABLE(
  [oculus-rift],
  [AC_HELP_STRING([--enable-oculus-rift=@<:@no/yes@:>@], [Enable Oculus Rift support @<:@default=yes@:>@])],
  [],
  enable_oculus_rift=yes
)
AS_IF([test "x$emscripten_compiler" = xyes], [enable_oculus_rift=no])
AS_IF([test "x$enable_oculus_rift" = "xyes"],
      [
        AC_DEFINE([ENABLE_OCULUS_RIFT], [1], [Whether to enable Oculus Rift supporta])
        EXTRA_FEATURES="$EXTRA_FEATURES Oculus Rift"
      ]
)
AM_CONDITIONAL(OCULUS_RIFT, test "x$enable_oculus_rift" = "xyes")


AC_MSG_CHECKING([for alignof])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([const int thing = alignof (int);],
                                   [(void) 0])],
                  [AC_DEFINE([HAVE_ALIGNOF], [1],
                             [Whether alignof can be used or not])
                   AC_MSG_RESULT([yes])],
                  [AC_MSG_RESULT([no])])
AC_MSG_CHECKING([for __alignof__])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([const int thing = __alignof__ (int);],
                                   [(void) 0])],
                  [AC_DEFINE([HAVE_ALIGNOF_UNDERSCORE], [1],
                             [Whether __alignof__ can be used or not])
                   AC_MSG_RESULT([yes])],
                  [AC_MSG_RESULT([no])])


dnl ================================================================
dnl Check for dependency packages.
dnl ================================================================

RIG_PKG_REQUIRES="$RIG_PKG_REQUIRES icu-uc harfbuzz harfbuzz-icu fontconfig freetype2 libprotobuf-c"

dnl libuv...
use_uv=no
AS_IF([test "x$emscripten_compiler" != xyes],
      [
        use_uv=yes
        AC_DEFINE([USE_UV], [1], [Use libuv])
      ])
AM_CONDITIONAL([USE_UV], [test "x$use_uv" = xyes])

dnl SpiderMonkey
have_mozjs=no
PKG_CHECK_EXISTS([mozjs-24],
                 [
                   have_mozjs=yes
                   AC_DEFINE([USE_MOZJS], [1], [Use Mozilla SpiderMonkey Javascript engine])
                   RIG_PKG_REQUIRES="$RIG_PKG_REQUIRES mozjs-24"
                 ])
AM_CONDITIONAL([USE_MOZJS], [test "x$have_mozjs" = xyes])

dnl     ============================================================
dnl     Check glib dependencies
dnl     ============================================================
AS_IF([test "x$emscripten_compiler" != xyes],
      [
       AM_PATH_GLIB_2_0([2.26.0],
                        [have_glib=yes], [have_glib=no],
                        [gobject gthread gmodule-no-export])
      ])
AS_IF([test "x$enable_editor" = xyes],
    RIG_PKG_REQUIRES="$RIG_PKG_REQUIRES pangocairo glib-2.0 gio-2.0 gdk-pixbuf-2.0"
    AC_DEFINE([USE_GLIB], [1], [Use Glib])
    AC_DEFINE([USE_PANGO], [1], [Use Pango])
    AC_DEFINE([USE_GDK_PIXBUF], [1], [Use GDK Pixbuf])
)
AM_CONDITIONAL([USE_GLIB], ["x$enable_editor" = xyes])

have_gstreamer=no
PKG_CHECK_EXISTS([gstreamer-1.0], [have_gstreamer=yes])
AC_ARG_ENABLE(
  [gstreamer],
  [AC_HELP_STRING([--enable-gstreamer=@<:@no/yes@:>@],
                  [Enable gstreamer support @<:@default=auto@:>@])],
  [enable_gstreamer="$enableval"],
  [
    AS_IF([test "x$enable_editor" != "xyes"],
          [ enable_gstreamer="$have_gstreamer"].
          [ enable_gstreamer="no"])
])
AS_IF([test "x$enable_gstreamer" = "xyes"],
      [
       enable_cogl_gst=yes
       RIG_PKG_REQUIRES="$RIG_PKG_REQUIRES gstreamer-1.0"
       AC_DEFINE([USE_GSTREAMER], [1], [Enable gstreamer integration])
       EXTRA_FEATURES="$EXTRA_FEATURES gstreamer"
      ])
AM_CONDITIONAL([USE_GSTREAMER], [test "x$enable_gstreamer" = "xyes"])

PKG_CHECK_EXISTS([x11],
                 [
                   RIG_PKG_REQUIRES="$RIG_PKG_REQUIRES x11"
                   AC_DEFINE([USE_XLIB], [1], [Enable x11 integration])
                 ])

AC_CHECK_FUNC(fmemopen,
              [
                have_fmemopen=yes
                AC_DEFINE([HAVE_FMEMOPEN], [1], [Have fmemopen(3) implementation])
              ], [have_fmemopen=no])
AS_IF([test "x$emscripten_compiler" = xyes], [have_fmemopen=yes])
AM_CONDITIONAL([NEED_FMEMOPEN], [test "x$have_fmemopen" = "xno"])

dnl Add the build flags for android_native_app_glue
AS_IF([test "x$have_android" = "xyes"],
      [ANDROID_CHECK_MODULE([ANDROID_NATIVE_APP_GLUE],
                            [android/native_app_glue],
                            [android_native_app_glue])
      ANDROID_CHECK_MODULE([GLIB_ANDROID],
                           [glib-android],
                           [glib-android])
      CFLAGS="$CFLAGS $ANDROID_NATIVE_APP_GLUE_CFLAGS $GLIB_ANDROID_CFLAGS"])

PKG_CHECK_MODULES([LIBCRYPTO],
                  [libcrypto],
                  [have_libcrypto=yes],
                  [have_libcrypto=no])
AM_CONDITIONAL([HAVE_LIBCRYPTO], [test "x$have_libcrypto" = "xyes"])

AC_CHECK_LIB(ncursesw, [subwin],
             [HAVE_NCURSES=yes],
             [HAVE_NCURSES=no])
AM_CONDITIONAL([USE_NCURSES], [test "x$HAVE_NCURSES" = "xyes"])

CheckForGtk()
{
  PKG_CHECK_EXISTS([gtk], [have_gtk=yes], [have_gtk=no])
  AC_ARG_ENABLE([gtk],
                AS_HELP_STRING([--enable-gtk],
                               [Use GTK/X11 for menus and dialog boxes]),
                [enable_gtk="$enableval"],
                [enable_gtk="$have_gtk"])
  AS_CASE([$enable_gtk],
          [yes],
          [RIG_PKG_REQUIRES="$RIG_PKG_REQUIRES gtk+-x11-3.0"
           AC_DEFINE([USE_GTK], [1], [Defined if GTK should be used])
          ],
          [no], [],
          [AC_MSG_ERROR([Invalid value for --enable-gtk])])
}

dnl ================================================================
dnl Check requirements for our C utility api
dnl ================================================================
AM_CLIB

RIG_EXTRA_CFLAGS="$RIG_EXTRA_CFLAGS $CLIB_EXTRA_CFLAGS"
RIG_EXTRA_LDFLAGS="$RIG_EXTRA_CFLAGS $CLIB_EXTRA_LDFLAGS"



dnl ================================================================
dnl Check requirements for our GPU utility api
dnl ================================================================

AS_IF([test "x$emscripten_compiler" = xyes],
      [
        enable_webgl=yes
        enable_cogl_path=yes
      ],
      [
        case "$host" in
          *-linux*)
            enable_glib=$enable_editor
            enable_cogl_path=yes
            enable_cogl_pango=$enable_editor
            enable_uv=yes
            enable_gl=yes
            enable_glx=yes
            enable_xlib_egl_platform=yes

            RIG_PKG_REQUIRES="$RIG_PKG_REQUIRES xkbcommon-x11"
            RIG_EXTRA_LDFLAGS="$RIG_EXTRA_LDFLAGS -lXi"
            AC_DEFINE([USE_X11], [1], [Defined if X11 should be used])

            AS_IF([test "x$enable_editor" = xyes],
                  CheckForGtk
                  RIG_PKG_REQUIRES="$RIG_PKG_REQUIRES avahi-glib avahi-client avahi-core"
                  AC_DEFINE([USE_AVAHI], [1], [Defined if Avahi should be used])
                  avahi_enabled=yes
                  )
            ;;

          *-darwin*)
            RIG_EXTRA_LDFLAGS="$RIG_EXTRA_LDFLAGS -lobjc -Wl,-framework,Cocoa"
            platform=osx
            ;;

          *)
            enable_cogl_path=yes
            enable_cogl_pango=$enable_editor
            enable_sdl2=yes
            enable_uv=yes
            enable_gles2=yes

            AC_DEFINE([USE_SDL], [1], [Defined if SDL should be used])
            RIG_PKG_REQUIRES="$RIG_PKG_REQUIRES sdl2-rig"
          ;;
        esac
      ])

AM_CONDITIONAL([USE_X11], [x$enable_glx = xyes])
AM_CONDITIONAL([USE_SDL], [x$enable_sdl2 = xyes])
AM_CONDITIONAL([HAVE_OSX], [test "x$platform" = "xosx"])
AM_CONDITIONAL([HAVE_GTK], [test "x$enable_gtk" = "xyes"])
AM_CONDITIONAL([HAVE_AVAHI], [test "x$avahi_enabled" = "xyes"])

AM_COGL

AC_SUBST(CLIB_PKG_REQUIRES)
AS_IF([test -n "$CLIB_PKG_REQUIRES"],
      [
        PKG_CHECK_MODULES(CLIB_DEP, [$CLIB_PKG_REQUIRES])
      ])

AC_SUBST(RIG_PKG_REQUIRES)
PKG_CHECK_MODULES(RIG_DEP, [$RIG_PKG_REQUIRES])

PKG_CHECK_VAR(ICU_PREFIX, icu-uc, prefix, [],
              [ AC_MSG_ERROR([Could not locate ICU data]) ])

dnl ============================================================
dnl Optional LLVM Enable
dnl ============================================================
dnl
dnl LLVM is pretty unwieldy so for now we default to not require it
dnl
dnl Note: Handled after pkg-config check so we can append to
dnl RIG_DEP_ vars
dnl
RIG_LLVM_PREFIX=""
RIG_LLVM_VERSION=""
AC_ARG_ENABLE(
  [llvm],
  [AC_HELP_STRING([--enable-llvm=@<:@no/yes@:>@],
                  [Enable tracking refcounts @<:@default=no@:>@])],
  [enable_llvm="$enableval"],
  [enable_llvm="no"]
)
AS_IF([test "x$emscripten_compiler" = xyes], [enable_llvm=no])
AS_CASE(
  [$enable_llvm],
  [yes],
  [
    AC_DEFINE([USE_LLVM], [1], [Define LLVM integration])
    dnl LLVM_COMPONENTS="jit interpreter nativecodegen bitreader bitwriter ipo selectiondag asmparser"
    LLVM_COMPONENTS="all"
    dnl Note empty "" implies 'all' but explicitly passing 'all' seems to confuse llvm-config
    dnl LLVM_COMPONENTS="support jit interpreter nativecodegen bitreader bitwriter codegen ipo linker selectiondag asmparser instrumentation irreeader"
    RIG_DEP_CFLAGS="$RIG_DEP_CFLAGS `llvm-config --cflags`"
    RIG_CLANG_LIBS="$RIG_CLANG_LIBS -lclangFrontendTool -lclangFrontend -lclangSerialization -lclangDriver -lclangCodeGen -lclangParse -lclangSema -lclangStaticAnalyzerFrontend -lclangStaticAnalyzerCheckers -lclangStaticAnalyzerCore -lclangAnalysis -lclangARCMigrate -lclangRewriteFrontend -lclangRewriteCore -lclangEdit -lclangAST -lclangLex -lclangBasic"
    RIG_CLANG_LIBS="$RIG_CLANG_LIBS `llvm-config --ldflags`"
    RIG_CLANG_LIBS="$RIG_CLANG_LIBS `llvm-config --libs $LLVM_COMPONENTS`"

    RIG_LLVM_PREFIX="`llvm-config --prefix`"
    RIG_LLVM_VERSION="`llvm-config --version`"
  ],
  [no], [],
  [AC_MSG_ERROR([Unknown argument for --enable-llvm])]
)
AM_CONDITIONAL([USE_LLVM], [test "x$enable_llvm" = "xyes"])
AC_SUBST(RIG_LLVM_PREFIX)
AC_SUBST(RIG_LLVM_VERSION)


dnl ================================================================
dnl Misc program dependencies.
dnl ================================================================
AC_PROG_INSTALL

dnl ================================================================
dnl GObject-Introspection check
dnl ================================================================
GOBJECT_INTROSPECTION_CHECK([gi_req_version])


dnl ================================================================
dnl What needs to be substituted in other files
dnl ================================================================
AC_SUBST(RIG_EXTRA_CFLAGS)
AC_SUBST(RIG_EXTRA_LDFLAGS)
AC_SUBST(RIG_DEP_CFLAGS)
AC_SUBST(RIG_DEP_LIBS)
AC_SUBST(RIG_CLANG_CFLAGS)
AC_SUBST(RIG_CLANG_LIBS)
AC_SUBST(RIG_EXTRA_EMCC_JSFLAGS)
AC_SUBST(RIG_EXTRA_EMCC_BCFLAGS)


dnl ================================================================
dnl Dah Da!
dnl ================================================================
AC_OUTPUT(
Makefile
test-fixtures/Makefile
libuv/Makefile
clib/Makefile
clib/src/Makefile
xdgmime/Makefile
rut/Makefile
data/Makefile
data/Info.plist
rig/Makefile
rig/rig-defines.h
toys/Makefile
tools/Makefile
research/particle-engine/Makefile
research/particle-engine/pe/Makefile
research/particle-engine/examples/Makefile
tests/Makefile
tests/config.env
tests/conform/Makefile
tests/unit/Makefile
tests/micro-perf/Makefile
tests/data/Makefile
LibOVR/Makefile
h2o/Makefile
po/Makefile.in
android/Makefile
)

echo ""
echo "Rig - $RIG_VERSION ($RIG_RELEASE_STATUS)"
echo ""
echo " • Editor: $enable_editor"
echo ""

if test -n "$EXTRA_FEATURES"; then
echo " • Optional Features: $EXTRA_FEATURES"
echo ""
fi

echo " • Compiler options:"
echo "        Rig debug: ${enable_debug}"
echo "        Refcount debugging: ${enable_refcount_debug}"
echo "        Compiler flags: ${CFLAGS} ${RIG_EXTRA_CFLAGS}"
echo "        Linker flags: ${LDFLAGS} ${RIG_EXTRA_LDFLAGS}"
echo ""
echo " • Extra:"
echo "        Build unit tests: ${enable_unit_tests}"
echo ""
