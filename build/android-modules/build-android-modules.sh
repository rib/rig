#!/bin/bash

# Please read the README before running this script!

set -e
#set -x

UNSET_VARS=(
    LD_LIBRARY_PATH
    PKG_CONFIG_PATH
    PKG_CONFIG
    GI_TYPELIB_PATH
    XDG_DATA_DIRS
    CMAKE_PREFIX_PATH
    GIO_EXTRA_MODULES
    GST_PLUGIN_PATH
)

EXPORT_VARS=(
    LD_LIBRARY_PATH
    PATH
    PKG_CONFIG_PATH
    PKG_CONFIG
    GI_TYPELIB_PATH
    XDG_DATA_DIRS
    CMAKE_PREFIX_PATH
    GIO_EXTRA_MODULES
    GST_PLUGIN_PATH
    PYTHONPATH
    CFLAGS
    LDFLAGS
    ACLOCAL_FLAGS
)

GL_HEADER_URLS=( \
    http://www.opengl.org/registry/api/glext.h );

GL_HEADERS=( glext.h );

function add_to_path()
{
    local var="$1"; shift;
    local val="$1"; shift;
    local sep;

    if ! echo "${!var}" | grep -q -E "(^|:)$val(:|$)"; then
        if test -z "${!var}"; then
            sep="";
        else
            sep=":";
        fi;
        if test -z "$1"; then
            eval "$var"="${!var}$sep$val";
        else
            eval "$var"="\"$val$sep${!var}\"";
        fi;
    fi;
}

function add_flag()
{
    local var="$1"; shift;
    local letter="$1"; shift;
    local dir="$1"; shift;

    eval "$var"="\"${!var} -$letter$dir\"";
}

function add_prefix()
{
    local prefix;
    local aclocal_dir;
    local x;

    prefix="$1"; shift;

    add_to_path LD_LIBRARY_PATH "$prefix/lib"
    add_to_path PATH "$prefix/bin" backwards
    add_to_path PKG_CONFIG_PATH "$prefix/lib/pkgconfig"
    add_to_path PKG_CONFIG_PATH "$prefix/share/pkgconfig"
    add_to_path GI_TYPELIB_PATH "$prefix/lib/girepository-1.0"
    if test -z "$XDG_DATA_DIRS"; then
        # glib uses these by default if the environment variable is
        # not set and we don't really want to override the default -
        # just add to it so we explicitly add it back in here
        XDG_DATA_DIRS="/usr/local/share/:/usr/share/";
    fi;
    add_to_path XDG_DATA_DIRS "$prefix/share" backwards
    add_to_path CMAKE_PREFIX_PATH "$prefix"
    add_to_path GIO_EXTRA_MODULES "$prefix/lib/gio/modules"
    add_to_path GST_PLUGIN_PATH "$prefix/lib/gstreamer-0.10"
    add_to_path PYTHONPATH "$prefix/lib64/python2.7/site-packages"
    add_flag CFLAGS I "$prefix/include"
    add_flag CXXFLAGS I "$prefix/include"
    add_flag LDFLAGS L "$prefix/lib"
    if test -d $prefix/share/aclocal; then
        add_flag ACLOCAL_FLAGS I "$prefix/share/aclocal"
    fi
}

function download_file ()
{
    local url="$1"; shift;
    local filename="$1"; shift;

    if test -f "$DOWNLOAD_DIR/$filename"; then
        echo "Skipping download of $filename because the file already exists";
        return 0;
    fi;

    case "$DOWNLOAD_PROG" in
	curl)
	    curl -L -o "$DOWNLOAD_DIR/$filename" "$url";
	    ;;
	*)
	    $DOWNLOAD_PROG -O "$DOWNLOAD_DIR/$filename" "$url";
	    ;;
    esac;

    if [ $? -ne 0 ]; then
	echo "Downloading ${url} failed.";
	exit 1;
    fi;
}

