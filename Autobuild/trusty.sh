AUTOBUILD_CONFIGURE_EXTRA="${AUTOBUILD_CONFIGURE_EXTRA:-} --nowerror --disable-ffmpeg_static"
DEBDIST=trusty
source Autobuild/debian.sh
