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
DRYRUN="0"

while getopts "t:p:f:n" OPTION
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
        n)
            DRYRUN="1"
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
    trusty|xenial|bionic|focal|impish|jammy|kinetic|lunar|mantic)
        OS="ubuntu";;
    raspios*)
        OS="raspbian";;
    *) echo "OS $TARGET could not be recognized" && exit 1;;
esac

export LC_ALL=C.UTF-8
export LANG=C.UTF-8
export DEBIAN_FRONTEND=noninteractive

# Since Python 3.11, this flag is required to install pip packages globally
export PIP_BREAK_SYSTEM_PACKAGES=1
    
apt install --force-yes -y python3.7 || true

# Use ensurepip to bootstrap pip if available, else install from apt
python3 -m ensurepip || apt install --force-yes -y python3-pip || apt install --force-yes -y python-pip

# Get the major and minor version of the installed Python
python_version=$(python3 -c 'import sys; print("{}.{}".format(sys.version_info.major, sys.version_info.minor))')

# Compare versions and upgrade pip accordingly
if [[ "$python_version" == "3.3" ]]; then
    python3 -m pip install --upgrade 'pip<10.0.2' 'colorama==0.4.1' 'urllib3==1.22'
elif [[ "$python_version" == "3.4" ]]; then
    python3 -m pip install --ignore-installed --upgrade 'pip<19.2' 'colorama==0.4.1' 'urllib3==1.24.3' 'requests==2.21.0' 'six==1.16.0' 'certifi==2021.10.8'
elif [[ "$python_version" == "3.5" ]]; then
    python3 -m pip install --upgrade 'pip<20.4'
elif [[ "$python_version" == "3.6" ]]; then
    python3 -m pip install --upgrade 'pip<22.0'
else
    # For Python 3.7 and above, install the latest version of pip
    python3 -m pip install --upgrade pip
fi

pip3 install --upgrade cloudsmith-cli || pip install --upgrade cloudsmith-cli || pip2 install --upgrade cloudsmith-cli
python3 /usr/local/bin/cloudsmith --version || python /usr/local/bin/cloudsmith --version || cloudsmith --version

FILEARRAY=($FILE)

for package in "${FILEARRAY[@]}"; do
    if [ $DRYRUN = "1" ]; then
        echo "DRYRUN MODE: Skip pushing tvheadend/tvheadend/$OS/$TARGET $package"
    else
        python3 /usr/local/bin/cloudsmith push deb "tvheadend/tvheadend/$OS/$TARGET" $package || python /usr/local/bin/cloudsmith push deb "tvheadend/tvheadend/$OS/$TARGET" $package || cloudsmith push deb "tvheadend/tvheadend/$OS/$TARGET" $package
    fi
done