function guess_dir ()
{
    local var="$1"; shift;
    local suffix="$1"; shift;
    local msg="$1"; shift;
    local prompt="$1"; shift;
    local dir="${!var}";

    if [ -z "$dir" ]; then
	echo "Please enter ${msg}.";
	dir="$PWD/$suffix";
	read -r -p "$prompt [$dir] ";
	if [ -n "$REPLY" ]; then
	    dir="$REPLY";
	fi;
    else
	echo "$prompt = [$dir]"
    fi;

    eval $var="\"$dir\"";
    guess_saves="$guess_saves\n$var=\"$dir\""

    if [ ! -d "$dir" ]; then
	if ! mkdir -p "$dir"; then
	    echo "Error making directory $dir";
	    exit 1;
	fi;
    fi;
}

function y_or_n ()
{
    local prompt="$1"; shift;

    while true; do
	read -p "${prompt} [y/n] " -n 1;
	echo;
	case "$REPLY" in
	    y) return 0 ;;
	    n) return 1 ;;
	    *) echo "Please press y or n" ;;
	esac;
    done;
}

function do_untar_source ()
{
    local tarfile="$1"; shift;

    if echo "$tarfile" | grep -q '\.xz$'; then
        unxz -d -c "$tarfile" | tar -C "$BUILD_DIR" -xvf - "$@"
    elif echo "$tarfile" | grep -q '\.bz2$'; then
        bzip2 -d -c "$tarfile" | tar -C "$BUILD_DIR" -xvf - "$@"
    else
        tar -C "$BUILD_DIR" -zxvf "$tarfile" "$@"
    fi

    if [ "$?" -ne 0 ]; then
	echo "Failed to extract $tarfile"
	exit 1
    fi;
}

function apply_patches ()
{
    local patches_dir="$1"; shift
    local project_dir="$1"; shift

    cd "$project_dir"

    if test -d "$patches_dir"; then
        for patch in "$patches_dir/"*.patch; do
            patch -p1 < "$patch"
            if grep -q '^+++ .*/\(Makefile\.am\|configure\.ac\)\b' "$patch" ; then
                retool=yes;
            fi
        done
    fi
}

function git_clone ()
{
    local name
    local url
    local branch
    local build_dir="$BUILD_DIR"

    while true; do
        case "$1" in
	    -url) shift; url="$1"; shift ;;
            -name) shift; name="$1"; shift ;;
	    -branch) shift; branch="$1"; shift ;;
            -build_dir) shift; build_dir="$1"; shift ;;
            -*) echo "Unknown option $1"; exit 1 ;;
            *) break ;;
        esac
    done

    cd $build_dir

    if ! test -d $name; then
        git clone $url $name
        if [ $? -ne 0 ]; then
            echo "Cloning ${url} failed."
            exit 1
        fi

        cd $name
        git checkout -b rig-build $branch
        if [ $? -ne 0 ]; then
            echo "Checking out $branch failed"
            exit 1
        fi
    fi
}

