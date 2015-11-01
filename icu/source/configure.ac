# -*-autoconf-*-
AC_COPYRIGHT([ Copyright (c) 1999-2013, International Business Machines Corporation and others. All Rights Reserved. ])
# configure.in for ICU
# Stephen F. Booth, heavily modified by Yves and others

# Check for autoconf version
AC_PREREQ(2.68)

m4_define([u_major_version], 53)
m4_define([u_minor_version], 1)
m4_define([u_patchlevel_version], 0)
m4_define([u_version], u_major_version.u_minor_version.u_patchlevel_version)

# Process this file with autoconf to produce a configure script
AC_INIT([ICU],
	[u_version],
        [http://icu-project.org/bugs],
        [International Components for Unicode],
        [http://icu-project.org])
AC_CONFIG_SRCDIR([common/unicode/utypes.h])
AC_CONFIG_HEADERS(config.h)

# Determine the host system
AC_CANONICAL_SYSTEM
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([1.14 foreign subdir-objects])
AM_SILENT_RULES([yes])

PACKAGE="icu"
AC_SUBST(PACKAGE)


U_MAJOR_VERSION=u_major_version
U_MINOR_VERSION=u_minor_version
U_PATCHLEVEL_VERSION=u_patchlevel_version
AC_SUBST(U_MAJOR_VERSION)
AC_SUBST(U_MINOR_VERSION)
AC_SUBST(U_PATCHLEVEL_VERSION)

# Compute a reasonable library version from the release version. This is
# very bad, but that's wanted... We want to make sure that the LIB_VERSION
# has at least a dot in it, so we'll add a .0 if needed.
#[LIB_VERSION=`echo $VERSION | sed -e 's/\.//' -e 's/^\([^.]*\)$/\1.0/'`]
LIB_VERSION=u_major_version.u_minor_version.u_patchlevel_version
LIB_VERSION_MAJOR=u_major_version
AC_SUBST(VERSION)
AC_SUBST(LIB_VERSION)
AC_SUBST(LIB_VERSION_MAJOR)


AC_MSG_CHECKING(for ICU Unicode version number)

# Get the ICU unicode version from uchar.h
getuversion() {
    [sed -n 's/^[       ]*#[    ]*define[       ]*U_UNICODE_VERSION[    ]*"\([^"]*\)".*/\1/p' "$@"]
}
UNICODE_VERSION=`getuversion $srcdir/common/unicode/uchar.h`
if test x"$UNICODE_VERSION" = x; then
    AC_MSG_ERROR([Cannot determine Unicode version number from uchar.h header file])
fi

AC_MSG_RESULT([unicode version $UNICODE_VERSION])
AC_SUBST(UNICODE_VERSION)


AC_PROG_CC
AC_PROG_CXX

LT_PREREQ([2.2.6])
LT_INIT


# Accumulate #defines

# These are defines that are set for ICU Build time only.
# They are only needed for building ICU itself. Example: platform stuff
U_CPPFLAGS=""
U_CFLAGS=""
U_CXXFLAGS=""

# UCONFIG_CPPFLAGS: These are defines which are set for ICU build time,
# and also a notice is output that they need to be set
# for end-users of ICU also. uconfig.h.prepend is generated
# with, for example, "#define U_DISABLE_RENAMING 1"
# Example: ICU configuration stuff
UCONFIG_CPPFLAGS=""

# Check whether to build debug libraries
AC_MSG_CHECKING([whether to build debug libraries])
enabled=no
ENABLE_DEBUG=0
AC_ARG_ENABLE(debug,
    [  --enable-debug          build debug libraries and enable the U_DEBUG define [default=no]],
    [ case "${enableval}" in
         yes|"") enabled=yes; ENABLE_DEBUG=1; U_CPPFLAGS="$U_CPPFLAGS -DU_DEBUG=1" ;;
         *) ;;
    esac],
)
AC_MSG_RESULT($enabled)
AC_SUBST(ENABLE_DEBUG)

# Check for doxygen to generate documentation
AC_PATH_PROG(DOXYGEN,doxygen,,$PATH:/usr/local/bin:/usr/bin)

AC_SUBST(ARFLAGS)
AC_SUBST(COMPILE_LINK_ENVVAR)

ENABLE_STATIC=YES
ENABLE_SHARED=NO

dnl Check whether to enable auto cleanup of libraries
AC_MSG_CHECKING([whether to enable auto cleanup of libraries])
enabled=no
UCLN_NO_AUTO_CLEANUP=1
AC_ARG_ENABLE(auto-cleanup,
    [  --enable-auto-cleanup   enable auto cleanup of libraries [default=no]],
    [ case "${enableval}" in
         yes) enabled=yes;
         U_CPPFLAGS="$U_CPPFLAGS -DUCLN_NO_AUTO_CLEANUP=0";
         UCLN_NO_AUTO_CLEANUP=0
          ;;
         *) ;;
    esac],
)
AC_MSG_RESULT($enabled)
AC_SUBST(UCLN_NO_AUTO_CLEANUP)

