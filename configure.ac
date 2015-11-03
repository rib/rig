dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)

dnl ================================================================
dnl Package version (i.e. the pretty number that users see)
dnl ================================================================
m4_define([rig_version], 2)

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

RIG_VERSION=rig_version

AC_SUBST(RIG_VERSION)


dnl Compiler stuff.
dnl ================================================================
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AC_PROG_OBJC
AC_PROG_CXX

LT_INIT

dnl Gettext
dnl ================================================================
AM_GNU_GETTEXT_VERSION([0.17])
AM_GNU_GETTEXT([external])

GETTEXT_PACKAGE="Rig"
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,
                   "$GETTEXT_PACKAGE",
                   [The prefix for our gettext translation domains.])
AS_ALL_LINGUAS

dnl Configure options
dnl ================================================================

AC_ARG_ENABLE(
  [debug],
  [AC_HELP_STRING([--enable-debug=@<:@no/yes@:>@],
                  [Control debugging level @<:@default=yes@:>@])],
  [enable_debug="$enableval"],
  [enable_debug=yes]
)
AS_CASE(
  [$enable_debug],
  [yes],
  [
    test "$cflags_set" = set || CFLAGS="$CFLAGS -g -O0"
    RIG_EXTRA_CFLAGS="$RIG_EXTRA_CFLAGS -DRIG_ENABLE_DEBUG"

    # We can use libprotobuf for pretty printing protocol buffer streams
    RIG_PKG_REQUIRES="$RIG_PKG_REQUIRES protobuf"
    RIG_EXTRA_LDFLAGS="$RIG_EXTRA_LDFLAGS -lstdc++"
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

AC_ARG_ENABLE(
  [maintainer-flags],
  [AC_HELP_STRING([--enable-maintainer-flags=@<:@no/yes/error@:>@],
                  [Use strict compiler flags @<:@default=yes@:>@])],
  [],
  enable_maintainer_flags=yes
)

MAINTAINER_COMPILER_FLAGS="-Wall -Wcast-align -Wuninitialized
                           -Wno-strict-aliasing -Wempty-body -Wformat
                           -Wformat-security -Winit-self
                           -Wpointer-arith"

AS_CASE(
  [$enable_maintainer_flags],
  [yes],
  [ AS_COMPILER_FLAGS([MAINTAINER_CFLAGS], [$MAINTAINER_COMPILER_FLAGS]) ],
  [no],
  [ ],
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
AM_CONDITIONAL(UNIT_TESTS, test "x$enable_unit_tests" = "xyes")


AC_ARG_ENABLE(installed_tests,
              AS_HELP_STRING([--enable-installed-tests],
                             [Install test programs (default: no)]),,
              [enable_installed_tests=no])
AM_CONDITIONAL(ENABLE_INSTALLED_TESTS, test x$enable_installed_tests = xyes)


AC_ARG_ENABLE(editor,
              AC_HELP_STRING([--disable-editor],
			     [Disable building the interactive editor]),
              [],
	      [enable_editor=yes])
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
AS_IF([test "x$enable_oculus_rift" = "xyes"],
      [
        AC_DEFINE([ENABLE_OCULUS_RIFT], [1], [Whether to enable Oculus Rift supporta])
        EXTRA_FEATURES="$EXTRA_FEATURES Oculus Rift"
      ]
)
AM_CONDITIONAL(OCULUS_RIFT, test "x$enable_oculus_rift" = "xyes")


dnl Check for dependency packages.
dnl ================================================================

RIG_PKG_REQUIRES="$RIG_PKG_REQUIRES libpng icu-uc harfbuzz harfbuzz-icu fontconfig freetype2 libprotobuf-c mozjs-24"

dnl libuv...
AM_CONDITIONAL([USE_UV], [true])
AC_DEFINE([USE_UV], [1], [Use libuv])

dnl SpiderMonkey
have_mozjs=no
PKG_CHECK_EXISTS([mozjs-24],
                 [
                   have_mozjs=yes
                   AC_DEFINE([USE_MOZJS], [1], [Use Mozilla SpiderMonkey Javascript engine])
                   RIG_PKG_REQUIRES="$RIG_PKG_REQUIRES mozjs-24"
                 ])
AM_CONDITIONAL([USE_MOZJS], [test "x$have_mozjs" = xyes])

have_opencv=no
PKG_CHECK_EXISTS([opencv >= 3.0.0],
                 [
                   have_opencv=yes
                   AC_DEFINE([USE_OPENCV], [1], [Use OpenCV])
                   RIG_PKG_REQUIRES="$RIG_PKG_REQUIRES opencv"
                   EXTRA_FEATURES="$EXTRA_FEATURES OpenCV"
                 ])
AM_CONDITIONAL([USE_OPENCV], [test "x$have_opencv" = xyes])
PKG_CHECK_MODULES(OPENCV_DEP, [opencv >= 3.0.0])

dnl     Check glib dependencies
dnl     ============================================================
AM_PATH_GLIB_2_0([2.26.0],
                 [have_glib=yes], [have_glib=no],
                 [gobject gthread gmodule-no-export])

AC_DEFINE([USE_GDK_PIXBUF], [1], [Use GDK Pixbuf])
AC_DEFINE([USE_GLIB], [1], [Use Glib])
AM_CONDITIONAL([USE_GLIB], [true])

RIG_PKG_REQUIRES="$RIG_PKG_REQUIRES glib-2.0 gio-2.0 gdk-pixbuf-2.0"

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
       enable_cglib_gst=yes
       RIG_PKG_REQUIRES="$RIG_PKG_REQUIRES gstreamer-1.0"
       AC_DEFINE([USE_GSTREAMER], [1], [Enable gstreamer integration])
       EXTRA_FEATURES="$EXTRA_FEATURES gstreamer"
      ])
AM_CONDITIONAL([USE_GSTREAMER], [test "x$enable_gstreamer" = "xyes"])
AM_CONDITIONAL([USE_CGLIB_GST], [test "x$enable_gstreamer" = "xyes"])

AM_CONDITIONAL([USE_CGLIB_PANGO], [false])
AM_CONDITIONAL([USE_CGLIB_PATH], [false])

PKG_CHECK_EXISTS([x11],
                 [
                   RIG_PKG_REQUIRES="$RIG_PKG_REQUIRES x11"
                   AC_DEFINE([USE_XLIB], [1], [Enable x11 integration])
                 ])

PKG_CHECK_MODULES([LIBCRYPTO],
                  [libcrypto],
                  [have_libcrypto=yes],
                  [have_libcrypto=no])
AM_CONDITIONAL([HAVE_LIBCRYPTO], [test "x$have_libcrypto" = "xyes"])

AC_CHECK_LIB(ncursesw, [subwin],
             [
               HAVE_NCURSES=yes
               AC_DEFINE([USE_NCURSES], [1], ["Use ncurses for debugging view"])
             ],
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

dnl Check requirements for our C utility api
dnl ================================================================
AM_CONDITIONAL([USE_SYSTEM_UV], [false])
AM_CLIB

RIG_EXTRA_CFLAGS="$RIG_EXTRA_CFLAGS $CLIB_EXTRA_CFLAGS"
RIG_EXTRA_LDFLAGS="$RIG_EXTRA_CFLAGS $CLIB_EXTRA_LDFLAGS"



dnl Check requirements for our GPU utility api
dnl ================================================================

case "$host" in
  *-linux*)
    enable_glib=yes # TODO: need to remove dependency on gdk-pixbuf
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
    enable_sdl2=yes
    enable_uv=yes
    enable_gles2=yes

    AC_DEFINE([USE_SDL], [1], [Defined if SDL should be used])
    RIG_PKG_REQUIRES="$RIG_PKG_REQUIRES sdl2-rig"
  ;;
esac

AM_CONDITIONAL([USE_X11], [x$enable_glx = xyes])
AM_CONDITIONAL([USE_SDL], [x$enable_sdl2 = xyes])
AM_CONDITIONAL([HAVE_OSX], [test "x$platform" = "xosx"])
AM_CONDITIONAL([HAVE_GTK], [test "x$enable_gtk" = "xyes"])
AM_CONDITIONAL([HAVE_AVAHI], [test "x$avahi_enabled" = "xyes"])

AM_CGLIB

AC_SUBST(CLIB_PKG_REQUIRES)
AS_IF([test -n "$CLIB_PKG_REQUIRES"],
      [
        PKG_CHECK_MODULES(CLIB_DEP, [$CLIB_PKG_REQUIRES])
      ])

AC_SUBST(RIG_PKG_REQUIRES)
PKG_CHECK_MODULES(RIG_DEP, [$RIG_PKG_REQUIRES])

PKG_CHECK_VAR(ICU_PREFIX, icu-uc, prefix, [],
              [ AC_MSG_ERROR([Could not locate ICU data]) ])

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


dnl What needs to be substituted in other files
dnl ================================================================
AC_SUBST(RIG_EXTRA_CFLAGS)
AC_SUBST(RIG_EXTRA_LDFLAGS)
AC_SUBST(RIG_DEP_CFLAGS)
AC_SUBST(RIG_DEP_LIBS)
AC_SUBST(RIG_CLANG_CFLAGS)
AC_SUBST(RIG_CLANG_LIBS)


dnl Done!
dnl ================================================================
AC_OUTPUT(
Makefile
test-fixtures/Makefile
libuv/Makefile
clib/Makefile
clib/clib/Makefile
cglib/Makefile
cglib/cglib/Makefile
cglib/cglib/cg-defines.h
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
h2o/Makefile
LibOVR/Makefile
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
