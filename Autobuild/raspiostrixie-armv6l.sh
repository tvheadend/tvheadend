AUTOBUILD_CONFIGURE_EXTRA="${AUTOBUILD_CONFIGURE_EXTRA:-} --disable-libx265_static --disable-libx265"
source Autobuild/armv6l.sh
EXTRA_X265_CMAKE_OPTS="${EXTRA_X265_CMAKE_OPTS:-} -DCROSS_COMPILE_ARM=0"
source Autobuild/raspiostrixie.sh
