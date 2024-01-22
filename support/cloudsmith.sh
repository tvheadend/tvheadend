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
    bookworm|bullseye|buster|sid|stretch|jessie|trixie)
        OS="debian";;
    bionic|focal|jammy|kinetic|impish|trusty|xenial)
        OS="ubuntu";;
    raspios*)
        OS="raspbian";;
    *) echo "OS $TARGET could not be recognized" && exit 1;;
esac

export LC_ALL=C.UTF-8
export LANG=C.UTF-8
export DEBIAN_FRONTEND=noninteractive

apt install --force-yes -y python3.7 || true

# Function to initialize pip using ensurepip and upgrade if necessary
initialize_pip() {
    # Use ensurepip to bootstrap pip if available
    python3 -m ensurepip || true

    PYTHON_VERSION=$(python3 -c 'import platform; print(platform.python_version())')
    MAJOR_VERSION=$(echo $PYTHON_VERSION | cut -d. -f1)
    MINOR_VERSION=$(echo $PYTHON_VERSION | cut -d. -f2)

    if [[ $MAJOR_VERSION -eq 3 ]] && [[ $MINOR_VERSION -ge 6 ]]; then
        # For Python 3.6 and above, upgrade pip to the latest version
        python3 -m pip install --upgrade pip
    fi
}

# Initialize and upgrade pip
initialize_pip

pip3 install --upgrade cloudsmith-cli || pip install --upgrade cloudsmith-cli || pip2 install --upgrade cloudsmith-cli

FILEARRAY=($FILE)

for package in "${FILEARRAY[@]}"; do
    python3 /usr/local/bin/cloudsmith push deb "tvheadend/tvheadend/$OS/$TARGET" $package || python /usr/local/bin/cloudsmith push deb "tvheadend/tvheadend/$OS/$TARGET" $package || cloudsmith push deb "tvheadend/tvheadend/$OS/$TARGET" $package
done
