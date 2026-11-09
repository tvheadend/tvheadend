#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Build and install libdvbdab (DAB/DAB+ in DVB streams) for the container
# images: static library, headers and dvbdab.pc under the prefix.
#
#   support/build-libdvbdab.sh <git-ref> [prefix]
#
# libdvbdab's CMakeLists.txt pins /usr/local/gcc-15 when it is the top-level
# project. A small wrapper project builds it as a subproject instead, with
# the compiler of the image (library only, no test tools).

set -eu

ref="${1:?usage: $0 <git-ref> [prefix]}"
prefix="${2:-/usr/local}"
dir="$(mktemp -d)"

# outside the tvheadend tree: git would otherwise inspect its .git
cd "${dir}"
git clone --quiet 'https://github.com/L-S-D/libdvbdab.git' "${dir}/libdvbdab"
git -C "${dir}/libdvbdab" checkout --quiet "${ref}"

cat > "${dir}/CMakeLists.txt" <<EOF
cmake_minimum_required(VERSION 3.20)
project(libdvbdab_container LANGUAGES CXX)
add_subdirectory(libdvbdab)
EOF

cmake -S "${dir}" -B "${dir}/build" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="${prefix}"
cmake --build "${dir}/build" --parallel "$(nproc)"
cmake --install "${dir}/build"

cd /
rm -rf "${dir}"
