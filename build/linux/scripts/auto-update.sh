#!/bin/bash

BASE_URL="http://rig.sixbynine.org/osx"
VERSION_URL="$BASE_URL/latest_version"

EXE_DIR=`dirname $0`
BASE_DIR=`cd "$EXE_DIR"/.. && pwd`
UPDATE_FILE="$BASE_DIR"/Resources/rig-update.tar.gz
LOCK_FILE="$BASE_DIR/Resources/update-lock-file"
KEY_FILE="$BASE_DIR/Resources/share/rig/rig-key-pub.pem"

# Try to exclusively create a lock file so that we don't run the
# script again if it's already running
if ( set -o noclobber; echo "locked" > "$LOCK_FILE" ) 2> /dev/null; then
    # We've successfully created the lock file so we'll make sure it's
    # automatically deleted when the script exits
    trap 'rm -f "$LOCK_FILE"; exit $?' INT TERM EXIT
else
    echo "Lock failed - exit" >&2
    exit 1
fi

# Function to store a failure message. This message should be
# displayed to the user at some point.
function fail_message ()
{
    echo "$@" > "$BASE_DIR/Resources/update-fail-message.txt";
    exit 1;
}

# Extract the installed version from the info file
# (note the + regexp operator doesn't seem to work on BSD sed)
CURRENT_VERSION=`sed -n \
                     -e '/<key>CFBundleVersion<\/key>/,/./!b' \
                     -e 's/^.*<string>\([0-9]\{1,\}\)<\/string>.*/\1/p' \
                     < "$BASE_DIR/Info.plist"`

# Extract the latest version from the website
LATEST_VERSION=`curl -f -s -L "$VERSION_URL"`

# We'll silently ignore errors from fetching the version number so
# that for example if the user doesn't currently have an internet
# connection it won't keep bothering them with dialogs
if test "$?" -ne "0"; then
    exit 1;
fi

# Validate the two version numbers we got
function validate_version ()
{
    # Make sure it's only one line
    test `echo "$1" | wc -l` -eq 1 || return 1;
    # Make sure it's just a number
    echo "$1" | grep -q '^[0-9]\+$' || return 1;
}

validate_version "$CURRENT_VERSION" || \
    fail_message "Failed to extract the current version number of Rig.";
validate_version "$LATEST_VERSION" || \
    fail_message "Failed to fetch the latest version number of Rig.";

# No need to do anything if we're already up-to-date
if test "$CURRENT_VERSION" -ge "$LATEST_VERSION"; then
    exit 0;
fi

# Download the latest version
if ! curl -f -s -L "$BASE_URL/rig-osx-$LATEST_VERSION.tar.gz.signed" \
          > "$UPDATE_FILE.tmp.signed"; then
    fail_message "Failed to download the latest Rig update.";
fi

if ! test -f "$UPDATE_FILE.tmp.signed"; then
    fail_message "Something went wrong while downloading the latest" \
        "Rig update."
fi

# Verify the signature
"$EXE_DIR"/rig-check-signature \
    -k "$KEY_FILE" \
    -d "$UPDATE_FILE.tmp.signed" \
    -o "$UPDATE_FILE.tmp"
sign_result="$?"

# rig-check-signature returns an error code of 6 if it failed
# specifically because the signature was invalid
if test "$sign_result" = "6"; then
    fail_message "The signature for the update was invalid"
elif test "$sign_result" != "0"; then
    fail_message "Failed to check the signature of the update"
fi

rm "$UPDATE_FILE.tmp.signed"

# If we make it here then the download worked so we'll atomically
# rename the file so that the next time Rig starts it will recognise
# that there is an update
if ! mv "$UPDATE_FILE.tmp" "$UPDATE_FILE"; then
    fail_message "Failed to move the update file to its current location"
fi
