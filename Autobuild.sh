#!/bin/bash
#
# Entry point for the Doozer autobuild system
#
# (c) Andreas Öman 2011. All rights reserved.
#
#

set -eu

BUILD_API_VERSION=3
EXTRA_BUILD_NAME=""
JARGS=""
JOBSARGS=""
TARGET=""
ARCHOVR=""
RELEASE="--release"
WORKINGDIR="/var/tmp/showtime-autobuild"
FILELIST="$PWD/filelist.txt"
OP="build"
OSPREFIX=""

while getopts "vht:e:j:w:o:p:a:c:" OPTION
do
  case $OPTION in
      v)
	  echo $BUILD_API_VERSION
	  exit 0
	  ;;
      h)
	  echo "This script is intended to be used by the autobuild system only"
	  exit 0
	  ;;
      t)
	  TARGET="$OPTARG"
	  ;;
      e)
	  EXTRA_BUILD_NAME="$OPTARG"
	  ;;
      j)
	  JOBSARGS="--jobs=$OPTARG"
	  JARGS="-j$OPTARG"
	  ;;
      w)
	  WORKINGDIR="$OPTARG"
	  ;;
      a)
	  ARCHOVR="$OPTARG"
	  ;;
      p)
      OSPREFIX="$OPTARG"
      ;;
      o)
	  OP="$OPTARG"
	  ;;
  esac
done

if [[ -z $TARGET ]]; then
    source Autobuild/identify-os.sh
    if ! [[ -z $ARCHOVR ]]; then
        ARCH=$ARCHOVR
    fi
    TARGET="$CODENAME-$ARCH"
fi

TARGET=$OSPREFIX$TARGET

git status

if [ -f Autobuild/${TARGET}.sh ]; then
    echo "Building for $TARGET"
    source Autobuild/${TARGET}.sh
else
    echo "target $TARGET not supported"
    exit 1
fi

