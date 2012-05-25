CHANGELOG=debian/changelog
NOW=`date -R`
VER=`git describe | sed "s/\([0-9]*\)\.\([0-9]*\)-\([0-9]*\)-.*/\1.\2.\3/"`
echo >${CHANGELOG} "tvheadend (${VER}) unstable; urgency=low"
echo >>${CHANGELOG}
echo >>${CHANGELOG} "  * The full changelog can be found at "
echo >>${CHANGELOG} "    http://www.lonelycoder.com/tvheadend/download"
echo >>${CHANGELOG}
echo >>${CHANGELOG} " -- Andreas Öman <andreas@lonelycoder.com>  ${NOW}"
cat ${CHANGELOG}
export JOBSARGS
export JARGS
dpkg-buildpackage -b -us -uc

for a in ../tvheadend*${VER}*.deb; do
    artifact $a deb application/x-deb `basename $a`
    rm -f $a
done

for a in ../tvheadend*${VER}*.changes; do
    artifact $a changes text/plain `basename $a`
    rm -f $a
done

rm -f ${CHANGELOG}
dh_clean