# look for 'ar' the proper way
AC_CHECK_TOOL(AR, ar, false)

U_DISABLE_RENAMING=1
UCONFIG_CPPFLAGS="${UCONFIG_CPPFLAGS} -DU_DISABLE_RENAMING=1"
AC_SUBST(U_DISABLE_RENAMING)

AC_MSG_CHECKING([whether to enable function and data tracing])
enabled=no
U_ENABLE_TRACING=0
AC_ARG_ENABLE(tracing,
    [  --enable-tracing        enable function and data tracing [default=no]],
    [ case "${enableval}" in
	     yes|"") enabled=yes;
                     U_CPPFLAGS="$U_CPPFLAGS -DU_ENABLE_TRACING=1";
                     U_ENABLE_TRACING=1 ;;
	     no) enabled=no; U_ENABLE_TRACING=0 ;;
	     *) ;;
    esac],
)
AC_MSG_RESULT($enabled)
AC_SUBST(U_ENABLE_TRACING)

# check if elf.h is present.
AC_CHECK_HEADERS([elf.h])
if test "x$ac_cv_header_elf_h" = "xyes"; then
    U_CPPFLAGS="$U_CPPFLAGS -DU_HAVE_ELF_H=1";
fi

U_ENABLE_DYLOAD=1
enable=yes
AC_MSG_CHECKING([whether to enable dynamic loading of plugins])
AC_ARG_ENABLE(dyload,
    [  --disable-dyload        disable dynamic loading [default=no]],
    [ case "${enableval}" in
	     yes|"")
		     U_ENABLE_DYLOAD=1
                     enable=yes
                     ;;
	     no)
		 U_ENABLE_DYLOAD=0;
                 enable=no;
                 U_CPPFLAGS="$U_CPPFLAGS -DU_ENABLE_DYLOAD=0";
                 ;;
	     *) ;;
    esac],
)
AC_MSG_RESULT($enable)
AC_SUBST(U_ENABLE_DYLOAD)

if test "$enable" = "yes"; then
   AC_CHECK_HEADERS([dlfcn.h])
   #AC_MSG_RESULT($enabled)
   AC_SEARCH_LIBS([dlopen], [dl])
   AC_CHECK_FUNCS([dlopen])

   if test "x$ac_cv_func_dlopen" != xyes; then
      U_CPPFLAGS="$U_CPPFLAGS -DHAVE_DLOPEN=0"
   fi
fi

# Check for miscellanous functions.
# So, use for putil / tools only.
# Note that this will generate HAVE_GETTIMEOFDAY, not U_HAVE_GETTIMEOFDAY
AC_CHECK_FUNCS([gettimeofday])

if [[ "$GXX" = yes ]]; then
    # if CXXFLAGS does not have a "-std=" setting, set it now to -std=c++11
    if ! echo "$CXXFLAGS" | grep '\-std=' >/dev/null 2>&1; then
        OLD_CXXFLAGS="${CXXFLAGS}"
        U_CXXFLAGS="$U_CXXFLAGS --std=c++11"
    fi
fi

U_HAVE_ATOMIC=1
U_CPPFLAGS="${U_CPPFLAGS} -DU_HAVE_ATOMIC=${U_HAVE_ATOMIC}"
AC_SUBST(U_HAVE_ATOMIC)

AC_MSG_CHECKING([for genccode assembly])

# Check to see if genccode can generate simple assembly.
GENCCODE_ASSEMBLY=
if test "$GCC" = yes; then
  GENCCODE_ASSEMBLY="-a gcc"
fi
AC_SUBST(GENCCODE_ASSEMBLY)

AC_MSG_RESULT($GENCCODE_ASSEMBLY)

