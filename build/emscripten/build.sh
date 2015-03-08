#!/bin/bash

# Please read the README before running this script!

set -e
set -x

UNSET_VARS=(
    ACLOCAL
    LD_LIBRARY_PATH
    PKG_CONFIG_PATH
    EM_PKG_CONFIG_PATH
    PKG_CONFIG
    GI_TYPELIB_PATH
    XDG_DATA_DIRS
    GIO_EXTRA_MODULES
    GST_PLUGIN_PATH
)

EXPORT_VARS=(
    LD_LIBRARY_PATH
    PATH
    PKG_CONFIG_PATH
    EM_PKG_CONFIG_PATH
    PKG_CONFIG
    GI_TYPELIB_PATH
    XDG_DATA_DIRS
    GIO_EXTRA_MODULES
    GST_PLUGIN_PATH
    CFLAGS
    LDFLAGS
    ACLOCAL_FLAGS
)

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

function add_em_prefix()
{
    local prefix;
    local aclocal_dir;
    local x;

    prefix="$1"; shift;

    add_to_path EM_PKG_CONFIG_PATH "$prefix/lib/pkgconfig"
    add_to_path EM_PKG_CONFIG_PATH "$prefix/share/pkgconfig"

    add_flag CFLAGS I "$prefix/include"
    add_flag CXXFLAGS I "$prefix/include"
    add_flag LDFLAGS L "$prefix/lib"
    if test -d $prefix/share/aclocal; then
        add_flag ACLOCAL_FLAGS I "$prefix/share/aclocal"
    fi
}

function add_host_prefix()
{
    local prefix;
    local aclocal_dir;
    local x;

    prefix="$1"; shift;

    add_to_path LD_LIBRARY_PATH "$prefix/lib"
    add_to_path PATH "$prefix/bin" backwards
    add_to_path PKG_CONFIG_PATH "$prefix/lib/pkgconfig"
    add_to_path PKG_CONFIG_PATH "$prefix/share/pkgconfig"

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
	    -module) shift; shift ;;
	    -dep) shift; deps="$deps $1"; shift ;;
	    -prefix) shift; prefix="$1"; shift ;;
	    -unpackdir) shift; unpack_dir="$1"; shift ;;
	    -branch) shift; branch="$1"; shift ;;
	    -configure) shift; config_name="$1"; shift ;;
	    -top_srcdir) shift; top_srcdir="$1"; shift ;;
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
    if test -z $is_tool; then
        add_to_path EM_PKG_CONFIG_PATH "$prefix/lib/pkgconfig"
        add_to_path EM_PKG_CONFIG_PATH "$prefix/share/pkgconfig"
    else
        add_to_path PKG_CONFIG_PATH "$prefix/lib/pkgconfig"
        add_to_path PKG_CONFIG_PATH "$prefix/share/pkgconfig"
    fi

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

    save_CPPFLAGS=$CPPFLAGS
    save_CFLAGS=$CFLAGS
    save_LDFLAGS=$LDFLAGS

    if ! test -e ./"$config_name"; then
        retool=yes
    fi

    if test "x$retool" = "xyes"; then
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

    echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
    echo "PKG_CONFIG_PATH=$PKG_CONFIG_PATH"
    echo "EM_PKG_CONFIG_PATH=$EM_PKG_CONFIG_PATH"
    if test -z $is_tool; then
        emconfigure ./"$config_name" "$@" \
            --prefix="$prefix" \
            CFLAGS="$CFLAGS" \
            CXXFLAGS="$CXXFLAGS" \
            LDFLAGS="$LDFLAGS"
        emmake env
        emmake make V=1 $make_args -j"$jobs"
        emmake make $make_args $install_target
    else
        ./"$config_name" "$@" \
            --prefix="$prefix" \
            CFLAGS="$CFLAGS" \
            CXXFLAGS="$CXXFLAGS" \
            LDFLAGS="$LDFLAGS"
        env
        make V=1 $make_args -j"$jobs"
        make $make_args $install_target
    fi

    CPPFLAGS=$save_CPPFLAGS
    CFLAGS=$save_CFLAGS
    LDFLAGS=$save_LDFLAGS

    # XXX: we want to be careful to avoid linking with static libraries
    # otherwise we'll end up with duplicate symbol errors when compiling to
    # javascript
    find $prefix \( -iname '*.la' -o -iname '*.a' \) -exec rm {} \;

    echo "$build_dir" >> "$BUILD_DIR/installed-projects.txt"
}