function build_source ()
{
    local unpack_dir
    local module
    local project_name
    local build_dir
    local prefix
    local branch
    local config_name="configure"
    local top_srcdir="."
    local is_autotools=1
    local is_cmake
    local cmake_args=""
    local jobs=4
    local config_guess_dirs="."
    local deps
    local make_args=""
    local install_target=install
    local retool=no
    local retool_cmd
    local is_tool
    local onlybuild

    while true; do
        case "$1" in
	    -module) shift; prefix="$MODULES_DIR/$1"; module="$1"; shift ;;
	    -dep) shift; deps="$deps $1"; shift ;;
	    -prefix) shift; prefix="$1"; shift ;;
	    -unpackdir) shift; unpack_dir="$1"; shift ;;
	    -branch) shift; branch="$1"; shift ;;
	    -configure) shift; config_name="$1"; shift ;;
	    -top_srcdir) shift; top_srcdir="$1"; shift ;;
            -not_autotools) shift; unset is_autotools ;;
            -cmake) shift; unset is_autotools; is_cmake=1 ;;
            -cmake_arg) shift; cmake_args="$cmake_args $1"; shift ;;
	    -make_arg) shift; make_args="$make_args $1"; shift ;;
            -install_target) shift; install_target="$1"; shift ;;
	    -retool) shift; retool=yes ;;
	    -retool_cmd) shift; retool_cmd=$1; shift ;;
	    -j) shift; jobs="$1"; shift ;;
	    -autotools_dir) shift; config_guess_dirs="$config_guess_dirs $1"; shift ;;
	    -onlybuild) shift; onlybuild=1 ;;
	    -tool) shift; is_tool=1 ;;
            -*) echo "Unknown option $1"; exit 1 ;;
            *) break ;;
        esac
    done

    local source="$1"; shift;
    local tarfile=`basename "$source"`

    if test -z "$unpack_dir"; then
        unpack_dir=`echo "$tarfile" | sed -e 's/\.tar\.[0-9a-z]*$//' -e 's/\.tgz$//' -e 's/\.git$//'`
    fi;

    if test -n "$module"; then
      project_name="$module"
      build_dir="$unpack_dir"
    elif test -z "$is_tool"; then
      project_name=`echo "$unpack_dir" | sed 's/-[0-9\.]*$//'`
      build_dir="$unpack_dir"
    else
      project_name=`echo "$unpack_dir" | sed 's/-[0-9\.]*$//'`
      build_dir="tool-$unpack_dir"
    fi

    #add_to_path LD_LIBRARY_PATH "$prefix/lib"
    add_to_path PKG_CONFIG_PATH "$prefix/lib/pkgconfig"
    add_to_path PKG_CONFIG_PATH "$prefix/share/pkgconfig"

    if test -n "$module"; then
        mkdir -p $MODULES_DIR/$module
        cp $RIG_BUILD_META_DIR/makefiles/$module-Android.mk $MODULES_DIR/$module/Android.mk
    fi

    if test -e "$BUILD_DIR/installed-projects.txt" && \
        grep -q "^$build_dir$" "$BUILD_DIR/installed-projects.txt"; then
        echo "Skipping $project_name ($build_dir) as it is already installed"
        return
    fi

    if test -z "$onlybuild" -a ! -d $BUILD_DIR/$build_dir; then
        if echo "$source" | grep -q "git$"; then
            git_clone -url "$source" -branch "$branch" -name "$build_dir"
            retool=yes
        else
            download_file "$source" "$tarfile"

            do_untar_source "$DOWNLOAD_DIR/$tarfile"
	    if test "$BUILD_DIR/$unpack_dir" != "$BUILD_DIR/$build_dir"; then
		mv "$BUILD_DIR/$unpack_dir" "$BUILD_DIR/$build_dir"
	    fi
        fi
        apply_patches "$PATCHES_DIR/$project_name" "$BUILD_DIR/$build_dir"
    fi

    cd "$BUILD_DIR/$build_dir/$top_srcdir"

    for i in $config_guess_dirs
    do
	cp $DOWNLOAD_DIR/config.{sub,guess} $i/
    done

    save_CPPFLAGS=$CPPFLAGS
    save_CFLAGS=$CFLAGS
    save_LDFLAGS=$LDFLAGS

    for dep in $deps
    do
	export CPPFLAGS="-I$MODULES_DIR/$dep/include $CPPFLAGS"
	export CFLAGS="-I$MODULES_DIR/$dep/include $CFLAGS"
	export LDFLAGS="-L$MODULES_DIR/$dep/lib $LDFLAGS"
	if test -d $MODULES_DIR/$dep/share/aclocal; then
	    export ACLOCAL_FLAGS="-I $MODULES_DIR/$dep/share/aclocal $ACLOCAL_FLAGS"
	fi
    done
    export CFLAGS="$CFLAGS $ANDROID_CFLAGS"
    export LDFLAGS="$LDFLAGS $ANDROID_LDFLAGS"

    if ! test -e ./"$config_name"; then
        retool=yes
    fi

    if test "x$retool" = "xyes" -a -z "$is_cmake"; then
        if test -z "$retool_cmd"; then
            if test -f ./autogen.sh; then
                retool_cmd="./autogen.sh"
            else
                retool_cmd="autoreconf -fvi"
            fi
        fi
        export NOCONFIGURE=1
        eval "$retool_cmd"
        unset NOCONFIGURE
    fi

    #Note we have to pass $@ first since we need to pass a special
    #Linux option as the first argument to the icu configure script
    if test -n "$is_autotools"; then
        ./"$config_name" "$@" \
            --prefix="$prefix" \
            CFLAGS="$CFLAGS" \
            CXXFLAGS="$CXXFLAGS" \
            LDFLAGS="$LDFLAGS"
    elif test -n "$is_cmake"; then
        if test -d cross-android; then
            rm -fr cross-android
        fi
        mkdir cross-android && cd cross-android
        cmake $cmake_args \
              -D CMAKE_INSTALL_PREFIX="$prefix" ..
    else
        ./"$config_name" "$@" --prefix="$prefix"
    fi
    make $make_args -j"$jobs"
    make $make_args $install_target

    CPPFLAGS=$save_CPPFLAGS
    CFLAGS=$save_CFLAGS
    LDFLAGS=$save_LDFLAGS

    find $prefix -iname '*.la' -exec rm {} \;

    echo "$build_dir" >> "$BUILD_DIR/installed-projects.txt"
}

