#!/bin/bash
#
# Entry point for the Cloudsmith upload system
#
# (c) Flole 2022. All rights reserved.
#
#

set -eu

TARGET=""
OSPREFIX=""
OS=""
FILE=""

while getopts "t:p:f:" OPTION
do
  case $OPTION in
      t)
          TARGET="$OPTARG"
          ;;
      p)
          OSPREFIX="$OPTARG"
          ;;
      f)
          FILE="$OPTARG"
          ;;
  esac
done

if [[ -z $FILE ]]; then
    echo "No file specified"
    exit 1
fi

if [[ -z $TARGET ]]; then
    source Autobuild/identify-os.sh
    TARGET="$DISTRO"
fi

case $OSPREFIX$TARGET in
    bookworm|bullseye|buster|sid|stretch|jessie)
        OS="debian";;
    bionic|focal|jammy|trusty|xenial)
        OS="ubuntu";;
    raspiosbullseye|raspiosbuster|raspiosjessieraspiosstretch)
        OS="raspbian";;
    *) echo "OS $TARGET could not be recognized" && exit 1;;
esac

apt install -y python3-pip || apt install -y python-pip

pip3 install --upgrade cloudsmith-cli || pip install --upgrade cloudsmith-cli || pip2 install --upgrade cloudsmith-cli

FILEARRAY=($FILE)

for package in "${FILEARRAY[@]}"; do
    cloudsmith push deb "tvheadend/tvheadend/$OS/$TARGET" $package
done
