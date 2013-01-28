#!/bin/bash

set -e

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

function get_git_source ()
{
    local url="$1"; shift
    local commit="$1"; shift
    local project_name=`basename "$source"`;

    project_name="${project_name%.git}"

    local bare_dir="$DOWNLOAD_DIR/$project_name.git"

    if ! test -d "$bare_dir"; then
        git clone --bare --mirror "$url" "$bare_dir"
    fi

    local build_dir="$BUILD_DIR/$project_name"

    if ! test -d "$build_dir"; then
        git clone "$bare_dir" "$build_dir"
        if test -n "$commit"; then
            pushd "$build_dir"
            git checkout "$commit"
            popd
        fi
    fi
}

function build_source ()
{
    local project_dir
    local prefix
    local commit
    local config_name="configure"
    local jobs=4

    while true; do
        case "$1" in
            -p) shift; prefix="$1"; shift ;;
            -d) shift; project_dir="$1"; shift ;;
            -c) shift; commit="$1"; shift ;;
            -C) shift; config_name="$1"; shift ;;
	    -j) shift; jobs="$1"; shift ;;
            -*) echo "Unknown option $1"; exit 1 ;;
            *) break ;;
        esac
    done

    local source="$1"; shift;
    local tarfile=`basename "$source"`

    if test -z "$project_dir"; then
        project_dir=`echo "$tarfile" | sed -e 's/\.tar\.[0-9a-z]*$//' -e 's/\.git$//'`
    fi;

    local project_name=`echo "$project_dir" | sed 's/-[0-9\.]*$//'`
    local retool=no
    local patch

    if test -e "$BUILD_DIR/installed-projects.txt" && \
        grep -q -F "$project_dir" "$BUILD_DIR/installed-projects.txt"; then
        echo "Skipping $project_dir as it is already installed"
        return
    fi        

    if echo "$source" | grep -q "^git"; then
        get_git_source "$source" "$commit"
        retool=yes
    else
        download_file "$source" "$tarfile"

        do_untar_source "$DOWNLOAD_DIR/$tarfile"
    fi

    cd "$BUILD_DIR/$project_dir"

    if test -d "$PATCHES_DIR/$project_name"; then
        for patch in "$PATCHES_DIR/$project_name/"*.patch; do
            patch -p1 < "$patch"
            if grep -q '^+++ .*/Makefile\.am\b' "$patch"; then
                retool=yes;
            fi
        done
    fi

    if test "x$retool" = "xyes"; then
        NOCONFIGURE=1 ./autogen.sh
    fi

    ./"$config_name" --prefix="$prefix" "$@"
    make -j"$jobs"
    make install

    echo "$project_dir" >> "$BUILD_DIR/installed-projects.txt"
}

function build_dep ()
{
    build_source -p "$ROOT_DIR" "$@"
}

function build_tool ()
{
    build_source -p "$TOOLS_DIR" "$@"
}

guess_dir PKG_DIR "rig.app" \
    "the directory to store the resulting application bundle" "Bundle dir"
PKG_PREFIX="$PKG_DIR/Contents/Resources"
PKG_LIBDIR="$PKG_PREFIX/lib"
PKG_DATADIR="$PKG_PREFIX/share"

PATCHES_DIR=`dirname "$0"`/patches
PATCHES_DIR=`cd $PATCHES_DIR && pwd`

# If a download directory hasn't been specified then try to guess one
# but ask for confirmation first
guess_dir DOWNLOAD_DIR "downloads" \
    "the directory to download to" "Download directory";

DOWNLOAD_PROG=curl

guess_dir ROOT_DIR "local" \
    "the root prefix for the build environment" "Root dir";

guess_dir TOOLS_DIR "tools" \
    "the root prefix for internal build dependencies" "Tools dir";

# If a build directory hasn't been specified then try to guess one
# but ask for confirmation first
guess_dir BUILD_DIR "build" \
    "the directory to build source dependencies in" "Build directory";

for dep in "${GL_HEADER_URLS[@]}"; do
    bn="${dep##*/}";
    download_file "$dep" "$bn";
done;

echo "Copying GL headers...";
if ! test -d "$TOOLS_DIR/include/OpenGL"; then
    mkdir -p "$TOOLS_DIR/include/OpenGL"
fi;
for header in "${GL_HEADERS[@]}"; do
    cp "$DOWNLOAD_DIR/$header" "$TOOLS_DIR/include/OpenGL/"
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

export PKG_CONFIG_LIBDIR="$ROOT_DIR/lib/pkgconfig"

exec pkg-config "\$@"
EOF

chmod a+x "$RUN_PKG_CONFIG";

