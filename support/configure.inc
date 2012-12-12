#!/bin/bash
#
# Generic/Simple configure script
#
# Copyright (c) 2012 Adam Sutton <dev@adamsutton.me.uk>
#

# ###########################################################################
# Defaults
# ###########################################################################

# System setup
[ -z "$PLATFORM" ] && PLATFORM=linux
[ -z "$CPU"      ] && CPU=generic
[ -z "$ARCH"     ] && ARCH=`uname -m`
[ -z "$OSENV"    ] && OSENV=posix
[ -z "$CC"       ] && CC=cc
[ -z "$PYTHON"   ] && PYTHON=python

# Paths
[ -z "$prefix"   ] && prefix=/usr/local
[ -z "$bindir"   ] && bindir=\${prefix}/bin
[ -z "$libdir"   ] && libdir=\${prefix}/lib
[ -z "$datadir"  ] && datadir=\${prefix}/share
[ -z "$mandir"   ] && mandir=\${datadir}/man

# Compiler
[ -z "$CC"       ] && CC=cc
[ -z "$CFLAGS"   ] && CFLAGS=
[ -z "$LDFLAGS"  ] && LDFLAGS=

# Environment
[ -z "$ROOTDIR"  ] && ROOTDIR=$(cd $(dirname $0); pwd)
[ -z "$BUILDDIR" ] && BUILDDIR=$ROOTDIR/build.$PLATFORM
[ -z "$TMPDIR"   ] && TMPDIR=/tmp

# Options/Package Lists
[ -z "$OPTIONS"  ] && OPTIONS=()
[ -z "$PACKAGES" ] && PACKAGES=()

# ###########################################################################
# Utilities
# ###########################################################################

# Output
TAB=" \\033[40G"

# Text conversion
function toupper
{
  echo "$@" | tr abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ
}

function die
{
  [ -z "$1" ] || echo "ERROR: $1"
  exit 1
}

# ###########################################################################
# Enable/Disable
# ###########################################################################

