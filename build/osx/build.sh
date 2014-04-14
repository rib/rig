#!/bin/bash

set -e

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
    add_flag LDFLAGS L "$prefix/lib"
    add_flag ACLOCAL_FLAGS I "$prefix/share/aclocal"
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
    fi;

    eval $var="\"$dir\"";

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
    do_untar_source_d "$BUILD_DIR" "$@";
}

function do_untar_source_d ()
{
    local exdir="$1"; shift;
    local tarfile="$1"; shift;

    if echo "$tarfile" | grep -q '\.xz$'; then
        unxz -d -c "$tarfile" | tar -C "$exdir" -xvf - "$@"
    elif echo "$tarfile" | grep -q '\.bz2$'; then
        bzip2 -d -c "$tarfile" | tar -C "$exdir" -xvf - "$@"
    else
        tar -C "$exdir" -zxvf "$tarfile" "$@"
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
	    echo "Applying patch $patch"
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
    local commit
    local build_dir="$BUILD_DIR"

    while true; do
        case "$1" in
            -url) shift; url="$1"; shift ;;
            -name) shift; name="$1"; shift ;;
            -branch) shift; branch="$1"; shift ;;
            -commit) shift; commit="$1"; shift ;;
            -build_dir) shift; build_dir="$1"; shift ;;
            -*) echo "Unknown option $1"; exit 1 ;;
            *) break ;;
        esac
    done

    cd $build_dir

    if test -d $name; then
	cd $name
	if ! `git log|grep -q "commit $commit"`; then
	    echo "Found existing $name clone not based on $commit"
	    exit 1
	fi
    else
	git clone $url $name
	if [ $? -ne 0 ]; then
	    echo "Cloning ${url} failed."
	    exit 1
	fi

	if test -n "$commit"; then
	    start_point="$commit"
	else
	    start_point="$branch"
	fi

	cd $name
	git checkout -b rig-build $start_point
	if [ $? -ne 0 ]; then
	    echo "Checking out $commit failed"
	    exit 1
	fi

        apply_patches "$PATCHES_DIR/$name" "$build_dir/$name"
    fi
}

function build_source ()
{
    local project_dir
    local prefix
    local branch
    local commit
    local config_name="configure"
    local jobs=4
    local onlybuild

    while true; do
        case "$1" in
            -p) shift; prefix="$1"; shift ;;
            -d) shift; project_dir="$1"; shift ;;
            -branch) shift; branch="$1"; shift ;;
            -commit) shift; commit="$1"; shift ;;
            -configure) shift; config_name="$1"; shift ;;
	    -j) shift; jobs="$1"; shift ;;
	    -onlybuild) shift; onlybuild=1 ;;
            -*) echo "Unknown option $1"; exit 1 ;;
            *) break ;;
        esac
    done

    local source="$1"; shift;
    local tarfile=`basename "$source"`

    if test -z "$project_dir"; then
        project_dir=`echo "$tarfile" | sed -e 's/\.tar\.[0-9a-z]*$//' -e 's/\.tgz$//' -e 's/\.git$//'`
    fi;

    local project_name=`echo "$project_dir" | sed 's/-[0-9][0-9a-z\.]*$//'`
    local retool=no
    local patch

    if test -e "$BUILD_DIR/installed-projects.txt" && \
        grep -q -F "$project_dir" "$BUILD_DIR/installed-projects.txt"; then
        echo "Skipping $project_dir as it is already installed"
        return
    fi

    if test -z "$onlybuild"; then
	if echo "$source" | grep -q "git$"; then
	    git_clone -name "$project_name" -url "$source" -branch "$branch" -commit "$commit"
	    retool=yes
	else
	    download_file "$source" "$tarfile"

	    do_untar_source "$DOWNLOAD_DIR/$tarfile"

	    apply_patches "$PATCHES_DIR/$project_name" "$BUILD_DIR/$project_dir"
	fi
    fi

    cd "$BUILD_DIR/$project_dir"

    if test "x$retool" = "xyes"; then
        NOCONFIGURE=1 ./autogen.sh
    fi

    echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
    echo "PKG_CONFIG_PATH=$PKG_CONFIG_PATH"
    #Note we have to pass $@ first since we need to pass a special
    #Linux option as the first argument to the icu configure script
    ./"$config_name" "$@" --prefix="$prefix"
    make -j"$jobs"
    make install

    echo "$project_dir" >> "$BUILD_DIR/installed-projects.txt"
}

