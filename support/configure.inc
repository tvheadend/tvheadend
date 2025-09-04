#!/bin/bash
#
# Generic/Simple configure script
#
# Copyright (c) 2012 Adam Sutton <dev@adamsutton.me.uk>
#

# ###########################################################################
# Defaults
# ###########################################################################

CONFIGURE_ARGS="$*"

# Compiler
[ -z "$CC"       ] && CC=cc
[ -z "$CFLAGS"   ] && CFLAGS=
[ -z "$LDFLAGS"  ] && LDFLAGS=

# System setup
[ -z "$PLATFORM" ] && PLATFORM=$(uname -s | tr "[:upper:]" "[:lower:]")
[ -z "$CPU"      ] && CPU=generic
[ -z "$ARCH"     ] && ARCH=$($CC -dumpmachine | cut -d '-' -f 1)
[ -z "$ARCH"     ] && ARCH=$(uname -m)
[ -z "$OSENV"    ] && OSENV=posix
[ -z "$PYTHON"   ] && PYTHON=python
[ -z "$GZIPCMD"  ] && GZIPCMD=gzip
[ -z "$BZIP2"    ] && BZIP2=bzip2
[ -z "$XGETTEXT" ] && XGETTEXT=xgettext
[ -z "$MSGMERGE" ] && MSGMERGE=msgmerge

# Paths
[ -z "$prefix"   ] && prefix=/usr/local
[ -z "$bindir"   ] && bindir=\${prefix}/bin
[ -z "$libdir"   ] && libdir=\${prefix}/lib
[ -z "$datadir"  ] && datadir=\${prefix}/share
[ -z "$mandir"   ] && mandir=\${datadir}/man

# Environment
[ -z "$ROOTDIR"  ] && ROOTDIR=$(cd "$(dirname "$0")"; pwd)
[ -z "$BUILDDIR" ] && BUILDDIR=$ROOTDIR/build.$PLATFORM
[ -z "$TMPDIR"   ] && TMPDIR=/tmp

# Options/Package Lists
[ -z "$OPTIONS"  ] && OPTIONS=()
[ -z "$PACKAGES" ] && PACKAGES=()

# ###########################################################################
# Utilities
# ###########################################################################

# Output
TAB="  %-50s"

# Text conversion
toupper ()
{
  echo "$@" | tr abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ
}

# Terminate
die ()
{
  echo >&2 "ERROR: $@"
  exit 1
}

# ###########################################################################
# Enable/Disable
# ###########################################################################

