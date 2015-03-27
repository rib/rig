AC_DEFUN([AM_CLIB],
[
  AC_REQUIRE([AC_CANONICAL_SYSTEM])
  AC_REQUIRE([AC_CANONICAL_HOST])

  CLIB_EXTRA_CFLAGS="$CLIB_EXTRA_FLAGS -std=c11"

  if test "x$enable_debug" = "xyes"; then
    CLIB_EXTRA_CFLAGS="$CLIB_EXTRA_CFLAGS -DC_DEBUG"
  fi

  dnl ================================================================
  dnl See what platform we are building for
  dnl ================================================================
  AM_CONDITIONAL(CROSS_COMPILING, [test x$cross_compiling = xyes])

  platform_linux=no
  platform_darwin=no
  platform_win32=no
  platform_web=no
  need_vasprintf=no
  AS_IF([test x"$emscripten_compiler" = xyes],
        [
          platform_web=yes
          OS="WEB"
        ],
        [
          case $host in
            *-*-msdos* | *-*-go32* | *-*-mingw32* | *-*-cygwin* | *-*-windows*)
              OS="WIN32"
              LDFLAGS="$LDFLAGS -no-undefined"
              platform_win32=yes
              need_vasprintf=yes
              ;;
            *-*darwin*)
              platform_darwin=yes
              OS="UNIX"
              ;;
            *-*-linux-android*)
              AC_MSG_ERROR([Use build/android/project when building for Android])
              ;;
            *-linux*)
              platform_linux=yes
              OS="UNIX"
              ;;
            *)
              AC_MSG_ERROR([Unsupported host: Please add to configure.ac])
              ;;
          esac
     ])
  AM_CONDITIONAL(OS_LINUX, [test "$platform_linux" = "yes"])
  AM_CONDITIONAL(OS_DARWIN, [test "$platform_darwin" = "yes"])
  AM_CONDITIONAL(OS_UNIX, [test "$platform_linux" = "yes" -o "$platform_darwin" = "yes"])
  AM_CONDITIONAL(OS_WIN32, [test "$platform_win32" = "yes"])
  AM_CONDITIONAL(OS_WEB, [test "$platform_web" = "yes"])

  case $target in
  arm*-darwin*)
      CLIB_EXTRA_CFLAGS="$CLIB_EXTRA_CFLAGS -U_FORTIFY_SOURCE"
      ;;
  esac

  target_osx=no
  target_ios=no

  if test "x$platform_darwin" = "xyes"; then
          AC_TRY_COMPILE([#include "TargetConditionals.h"],[
          #if TARGET_IPHONE_SIMULATOR == 1 || TARGET_OS_IPHONE == 1
          #error fail this for ios
          #endif
          return 0;
          ], [ target_osx=yes ], [ target_ios=yes ])
  fi
  AM_CONDITIONAL(OS_IOS, [test "$target_ios" = "yes"])
  AM_CONDITIONAL(OS_OSX, [test "$target_osx" = "yes"])

  AM_CONDITIONAL(NEED_VASPRINTF, test x$need_vasprintf = xyes )
  AM_ICONV()
  AC_SEARCH_LIBS(sqrtf, m)

  # nanosleep may not be part of libc, also search it in other libraries
  AC_SEARCH_LIBS(nanosleep, rt)

  AC_CHECK_HEADERS(langinfo.h iconv.h)

  dnl Note we don't simply use AC_CHECK_HEADERS() to check for
  dnl localcharset.h since we want to take into account which
  dnl libiconv AM_ICONV() found...
  save_LDFLAGS="$LDFLAGS"
  LDFLAGS=$LIBICONV
  AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <localcharset.h>],
                                  [locale_charset();])],
                    [AC_DEFINE([HAVE_LOCALCHARSET_H], [1],[localcharset.h])])
  LDFLAGS=$save_LDFLAGS

  AC_SUBST(CLIB_EXTRA_CFLAGS)
])