function build_bzip2 ()
{
    local prefix="$STAGING_PREFIX"
    local source="$1"; shift;
    local tarfile=`basename "$source"`

    if test -z "$project_dir"; then
      project_dir=`echo "$tarfile" | sed -e 's/\.tar\.[0-9a-z]*$//' -e 's/\.tgz$//' -e 's/\.git$//'`
    fi;

    local project_name=bzip2

    if test -e "$BUILD_DIR/installed-projects.txt" && \
               grep -q -F "$project_dir" "$BUILD_DIR/installed-projects.txt"; then
        echo "Skipping $project_dir as it is already installed"
        return
    fi

    download_file "$source" "$tarfile"

    do_untar_source "$DOWNLOAD_DIR/$tarfile"

    apply_patches "$PATCHES_DIR/$project_name" "$BUILD_DIR/$project_dir"

    cd "$BUILD_DIR/$project_dir"

    make
    cp -av libbz2.a "$prefix/lib/"

    echo "$project_dir" >> "$BUILD_DIR/installed-projects.txt"
}

function build_dep ()
{
    build_source -p "$STAGING_PREFIX" "$@"
}

function build_tool ()
{
    build_source -p "$TOOLS_PREFIX" "$@"
}

unset "${UNSET_VARS[@]}"

guess_dir PKG_DIR "rig.app" \
    "the directory to store the resulting application bundle" "Bundle dir"
PKG_PREFIX="$PKG_DIR/Contents/Resources"
PKG_LIBDIR="$PKG_PREFIX/lib"
PKG_DATADIR="$PKG_PREFIX/share"

cd `dirname "$0"`
BUILD_DATA_DIR=$PWD

RIG_GITDIR=`cd $BUILD_DATA_DIR/../../.git && pwd`

PATCHES_DIR=$BUILD_DATA_DIR/patches
PATCHES_DIR=`cd $PATCHES_DIR && pwd`

# If a download directory hasn't been specified then try to guess one
# but ask for confirmation first
guess_dir DOWNLOAD_DIR "downloads" \
    "the directory to download to" "Download directory";

DOWNLOAD_PROG=curl

guess_dir STAGING_PREFIX "local" \
    "the staging prefix for installing package dependencies" "Staging dir";

guess_dir TOOLS_PREFIX "tools" \
    "the install prefix for internal build dependencies" "Tools dir";

# If a build directory hasn't been specified then try to guess one
# but ask for confirmation first
guess_dir BUILD_DIR "build" \
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

# The MinGW compiler on Fedora tries to do a similar thing except that
# it also unsets PKG_CONFIG_PATH. This breaks any attempts to add a
# local search path so we need to avoid using that script.

export PKG_CONFIG_LIBDIR="$STAGING_PREFIX/lib/pkgconfig"

exec pkg-config "\$@"
EOF

chmod a+x "$RUN_PKG_CONFIG";

add_prefix "$STAGING_PREFIX"
add_prefix "$TOOLS_PREFIX"

PKG_CONFIG="$RUN_PKG_CONFIG"

ENV_SCRIPT="$BUILD_DIR/env.sh"
echo > "$ENV_SCRIPT"
for x in "${EXPORT_VARS[@]}"; do
    printf "export %q=%q\n" "$x" "${!x}" >> "$ENV_SCRIPT"
done

export "${EXPORT_VARS[@]}"

build_tool "http://ftp.gnu.org/gnu/m4/m4-1.4.16.tar.gz"
build_tool "http://ftp.gnu.org/gnu/autoconf/autoconf-2.69.tar.gz"
build_tool "http://ftp.gnu.org/gnu/automake/automake-1.12.tar.gz"
build_tool "http://ftp.gnu.org/gnu/libtool/libtool-2.4.2.tar.gz"
build_tool "http://pkgconfig.freedesktop.org/releases/pkg-config-0.27.1.tar.gz" \
    --with-internal-glib
build_tool "http://ftp.gnome.org/pub/gnome/sources/intltool/0.40/intltool-0.40.6.tar.bz2"

