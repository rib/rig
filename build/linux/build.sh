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
            patch -p1 < "$patch"
            if grep -q '^+++ .*/\(Makefile\.am\|configure\.ac\)\b' "$patch" ; then
                retool=yes;
            fi
        done
    fi
}

function git_clone ()
{
    local name="$1"; shift
    local url="$1"; shift
    local commit="$1"; shift

    pushd $BUILD_DIR 1>/dev/null

    if test -d $name; then
	pushd $name 1>/dev/null
	if ! `git log|grep -q "commit $commit"`; then
	    echo "Found existing $name clone not based on $commit"
	    exit 1
	fi
	popd 1>/dev/null
    else
	git clone $url $name
	if [ $? -ne 0 ]; then
	    echo "Cloning ${url} failed."
	    exit 1
	fi

	pushd $name 1>/dev/null
	git checkout -b rig-build $commit
	if [ $? -ne 0 ]; then
	    echo "Checking out $commit failed"
	    exit 1
	fi

        apply_patches "$PATCHES_DIR/$name" "$BUILD_DIR/$name"
    fi

    popd 1>/dev/null
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
        project_dir=`echo "$tarfile" | sed -e 's/\.tar\.[0-9a-z]*$//' -e 's/\.tgz$//' -e 's/\.git$//'`
    fi;

    local project_name=`echo "$project_dir" | sed 's/-[0-9\.]*$//'`
    local retool=no
    local patch

    if test -e "$BUILD_DIR/installed-projects.txt" && \
        grep -q -F "$project_dir" "$BUILD_DIR/installed-projects.txt"; then
        echo "Skipping $project_dir as it is already installed"
        return
    fi

    if echo "$source" | grep -q "git$"; then
        git_clone "$project_name" "$source" "$commit"
        retool=yes
    else
        download_file "$source" "$tarfile"

        do_untar_source "$DOWNLOAD_DIR/$tarfile"

        apply_patches "$PATCHES_DIR/$project_name" "$BUILD_DIR/$project_dir"
    fi

    cd "$BUILD_DIR/$project_dir"

    if test "x$retool" = "xyes"; then
        NOCONFIGURE=1 ./autogen.sh
    fi

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

    make -f Makefile-libbz2_so
    ln -sf libbz2.so.1.0.6 libbz2.so
    cp -av libbz2.so* "$prefix/lib/"

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

if test "$1"; then
    RIG_COMMIT=$1; shift
fi

unset "${UNSET_VARS[@]}"

guess_dir PKG_DIR "pkg" \
    "the directory to store the resulting package" "Package dir"
PKG_RELEASEDIR="$PKG_DIR/release"
PKG_LIBDIR="$PKG_RELEASEDIR/lib"
PKG_DATADIR="$PKG_RELEASEDIR/share"

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

build_tool "ftp://xmlsoft.org/libxml2/libxml2-2.9.0.tar.gz"
build_dep "http://tukaani.org/xz/xz-5.0.4.tar.gz"

build_dep -C Configure -j 1 \
    "http://mirrors.ibiblio.org/openssl/source/openssl-1.0.1c.tar.gz" \
    linux-elf \
    no-shared

build_bzip2 "http://www.bzip.org/1.0.6/bzip2-1.0.6.tar.gz"

build_dep "ftp://sourceware.org/pub/libffi/libffi-3.0.11.tar.gz"
build_dep "http://ftp.gnu.org/pub/gnu/gettext/gettext-0.18.2.1.tar.gz"
export CFLAGS="-g3 -O0"
build_dep \
    "ftp://ftp.gnome.org/pub/gnome/sources/glib/2.34/glib-2.34.2.tar.xz"
unset CFLAGS

# The makefile for this package seems to choke on paralell builds
build_dep -j 1 "http://freedesktop.org/~hadess/shared-mime-info-1.0.tar.xz"

build_dep "http://download.osgeo.org/libtiff/tiff-4.0.3.tar.gz" \
    --without-x --without-apple-opengl-framework
build_dep -d jpeg-8d "http://www.ijg.org/files/jpegsrc.v8d.tar.gz"
build_dep \
    "http://downloads.sourceforge.net/project/libpng/libpng16/1.6.3/libpng-1.6.3.tar.xz"
#build_dep \
#    "mirrorservice.org/sites/dl.sourceforge.net/pub/sourceforge/l/li/libjpeg/6b/jpegsr6.tar.gz"

export CFLAGS="-g3 -O0"
export CPPFLAGS="-I$STAGING_PREFIX/include"
build_dep \
    "http://ftp.gnome.org/pub/GNOME/sources/gdk-pixbuf/2.26/gdk-pixbuf-2.26.5.tar.xz" \
    --disable-modules \
    --with-included-loaders=png,jpeg,tiff \
    --disable-glibtest \
    --disable-introspection
unset CFLAGS
unset CPPFLAGS

#export CFLAGS="-DUNISTR_FROM_CHAR_EXPLICIT -DUNISTR_FROM_STRING_EXPLICIT=explicit"
#build_dep -d icu -C source/runConfigureICU "http://download.icu-project.org/files/icu4c/51.1/icu4c-51_1-src.tgz" Linux
#unset CFLAGS

build_dep "http://download.savannah.gnu.org/releases/freetype/freetype-2.4.10.tar.bz2"
build_dep "http://www.freedesktop.org/software/fontconfig/release/fontconfig-2.10.92.tar.bz2" \
    --disable-docs \
    --enable-libxml2
build_dep "http://www.freedesktop.org/software/harfbuzz/release/harfbuzz-0.9.16.tar.bz2"

build_dep "http://www.cairographics.org/releases/pixman-0.28.0.tar.gz" \
    --disable-gtk

#NB: we make sure to build cairo after freetype/fontconfig so we have support for these backends
build_dep "http://www.cairographics.org/releases/cairo-1.12.8.tar.xz" \
    --enable-xlib

build_dep "http://ftp.gnome.org/pub/GNOME/sources/pango/1.34/pango-1.34.1.tar.xz" \
    --disable-introspection \
    --with-included-modules=yes \
    --without-dynamic-modules

build_dep -d SDL2-2.0.0 "http://www.libsdl.org/release/SDL2-2.0.0.tar.gz"

build_dep "http://ftp.gnu.org/gnu/gdbm/gdbm-1.10.tar.gz"

build_dep "http://dbus.freedesktop.org/releases/dbus/dbus-1.7.2.tar.gz"

build_dep "http://0pointer.de/lennart/projects/libdaemon/libdaemon-0.14.tar.gz"

export CPPFLAGS="-I$STAGING_PREFIX/include"
build_dep "http://avahi.org/download/avahi-0.6.31.tar.gz" \
  --disable-qt3 \
  --disable-qt4 \
  --disable-gtk \
  --disable-gtk3 \
  --disable-python \
  --disable-mono \
  --disable-introspection
unset CPPFLAGS

build_dep "http://protobuf.googlecode.com/files/protobuf-2.5.0.tar.gz"

export CXXFLAGS="-I$STAGING_PREFIX/include"
build_dep "http://protobuf-c.googlecode.com/files/protobuf-c-0.15.tar.gz"
unset CXXFLAGS

build_dep "http://gstreamer.freedesktop.org/src/gstreamer/gstreamer-1.0.7.tar.xz"
build_dep "http://gstreamer.freedesktop.org/src/gst-plugins-base/gst-plugins-base-1.0.7.tar.xz"

export CFLAGS="-g3 -O0"

build_dep \
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
    --enable-cogl-gst \
    --disable-introspection \
    --enable-debug

if test -z $RIG_COMMIT; then
    build_dep -d rig "$RIG_GITDIR"
else
    build_dep -d rig -c "$RIG_COMMIT" "$RIG_GITDIR"
fi

mkdir -p "$PKG_RELEASEDIR"

cp -v "$STAGING_PREFIX/bin/rig" "$PKG_RELEASEDIR/rig-bin"

mkdir -p "$PKG_LIBDIR"

for lib in \
    libSDL2 \
    libasprintf \
    libavahi-client \
    libavahi-common \
    libavahi-core \
    libavahi-glib \
    libbz2 \
    libcairo \
    libcogl-gst \
    libcogl-pango2 \
    libcogl-path \
    libcogl2 \
    libdaemon \
    libdbus-1 \
    libffi \
    libfontconfig \
    libfreetype \
    libgdk_pixbuf-2.0 \
    libgettextlib-0.18.2 \
    libgettextlib \
    libgettextpo \
    libgettextsrc-0.18.2 \
    libgettextsrc \
    libgio-2.0 \
    libglib-2.0 \
    libgmodule-2.0 \
    libgobject-2.0 \
    libgthread-2.0 \
    libgstfft-1.0 \
    libgstaudio-1.0 \
    libgstvideo-1.0 \
    libgsttag-1.0 \
    libgstcontroller-1.0 \
    libgstbase-1.0 \
    libgstreamer-1.0 \
    libgstfft-1.0 \
    libgstaudio-1.0 \
    libgstvideo-1.0 \
    libgstbase-1.0 \
    libgsttag-1.0 \
    libgstcontroller-1.0 \
    libgstreamer-1.0 \
    libharfbuzz \
    libjpeg \
    liblzma \
    libpango-1.0 \
    libpangocairo-1.0 \
    libpangoft2-1.0 \
    libpixman-1 \
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

mkdir -p "$PKG_LIBDIR"/gio
cp -av $STAGING_PREFIX/lib/gio/modules "$PKG_LIBDIR"/gio

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