function build_bzip2 ()
{
    local prefix="$MODULES_DIR/bzip2"
    local source="$1"; shift;
    local tarfile=`basename "$source"`
    local unpack_dir=`echo "$tarfile" | sed -e 's/\.tar\.[0-9a-z]*$//' -e 's/\.tgz$//' -e 's/\.git$//'`

    if test -e "$BUILD_DIR/installed-projects.txt" && \
               grep -q "^$unpack_dir$" "$BUILD_DIR/installed-projects.txt"; then
        echo "Skipping bzip2 as it is already installed"
        return
    fi

    download_file "$source" "$tarfile"

    do_untar_source "$DOWNLOAD_DIR/$tarfile"

    apply_patches "$PATCHES_DIR/bzip2" "$BUILD_DIR/$unpack_dir"

    cd "$BUILD_DIR/$unpack_dir"

    make -f Makefile-libbz2_so
    ln -sf libbz2.so.1.0.6 libbz2.so
    mkdir -p "$prefix/lib/"
    cp -av libbz2.so* "$prefix/lib/"

    echo "$unpack_dir" >> "$BUILD_DIR/installed-projects.txt"
}

function build_dep ()
{
    build_source "$@" --host=$ANDROID_HOST_TRIPPLE
}

function build_tool ()
{
    build_source -tool -prefix "$TOOLS_PREFIX" "$@"
}

function usage ()
{
    echo "Usage:"
    echo "$0 <rig-src-dir> <build-bashrc>"
    exit 1
}

if test $# -ne 2; then
    usage
fi

RIG_SRC_DIR=`realpath $1`
RIG_BUILD_META_DIR=$RIG_SRC_DIR/build/android-modules

if ! test -f $RIG_BUILD_META_DIR/makefiles/icu-Android.mk; then
    echo "Could not find Rig source at $1"
    usage
fi

if ! test -f $2; then
    echo "Couldn't find bashrc file $2"
    usage
fi

source $2

unset "${UNSET_VARS[@]}"

guess_dir PKG_DIR "pkg" \
    "the directory to store the resulting package" "Package dir"
PKG_RELEASEDIR="$PKG_DIR/release"
PKG_LIBDIR="$PKG_RELEASEDIR/lib"
PKG_DATADIR="$PKG_RELEASEDIR/share"

