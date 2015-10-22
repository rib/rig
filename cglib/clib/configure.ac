AC_PREREQ(2.59)

cflags_set=${CFLAGS+set} # See if user set custom cflags

AC_INIT(clib, [1.0.0], [http://github.com/rib/clib/issues])
AC_CONFIG_SRCDIR(clib/clib.h)
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([build/autotools])
AC_CONFIG_HEADERS(config.h)
AC_CANONICAL_SYSTEM
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([1.11 foreign -Wno-portability no-define no-dist-gzip dist-bzip2 subdir-objects])
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AM_SILENT_RULES([yes])
LT_INIT

AC_ARG_ENABLE(
  [debug],
  [AC_HELP_STRING([--enable-debug=@<:@no/yes@:>@],
                  [Control debugging level @<:@default=yes@:>@])],
  [enable_debug="$enableval"],
  [enable_debug=yes]
)
AS_IF([test "x$enable_debug" = "xyes"],
      [ test "$cflags_set" = set || CFLAGS="$CFLAGS -g -O0" ])

AC_ARG_ENABLE(
  [unit-tests],
  [AC_HELP_STRING([--enable-unit-tests=@<:@no/yes@:>@], [Build unit tests @<:@default=yes@:>@])],
  [],
  enable_unit_tests=yes
)
AS_IF([test "x$enable_unit_tests" = "xyes"],
      [ AC_DEFINE([ENABLE_UNIT_TESTS], [1], [Whether to enable building unit tests]) ])
AM_CONDITIONAL(UNIT_TESTS, test "x$enable_unit_tests" = "xyes")


PKG_CHECK_MODULES(LIBUV_DEP, [libuv])
AM_CONDITIONAL([USE_UV], [true])
AM_CONDITIONAL([USE_SYSTEM_UV], [true])
AC_DEFINE([USE_UV], [1], [Use libuv])

AM_CLIB

AC_OUTPUT(
Makefile
clib/Makefile
)

echo ""
echo "Clib configured:"
echo ""
echo " • Compiler options:"
echo "        Clib debug: ${enable_debug}"
echo "        Compiler flags: ${CFLAGS} ${CLIB_EXTRA_CFLAGS}"
echo "        Linker flags: ${LDFLAGS}"
echo ""