# Enable/Disable option
_enable ()
{
  local opt=$1 val=$2 ignore=$3 row= k= v=
  for row in ${OPTIONS[*]}; do
    k=${row%:*}
    [ "$k" = "$opt" ] || continue
    v=${row#*:}
    if [ $v != "$val" ]; then
      OPTIONS=(${OPTIONS[@]//$row/$k:$val})
    fi
    return
  done
  [ -z "$ignore" ] && OPTIONS=(${OPTIONS[@]} $opt:$val)
}

# Enable option
enable ()
{
  _enable $1 yes $2
}

# Disable option
disable ()
{
  _enable $1 no $2
}

# Enable package
enable_pkg ()
{
  local opt=$1 row= k= v=
  for row in ${PACKAGES[*]}; do
    [ "$row" = "$opt" ] && return
  done
  PACKAGES=(${PACKAGES[@]} $opt)
}

# Get enabled state
_enabled ()
{
  local opt=$1 row= k=
  for row in ${OPTIONS[*]}; do
    k=${row%:*}
    [ "$k" = "$opt" ] || continue
    echo ${row#*:}
    return
  done
  echo "no"
}

# Check if enabled
enabled ()
{
  local val=$(_enabled $1)
  [ "$val" = "yes" ] && return 0 || return 1
}

# Check if disabled
disabled ()
{
  local val=$(_enabled $1)
  [ "$val" = "no" ] && return 0 || return 1
}

# Check if enabled (or auto)
enabled_or_auto ()
{
  local val=$(_enabled $1)
  [ "$val" = "yes" -o "$val" = "auto" ] && return 0 || return 1
}

# Check if disabled (or auto)
disabled_or_auto ()
{
  local val=$(_enabled $1)
  [ "$val" = "no" -o "$val" = "auto" ] && return 0 || return 1
}

# ###########################################################################
# Command Line
# ###########################################################################

# Show help
show_help ()
{ 
  local opt= val= fmt="%-30s"
  echo "Usage: $0 [options]"
  echo ""
  echo "Miscellaneous"
  printf "  $fmt Print this message\n" "--help"
  echo ""
  echo "Installation Paths"
  printf "  $fmt Installation root [$prefix]\n" "--prefix=DIR$"
  printf "  $fmt Install binaries in DIR [$bindir]\n" "--bindir=DIR"
  printf "  $fmt Install libraries in DIR [$libdir]\n" "--libdir=DIR"
  printf "  $fmt Install man pages in DIR [$mandir]\n" "--mandir=DIR"
  printf "  $fmt Install data files in DIR [$datadir]\n" "--datadir=DIR"
  echo ""
  echo "Compiler/Arch"
  printf "  $fmt Build using compiler [$CC]\n" "--cc=CC"
  printf "  $fmt Build using C flags\n" "--cflags=CFLAGS"
  printf "  $fmt Build using LD flags\n" "--ldflags=LDFLAGS"
  printf "  $fmt Build and optimize for specific CPU\n" "--cpu=CPU"
  printf "  $fmt Build for architecture [$ARCH]\n" "--arch=ARCH"
  printf "  $fmt Build for platform [$PLATFORM]\n" "--platform=PLATFORM"
  printf "  $fmt Build without -Werror CFLAGS\n" "--nowerror"
  printf "  $fmt Use python binary [$PYTHON]\n" "--python=PYTHON"
  echo ""
  echo "Options"
  for opt in ${OPTIONS[*]}; do
    val=${opt#*:}
    opt=${opt%:*}
    if [ "$val" = "yes" ]; then
      printf "  $fmt Disable ${opt}\n" "--disable-${opt}"
    elif [ "$val" = "no" ]; then
      printf "  $fmt Enable ${opt}\n" "--enable-${opt}"
    else
      printf "  $fmt Disable ${opt}\n" "--disable-${opt}"
      printf "  $fmt Enable ${opt}\n" "--enable-${opt}"
    fi
  done
  exit 0
}

# ###########################################################################
# Package tests
# ###########################################################################

# Check package
check_pkg ()
{
  local pkg=$1; shift
  local ver=$*

  # Version test
  cver=$(echo $ver | sed 's/>=/ --atleast-version /'\
                  | sed 's/<=/ --max-version /'\
                  | sed 's/==/ --exact-version /')

  printf "$TAB" "checking for pkg $pkg $ver ..."

  dver=$(${PKG_CONFIG} --modversion $pkg 2> /dev/null)
  test -z "$dver" && dver="<none>"

  # Check for package
  if ${PKG_CONFIG} $pkg $cver; then
    echo "ok (detected ${dver})"
    enable_pkg $pkg
  else
    echo "fail (detected ${dver})"
    return 1
  fi
}

# ###########################################################################
# Compiler Tests
# ###########################################################################

# Check compiler
check_cc ()
{
  local hdr=$1
  local opt=$2
  cat >$TMPDIR/$$.c <<EOF
$hdr
int main() {
#ifdef TEST
  return TEST();
#else
  return 0;
#endif
}
EOF
  $CC $CFLAGS $LDFLAGS $TMPDIR/$$.c -o $TMPDIR/$$.bin $opt > /dev/null 2>&1
  RET=$?
  rm -f $TMPDIR/$$.{c,bin}
  return $RET
}

# Check compiler header
check_cc_header ()
{
  local hdr=$1
  local nam=$2
  [ -z "$nam" ] && nam=$hdr

  printf "$TAB" "checking for cc $hdr.h ..."

  # Enable if supported
  if check_cc "#include <$1.h>"; then
    echo "ok"
    enable $nam
  else
    echo "fail"
    return 1
  fi
}

# Check some compiler snippet
check_cc_snippet ()
{
  local nam=$1
  local snp=$2
  local opt=$3

  printf "$TAB" "checking for cc $nam ..."

  # Check if supported
  if check_cc "$snp" "$opt"; then
    echo "ok"
    enable $nam
  else
    echo "fail"
    return 1
  fi
}

# Check compiler option
check_cc_option ()
{
  local opt=$1
  local nam=$2
  [ -z "$nam" ] && nam=$opt
  
  printf "$TAB" "checking for cc -m$opt ..."

  # Enable if supported
  if check_cc "" -m${opt}; then
    echo "ok"
    enable $nam
  else
    echo "fail"
    return 1
  fi
}

# Check compiler option
check_cc_optionW ()
{
  local opt=$1
  local nam=$2
  [ -z "$nam" ] && nam=$opt
  nam=$(echo "W_$nam" | sed -e 's/-/_/')

  printf "$TAB" "checking for cc -W$opt ..."

  # Enable if supported
  if check_cc "" -W${opt}; then
    echo "ok"
    enable $nam
  else
    echo "fail"
    return 1
  fi
}

# Check compiler option
check_cc_optionf ()
{
  local opt=$1
  local nam=$2
  [ -z "$nam" ] && nam=$opt
  nam=$(echo "f_$nam" | sed -e 's/[-=]/_/g')

  printf "$TAB" "checking for cc -f$opt ..."

  # Enable if supported
  if check_cc "" -f${opt}; then
    echo "ok"
    enable $nam
  else
    echo "fail"
    return 1
  fi
}


# Check compiler library
check_cc_lib ()
{
  local opt=$1
  local nam=$2
  [ -z "$nam" ] && nam=$opt
  
  printf "$TAB" "checking for cc -l$opt ..."

  # Enable if supported
  if check_cc "" -l${opt}; then
    echo "ok"
    enable $nam
  else
    echo "fail"
    return 1
  fi
}

# ###########################################################################
# Python tests
# ###########################################################################

# Check python
check_py ()
{
  local hdr=$1
  cat >$TMPDIR/$$.py <<EOF
$hdr
EOF
  $PYTHON $TMPDIR/$$.py > /dev/null 2>&1
  RET=$?
  rm -f $TMPDIR/$$.py
  return $RET
}

# Check python import
check_py_import ()
{
  local hdr=$1
  local nam=$2
  [ -z "$nam" ] && nam=py_${hdr}

  printf "$TAB" "checking for py module $hdr ..."

  # Enable if supported
  if check_py "import $hdr"; then
    echo "ok"
    enable $nam
  else
    echo "fail"
    return 1
  fi
}

# ###########################################################################
# Binary checks
# ###########################################################################

check_bin ()
{
  local bin=$1
  local nam=$2
  [ -z "$nam" ] && nam=bin_${bin}
  printf "$TAB" "checking for $bin ..."

  if which $bin &> /dev/null; then
    echo "ok"
    enable $nam
  else
    echo "fail"
    return 1
  fi
}

# ###########################################################################
# Config output
# ###########################################################################

# Print config
print_config ()
{
  local pkg= fmt="  %-40s %s\n"

  # Compiler settings
  echo ""
  echo "Compiler:"
  printf "$fmt" "Using C compiler:" "${CC}"
  if [ "${CFLAGS}" != "" ]; then
     printf "$fmt" "Using C flags:" "${CFLAGS}"
  fi
  if [ "${LDFLAGS}" != "" ]; then
     printf "$fmt" "Using LD flags:" "${LDFLAGS}"
  fi
  printf "$fmt" "Build for arch:" "${ARCH}"
  echo ""

  echo "Binaries:"
  printf "$fmt" "Using PYTHON:" "${PYTHON}"
  printf "$fmt" "Using GZIP:" "${GZIPCMD}"
  printf "$fmt" "Using BZIP2:" "${BZIP2}"
  echo ""

  # Options
  echo "Options:"
  for opt in ${OPTIONS[*]}; do
    k=${opt%:*}
    v=${opt#*:}
    if [ "$v" = "yes" ]; then
      printf "$fmt" "$k" "yes"
    else
      printf "$fmt" "$k" "no"
    fi
  done
  echo ""

  # Packages
  echo "Packages:"
  for pkg in ${PACKAGES[*]}; do
    printf "$fmt" "${pkg}" "$(${PKG_CONFIG} --modversion $pkg)"
  done
  echo ""
  
  # Installation
  echo "Installation paths:"
  printf "$fmt" "Prefix:" "${prefix}"
  printf "$fmt" "Binaries:" "${bindir}"
  printf "$fmt" "Libraries:" "${libdir}"
  printf "$fmt" "Data files:" "${datadir}"
  printf "$fmt" "Man pages:" "${mandir}"
  echo ""
}

# Write configuration
write_config ()
{
  local pkg= opt= k= v=

  # Create build directory
  mkdir -p "${BUILDDIR}"
  BUILDDIR=$(cd "${BUILDDIR}" && pwd)

  # Create make include
  CONFIG_MK=${ROOTDIR}/.config.mk
  cat > "${CONFIG_MK}" <<EOF
# Automatically generated by configure - DO NOT EDIT!
CONFIGURE_ARGS = ${CONFIGURE_ARGS}
ROOTDIR  ?= ${ROOTDIR}
BUILDDIR ?= ${BUILDDIR}
PLATFORM ?= ${PLATFORM}
OSENV    ?= ${OSENV}
ARCH     ?= ${ARCH}
CPU      ?= ${CPU}
COMPILER ?= ${COMPILER}
ifeq (\$(origin CC),default)
CC        = ${CC}
endif
PYTHON   ?= ${PYTHON}
GZIPCMD  ?= ${GZIPCMD}
BZIP2    ?= ${BZIP2}
XGETTEXT ?= ${XGETTEXT}
MSGMERGE ?= ${MSGMERGE}
PKG_CONFIG ?= ${PKG_CONFIG}
CFLAGS   += ${CFLAGS}
LDFLAGS  += ${LDFLAGS}
prefix    = ${prefix}
bindir    = ${bindir}
mandir    = ${mandir}
datadir   = ${datadir}
libdir    = ${libdir}
EOF

  # no -Werror
  if test -n "${NOWERROR}"; then
    echo "CFLAGS_NO_WERROR = yes" >> "${CONFIG_MK}"
  fi

  # Create C include
  CONFIG_H="${BUILDDIR}/build.h"
  cat > "${CONFIG_H}" <<EOF
// Automatically generated by configure - DO NOT EDIT!
#pragma once
EOF

  # Create Platform defines
  cat >> ${CONFIG_H} <<EOF
#define PLATFORM_$(toupper ${PLATFORM}) 1
EOF

  # Add package config
  for pkg in ${PACKAGES[*]}; do
    cat >>"${CONFIG_MK}" <<EOF
LDFLAGS += $(${PKG_CONFIG} --libs $pkg)
CFLAGS  += $(${PKG_CONFIG} --cflags $pkg)
EOF
  done

  # Add configuration
  for row in ${OPTIONS[*]}; do
    k=$(toupper ${row%:*})
    v=${row#*:}
    if [ "$v" = "yes" ]; then
      cat >>"${CONFIG_H}" <<EOF
#define ENABLE_${k} 1
#define CONFIG_${k} 1
EOF
      cat >>"${CONFIG_MK}" <<EOF
CONFIG_${k} = yes
EOF
    else
      cat >>"${CONFIG_H}" <<EOF
#define ENABLE_${k} 0
EOF
      cat >>"${CONFIG_MK}" <<EOF
CONFIG_${k} = no
EOF
    fi
  done

  # Add build config string declaration
  cat >> "${CONFIG_H}" <<EOF
#define DESCRAMBLER_MAX_KEYS 8
EOF

  # Add build config string declaration
  cat >> "${CONFIG_H}" <<EOF
extern const char* build_config_str;
extern const char* build_timestamp;
EOF

  # Create C source
  CONFIG_C="${BUILDDIR}/build.c"
  cat > "${CONFIG_C}" <<EOF
// Automatically generated by configure - DO NOT EDIT!

#include "build.h"

const char* build_config_str =
EOF

  # Add configure command
  echo "  \"Configure arguments:\\n\"" >> "${CONFIG_C}"
  echo "  ${CONFIGURE_ARGS}" | sed -e 's/"/\\"/g' | sed -e 's/.*/  "\0" "\\n"/g' >> "${CONFIG_C}"

  # Add build config string
  print_config | sed -e 's/"/\\"/g' | sed -e 's/.*/  "\0" "\\n"/g' >> "${CONFIG_C}"
  echo '  "";' >> "${CONFIG_C}"
}

