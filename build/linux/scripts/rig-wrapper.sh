#!/bin/sh

save_pwd=$PWD
pkg_dir=`dirname "$0"`
cd $pkg_dir
pkg_dir=$PWD
cd $save_pwd

release_dir=$pkg_dir/release
updates_dir=$pkg_dir/updates

# If there is a complete update ready then switch to that instead
if test -f "$updates_dir/rig-update.tar.gz"; then
    # Exit on error
    set -e
    # Remove any partial attempt to extract a release
    rm -rf "$updates_dir/update"
    # Extract the new version
    tar -zxf "$updates_dir/rig-update.tar.gz" -C "$updates_dir"

    # Remove any old saved release
    #
    # TODO: verify that the version of any previous release deleted
    # here really does correspond to an older release
    rm -rf "$updates_dir/previous"
    # Swap out current release
    mv "$release_dir" "$updates_dir/previous"
    # Swap in the new release
    mv "$updates_dir/update" "$release_dir"

    # Create a file so that Rig will report that it has been updated
    touch "$release_dir/rig-updated"
    # Run the new version of the program instead
    exec "$pkg_dir/rig"
    exit 1
fi

# Check for an update in the background
bash "$release_dir/auto-update.sh" &

export LD_LIBRARY_PATH="$release_dir/lib:$LD_LIBRARY_PATH"
if test -z "$XDG_DATA_DIRS"; then
    XDG_DATA_DIRS="/usr/local/share:/usr/share"
fi
export XDG_DATA_DIRS="$release_dir/share:$XDG_DATA_DIRS"
export GIO_EXTRA_MODULES="$release_dir/lib/gio"

xdg-mime install $release_dir/share/mime/application/rig-mime.xml
xdg-icon-resource install --context mimetypes --size 64 $release_dir/share/pixmaps/rig-logo-64.png application-x-rig
xdg-icon-resource install --context apps --size 64 $release_dir/share/pixmaps/rig-logo-64.png application-x-rig
sed -i "s|Exec=*.|Exec=$0 %f|" $release_dir/share/applications/rig.desktop
xdg-desktop-menu install --novendor $release_dir/share/applications/rig.desktop

while true; do
    case "$1" in
        -g) shift; gdb="yes"; shift ;;
        -*) echo "Unknown option $1"; exit 1 ;;
        *) break ;;
    esac
done

if test "$#" -eq 0; then
    mkdir -p "$HOME/Documents/rig"
    project=$HOME/Documents/rig/ui.rig
else
    project=$1; shift
fi

if test "$gdb"; then
    gdb --args "$release_dir/rig-bin" "$project"
else
    exec "$release_dir/rig-bin" "$project"
fi
