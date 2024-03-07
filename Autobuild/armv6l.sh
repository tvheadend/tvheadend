AUTOBUILD_CONFIGURE_EXTRA="${AUTOBUILD_CONFIGURE_EXTRA:-} --arch=armel"
sed -i -E '/-armv6l$/! s/^Package: (.*)(-armv6l)*$/Package: \1-armv6l/g' debian/control
sed -i -E 's/^Depends: tvheadend/Depends: tvheadend-armv6l/g' debian/control
sed -i -E '/armv6l/! s/package=tvheadend([^ ]*)/package=tvheadend\1-armv6l/g' debian/rules