add_prefix "$ROOT_DIR"
add_prefix "$TOOLS_DIR"

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
# Libxml2 is only needed to build the mime database so it's not a
# depedency of Rig
build_tool "ftp://xmlsoft.org/libxml2/libxml2-2.9.0.tar.gz"
build_tool "http://ftp.gnome.org/pub/gnome/sources/intltool/0.40/intltool-0.40.6.tar.bz2"

build_dep "http://tukaani.org/xz/xz-5.0.4.tar.gz"

build_tool "ftp://ftp.gnome.org/pub/gnome/sources/gnome-common/3.6/gnome-common-3.6.0.tar.xz"

build_dep -C Configure -j 1 \
    "https://www.openssl.org/source/openssl-1.0.1c.tar.gz" \
    darwin64-x86_64-cc \
    no-shared

# At least on OSX, ld seems to prefer to load .dylib files instead of .a files
# regardless of the load path. As a bodge to work around this, we'll rename the
# library file it installs and hack the .pc file
if test -f "$ROOT_DIR"/lib/libcrypto.a; then
 mv -v "$ROOT_DIR"/lib/libcrypto{,-static}.a 
 sed -i .tmp s/-lcrypto/-lcrypto-static/g "$ROOT_DIR"/lib/pkgconfig/libcrypto.pc
fi

build_dep "ftp://sourceware.org/pub/libffi/libffi-3.0.11.tar.gz"
build_dep "http://ftp.gnu.org/pub/gnu/gettext/gettext-0.18.1.1.tar.gz"
build_dep \
    "ftp://ftp.gnome.org/pub/gnome/sources/glib/2.34/glib-2.34.2.tar.xz"

# The makefile for this package seems to choke on paralell builds
build_dep -j 1 "http://freedesktop.org/~hadess/shared-mime-info-1.0.tar.xz"

build_dep "http://download.osgeo.org/libtiff/tiff-4.0.3.tar.gz" \
    --without-x --without-apple-opengl-framework
build_dep -d jpeg-8d "http://www.ijg.org/files/jpegsrc.v8d.tar.gz"
build_dep \
    "http://downloads.sourceforge.net/project/libpng/libpng15/1.5.13/libpng-1.5.13.tar.xz"
build_dep \
    "http://ftp.gnome.org/pub/GNOME/sources/gdk-pixbuf/2.26/gdk-pixbuf-2.26.5.tar.xz" \
    --disable-modules \
    --with-included-loaders=png,jpeg,tiff \
    --disable-glibtest

build_dep "http://www.cairographics.org/releases/pixman-0.28.0.tar.gz" \
    --disable-gtk
build_dep "http://www.cairographics.org/releases/cairo-1.12.8.tar.xz" \
    --enable-quartz --disable-xlib --without-x

build_dep "http://download.savannah.gnu.org/releases/freetype/freetype-2.4.10.tar.bz2"
build_dep "http://www.freedesktop.org/software/fontconfig/release/fontconfig-2.10.1.tar.bz2" \
    --disable-docs
build_dep "http://ftp.gnome.org/pub/GNOME/sources/pango/1.32/pango-1.32.1.tar.xz" \
    --without-x --with-included-modules=yes --without-dynamic-modules

build_dep -d SDL-2.0.0-6673 "http://www.libsdl.org/tmp/SDL-2.0.tar.gz"

build_dep -c 2c0cfdefbb9d1ac5097d98887d3581b67a324fae \
    "git://git.gnome.org/cogl.git" \
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
    --disable-introspection

build_dep -c 2 "git://github.com/01org/rig.git"

mkdir -p "$PKG_DIR/Contents/MacOS"

cp -v "$ROOT_DIR/bin/rig" "$PKG_DIR/Contents/MacOS/rig-bin"

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
    cp -v "$ROOT_DIR/lib/$lib" "$PKG_LIBDIR"
done

mkdir -p "$PKG_DIR"

cp -vR "$BUILD_DIR/rig/build/osx/rig.app/Contents/MacOS/"{auto-update.sh,rig} \
    "$BUILD_DIR/rig/tools/rig-check-signature" \
    "$PKG_DIR"/Contents/MacOS

mkdir -p "$PKG_DATADIR/rig"

cp -v "$ROOT_DIR/share/rig"/* "$PKG_DATADIR/rig"
cp -v "$BUILD_DIR/rig/data/Info.plist" "$PKG_DIR/Contents/Info.plist"

mkdir -p "$PKG_DATADIR/mime"

cp -vR "$ROOT_DIR/share/mime"/* "$PKG_DATADIR/mime"
