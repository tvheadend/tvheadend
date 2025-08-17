# Determine OS platform
UNAME=$(uname | tr "[:upper:]" "[:lower:]")
# If Linux, try to determine specific distribution
if [ "$UNAME" == "linux" ]; then
    # If available, use LSB to identify distribution
    if [ -x "$(command -v lsb_release)" ]; then
        export DISTRO=$(lsb_release -i | cut -d: -f2 | sed 's/^[[:space:]]*//' | tr "[:upper:]" "[:lower:]")
        export CODENAME=$(lsb_release -c | cut -d: -f2 | sed s/'^\t'//)
        echo "OS identified using lsb_release command"
    elif [ -f /etc/lsb-release ]; then
         export DISTRO=$(awk -F= '/^DISTRIB_ID/{print $2}' /etc/lsb-release | tr "[:upper:]" "[:lower:]")
         export CODENAME=$(awk -F= '/^DISTRIB_CODENAME/{print $2}' /etc/lsb-release)
        echo "OS identified using lsb_release file"
    # Otherwise, use release info file
    elif [ -f /etc/os-release ]; then
        # Detect EL based
        ID_LIKE=$(awk -F= '/^ID_LIKE=/{print $2}' /etc/os-release | tr -d '"')
        if [[ "$ID_LIKE" == *rhel* ]]; then
            export DISTRO="rhel"
        else # default to ID
            export DISTRO=$(awk -F= '/^ID=/{print $2}' /etc/os-release | tr -d '"')
        fi
        export CODENAME=$(awk -F= '/^VERSION_CODENAME=/{print $2}' /etc/os-release)
        export VERSION=$(awk -F= '/^VERSION_ID=/{print $2}' /etc/os-release | tr -d '"')
        echo "OS identified using os-release file"
    else
        export DISTRO=$(ls -d /etc/[A-Za-z]*[_-][rv]e[lr]* | grep -v "lsb" | cut -d'/' -f3 | cut -d'-' -f1 | cut -d'_' -f1)
        echo "OS identified using fallback"
    fi
fi
# For everything else (or if above failed), just use generic identifier
[ "$DISTRO" == "" ] && export DISTRO=$UNAME
unset UNAME
export ARCH=$(uname -m)
