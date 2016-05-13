#!/usr/bin/env bash
#
# Copyright (C) 2008-2014 Tvheadend Foundation CIC
#
# This file is part of Tvheadend
#
# Tvheadend is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Tvheadend is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Tvheadend.  If not, see <http://www.gnu.org/licenses/>.
#
# For more details, including opportunities for alternative licensing,
# please read the LICENSE file.
#
# ############################################################################

# ############################################################################
# 3rd Party library processing support
#
# Provides support routine for the management of 3rd party libraries for
# which we include our own static build to simplify integration.
# ############################################################################

#set -x

# ############################################################################
# Config
# ############################################################################

[ -z "$BINTRAY_REPO" ] && BINTRAY_REPO=tvheadend/misc
[ -z "$ARCH"         ] && ARCH=$(uname -m)
if [ -z "$CODENAME" ]; then
  if [ -f /etc/lsb-release ]; then
    . /etc/lsb-release
    CODENAME=${DISTRIB_CODENAME}
  fi
  # TODO: support more than debian
fi

# Convert x86_64 (ensure uniformity)
[ "${ARCH}" = "x86_64" ] && ARCH=amd64

# ############################################################################
# Config
# ############################################################################


# ############################################################################
# Functions
# ############################################################################

function hash ()
{
  LIB_NAME=$1
  T="
$(cat ${ROOTDIR}/Makefile.${LIB_NAME})
$(grep ^CONFIG_ ${ROOTDIR}/.config.mk)
"
  H=$(echo ${T} | sha1sum | cut -d' ' -f1)
  echo ${H}
}

#
# Attempt to download
#
function download
{
  LIB_NAME=$1
  LIB_HASH=$(hash ${LIB_NAME})
  P=${BUILDDIR}/.${LIB_NAME}-${LIB_HASH}.tgz

  # Cleanup
  rm -f ${BUILDDIR}/.${LIB_NAME}*.tmp

  # Already downloaded
  [ -f ${P} ] && return 0

  # Create directory
  [ ! -d ${BUILDDIR} ] && mkdir -p ${BUILDDIR}

  # Attempt to fetch
  N=staticlib/${CODENAME}/${ARCH}/${LIB_NAME}-${LIB_HASH}.tgz
  URL="https://dl.bintray.com/${BINTRAY_REPO}/${N}"
  echo "DOWNLOAD        ${URL}"
  wget -O ${P}.tmp "${URL}"
  R=$?
  # Don't know why but having weird issue with curl not returning data

  # Failed
  if [ ${R} -ne 0 ]; then
    echo "FAILED TO DOWNLOAD ${URL} (BUT THIS IS NOT A FATAL ERROR! DO NOT REPORT THAT!)"
    rm -f ${P}.tmp
    return ${R}
  fi

  # Move tmp file
  mv ${P}.tmp ${P}

  return ${R}
}

#
# Unpack tarball
#
function unpack
{
  LIB_NAME=$1
  LIB_HASH=$(hash ${LIB_NAME})
  P=${BUILDDIR}/.${LIB_NAME}-${LIB_HASH}.tgz
  U=${BUILDDIR}/.${LIB_NAME}.unpack

  # Not downloaded
  [ ! -f ${P} ] && return 1

  # Already unpacked?
  if [ -f ${U} ]; then
    H=$(cat ${U})
    [ "${LIB_HASH}" = ${H} ] && return 0
  fi

  # Cleanup
  rm -rf ${BUILDDIR}/${LIB_NAME}
  mkdir -p ${BUILDDIR}/${LIB_NAME}
  
  # Unpack
  echo "UNPACK          ${P}"
  tar -C ${BUILDDIR}/${LIB_NAME} -xf ${P}

  # Record
  echo ${LIB_HASH} > ${U}
}

#
# Upload
#
function upload
{
  LIB_NAME=$1; shift
  LIB_FILES=$*
  LIB_HASH=$(hash ${LIB_NAME})
  P=${BUILDDIR}/.${LIB_NAME}-${LIB_HASH}.tgz

  # Can't upload
  [ -z "${BINTRAY_USER}" -o -z "${BINTRAY_PASS}" ] && return 0
  
  # Don't need to upload
  [ -f "${P}" ] && return 0

  # Make relative paths
  LIB_FILES=$(echo "${LIB_FILES}" | sed "s#${BUILDDIR}/${LIB_NAME}/##g")

  # Build temporary file
  echo "PACK            ${P} [${LIB_FILES}]"
  tar -C ${BUILDDIR}/${LIB_NAME} -zcf ${P}.tmp ${LIB_FILES}

  # Upload
  N=staticlib/${CODENAME}/${ARCH}/${LIB_NAME}-${LIB_HASH}.tgz
  URL="https://api.bintray.com/content/${BINTRAY_REPO}/staticlib/${LIB_NAME}-${LIB_HASH}/${N};publish=1"
  echo "UPLOAD          ${URL}"
  curl -s -T ${P}.tmp -u${BINTRAY_USER}:${BINTRAY_PASS} "${URL}" || return 1

  # Done
  mv ${P}.tmp ${P}
}

# Run command
C=$1; shift
case "${C}" in
  hash)
    hash $*
    ;;
  download)
    download $*
    ;;
  unpack)
    unpack $*
    ;;
  upload)
    upload $*
    ;;
esac

# ############################################################################
# Editor Configuration
#
# vim:sts=2:ts=2:sw=2:et
# ############################################################################
