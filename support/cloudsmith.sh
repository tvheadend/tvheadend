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
PKGMGR="apt"

YELLOW='\033[1;33m'
GREEN='\033[1;32m'
CYAN='\033[1;36m'
RED='\033[1;31m'
NC='\033[0m'

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
    echo -e "${RED}No file specified${NC}"
    exit 1
fi

if [[ -z $TARGET ]]; then
    source Autobuild/identify-os.sh
fi

if [ -n "$OSPREFIX" ]; then
    DISTRO=$OSPREFIX
fi

case $DISTRO in
    debian|raspbian*)
        case $CODENAME in
            bookworm|bullseye|buster|sid|stretch|jessie|trixie|forky)
                OS="debian"
                TARGET=$CODENAME;;
            *)
                echo -e "${RED}Debian release $CODENAME could not be recognized${NC}"
                exit 1;;
        esac;;
    ubuntu)
        case $CODENAME in
            trusty|xenial|bionic|focal|impish|jammy|kinetic|lunar|mantic|noble|plucky|questing)
                OS="ubuntu"
                TARGET=$CODENAME;;
            *)
                echo -e "${RED}Ubuntu release $CODENAME could not be recognized${NC}"
                exit 1;;
        esac;;
    raspios*)
        OS="raspbian"
        TARGET=$CODENAME;;
    rhel)
        PKGMGR="dnf"
        OS="rhel"
        TARGET="${VERSION%%.*}";;
    fedora)
        case $VERSION in
            37|38|39|40|41|42)
                PKGMGR="dnf"
                OS="fedora"
                TARGET=$VERSION;;
            43|44)
                echo -e "${YELLOW}Fedora 43 (current rawhide) is not (yet) supported by Cloudsmith${NC}"
                exit 0;;
            *)
                echo -e "${RED}Fedora release $VERSION could not be recognized${NC}" 
                exit 1;;
        esac;;
    *)
        echo -e "${RED}OS $DISTRO $TARGET could not be recognized${NC}"
        exit 1;;
esac

echo "OS Detected as $DISTRO $TARGET"

export LC_ALL=C.UTF-8
export LANG=C.UTF-8

if ! command -v curl &> /dev/null; then
    echo "curl not found, installing..."
    if [ "$PKGMGR" == "dnf" ]; then
        dnf install -y curl
    else
        export DEBIAN_FRONTEND=noninteractive
        apt update
        apt install -y curl
    fi
else
    echo "curl is already installed."
fi

FILEARRAY=($FILE)
for package in "${FILEARRAY[@]}"; do
    EXTENSION=${package##*.}
    PKGBASENAME=$(basename $package)
    # dryrun exit is performed as late as possible to catch any earlier potential issues
    if [ $DRYRUN = "1" ]; then
        echo -e "\n${YELLOW}DRYRUN MODE: Skipped pushing $OS $TARGET package $package${NC}"
        continue
    fi
    echo -e "\n${GREEN}Publishing $OS $TARGET package $package${NC}"
    # upload package to file upload endpoint
    echo -e "\n${CYAN}Uploading file${NC}"
    curlUpload=$(
        curl \
            -w "\n%{http_code}" \
            --upload-file $package \
            -u "$CLOUDSMITH_OWNER:$CLOUDSMITH_API_KEY" \
            -H "Content-Sha256: $(sha256sum "$package" | cut -f 1 -d ' ')" \
            https://upload.cloudsmith.io/$CLOUDSMITH_ORG/$CLOUDSMITH_REPO/$PKGBASENAME \
    )
    uploadHTTPCode=$(tail -n1 <<< "$curlUpload")
    uploadResponse=$(sed '$ d' <<< "$curlUpload")
    if [ "$uploadHTTPCode" != "200" ]; then
        echo -e "${RED}Error: received bad package upload response code${NC}"
        echo -e "${CYAN}HTTP code:${NC}"
        echo "$uploadHTTPCode"
        echo -e "${CYAN}Response:${NC}"
        echo "$uploadResponse"
        exit 1
    fi
    IDENTIFIER=$(echo $curlUpload | cut -f 4 -d '"')
    # finalize by POSTing to the create package endpoint
    echo -e "\n${CYAN}Creating package${NC}"
    curlPkgCreate=$(
        curl \
            -X POST \
            -w "\n%{http_code}" \
            -H "Content-Type: application/json" \
            -u "$CLOUDSMITH_OWNER:$CLOUDSMITH_API_KEY" \
            -d "{
                    \"package_file\": \"$IDENTIFIER\",
                    \"distribution\": \"$OS/$TARGET\"
                }" \
            https://api-prd.cloudsmith.io/v1/packages/$CLOUDSMITH_ORG/$CLOUDSMITH_REPO/upload/$EXTENSION/
    )
    pkgCreateHTTPCode=$(tail -n1 <<< "$curlPkgCreate")
    pkgCreateResponse=$(sed '$ d' <<< "$curlPkgCreate")
    if [ "$pkgCreateHTTPCode" != "201" ]; then
        echo -e "${RED}Error: received bad package create response code${NC}"
        echo -e "${CYAN}HTTP code:${NC}"
        echo "$pkgCreateHTTPCode"
        echo -e "${CYAN}Response:${NC}"
        echo "$pkgCreateResponse"
        exit 1
    fi
done
