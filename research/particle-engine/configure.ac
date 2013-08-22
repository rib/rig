# autoconf
AC_PREREQ(2.59)
AC_INIT([pe], [0.0.1], [christopher.e.cummins@intel.com])
AC_CONFIG_MACRO_DIR([build])
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_SRCDIR([pe/particle-engine.c])
AC_CONFIG_HEADERS([config.h])

# automake
AM_INIT_AUTOMAKE([1.11 -Wall foreign no-define])
AM_SILENT_RULES([yes])

AC_PROG_CC
AC_HEADER_STDC

AM_PROG_AR
AM_PROG_CC_C_O

LT_INIT

AC_DEFINE(COGL_ENABLE_EXPERIMENTAL_API, [], [Use the experimental Cogl API])

PKG_CHECK_MODULES([COGL], [cogl2 >= 1.99.0])
PKG_CHECK_MODULES([GLIB], [glib-2.0])

AC_OUTPUT([
	Makefile
	examples/Makefile
	pe/Makefile
])
