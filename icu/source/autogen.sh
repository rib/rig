#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`

cd $srcdir

if ! test -f common/unicode/utypes.h; then
	echo "You must run this script in the top-level directory"
	exit 1
fi

if test -z "$NOCONFIGURE"; then
        if test -z "$*"; then
                echo "I am going to run ./configure with no arguments - if you wish "
                echo "to pass any to it, please specify them on the $0 command line."
        fi
fi

autoreconf -vfi || exit $?
cd $ORIGDIR || exit $?

if test -z "$NOCONFIGURE"; then
        $srcdir/configure $AUTOGEN_CONFIGURE_ARGS "$@" || exit $?
        echo "Now type 'make' to compile $PROJECT."
fi