# Checks for header files
AC_CHECK_HEADERS(inttypes.h)
if test $ac_cv_header_inttypes_h = no; then
 U_HAVE_INTTYPES_H=0
 U_CPPFLAGS="${U_CPPFLAGS} -DU_HAVE_INTTYPES_H=0"
else
 U_HAVE_INTTYPES_H=1
fi
AC_SUBST(U_HAVE_INTTYPES_H)

AC_CHECK_HEADERS(dirent.h)
if test $ac_cv_header_dirent_h = no; then
 U_HAVE_DIRENT_H=0
 U_CPPFLAGS="${U_CPPFLAGS} -DU_HAVE_DIRENT_H=0"
else
 U_HAVE_DIRENT_H=1
fi
AC_SUBST(U_HAVE_DIRENT_H)

# Check for endianness
AC_C_BIGENDIAN()
if test $ac_cv_c_bigendian = no; then
U_IS_BIG_ENDIAN=0
U_ENDIAN_CHAR="l"
else
U_IS_BIG_ENDIAN=1
U_ENDIAN_CHAR="b"
fi
AC_SUBST(U_IS_BIG_ENDIAN)

# Do various POSIX related checks
U_HAVE_NL_LANGINFO_CODESET=0
U_NL_LANGINFO_CODESET=-1
AC_CHECK_FUNC(nl_langinfo,[U_HAVE_NL_LANGINFO=1],[U_HAVE_NL_LANGINFO=0])
dnl AC_SUBST(U_HAVE_NL_LANGINFO)
if test $U_HAVE_NL_LANGINFO -eq 1; then
  AC_CACHE_CHECK([for nl_langinfo argument to obtain the codeset],
  ac_cv_nl_langinfo_codeset,
  [ac_cv_nl_langinfo_codeset="unknown"
  for a in CODESET _NL_CTYPE_CODESET_NAME; do
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <langinfo.h>]], [[nl_langinfo($a);]])],[ac_cv_nl_langinfo_codeset="$a"; break],[])]
  done)
  if test x$ac_cv_nl_langinfo_codeset != xunknown
  then
      U_HAVE_NL_LANGINFO_CODESET=1
      U_NL_LANGINFO_CODESET=$ac_cv_nl_langinfo_codeset
      if test "x${ac_cv_nl_langinfo_codeset}" != "xCODESET"
      then
        U_CPPFLAGS="${U_CPPFLAGS} -DNL_LANGINFO_CODESET=${ac_cv_nl_langinfo_codeset}"
      fi
  else
      U_CPPFLAGS="${U_CPPFLAGS} -DU_HAVE_NL_LANGINFO_CODESET=0"
  fi
fi
AC_SUBST(U_HAVE_NL_LANGINFO_CODESET)
AC_SUBST(U_NL_LANGINFO_CODESET)

U_OVERRIDE_CXX_ALLOCATION=1
U_HAVE_PLACEMENT_NEW=1
AC_SUBST(U_OVERRIDE_CXX_ALLOCATION)
AC_SUBST(U_HAVE_PLACEMENT_NEW)

AC_LANG(C)
AC_CHECK_FUNC(popen)
if test x$ac_cv_func_popen = xyes
then
     U_HAVE_POPEN=1
else
     U_CPPFLAGS="${U_CPPFLAGS} -DU_HAVE_POPEN=0"
     U_HAVE_POPEN=0
fi
AC_SUBST(U_HAVE_POPEN)

AC_CHECK_FUNC(tzset)
U_HAVE_TZSET=0
if test x$ac_cv_func_tzset = xyes
then
    U_TZSET=tzset
    U_HAVE_TZSET=1
else
    AC_CHECK_FUNC(_tzset)
    if test x$ac_cv_func__tzset = xyes
    then
        U_TZSET=_tzset
        U_HAVE_TZSET=1
    else
      U_CPPFLAGS="${U_CPPFLAGS} -DU_HAVE_TZSET=0"
    fi
fi
AC_SUBST(U_HAVE_TZSET)
AC_SUBST(U_TZSET)