# Enable/Disable option
function _enable
{
  local opt=$1 val=$2 ignore=$3 row= k= v=
  for row in ${OPTIONS[*]}; do
    k=${row%:*}
    [ "$k" == "$opt" ] || continue
    v=${row#*:}
    if [ $v != "$val" ]; then
      OPTIONS=(${OPTIONS[@]//$row/$k:$val})
    fi
    return
  done
  [ -z "$ignore" ] && OPTIONS=(${OPTIONS[@]} $opt:$val)
}

# Enable option
function enable
{
  _enable $1 yes $2
}

# Disable option
function disable
{
  _enable $1 no $2
}

# Enable package
function enable_pkg
{
  local opt=$1 row= k= v=
  for row in ${PACKAGES[*]}; do
    [ "$row" == "$opt" ] && return
  done
  PACKAGES=(${PACKAGES[@]} $opt)
}

# Get enabled state
function _enabled
{
  local opt=$1 row= k=
  for row in ${OPTIONS[*]}; do
    k=${row%:*}
    [ "$k" == "$opt" ] || continue
    echo ${row#*:}
    return
  done
  echo "no"
}

# Check if enabled
function enabled
{
  local val=$(_enabled $1)
  [ "$val" == "yes" ] && return 0 || return 1
}

# Check if disabled
function disabled
{
  local val=$(_enabled $1)
  [ "$val" == "no" ] && return 0 || return 1
}

# Check if enabled (or auto)
function enabled_or_auto
{
  local val=$(_enabled $1)
  [ "$val" == "yes" -o "$val" == "auto" ] && return 0 || return 1
}

# Check if disabled (or auto)
function disabled_or_auto
{
  local val=$(_enabled $1)
  [ "$val" == "no" -o "$val" == "auto" ] && return 0 || return 1
}

# ###########################################################################
# Command Line
# ###########################################################################

# Show help
function show_help
{ 
  local opt= val=
  echo "Usage: $0 [options]"
  echo ""
  echo "Miscellaneous"
  echo -e "  --help${TAB}Print this message"
  echo ""
  echo "Installation Paths"
  echo -e "  --prefix=DIR${TAB}Installation root [$prefix]"
  echo -e "  --bindir=DIR${TAB}Install binaries in DIR [$bindir]"
  echo -e "  --libdir=DIR${TAB}Install libraries in DIR [$libdir]"
  echo -e "  --mandir=DIR${TAB}Install man pages in DIR [$mandir]"
  echo -e "  --datadir=DIR${TAB}Install data files in DIR [$datadir]"
  echo ""
  echo "Compiler/Arch"
  echo -e "  --cc=CC${TAB}Build using compile [$CC]"
  echo -e "  --cpu=CPU${TAB}Build and optimize for specific CPU"
  echo -e "  --arch=ARCH${TAB}Build for architecture [$ARCH]"
  echo -e "  --platform=PLATFORM${TAB}Build for platform [$PLATFORM]"
  echo -e "  --python=PYTHON${TAB}Use python binary [$PYTHON]"
  echo ""
  echo "Options"
  for opt in ${OPTIONS[*]}; do
    val=${opt#*:}
    opt=${opt%:*}
    if [ "$val" == "yes" ]; then
      echo -e "  --disable-${opt}${TAB}Enable $opt [$val]"
    elif [ "$val" == "no" ]; then
      echo -e "  --enable-${opt}${TAB}Enable $opt [$val]"
    else
      echo -e "  --(en|dis)able-${opt}${TAB}Enable $opt [$val]"
    fi
  done
  exit 0
}

# Process command line
function parse_args
{
  local opt= val=
  for opt do
    val=${opt#*=}
    opt=${opt%=*}
    opt=${opt#*--}
    case "$opt" in
      help)
        show_help
        ;;
      *dir|prefix)
        eval "$opt=$val"
        ;;
      cc|arch|cpu|platform|python)
        eval "`toupper $opt`=$val"
        ;;
      enable-*)
        opt=${opt#*-}
        enable $opt 1
        ;;
      disable-*)
        opt=${opt#*-}
        disable $opt 1
        ;;
    esac
  done
}


# ###########################################################################
# Package tests
# ###########################################################################

# Check package
function check_pkg
{
  local pkg=$1; shift
  local ver=$*

  # Version test
  ver=$(echo $ver | sed 's/>=/ --atleast-version /'\
                  | sed 's/<=/ --max-version /'\
                  | sed 's/==/ --exact-version /')

  echo -ne "checking for pkg $pkg $ver ...${TAB}"

  # Check for package
  if pkg-config $pkg $ver; then
    echo "ok"
    enable_pkg $pkg
  else
    echo "fail"
    return 1
  fi
}

# ###########################################################################
# Compiler Tests
# ###########################################################################

# Check compiler
function check_cc
{
  local hdr=$1
  local opt=$2
  cat >$TMPDIR/$$.c <<EOF
$hdr
int main() {
 return 0;
}
EOF
  $CC $CFLAGS $LDFLAGS $TMPDIR/$$.c -o $TMPDIR/$$.bin $opt &> /dev/null
  RET=$?
  rm -f $TMPDIR/$$.{c,bin}
  return $RET
}

# Check compiler header
function check_cc_header
{
  local hdr=$1
  local nam=$2
  [ -z "$nam" ] && nam=$hdr

  echo -ne "checking for cc $hdr.h ...${TAB}"

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
function check_cc_snippet
{
  local nam=$1
  local snp=$2
  local opt=$3

  echo -ne "checking for cc $nam ...${TAB}"

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
function check_cc_option
{
  local opt=$1
  local nam=$2
  [ -z "$nam" ] && nam=$opt
  
  echo -ne "checking for cc -m$opt ...${TAB}"

  # Enable if supported
  if check_cc "" -m${opt}; then
    echo "ok"
    enable $nam
  else
    echo "fail"
    return 1
  fi
}

# Check compiler library
function check_cc_lib
{
  local opt=$1
  local nam=$2
  [ -z "$nam" ] && nam=$opt
  
  echo -ne "checking for cc -l$opt ...${TAB}"

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
function check_py
{
  local hdr=$1
  cat >$TMPDIR/$$.py <<EOF
$hdr
EOF
  $PYTHON $TMPDIR/$$.py &> /dev/null
  RET=$?
  rm -f $TMPDIR/$$.py
  return $RET
}

# Check python import
function check_py_import
{
  local hdr=$1
  local nam=$2
  [ -z "$nam" ] && nam=py_${hdr}

  echo -ne "checking for py module $hdr ...${TAB}"

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

function check_bin
{
  local bin=$1
  local nam=$2
  [ -z "$nam" ] && nam=bin_${bin}
  echo -ne "checking for $bin ...${TAB}"

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
function print_config
{
  local pkg=

  # Compiler settings
  echo ""
  echo "Compiler:"
  echo -e "  Using C compiler:${TAB}${CC}"
  echo -e "  Build for arch:${TAB}${ARCH}"
  echo ""

  echo "Binaries:"
  echo -e "  Using PYTHON:${TAB}${PYTHON}"
  echo ""

  # Options
  echo "Options:"
  for opt in ${OPTIONS[*]}; do
    k=${opt%:*}
    v=${opt#*:}
    if [ "$v" == "yes" ]; then
      echo -e "  $k:${TAB}yes"
    else
      echo -e "  $k:${TAB}no"
    fi
  done
  echo ""

  # Packages
  echo "Packages:"
  for pkg in ${PACKAGES[*]}; do
    echo -e "  ${pkg}:${TAB}$(pkg-config --modversion $pkg)"
  done
  echo ""
  
  # Installation
  echo "Installation paths:"
  echo -e "  Prefix:${TAB}${prefix}"
  echo -e "  Binaries:${TAB}${bindir}"
  echo -e "  Libraries:${TAB}${libdir}"
  echo -e "  Data files:${TAB}${datadir}"
  echo -e "  Man pages:${TAB}${mandir}"
  echo ""
}

# Write configuration
function write_config
{
  local pkg= opt= k= v=

  # Create build directory
	mkdir -p ${BUILDDIR}
	BUILDDIR=`cd ${BUILDDIR} && pwd`

  # Create make include
	CONFIG_MK=${ROOTDIR}/.config.mk
  cat > ${CONFIG_MK} <<EOF
# Automatically generated by configure - DO NOT EDIT!
ROOTDIR  ?= ${ROOTDIR}
BUILDDIR ?= ${ROOTDIR}/build.${PLATFORM}
OSENV    ?= ${OSENV}
ARCH     ?= ${ARCH}
ifeq (\$(origin CC),default)
CC        = ${CC}
endif
PYTHON   ?= ${PYTHON}
CFLAGS   += ${CFLAGS}
LDFLAGS  += ${LDFLAGS}
prefix    = ${prefix}
bindir    = ${bindir}
mandir    = ${mandir}
datadir   = ${datadir}
libdir    = ${libdir}
EOF

  # Create C include
  CONFIG_H=${BUILDDIR}/config.h
  cat > ${CONFIG_H} <<EOF
// Automatically generated by configure - DO NOT EDIT!
EOF

  # Add package config
  for pkg in ${PACKAGES[*]}; do
    cat >>${CONFIG_MK} <<EOF
LDFLAGS += $(pkg-config --libs $pkg)
CFLAGS  += $(pkg-config --cflags $pkg)
EOF
  done

  # Add configuration
  for row in ${OPTIONS[*]}; do
    k=$(toupper ${row%:*})
    v=${row#*:}
    if [ "$v" == "yes" ]; then
      cat >>${CONFIG_H} <<EOF
#define ENABLE_${k} 1
#define CONFIG_${k} 1
EOF
      cat >>${CONFIG_MK} <<EOF
CONFIG_${k} = yes
EOF
    else
      cat >>${CONFIG_H} <<EOF
#define ENABLE_${k} 0
EOF
      cat >>${CONFIG_MK} <<EOF
CONFIG_${k} = no
EOF
    fi
  done
}