cd `dirname "$0"`
BUILD_DATA_DIR=$PWD

RIG_GITDIR=`cd $BUILD_DATA_DIR/../../.git && pwd`

PATCHES_DIR=$BUILD_DATA_DIR/android-patches
PATCHES_DIR=`cd $PATCHES_DIR && pwd`

# If a download directory hasn't been specified then try to guess one
# but ask for confirmation first
guess_dir DOWNLOAD_DIR "downloads" \
    "the directory to download to" "Download directory";

DOWNLOAD_PROG=curl

guess_dir MODULES_DIR "modules" \
    "the directory to create android ndk modules in" "Modules directory";

guess_dir TOOLS_PREFIX "android-tools" \
    "the install prefix for internal build dependencies" "Tools dir";

# If a build directory hasn't been specified then try to guess one
# but ask for confirmation first
guess_dir BUILD_DIR "android-build" \
    "the directory to build source dependencies in" "Build directory";

for dep in "${GL_HEADER_URLS[@]}"; do
    bn="${dep##*/}";
    download_file "$dep" "$bn";
done;

echo "Copying GL headers...";
if ! test -d "$TOOLS_PREFIX/include/OpenGL"; then
    mkdir -p "$TOOLS_PREFIX/include/OpenGL"
fi;
for header in "${GL_HEADERS[@]}"; do
    cp "$DOWNLOAD_DIR/$header" "$TOOLS_PREFIX/include/OpenGL/"
done;

RUN_PKG_CONFIG="$BUILD_DIR/run-pkg-config.sh";

echo "Generating $BUILD_DIR/run-pkg-config.sh";

cat > "$RUN_PKG_CONFIG" <<EOF
# This is a wrapper script for pkg-config that overrides the
# PKG_CONFIG_LIBDIR variable so that it won't pick up the local system
# .pc files.

export PKG_CONFIG_LIBDIR="/invalid/foo/path"

exec pkg-config "\$@"
EOF

CMAKE_TOOLCHAIN_SCRIPT="$BUILD_DIR/cmake-toolchain-script"

echo "Generating $CMAKE_TOOLCHAIN_SCRIPT"

cat > "$CMAKE_TOOLCHAIN_SCRIPT" <<EOF
SET(CMAKE_SYSTEM_NAME Linux)  # Tell CMake we're cross-compiling
include(CMakeForceCompiler)
# Prefix detection only works with compiler id "GNU"
# CMake will look for prefixed g++, cpp, ld, etc. automatically
CMAKE_FORCE_C_COMPILER($ANDROID_HOST_TRIPPLE-gcc GNU)
SET(ANDROID TRUE)
EOF

chmod a+x "$RUN_PKG_CONFIG";

add_prefix "$TOOLS_PREFIX"

PKG_CONFIG="$RUN_PKG_CONFIG"

export "${EXPORT_VARS[@]}"

download_file "http://git.savannah.gnu.org/gitweb/?p=automake.git;a=blob_plain;f=lib/config.guess" "config.guess";
download_file "http://git.savannah.gnu.org/gitweb/?p=automake.git;a=blob_plain;f=lib/config.sub" "config.sub";

build_tool "http://ftp.gnu.org/gnu/m4/m4-1.4.17.tar.gz"
build_tool "http://ftp.gnu.org/gnu/autoconf/autoconf-2.69.tar.gz"
build_tool "http://ftp.gnu.org/gnu/automake/automake-1.14.1.tar.gz"
build_tool "http://ftp.gnu.org/gnu/help2man/help2man-1.45.1.tar.xz"
#Libtool's Android support is only available from git :-/
build_tool -retool_cmd "./bootstrap" \
    "git://git.savannah.gnu.org/libtool.git"
build_tool "http://pkgconfig.freedesktop.org/releases/pkg-config-0.27.1.tar.gz" \
    --with-internal-glib

export gl_cv_header_working_stdint_h=yes

