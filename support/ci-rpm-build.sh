#!/bin/bash
#
# Shared CI build for the rpm based distros (Fedora and the Enterprise
# Linux clones): install the build dependencies, including RPM Fusion
# for libfdk-aac, then configure and build the packages into rpm/RPMS/.
# Used by build-ci.yml and build-cloudsmith.yml on every fedora:* and
# almalinux:* leg, on both x86_64 and aarch64.
#
# Usage: support/ci-rpm-build.sh <releasever>
#   <releasever>  release as used in the container tag and the RPM Fusion
#                 package name: "44", "rawhide", "9", ...

set -e

releasever="$1"
if [ -z "$releasever" ]; then
    echo "Usage: $0 <releasever>" >&2
    exit 2
fi

# The EL clones keep part of the build dependencies in the CRB
# repository, and RPM Fusion publishes their release package under
# free/el/ instead of free/fedora/
. /etc/os-release
if [ "$ID" = "fedora" ]; then
    fusion=fedora
else
    fusion=el
    dnf install -y dnf-plugins-core
    dnf config-manager --set-enabled crb
fi

dnf install -y "https://download1.rpmfusion.org/free/$fusion/rpmfusion-free-release-$releasever.noarch.rpm"
dnf install -y gcc-c++ which rpm-build rpmdevtools git make cmake gettext-devel dbus-devel avahi-devel openssl-devel zlib-devel libdvbcsa-devel wget bzip2 uriparser-devel pcre2-devel python python-requests ccache systemd-units systemd-devel
# not packaged on all releases
dnf install -y openssl-devel-engine || true

mkdir -p .staticlib
export STATICLIB_CACHE="$PWD/.staticlib"
export VUE_FILE_CACHE="$PWD/.vuecache"
# also forwarded by rpm/Makefile into the %configure run inside rpmbuild
export TVH_EXTRA_CONFIGURE="--enable-vue_cache --disable-vue_build"

./configure --disable-dvbscan --disable-libfdkaac_static --disable-ffmpeg_static --disable-hdhomerun_static --disable-libopus_static --disable-libtheora_static --disable-libvorbis_static --disable-libvpx_static --disable-libx264_static --disable-libx265_static --enable-libfdkaac --enable-hdhomerun_client --enable-libsystemd_daemon --python=/usr/bin/python3 $TVH_EXTRA_CONFIGURE

make -C rpm build -j"$(nproc)" || {
    echo "PARALLEL BUILD FAILED, DOING SINGLE THREADED BUILD"
    make -C rpm build
}
