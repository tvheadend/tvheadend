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
    37|38|39)
        OS="fedora";;
    40|41)
        echo "Fedora 40 and 41 (current rawhide) are not (yet) supported by Cloudsmith" && exit;;
    *) echo "OS $TARGET could not be recognized" && exit 1;;
esac

export LC_ALL=C.UTF-8
export LANG=C.UTF-8

if [ $OS == "fedora" ]; then
    dnf install -y curl
else
    export DEBIAN_FRONTEND=noninteractive
    apt update
    apt install -y curl
fi

FILEARRAY=($FILE)
for package in "${FILEARRAY[@]}"; do
    EXTENSION=${package##*.}
    PKGBASENAME=$(basename $package)
    # dryrun exit is performed as late as possible to catch any earlier potential issues
    if [ $DRYRUN = "1" ]; then
        echo "DRYRUN MODE: Skip pushing $OS $TARGET package $package"
        continue
    fi
    # upload package to file upload endpoint
    curlUpload=$(
        curl \
            --upload-file $package \
            -u "$CLOUDSMITH_OWNER:$CLOUDSMITH_API_KEY" \
            -H "Content-Sha256: $(sha256sum "$package" | cut -f1 -d' ')" \
            https://upload.cloudsmith.io/$CLOUDSMITH_ORG/$CLOUDSMITH_REPO/$PKGBASENAME \
    )
    IDENTIFIER=$(echo $curlUpload | cut -f 4 -d '"')
    # finalize by POSTing to the create package endpoint
    curl \
        -X POST \
        -H "Content-Type: application/json" \
        -u "$CLOUDSMITH_OWNER:$CLOUDSMITH_API_KEY" \
        -d "{
                \"package_file\": \"$IDENTIFIER\",
                \"distribution\": \"$OS/$TARGET\"
            }" \
        https://api-prd.cloudsmith.io/v1/packages/$CLOUDSMITH_ORG/$CLOUDSMITH_REPO/upload/$EXTENSION/
    echo
done
