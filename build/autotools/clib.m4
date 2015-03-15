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
  AC_C_BIGENDIAN([ORDER=G_BIG_ENDIAN],[ORDER=G_LITTLE_ENDIAN])

  platform_linux=no
  platform_darwin=no
  platform_win32=no
  platform_web=no
  PIDTYPE="int"
  PATHSEP="/"
  SEARCHSEP=":"
  need_vasprintf=no
  AS_IF([test x"$emscripten_compiler" = xyes],
        [
          platform_web=yes
          PATHSEP="/"
          SEARCHSEP=":"
          OS="WEB"
        ],
        [
          case $host in
            *-*-msdos* | *-*-go32* | *-*-mingw32* | *-*-cygwin* | *-*-windows*)
              PATHSEP='\\' # add ' to avoid syntax hightlighting getting confused by escaped quote
              SEARCHSEP=";"
              OS="WIN32"
              PIDTYPE="void *"
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
  i*86-*-darwin*)
      ORDER=G_LITTLE_ENDIAN
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
          ], [
                  AC_DEFINE(TARGET_OSX,1,[The JIT/AOT targets OSX])
                  target_osx=yes
          ], [
                  AC_DEFINE(TARGET_IOS,1,[The JIT/AOT targets iOS])
                  target_ios=yes
          ])
     AC_DEFINE(TARGET_MACH,1,[The JIT/AOT targets Apple platforms])
  fi
  AM_CONDITIONAL(TARGET_IOS, [test "$target_ios" = "yes"])
  AM_CONDITIONAL(TARGET_OSX, [test "$target_osx" = "yes"])

  AC_CHECK_HEADERS(fcntl.h limits.h unistd.h stdarg.h stddef.h stdint.h stdlib.h)
  AC_CHECK_FUNCS(strlcpy stpcpy strtok_r rewinddir)

  AC_CHECK_LIB(pthread, pthread_create,
               [
                   HAVE_PTHREADS=1
                   AC_DEFINE(HAVE_PTHREADS,1,[Have pthreads])
               ], [HAVE_PTHREADS=0])
  AM_CONDITIONAL(USE_PTHREADS, [test "x$HAVE_PTHREADS" = "x1"])
  AC_SUBST(HAVE_PTHREADS)

  #
  # iOS detection of strndup and getpwuid_r is faulty for some reason
  # so simply avoid it
  #
  if test x$target_ios = xno; then
  AC_CHECK_FUNCS(strndup getpwuid_r)
  fi

  AM_CONDITIONAL(NEED_VASPRINTF, test x$need_vasprintf = xyes )
  AM_ICONV()
  AC_SEARCH_LIBS(sqrtf, m)

  # nanosleep may not be part of libc, also search it in other libraries
  AC_SEARCH_LIBS(nanosleep, rt)

  AC_CHECK_HEADERS(getopt.h sys/time.h sys/wait.h pwd.h langinfo.h iconv.h sys/types.h)
  AC_CHECK_HEADER(alloca.h, [HAVE_ALLOCA_H=1], [HAVE_ALLOCA_H=0])
  AC_SUBST(HAVE_ALLOCA_H)

  dnl Note we don't simply use AC_CHECK_HEADERS() to check for
  dnl localcharset.h since we want to take into account which
  dnl libiconv AM_ICONV() found...
  save_LDFLAGS="$LDFLAGS"
  LDFLAGS=$LIBICONV
  AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <localcharset.h>],
                                  [locale_charset();])],
                    [AC_DEFINE([HAVE_LOCALCHARSET_H], [1],[localcharset.h])])
  LDFLAGS=$save_LDFLAGS

  AC_MSG_CHECKING([for _Static_assert])
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([_Static_assert (1, "");],
                                     [(void) 0])],
                    [AC_DEFINE([HAVE_STATIC_ASSERT], [1],
                               [Whether _Static_assert can be used or not])
                     HAVE_STATIC_ASSERT=1
                     AC_MSG_RESULT([yes])],
                    [AC_MSG_RESULT([no])]
                     HAVE_STATIC_ASSERT=0
                    )
  AC_SUBST(HAVE_STATIC_ASSERT)

  AC_SUBST(CLIB_EXTRA_CFLAGS)
  AC_SUBST(ORDER)
  AC_SUBST(PATHSEP)
  AC_SUBST(SEARCHSEP)
  AC_SUBST(OS)
  AC_SUBST(PIDTYPE)
])