# Libxml2 is only needed to build the mime database so it's not a
# depedency of Rig
build_tool "ftp://xmlsoft.org/libxml2/libxml2-2.9.0.tar.gz"

build_dep "http://tukaani.org/xz/xz-5.0.4.tar.gz"

build_tool "ftp://ftp.gnome.org/pub/gnome/sources/gnome-common/3.6/gnome-common-3.6.0.tar.xz"

build_dep -configure Configure -j 1 \
    "http://mirrors.ibiblio.org/openssl/source/openssl-1.0.1f.tar.gz" \
    darwin64-x86_64-cc \
    no-shared

# At least on OSX, ld seems to prefer to load .dylib files instead of .a files
# regardless of the load path. As a bodge to work around this, we'll rename the
# library file it installs and hack the .pc file
if test -f "$STAGING_PREFIX"/lib/libcrypto.a; then
 mv -v "$STAGING_PREFIX"/lib/libcrypto{,-static}.a 
 sed -i .tmp s/-lcrypto/-lcrypto-static/g "$STAGING_PREFIX"/lib/pkgconfig/libcrypto.pc
fi

build_bzip2 "http://www.bzip.org/1.0.6/bzip2-1.0.6.tar.gz"

#build_dep "http://ftp.gnu.org/gnu/libiconv/libiconv-1.14.tar.gz"
build_dep "ftp://sourceware.org/pub/libffi/libffi-3.0.11.tar.gz"
build_dep "http://ftp.gnu.org/pub/gnu/gettext/gettext-0.18.3.2.tar.gz"

#export CFLAGS="-g3 -O0"
build_dep \
    "ftp://ftp.gnome.org/pub/gnome/sources/glib/2.38/glib-2.38.2.tar.xz"
#    --with-libiconv=gnu
#unset CFLAGS

# The makefile for this package seems to choke on paralell builds
build_dep -j 1 "http://freedesktop.org/~hadess/shared-mime-info-1.0.tar.xz"

build_dep "http://download.osgeo.org/libtiff/tiff-4.0.3.tar.gz"
build_dep -d jpeg-8d "http://www.ijg.org/files/jpegsrc.v8d.tar.gz"
build_dep \
    "http://downloads.sourceforge.net/project/libpng/libpng16/1.6.9/libpng-1.6.9.tar.gz"
#build_dep \
#    "mirrorservice.org/sites/dl.sourceforge.net/pub/sourceforge/l/li/libjpeg/6b/jpegsr6.tar.gz"

#old_CFLAGS="$CFLAGS"
#old_CPPFLAGS="$CPPFLAGS"
#export CFLAGS="$CFLAGS -g3 -O0"
#export CPPFLAGS="$CPPFLAGS -I$STAGING_PREFIX/include"
build_dep \
    "http://ftp.gnome.org/pub/GNOME/sources/gdk-pixbuf/2.28/gdk-pixbuf-2.28.2.tar.xz" \
    --disable-modules \
    --with-included-loaders=png,jpeg,tiff \
    --disable-glibtest \
    --disable-introspection
#CFLAGS="$old_CFLAGS"
#CPPFLAGS="$old_CPPFLAGS"

#export CFLAGS="-DUNISTR_FROM_CHAR_EXPLICIT -DUNISTR_FROM_STRING_EXPLICIT=explicit"
#build_dep -d icu -configure source/runConfigureICU "http://download.icu-project.org/files/icu4c/51.1/icu4c-51_1-src.tgz" Linux
#unset CFLAGS

build_dep "http://download.savannah.gnu.org/releases/freetype/freetype-2.5.2.tar.bz2"
build_dep "http://www.freedesktop.org/software/harfbuzz/release/harfbuzz-0.9.26.tar.bz2"

build_dep "http://www.cairographics.org/releases/pixman-0.32.4.tar.gz" \
    --disable-gtk
build_dep "http://www.cairographics.org/releases/cairo-1.12.16.tar.xz" \
    --disable-xlib --without-x
#    --enable-quartz --disable-xlib --without-x

#build_dep "http://download.savannah.gnu.org/releases/freetype/freetype-2.5.2.tar.bz2"
build_dep "http://www.freedesktop.org/software/fontconfig/release/fontconfig-2.10.95.tar.bz2" \
    --disable-docs
