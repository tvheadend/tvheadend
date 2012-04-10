#!/bin/sh

revision=`$1/support/getver.sh`

NEW_REVISION="#define BUILD_VERSION \"$revision\""
OLD_REVISION=`cat $2 2> /dev/null`

# Update version.h only on revision changes to avoid spurious rebuilds
if test "$NEW_REVISION" != "$OLD_REVISION"; then
    echo "$NEW_REVISION" > "$2"
fi