#XXX: Note re-autotooling libiconv is awkward so instead of getting
#     it to use a newer version of libtool with Android support, we
#     patch it to use -avoid-version for a pristine soname that
#     Android can cope with.
build_dep -module libiconv -autotools_dir ./build-aux -autotools_dir ./libcharset/build-aux \
    "http://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.14.tar.gz"

build_dep -module libffi -retool_cmd "autoreconf -fvi" -retool \
    "ftp://sourceware.org/pub/libffi/libffi-3.0.13.tar.gz"

#XXX: Note re-autotooling gettext is awkward so instead of getting
#     it to use a newer version of libtool with Android support, we
#     patch it to use -avoid-version for a pristine soname that
#     Android can cope with.
build_dep -module gettext -dep libiconv -autotools_dir ./build-aux \
    -make_arg -C -make_arg gettext-tools/intl \
    "http://ftp.gnu.org/gnu/gettext/gettext-0.18.3.2.tar.gz" \
    --without-included-regex --disable-java --disable-openmp --without-libiconv-prefix --without-libintl-prefix --without-libglib-2.0-prefix --without-libcroco-0.6-prefix --with-included-libxml --without-libncurses-prefix --without-libtermcap-prefix --without-libcurses-prefix --without-libexpat-prefix --without-emacs

#make -C gettext-tools/intl install

build_dep -module libpng -retool -retool_cmd "autoreconf -v" \
    "http://downloads.sourceforge.net/project/libpng/libpng16/1.6.15/libpng-1.6.15.tar.gz"
export LIBPNG_CFLAGS="-I$MODULES_DIR/libpng/include"
export LIBPNG_LDFLAGS="-L$MODULES_DIR/libpng/lib -lpng16"

build_dep -module tiff -autotools_dir ./config -retool \
    "http://download.osgeo.org/libtiff/tiff-4.0.3.tar.gz"
build_dep -module libjpeg -retool \
    -unpackdir jpeg-9a "http://www.ijg.org/files/jpegsrc.v9a.tar.gz"

build_dep -module freetype -retool \
    "http://download.savannah.gnu.org/releases/freetype/freetype-2.5.2.tar.bz2"

build_dep -module libxml2 -retool \
    "ftp://xmlsoft.org/libxml2/libxml2-2.9.0.tar.gz" \
    --without-python --without-debug --without-legacy --without-catalog \
    --without-docbook --with-c14n --with-gnu-ld --disable-tests

build_dep -module fontconfig -retool \
    "http://www.freedesktop.org/software/fontconfig/release/fontconfig-2.10.95.tar.bz2" \
    --with-default-fonts=/system/fonts \
    --disable-docs \
    --enable-libxml2

build_tool -branch origin/rig \
    -top_srcdir "source" \
    -retool \
    "https://github.com/rig-project/icu.git" \
    --disable-samples \
    --disable-tests \
    --disable-extras

build_dep -module icu -branch origin/rig \
    -top_srcdir "source" \
    -retool \
    "https://github.com/rig-project/icu.git" \
    --disable-tools \
    --disable-data \
    --disable-samples \
    --disable-tests \
    --disable-extras

#We don't want modules picking up the native build of ICU
rm -f $TOOLS_PREFIX/lib/pkg-config/icu*

build_dep -module harfbuzz -dep gettext -dep icu -dep freetype -retool \
    "http://www.freedesktop.org/software/harfbuzz/release/harfbuzz-0.9.26.tar.bz2" \
    --with-icu=yes

build_dep -module libuv \
    "https://github.com/libuv/libuv.git"

#save_ANDROID_NDK=$ANDROID_NDK
#unset ANDROID_NDK
#export ANDROID_STANDALONE_TOOLCHAIN=$ANDROID_NDK_TOOLCHAIN
#build_dep -module opencv -cmake \
#    -cmake_arg -D -cmake_arg CMAKE_TOOLCHAIN_FILE=$BUILD_DIR/opencv/platforms/android/android.toolchain.cmake \
#    "https://github.com/Itseez/opencv.git"
#ANDROID_NDK=$save_ANDROID_NDK