U_HAVE_TZNAME=0
AC_CACHE_CHECK(for tzname,ac_cv_var_tzname,
[AC_LINK_IFELSE([AC_LANG_PROGRAM([[#ifndef __USE_POSIX
#define __USE_POSIX
#endif
#include <stdlib.h>
#include <time.h>
#ifndef tzname /* For SGI.  */
extern char *tzname[]; /* RS6000 and others reject char **tzname.  */
#endif]], [atoi(*tzname);])],[ac_cv_var_tzname=yes],[ac_cv_var_tzname=no])])
if test $ac_cv_var_tzname = yes; then
  U_TZNAME=tzname
  U_HAVE_TZNAME=1
else
  AC_CACHE_CHECK(for _tzname,ac_cv_var__tzname,
  [AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <stdlib.h>
  #include <time.h>
  extern char *_tzname[];]], [atoi(*_tzname);])],[ac_cv_var__tzname=yes],[ac_cv_var__tzname=no])])
    if test $ac_cv_var__tzname = yes; then
      U_TZNAME=_tzname
      U_HAVE_TZNAME=1
    else
     U_CPPFLAGS="${U_CPPFLAGS} -DU_HAVE_TZNAME=0"
    fi
fi
AC_SUBST(U_HAVE_TZNAME)
AC_SUBST(U_TZNAME)

AC_CACHE_CHECK(for timezone,ac_cv_var_timezone,
[AC_LINK_IFELSE([AC_LANG_PROGRAM([#ifndef __USE_POSIX
#define __USE_POSIX
#endif
#ifndef __USE_XOPEN
#define __USE_XOPEN
#endif
#include <time.h>
], [[timezone = 1;]])],[ac_cv_var_timezone=yes],[ac_cv_var_timezone=no])])
U_HAVE_TIMEZONE=0
if test $ac_cv_var_timezone = yes; then
  U_TIMEZONE=timezone
  U_HAVE_TIMEZONE=1
else
  AC_CACHE_CHECK(for __timezone,ac_cv_var___timezone,
  [AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <time.h>]], [[__timezone = 1;]])],[ac_cv_var___timezone=yes],[ac_cv_var___timezone=no])])
  if test $ac_cv_var___timezone = yes; then
    U_TIMEZONE=__timezone
    U_HAVE_TIMEZONE=1
  else
    AC_CACHE_CHECK(for _timezone,ac_cv_var__timezone,
    [AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <time.h>]], [[_timezone = 1;]])],[ac_cv_var__timezone=yes],[ac_cv_var__timezone=no])])
    if test $ac_cv_var__timezone = yes; then
      U_TIMEZONE=_timezone
      U_HAVE_TIMEZONE=1
    else
     U_CPPFLAGS="${U_CPPFLAGS} -DU_HAVE_TIMEZONE=0"
    fi
  fi
fi
AC_SUBST(U_HAVE_TIMEZONE)
AC_SUBST(U_TIMEZONE)

# Enable/disable extras
AC_ARG_ENABLE(extras,
	[  --enable-extras         build ICU extras [default=yes]],
	[case "${enableval}" in
		yes) extras=true ;;
		no)  extras=false ;;
		*) AC_MSG_ERROR(bad value ${enableval} for --enable-extras) ;;
		esac],
	extras=true)
AM_CONDITIONAL(EXTRAS, test "$extras" = true)
AC_ARG_ENABLE(icuio,
	[  --enable-icuio          build icuio library [default=yes]],
	[case "${enableval}" in
		yes) icuio=true ;;
		no)  icuio=false ;;
		*) AC_MSG_ERROR(bad value ${enableval} for --enable-icuio) ;;
		esac],
	icuio=true)
AM_CONDITIONAL(IO, test "$icuio" = true)

# Enable/disable tools
AC_ARG_ENABLE(tools,
	[  --enable-tools         build tools [default=yes]],
	[case "${enableval}" in
		yes) tools=true ;;
		no)  tools=false ;;
		*) AC_MSG_ERROR(bad value ${enableval} for --enable-tools) ;;
		esac],
	tools=true)
AM_CONDITIONAL(TOOLS, test "$tools" = true)

# Enable/disable building of data
AC_ARG_ENABLE(data,
	[  --enable-data         build data [default=yes]],
	[case "${enableval}" in
		yes) data=true ;;
		no)  data=false ;;
		*) AC_MSG_ERROR(bad value ${enableval} for --enable-data) ;;
		esac],
	data=true)
if test "$data" = true -a "$tools" != true; then
    AC_MSG_ERROR(The tools are needed to build the data)
fi
AM_CONDITIONAL(DATA, test "$data" = true)

# Enable/disable tests
AC_ARG_ENABLE(tests,
	[  --enable-tests          build ICU tests [default=yes]],
	[case "${enableval}" in
		yes) tests=true ;;
		no)  tests=false ;;
		*) AC_MSG_ERROR(bad value ${enableval} for --enable-tests) ;;
		esac],
	tests=true)
