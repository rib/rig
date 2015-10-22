AC_DEFUN([AM_CLIB],
[
  AC_REQUIRE([AC_CANONICAL_SYSTEM])
  AC_REQUIRE([AC_CANONICAL_HOST])

  CLIB_EXTRA_CFLAGS="$CLIB_EXTRA_FLAGS -std=c11"

  if test "x$enable_debug" = "xyes"; then
    CLIB_EXTRA_CFLAGS="$CLIB_EXTRA_CFLAGS -DC_DEBUG"
  fi

  POSSIBLE_W_FLAGS="-Wall -Wcast-align -Wuninitialized
                    -Wno-strict-aliasing -Wempty-body -Wformat
                    -Wformat-security -Winit-self
                    -Wpointer-arith"
  AS_COMPILER_FLAGS([W_CFLAGS], [$POSSIBLE_W_FLAGS])

  CLIB_EXTRA_CFLAGS="$CLIB_EXTRA_CFLAGS ${W_CFLAGS/#  }"

  AM_CONDITIONAL(CROSS_COMPILING, [test x$cross_compiling = xyes])

  platform_linux=no
  platform_darwin=no
  platform_win32=no
  platform_web=no
  AS_IF([test x"$emscripten_compiler" = xyes], [platform_web=yes],
        [
          AS_CASE([$host_os],[linux*],    [platform_linux=yes])
          AS_CASE([$host_os],[darwin*],   [platform_darwin=yes])
          AS_CASE([$host_os],[mingw*],    [platform_win32=yes])
        ])

  AM_CONDITIONAL(OS_LINUX, [test "$platform_linux" = "yes"])
  AM_CONDITIONAL(OS_DARWIN, [test "$platform_darwin" = "yes"])
  AM_CONDITIONAL(OS_UNIX, [test "$platform_linux" = "yes" -o "$platform_darwin" = "yes"])
  AM_CONDITIONAL(OS_WIN32, [test "$platform_win32" = "yes"])
  AM_CONDITIONAL(OS_WEB, [test "$platform_web" = "yes"])

  target_osx=no
  target_ios=no

  AS_IF([test "x$platform_darwin" = "xyes"],
        [
          AC_TRY_COMPILE([#include "TargetConditionals.h"],[
          #if TARGET_IPHONE_SIMULATOR == 1 || TARGET_OS_IPHONE == 1
          #error fail this for ios
          #endif
          return 0;
          ], [ target_osx=yes ], [ target_ios=yes ])
        ])
  AM_CONDITIONAL(OS_IOS, [test "$target_ios" = "yes"])
  AM_CONDITIONAL(OS_OSX, [test "$target_osx" = "yes"])

  AC_SUBST(CLIB_EXTRA_CFLAGS)
])