function build_dep ()
{
    build_source -prefix "$STAGING_PREFIX" "$@"
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
RIG_BUILD_META_DIR=$RIG_SRC_DIR/build/emscripten

if ! test -f $RIG_BUILD_META_DIR/build.sh; then
    echo "Could not find Rig source at $1"
    usage
fi

if ! test -f $2; then
    echo "Couldn't find bashrc file $2"
    usage
fi

source $2

unset "${UNSET_VARS[@]}"

cd `dirname "$0"`
BUILD_DATA_DIR=$PWD

RIG_GITDIR=`cd $BUILD_DATA_DIR/../../.git && pwd`

PATCHES_DIR=$BUILD_DATA_DIR/patches

# If a download directory hasn't been specified then try to guess one
# but ask for confirmation first
guess_dir DOWNLOAD_DIR "downloads" \
    "the directory to download to" "Download directory";

DOWNLOAD_PROG=curl

guess_dir STAGING_PREFIX "staging" \
    "the staging prefix for installing package dependencies" "Staging dir";

guess_dir TOOLS_PREFIX "tools" \
    "the install prefix for internal build dependencies" "Tools dir";

# If a build directory hasn't been specified then try to guess one
# but ask for confirmation first
guess_dir BUILD_DIR "build" \
    "the directory to build source dependencies in" "Build directory";

guess_dir INSTALL_PREFIX "prebuilt" \
    "the prefix for installing binaries that Rig can find at runtime" "Install dir";

RUN_PKG_CONFIG="$BUILD_DIR/run-pkg-config.sh";

echo "Generating $BUILD_DIR/run-pkg-config.sh";

cat > "$RUN_PKG_CONFIG" <<EOF
# This is a wrapper script for pkg-config that overrides the
# PKG_CONFIG_LIBDIR variable so that it won't pick up the local system
# .pc files.

export PKG_CONFIG_LIBDIR="/invalid/foo/path"

exec pkg-config "\$@"
EOF

chmod a+x "$RUN_PKG_CONFIG";

add_em_prefix "$STAGING_PREFIX"
add_host_prefix "$TOOLS_PREFIX"

PKG_CONFIG="$RUN_PKG_CONFIG"

XDG_DATA_DIRS="/invalid/path";

export "${EXPORT_VARS[@]}"

build_dep -module freetype -retool_cmd echo -branch origin/emscripten \
    "https://github.com/rig-project/freetype.git"

build_dep -module libxml2 -retool \
    "ftp://xmlsoft.org/libxml2/libxml2-2.9.0.tar.gz" \
    --without-python --without-debug --without-legacy --without-catalog \
    --without-docbook --with-c14n --disable-tests

build_dep -module fontconfig -retool -branch origin/emscripten \
    "https://github.com/rig-project/fontconfig.git" \
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

build_dep -module harfbuzz -dep gettext -dep icu -dep freetype -retool \
    "http://www.freedesktop.org/software/harfbuzz/release/harfbuzz-0.9.26.tar.bz2" \
    --with-icu=yes

build_tool "http://protobuf.googlecode.com/files/protobuf-2.5.0.tar.gz"

# We have to build protobuf-c twice; once to build the protoc-c tool
# and then again to cross-compile libprotobuf-c.so
build_tool -branch origin/rig \
  "https://github.com/rig-project/protobuf-c.git" \
  --enable-protoc-c

build_dep -module protobuf-c -branch origin/rig \
  "https://github.com/rig-project/protobuf-c.git" \
  --disable-protoc-c

mkdir -p $INSTALL_PREFIX/lib
cp -av $STAGING_PREFIX/{lib/lib*.so,lib/lib*.so.*} $INSTALL_PREFIX/lib
cp -av $STAGING_PREFIX/include $INSTALL_PREFIX

