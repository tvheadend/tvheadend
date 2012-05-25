#!/bin/sh
revision=`cd "$1" && git describe --dirty --abbrev=5 2>/dev/null | sed  -e 's/-/./g'`

if ! test $revision; then
    test $revision || revision=`cd "$1" && git describe --abbrev=5 2>/dev/null | sed  -e 's/-/./g'`
fi

echo $revision
