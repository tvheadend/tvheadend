CHANGELOG=debian/changelog
NOW=`date -R`
VER=`$(dirname $0)/support/version`
[ -z "${DEBDIST:-}" ] && DEBDIST=""

BUILD_DEPS=`awk '\
BEGIN {cnt = 1;}
/^Build-Depends:/ {
  split($0, line, ":");
  split(line[2], deps, ",");
  for (i in deps) {
    d = deps[i];
    sub(/^ */, "", d);
    sub(/ *$/, "", d);
    n = split(d, tokens, " ");
    packages[cnt++] = tokens[1];
  }
}
END {
  out = "";
  for(i = 1; i <= cnt; i++) {
    out = out packages[i] " ";
  }
  print out;
}' debian/control`

build() 
{
    $(dirname $0)/support/changelog "$CHANGELOG" "$DEBDIST" "$VER"
    
    export JOBSARGS
    export JARGS
    export AUTOBUILD_CONFIGURE_EXTRA

    if ccache=$(which ccache); then
        echo "Using ccache"
        ccache -s
        USE_CCACHE="--ccache"
    else
        USE_CCACHE=""
    fi

    export USE_CCACHE

    dpkg-buildpackage -b -us -uc
}

clean() 
{
    for a in ../tvheadend*${VER}*.deb; do
        rm -f "$a"
    done

    for a in ../tvheadend*${VER}*.changes; do
        rm -f "$a"
    done

    rm -f ${CHANGELOG}
    dh_clean
}

deps() 
{
    if [[ $EUID -ne 0 ]]; then
        echo "Build dependencies must be installed as root"
        exit 1
    fi
    apt-get -y install ${BUILD_DEPS}
}

buildenv() 
{
    echo $BUILD_DEPS | shasum | awk '{print $1}'
}

eval $OP