build_tool "http://protobuf.googlecode.com/files/protobuf-2.5.0.tar.gz"

# We have to build protobuf-c twice; once to build the protoc-c tool
# and then again to cross-compile libprotobuf-c.so
build_tool -branch origin/rig \
  "https://github.com/rig-project/protobuf-c.git" \
  --enable-protoc-c

#export CXXFLAGS="-I$STAGING_PREFIX/include"
build_dep -module protobuf-c -branch origin/rig \
  "https://github.com/rig-project/protobuf-c.git" \
  --disable-protoc-c
#unset CXXFLAGS

#prev_CPPFLAGS="$CPPFLAGS"
#export CPPFLAGS="$prev_CPPFLAGS -DANDROID_HARDWARE_generic"
#build_dep -module valgrind -retool_cmd "autoreconf -fvi" \
#    "https://github.com/svn2github/valgrind.git" \
#    AR=$ANDROID_HOST_TRIPPLE-ar \
#    LD=$ANDROID_HOST_TRIPPLE-ld \
#    CC=$ANDROID_HOST_TRIPPLE-gcc \
#    --host=armv7-unknown-linux
#CPPFLAGS="$prev_CPPFLAGS"

versioned_sonames=`$ANDROID_HOST_TRIPPLE-readelf -d $MODULES_DIR/*/lib/*.so|grep 'soname:'|grep -v '.so]$'|awk '{print $5}'`
if test -n "$versioned_sonames"; then
    echo "WARNING: The following versioned module sonames were found, but Android doesn't support library versioning:"
    echo ""
    echo "$versioned_sonames"
    exit 1
fi

exit 0

if test -z $RIG_COMMIT; then
    build_dep -d rig "$RIG_GITDIR"
else
    build_dep -d rig -c "$RIG_COMMIT" "$RIG_GITDIR"
fi

mkdir -p "$PKG_RELEASEDIR"

cp -v "$STAGING_PREFIX/bin/rig" "$PKG_RELEASEDIR/rig-bin"

mkdir -p "$PKG_LIBDIR"

for lib in \
    libasprintf \
    libbz2 \
    libdaemon \
    libdbus-1 \
    libffi \
    libfontconfig \
    libfreetype \
    libgettextlib-0.18.2 \
    libgettextlib \
    libgettextpo \
    libgettextsrc-0.18.2 \
    libgettextsrc \
    libharfbuzz \
    libjpeg \
    liblzma \
    libpng \
    libpng16 \
    libprotobuf-c \
    libtiff \
    libtiffxx \
    libxml2 \
    preloadable_libintl
do
    cp -av $STAGING_PREFIX/lib/$lib*.so* "$PKG_LIBDIR"
done

cp -av "$BUILD_DATA_DIR/scripts/"{auto-update.sh,rig-wrapper.sh} \
    "$BUILD_DIR/rig/tools/rig-check-signature" \
    "$PKG_RELEASEDIR"

mkdir -p "$PKG_DATADIR/pixmaps"
cp -av $STAGING_PREFIX/share/pixmaps/rig* $PKG_DATADIR/pixmaps

mkdir -p "$PKG_DATADIR/applications"
cp -av $STAGING_PREFIX/share/applications/rig* $PKG_DATADIR/applications

mkdir -p "$PKG_DATADIR/mime/application"
cp -av $STAGING_PREFIX/share/mime/application/rig-* $PKG_DATADIR/mime/application

cd $PKG_DIR
ln -sf ./release/rig-wrapper.sh $PKG_DIR/rig

mkdir -p $PKG_DIR/cache

mkdir -p "$PKG_DATADIR/rig"

cp -av "$STAGING_PREFIX/share/rig"/* "$PKG_DATADIR/rig"