AM_CONDITIONAL(TESTS, test "$tests" = true)

# Enable/disable samples
AC_ARG_ENABLE(samples,
	[  --enable-samples        build ICU samples [default=yes]

Additionally, the variable FORCE_LIBS may be set before calling configure.
If set, it will REPLACE any automatic list of libraries.],
	[case "${enableval}" in
		yes) samples=true ;;
		no)  samples=false ;;
		*) AC_MSG_ERROR(bad value ${enableval} for --enable-samples) ;;
		esac],
	samples=true)
AM_CONDITIONAL(SAMPLES, test "$samples" = true)

ICUDATA_CHAR=$U_ENDIAN_CHAR

# Platform-specific Makefile setup
# set ICUDATA_CHAR to 'e' for any EBCDIC (which should be big endian) platform.
case "${host}" in
	*-*-solaris*) 	platform=U_SOLARIS ;;
	*-*-linux*|*-*-gnu|*-*-k*bsd*-gnu|*-*-kopensolaris*-gnu)        platform=U_LINUX ;;
	*-*-*bsd*|*-*-dragonfly*) 	platform=U_BSD ;;
	*-*-aix*) 	platform=U_AIX ;;
	*-*-hpux*) 	platform=U_HPUX ;;
	*-apple-darwin*|*-apple-rhapsody*)	platform=U_DARWIN ;;
	*-*-cygwin*)	platform=U_CYGWIN ;;
	*-*-mingw*)	platform=U_MINGW ;;
	*-*ibm-openedition*|*-*-os390*)	platform=OS390
			if test "${ICU_ENABLE_ASCII_STRINGS}" != "1"; then
				ICUDATA_CHAR="e"
			fi ;;
	*-*-os400*)	platform=OS400
			if test "${ICU_ENABLE_ASCII_STRINGS}" != "1"; then
				ICUDATA_CHAR="e"
			fi ;;
	*-*-nto*)	platform=U_QNX ;;
	*-dec-osf*) 	platform=U_OSF ;;
	*-*-beos)		platform=U_BEOS ;;
	*-*-irix*)		platform=U_IRIX ;;
	*-ncr-*)		platform=U_MPRAS ;;
	*) 		platform=U_UNKNOWN_PLATFORM ;;
esac
AC_SUBST(ICUDATA_CHAR)
AC_SUBST(platform)

if test "${FORCE_LIBS}" != ""; then
   echo " *** Overriding automatically chosen [LIBS=$LIBS], using instead [FORCE_LIBS=${FORCE_LIBS}]"  1>&6
   LIBS=${FORCE_LIBS}
fi

AC_SUBST(U_CFLAGS)
AC_SUBST(U_CXXFLAGS)
dnl append all config cppflags
U_CPPFLAGS="$U_CPPFLAGS $UCONFIG_CPPFLAGS"
AC_SUBST(U_CPPFLAGS)

# output the Makefiles
AC_CONFIG_FILES([Makefile \
		icu-i18n.pc \
		icu-uc.pc \
		icu-io.pc \
		common/Makefile \
		common/unicode/uvernum.h \
		stubdata/Makefile \
		i18n/Makefile \
		io/Makefile \
		extra/Makefile \
		extra/uconv/Makefile \
		extra/scrptrun/Makefile \
		tools/Makefile \
		tools/ctestfw/Makefile \
		tools/toolutil/Makefile \
		tools/makeconv/Makefile \
		tools/genrb/Makefile \
		tools/genccode/Makefile \
		tools/gencmn/Makefile \
		tools/gencnval/Makefile \
		tools/gendict/Makefile \
		tools/gentest/Makefile \
		tools/gennorm2/Makefile \
		tools/genbrk/Makefile \
		tools/gensprep/Makefile \
		tools/icuinfo/Makefile \
		tools/icupkg/Makefile \
		tools/icuswap/Makefile \
		tools/pkgdata/Makefile \
		tools/tzcode/Makefile \
		tools/gencfu/Makefile \
		data/Makefile
])
AC_OUTPUT

echo
echo "ICU for C/C++ $VERSION is ready to be built."
echo "=== Important Notes: ==="

AC_SUBST(UCONFIG_CPPFLAGS)

if test "$tools" = false; then
        echo "## Note: you have disabled ICU's tools. This ICU cannot build its own data or tests."
fi