build_dep "http://ftp.gnome.org/pub/GNOME/sources/pango/1.36/pango-1.36.2.tar.xz" \
    --without-x --with-included-modules=yes --without-dynamic-modules

build_dep -branch origin/rig "https://github.com/rig-project/sdl.git"

build_dep "http://protobuf.googlecode.com/files/protobuf-2.5.0.tar.gz"

export CXXFLAGS="-I$STAGING_PREFIX/include"
build_dep -branch origin/rig "https://github.com/rig-project/protobuf-c.git"
unset CXXFLAGS


export CFLAGS="-g3 -O0"

build_dep \
    -branch origin/rig \
    "https://github.com/rig-project/cogl.git" \
    --enable-cairo \
    --disable-profile \
    --enable-gdk-pixbuf \
    --enable-quartz-image \
    --disable-examples-install \
    --disable-gles1 \
    --disable-gles2 \
    --enable-gl \
    --disable-cogl-gles2 \
    --disable-glx \
    --disable-wgl \
    --enable-sdl2 \
    --disable-gtk-doc \
    --enable-glib \
    --enable-cogl-pango \
    --disable-cogl-gst \
    --disable-introspection \
    --enable-debug

git_clone -name llvm -url "https://github.com/rig-project/llvm.git" -branch origin/rig
git_clone -name clang -url "https://github.com/rig-project/clang.git" -branch origin/rig -build_dir "$BUILD_DIR/llvm/tools"
build_dep -onlybuild "llvm" \
    --enable-debug-runtime --enable-debug-symbols --enable-shared --enable-keep-symbols --with-python=`which python2`

#mclinker needs bison >= 2.5.4 and < 3.0.1
build_tool "http://ftp.gnu.org/gnu/bison/bison-2.7.1.tar.xz"

#the flex that comes with xcode generates incompatible prototypes so we have to build our own
build_tool "http://downloads.sourceforge.net/project/flex/flex-2.5.37.tar.bz2" \
    --disable-dependency-tracking

build_dep -branch origin/rig \
    "https://github.com/rig-project/mclinker.git" \
    --with-llvm-config=$STAGING_PREFIX/bin/llvm-config

build_dep -d rig -j 1 "$RIG_GITDIR" CFLAGS="-g3 -O0"

mkdir -p "$PKG_DIR/Contents/MacOS"

cp -v "$STAGING_PREFIX/bin/rig" "$PKG_DIR/Contents/MacOS/rig-bin"

mkdir -p "$PKG_LIBDIR"

for lib in \
    libcogl-pango2.0.dylib \
    libpangocairo-1.0.0.dylib \
    libcogl2.0.dylib \
    libpango-1.0.0.dylib \
    libcairo.2.dylib \
    libpixman-1.0.dylib \
    libfreetype.6.dylib \
    libgdk_pixbuf-2.0.0.dylib \
    libgio-2.0.0.dylib \
    libtiff.5.dylib \
    liblzma.5.dylib \
    libjpeg.8.dylib \
    libpng15.15.dylib \
    libSDL2-2.0.0.dylib \
    libgmodule-2.0.0.dylib \
    libgobject-2.0.0.dylib \
    libgthread-2.0.0.dylib \
    libffi.6.dylib \
    libglib-2.0.0.dylib \
    libintl.8.dylib
do
    cp -v "$STAGING_PREFIX/lib/$lib" "$PKG_LIBDIR"
done

mkdir -p "$PKG_DIR"

cp -vR "$BUILD_DIR/rig/build/osx/rig.app/Contents/MacOS/"{auto-update.sh,rig} \
    "$BUILD_DIR/rig/tools/rig-check-signature" \
    "$PKG_DIR"/Contents/MacOS

mkdir -p "$PKG_DATADIR/rig"

cp -v "$STAGING_PREFIX/share/rig"/* "$PKG_DATADIR/rig"
cp -v "$BUILD_DIR/rig/data/Info.plist" "$PKG_DIR/Contents/Info.plist"

mkdir -p "$PKG_DATADIR/mime"

cp -vR "$STAGING_PREFIX/share/mime"/* "$PKG_DATADIR/mime"
