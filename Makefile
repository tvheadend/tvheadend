#
#  Tvheadend streaming server.
#  Copyright (C) 2007-2009 Andreas Öman
#  Copyright (C) 2012-2015 Adam Sutton
#  Copyright (C) 2012-2018 Jaroslav Kysela
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

#
# Configuration
#

include $(dir $(lastword $(MAKEFILE_LIST))).config.mk
include $(dir $(lastword $(MAKEFILE_LIST)))Makefile.common
PROG      := $(BUILDDIR)/tvheadend
LANGUAGES ?= $(LANGUAGES_ALL)

#
# Common compiler flags
#

# https://wiki.debian.org/Hardening
CFLAGS  += -g
ifeq ($(CONFIG_CCDEBUG),yes)
CFLAGS  += -O0
else
CFLAGS  += -O2 -D_FORTIFY_SOURCE=2
endif
ifeq ($(CONFIG_PIE),yes)
CFLAGS  += -fPIE
else
CFLAGS  += -fPIC
endif
ifeq ($(CONFIG_W_UNUSED_RESULT),yes)
CFLAGS  += -Wunused-result
endif
ifneq ($(CFLAGS_NO_WERROR),yes)
CFLAGS  += -Werror
endif
CFLAGS  += -Wall -Wwrite-strings -Wno-deprecated-declarations
CFLAGS  += -Wmissing-prototypes
CFLAGS  += -fms-extensions -funsigned-char -fno-strict-aliasing
ifeq ($(COMPILER), gcc)
CFLAGS  += -Wno-stringop-truncation -Wno-stringop-overflow
endif
CFLAGS  += -D_FILE_OFFSET_BITS=64
CFLAGS  += -I${BUILDDIR} -I${ROOTDIR}/src -I${ROOTDIR}
ifeq ($(CONFIG_ANDROID),yes)
LDFLAGS += -ldl -lm
else
LDFLAGS += -ldl -lpthread -lm
endif
ifeq ($(CONFIG_PIE),yes)
LDFLAGS += -pie
endif
LDFLAGS += -Wl,-z,now
ifeq ($(CONFIG_LIBICONV),yes)
LDFLAGS += -liconv
endif
ifeq ($(PLATFORM), darwin)
LDFLAGS += -framework CoreServices
else
ifeq ($(CONFIG_ANDROID),no)
LDFLAGS += -lrt
endif
endif
ifeq ($(CONFIG_GPERFTOOLS),yes)
CFLAGS += -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
LDFLAGS += -lprofiler -ltcmalloc
endif

ifeq ($(COMPILER), clang)
CFLAGS  += -Wno-microsoft -Qunused-arguments -Wno-unused-function
CFLAGS  += -Wno-unused-value -Wno-tautological-constant-out-of-range-compare
CFLAGS  += -Wno-parentheses-equality
endif


# LIBAV ########################################################################

ifeq ($(CONFIG_LIBAV),yes)

FFMPEG_LIBS := \
    libavfilter \
    libswresample \
    libavresample \
    libswscale \
    libavformat \
    libavcodec \
    libavutil

# FFMPEG_STATIC
ifeq ($(CONFIG_FFMPEG_STATIC),yes)

ifeq (,$(wildcard ${BUILDDIR}/libffmpeg_stamp))
# build static FFMPEG as first for pkgconfig
ffmpeg_all: ${BUILDDIR}/libffmpeg_stamp
	$(MAKE) all
endif

FFMPEG_PREFIX := $(BUILDDIR)/ffmpeg/build/ffmpeg
FFMPEG_LIBDIR := $(FFMPEG_PREFIX)/lib
FFMPEG_INCDIR := $(FFMPEG_PREFIX)/include
FFMPEG_CONFIG := \
    PKG_CONFIG_LIBDIR=$(FFMPEG_LIBDIR)/pkgconfig $(PKG_CONFIG) \
    --define-variable=prefix=$(FFMPEG_PREFIX) \
    --define-variable=includedir=$(FFMPEG_INCDIR) \
    --define-variable=libdir=$(FFMPEG_LIBDIR) --static

ifeq ($(CONFIG_LIBX264_STATIC),yes)
FFMPEG_DEPS += libx264
endif

ifeq ($(CONFIG_LIBX265_STATIC),yes)
FFMPEG_DEPS += libx265
endif

ifeq ($(CONFIG_LIBVPX_STATIC),yes)
FFMPEG_DEPS += libvpx
endif

ifeq ($(CONFIG_LIBOGG_STATIC),yes)
FFMPEG_DEPS += libogg
endif

ifeq ($(CONFIG_LIBTHEORA_STATIC),yes)
FFMPEG_DEPS += libtheoraenc libtheoradec libtheora
endif

ifeq ($(CONFIG_LIBVORBIS_STATIC),yes)
FFMPEG_DEPS += libvorbisfile libvorbisenc libvorbis
endif

ifeq ($(CONFIG_LIBFDKAAC_STATIC),yes)
FFMPEG_DEPS += libfdk-aac
endif

ifeq ($(CONFIG_LIBOPUS_STATIC),yes)
FFMPEG_DEPS += libopus
endif

LDFLAGS += $(foreach lib,$(FFMPEG_LIBS),$(FFMPEG_LIBDIR)/$(lib).a)
LDFLAGS += $(foreach lib,$(FFMPEG_DEPS),$(FFMPEG_LIBDIR)/$(lib).a)

else # !FFMPEG_STATIC

FFMPEG_CONFIG := $(PKG_CONFIG)

endif # FFMPEG_STATIC

CFLAGS  += `$(FFMPEG_CONFIG) --cflags $(FFMPEG_LIBS)`
LDFLAGS += `$(FFMPEG_CONFIG) --libs $(FFMPEG_LIBS)`

endif

# LIBAV ########################################################################


ifeq ($(CONFIG_HDHOMERUN_STATIC),yes)
CFLAGS  += -I$(BUILDDIR)/hdhomerun
LDFLAGS += $(BUILDDIR)/hdhomerun/libhdhomerun/libhdhomerun.a
endif

vpath %.c $(ROOTDIR)
vpath %.h $(ROOTDIR)

#
# Other config
#

BUNDLE_FLAGS-${CONFIG_ZLIB} += -z
BUNDLE_FLAGS-${CONFIG_PNGQUANT} += -q
BUNDLE_FLAGS = ${BUNDLE_FLAGS-yes}

#
# Binaries/Scripts
#

MKBUNDLE = $(PYTHON) $(ROOTDIR)/support/mkbundle
XGETTEXT2 ?= $(XGETTEXT) --language=C --from-code=utf-8 --add-comments=/ -k_ -kN_ -s
MSGMERGE ?= msgmerge

#
# Debug/Output
#

BRIEF  = CC MKBUNDLE
ifndef V
ECHO   = printf "%-16s%s\n" $(1) $(2)
MSG    = $(subst $(BUILDDIR)/,,$@)
$(foreach VAR,$(BRIEF), \
	$(eval p$(VAR) = @$$(call ECHO,$(VAR),$$(MSG)); $($(VAR))))
else
$(foreach VAR,$(BRIEF), \
	$(eval p$(VAR) = $($(VAR))))
endif

#
# Core
#
SRCS-1 = \
	src/version.c \
	src/uuid.c \
	src/main.c \
	src/tvhlog.c \
	src/tprofile.c \
	src/idnode.c \
	src/prop.c \
	src/proplib.c \
	src/utils.c \
	src/wrappers.c \
	src/tvh_thread.c \
	src/tvhvfs.c \
	src/access.c \
	src/tcp.c \
	src/udp.c \
	src/url.c \
	src/http.c \
	src/notify.c \
	src/file.c \
	src/epg.c \
	src/epgdb.c\
	src/epggrab.c\
	src/spawn.c \
	src/packet.c \
	src/esstream.c \
	src/streaming.c \
	src/channels.c \
	src/subscriptions.c \
	src/service.c \
	src/htsp_server.c \
	src/htsmsg.c \
	src/htsmsg_binary.c \
	src/htsmsg_binary2.c \
	src/htsmsg_json.c \
	src/htsmsg_xml.c \
	src/misc/dbl.c \
	src/misc/json.c \
	src/misc/m3u.c \
	src/settings.c \
	src/htsbuf.c \
	src/trap.c \
	src/htsstr.c \
	src/tvhpoll.c \
	src/huffman.c \
	src/filebundle.c \
	src/config.c \
	src/lang_codes.c \
	src/lang_str.c \
	src/imagecache.c \
	src/tvhtime.c \
	src/service_mapper.c \
	src/input.c \
	src/httpc.c \
	src/rtsp.c \
	src/download.c \
	src/fsmonitor.c \
	src/cron.c \
	src/esfilter.c \
	src/intlconv.c \
	src/profile.c \
	src/bouquet.c \
	src/lock.c \
	src/string_list.c \
	src/wizard.c \
	src/memoryinfo.c

SRCS = $(SRCS-1)
I18N-C = $(SRCS-1)

SRCS-ZLIB = \
	src/zlib.c
SRCS-${CONFIG_ZLIB} += $(SRCS-ZLIB)

SRCS-UPNP = \
	src/upnp.c
SRCS-${CONFIG_UPNP} += $(SRCS-UPNP)
I18N-C += $(SRCS-UPNP)

# SATIP Server
SRCS-SATIP-SERVER = \
	src/satip/server.c \
	src/satip/rtsp.c \
	src/satip/rtp.c
SRCS-${CONFIG_SATIP_SERVER} += $(SRCS-SATIP-SERVER)
I18N-C += $(SRCS-SATIP-SERVER)

SRCS-2 = \
	src/api.c \
	src/api/api_config.c \
	src/api/api_status.c \
	src/api/api_idnode.c \
	src/api/api_raw.c \
	src/api/api_input.c \
	src/api/api_channel.c \
	src/api/api_service.c \
	src/api/api_mpegts.c \
	src/api/api_epg.c \
	src/api/api_epggrab.c \
	src/api/api_imagecache.c \
	src/api/api_esfilter.c \
	src/api/api_intlconv.c \
	src/api/api_access.c \
	src/api/api_dvr.c \
	src/api/api_caclient.c \
	src/api/api_profile.c \
	src/api/api_bouquet.c \
	src/api/api_language.c \
	src/api/api_satip.c \
	src/api/api_timeshift.c \
	src/api/api_wizard.c

SRCS-2 += \
        src/parsers/message.c \
	src/parsers/parsers.c \
	src/parsers/bitstream.c \
	src/parsers/parser_h264.c \
	src/parsers/parser_hevc.c \
	src/parsers/parser_latm.c \
	src/parsers/parser_avc.c \
	src/parsers/parser_teletext.c

SRCS-2 += \
	src/epggrab/module.c \
	src/epggrab/channel.c \
	src/epggrab/module/xmltv.c

SRCS-2 += \
	src/plumbing/tsfix.c \
	src/plumbing/globalheaders.c

SRCS-2 += \
	src/dvr/dvr_db.c \
	src/dvr/dvr_rec.c \
	src/dvr/dvr_autorec.c \
	src/dvr/dvr_timerec.c \
	src/dvr/dvr_vfsmgr.c \
	src/dvr/dvr_config.c \
	src/dvr/dvr_cutpoints.c

SRCS-2 += \
	src/webui/webui.c \
	src/webui/comet.c \
	src/webui/extjs.c \
	src/webui/simpleui.c \
	src/webui/statedump.c \
	src/webui/html.c \
	src/webui/webui_api.c \
	src/webui/xmltv.c \
	src/webui/doc_md.c

SRCS-2 += \
	src/muxer.c \
	src/muxer/muxer_pass.c \
	src/muxer/ebml.c \
	src/muxer/muxer_mkv.c \
	src/muxer/muxer_audioes.c

SRCS += $(SRCS-2)
I18N-C += $(SRCS-2)

#
# Optional code
#

# MPEGTS core, order by usage (psi lib, tsdemux)
SRCS-MPEGTS = \
	src/descrambler/descrambler.c \
	src/descrambler/caclient.c \
	src/descrambler/caid.c \
	src/input/mpegts.c \
	src/input/mpegts/mpegts_pid.c \
	src/input/mpegts/mpegts_input.c \
	src/input/mpegts/tsdemux.c \
	src/input/mpegts/dvb_psi_hbbtv.c \
	src/input/mpegts/dvb_psi_lib.c \
	src/input/mpegts/mpegts_network.c \
	src/input/mpegts/mpegts_mux.c \
	src/input/mpegts/mpegts_service.c \
	src/input/mpegts/mpegts_table.c \
	src/input/mpegts/dvb_support.c \
	src/input/mpegts/dvb_charset.c \
	src/input/mpegts/dvb_psi_pmt.c \
	src/input/mpegts/dvb_psi.c \
	src/input/mpegts/fastscan.c \
	src/input/mpegts/mpegts_mux_sched.c \
        src/input/mpegts/mpegts_network_scan.c \
        src/input/mpegts/mpegts_tsdebug.c \
        src/descrambler/tsdebugcw.c
SRCS-$(CONFIG_MPEGTS) += $(SRCS-MPEGTS)
I18N-C += $(SRCS-MPEGTS)

# MPEGTS DVB
SRCS-MPEGTS-DVB = \
	src/input/mpegts/mpegts_network_dvb.c \
	src/input/mpegts/mpegts_mux_dvb.c \
	src/input/mpegts/scanfile.c
SRCS-${CONFIG_MPEGTS_DVB} += $(SRCS-MPEGTS-DVB)
I18N-C += $(SRCS-MPEGTS-DVB)

# MPEGTS EPG
SRCS-MPEGTS-EPG = \
	src/epggrab/otamux.c\
	src/epggrab/module/eit.c \
	src/epggrab/module/eitpatternlist.c \
	src/epggrab/module/psip.c \
	src/epggrab/support/freesat_huffman.c \
	src/epggrab/module/opentv.c
SRCS-$(CONFIG_MPEGTS) += $(SRCS-MPEGTS-EPG)
I18N-C += $(SRCS-MPEGTS-EPG)

# LINUX DVB
SRCS-LINUXDVB = \
	src/input/mpegts/linuxdvb/linuxdvb.c \
	src/input/mpegts/linuxdvb/linuxdvb_adapter.c \
	src/input/mpegts/linuxdvb/linuxdvb_frontend.c \
	src/input/mpegts/linuxdvb/linuxdvb_satconf.c \
	src/input/mpegts/linuxdvb/linuxdvb_lnb.c \
	src/input/mpegts/linuxdvb/linuxdvb_switch.c \
	src/input/mpegts/linuxdvb/linuxdvb_rotor.c \
	src/input/mpegts/linuxdvb/linuxdvb_en50494.c
SRCS-${CONFIG_LINUXDVB} += $(SRCS-LINUXDVB)
I18N-C += $(SRCS-LINUXDVB)

# SATIP Client
SRCS-SATIP-CLIENT = \
	src/input/mpegts/satip/satip.c \
	src/input/mpegts/satip/satip_frontend.c \
	src/input/mpegts/satip/satip_satconf.c \
	src/input/mpegts/satip/satip_rtsp.c
SRCS-${CONFIG_SATIP_CLIENT} += $(SRCS-SATIP-CLIENT)
I18N-C += $(SRCS-SATIP-CLIENT)

# HDHOMERUN
SRCS-HDHOMERUN = \
	src/input/mpegts/tvhdhomerun/tvhdhomerun.c \
	src/input/mpegts/tvhdhomerun/tvhdhomerun_frontend.c
SRCS-${CONFIG_HDHOMERUN_CLIENT} += $(SRCS-HDHOMERUN)
I18N-C += $(SRCS-HDHOMERUN)

# IPTV
SRCS-IPTV = \
	src/input/mpegts/iptv/iptv.c \
	src/input/mpegts/iptv/iptv_mux.c \
	src/input/mpegts/iptv/iptv_service.c \
	src/input/mpegts/iptv/iptv_http.c \
	src/input/mpegts/iptv/iptv_udp.c \
	src/input/mpegts/iptv/iptv_rtsp.c \
	src/input/mpegts/iptv/iptv_rtcp.c \
	src/input/mpegts/iptv/iptv_pipe.c \
	src/input/mpegts/iptv/iptv_file.c \
	src/input/mpegts/iptv/iptv_auto.c
SRCS-${CONFIG_IPTV} += $(SRCS-IPTV)
I18N-C += $(SRCS-IPTV)

# TSfile
SRCS-TSFILE = \
	src/input/mpegts/tsfile/tsfile.c \
	src/input/mpegts/tsfile/tsfile_input.c \
	src/input/mpegts/tsfile/tsfile_mux.c
SRCS-$(CONFIG_TSFILE) += $(SRCS-TSFILE)
I18N-C += $(SRCS-TSFILE)

# Timeshift
SRCS-TIMESHIFT = \
	src/timeshift.c \
	src/timeshift/timeshift_filemgr.c \
	src/timeshift/timeshift_writer.c \
	src/timeshift/timeshift_reader.c
SRCS-${CONFIG_TIMESHIFT} += $(SRCS-TIMESHIFT)
I18N-C += $(SRCS-TIMESHIFT)

# Inotify
SRCS-INOTIFY = \
	src/dvr/dvr_inotify.c
SRCS-${CONFIG_INOTIFY} += $(SRCS-INOTIFY)
I18N-C += $(SRCS-INOTIFY)
ifeq ($(CONFIG_INOTIFY), yes)
ifeq ($(PLATFORM), freebsd)
LDFLAGS += -linotify
endif
endif

# Avahi
SRCS-AVAHI = \
	src/avahi.c
SRCS-$(CONFIG_AVAHI) += $(SRCS-AVAHI)
I18N-C += $(SRCS-AVAHI)

# Bonjour
SRCS-BONJOUR = \
	src/bonjour.c
SRCS-$(CONFIG_BONJOUR) += $(SRCS-BONJOUR)
I18N-C += $(SRCS-BONJOUR)

# codecs
SRCS-CODECS = $(wildcard src/transcoding/codec/*.c)
SRCS-CODECS += $(wildcard src/transcoding/codec/codecs/*.c)
ifneq (,$(filter yes,$(CONFIG_LIBX264) $(CONFIG_LIBX265)))
LIBS-CODECS += libx26x
endif
ifeq ($(CONFIG_LIBVPX),yes)
LIBS-CODECS += libvpx
endif
ifeq ($(CONFIG_LIBTHEORA),yes)
LIBS-CODECS += libtheora
endif
ifeq ($(CONFIG_LIBVORBIS),yes)
LIBS-CODECS += libvorbis
endif
ifeq ($(CONFIG_LIBFDKAAC),yes)
LIBS-CODECS += libfdk_aac
endif
ifeq ($(CONFIG_LIBOPUS),yes)
LIBS-CODECS += libopus
endif
ifeq ($(CONFIG_VAAPI),yes)
LIBS-CODECS += vaapi
endif
ifeq ($(CONFIG_NVENC),yes)
LIBS-CODECS += nvenc
endif
ifeq ($(CONFIG_OMX),yes)
LIBS-CODECS += omx
endif
SRCS-CODECS += $(foreach lib,$(LIBS-CODECS),src/transcoding/codec/codecs/libs/$(lib).c)

#hwaccels
ifeq ($(CONFIG_HWACCELS),yes)
SRCS-HWACCELS += src/transcoding/transcode/hwaccels/hwaccels.c
ifeq ($(CONFIG_VAAPI),yes)
SRCS-HWACCELS += src/transcoding/transcode/hwaccels/vaapi.c
endif
endif

# libav
DEPS-LIBAV = \
	src/main.c \
	src/tvhlog.c
SRCS-LIBAV = \
	src/libav.c \
	src/muxer/muxer_libav.c \
	src/api/api_codec.c
ifeq ($(CONFIG_IPTV),yes)
SRCS-LIBAV += src/input/mpegts/iptv/iptv_libav.c
endif
SRCS-LIBAV += $(wildcard src/transcoding/*.c)
SRCS-LIBAV += $(wildcard src/transcoding/transcode/*.c)
SRCS-LIBAV += $(SRCS-HWACCELS)
SRCS-LIBAV += $(SRCS-CODECS)
SRCS-$(CONFIG_LIBAV) += $(SRCS-LIBAV)
I18N-C += $(SRCS-LIBAV)

# Tvhcsa
SRCS-TVHCSA = \
	src/descrambler/tvhcsa.c
SRCS-${CONFIG_TVHCSA} += $(SRCS-TVHCSA)
I18N-C += $(SRCS-TVHCSA)

# Cardclient
SRCS-CARDCLIENT = \
        src/descrambler/cclient.c \
	src/descrambler/emm_reass.c
SRCS-${CONFIG_CARDCLIENT} += $(SRCS-CARDCLIENT)
I18N-C += $(SRCS-CARDCLIENT)

# CWC
SRCS-CWC = \
	src/descrambler/cwc.c
SRCS-${CONFIG_CWC} += $(SRCS-CWC)
I18N-C += $(SRCS-CWC)

# CCCAM
SRCS-CCCAM = \
	src/descrambler/cccam.c
SRCS-${CONFIG_CCCAM} += $(SRCS-CCCAM)
I18N-C += $(SRCS-CCCAM)

# CAPMT
SRCS-CAPMT = \
	src/descrambler/capmt.c
SRCS-${CONFIG_CAPMT} += $(SRCS-CAPMT)
I18N-C += $(SRCS-CAPMT)

# CONSTCW
SRCS-CONSTCW = \
	src/descrambler/constcw.c
SRCS-${CONFIG_CONSTCW} += $(SRCS-CONSTCW)
I18N-C += $(SRCS-CONSTCW)

# DVB CAM
SRCS-DVBCAM = \
        src/input/mpegts/en50221/en50221.c \
        src/input/mpegts/en50221/en50221_apps.c \
        src/input/mpegts/en50221/en50221_capmt.c \
	src/input/mpegts/linuxdvb/linuxdvb_ca.c \
	src/descrambler/dvbcam.c
SRCS-${CONFIG_LINUXDVB_CA} += $(SRCS-DVBCAM)
I18N-C += $(SRCS-DVBCAM)

SRCS-DDCI = \
	src/input/mpegts/linuxdvb/linuxdvb_ddci.c
SRCS-${CONFIG_DDCI} += $(SRCS-DDCI)
I18N-C += $(SRCS-DDCI)

# crypto algorithms
SRCS-${CONFIG_SSL} += src/descrambler/algo/libaesdec.c
SRCS-${CONFIG_SSL} += src/descrambler/algo/libaes128dec.c
SRCS-${CONFIG_SSL} += src/descrambler/algo/libdesdec.c

# DBUS
SRCS-${CONFIG_DBUS_1}  += src/dbus.c

# Watchdog
SRCS-${CONFIG_LIBSYSTEMD_DAEMON} += src/watchdog.c

# DVB scan
DVBSCAN-$(CONFIG_DVBSCAN) += check_dvb_scan
ALL-$(CONFIG_DVBSCAN)     += check_dvb_scan

# File bundles
SRCS-${CONFIG_BUNDLE}     += bundle.c
BUNDLES-yes               += src/webui/static
BUNDLES-yes               += data/conf
BUNDLES-${CONFIG_DVBSCAN} += data/dvb-scan
BUNDLES                    = $(BUNDLES-yes)

#
# Documentation
#

MD-TO-C    = PYTHONIOENCODING=utf-8 $(PYTHON) support/doc/md_to_c.py

SRCS-yes   += src/docs.c
I18N-C-DOCS = src/docs_inc.c
I18N-DOCS   = $(wildcard docs/markdown/*.md)
I18N-DOCS  += $(wildcard docs/markdown/inc/*.md)
I18N-DOCS  += $(wildcard docs/class/*.md)
I18N-DOCS  += $(wildcard docs/property/*.md)
I18N-DOCS  += $(wildcard docs/wizard/*.md)
MD-ROOT     = $(patsubst docs/markdown/%.md,%,$(sort $(wildcard docs/markdown/*.md)))
MD-ROOT    += $(patsubst docs/markdown/inc/%.md,inc/%,$(sort $(wildcard docs/markdown/inc/*.md)))
MD-CLASS    = $(patsubst docs/class/%.md,%,$(sort $(wildcard docs/class/*.md)))
MD-PROP     = $(patsubst docs/property/%.md,%,$(sort $(wildcard docs/property/*.md)))
MD-WIZARD   = $(patsubst docs/wizard/%.md,%,$(sort $(wildcard docs/wizard/*.md)))

#
# Internationalization
#
PO-FILES  = $(wildcard $(foreach f,$(LANGUAGES),intl/tvheadend.$(f).po))
PO-FILES += $(wildcard $(foreach f,$(LANGUAGES-DOC),intl/docs/tvheadend.doc.$(f).po))
SRCS += src/tvh_locale.c

POC_PY=PYTHONIOENCODING=utf-8 $(PYTHON) support/poc.py

#
# Add-on modules
#

SRCS_EXTRA = src/extra/capmt_ca.c

#
# Variable transformations
#

SRCS      += $(SRCS-yes)
OBJS       = $(SRCS:%.c=$(BUILDDIR)/%.o)
OBJS_EXTRA = $(SRCS_EXTRA:%.c=$(BUILDDIR)/%.so)
DEPS       = ${OBJS:%.o=%.d}

ifeq ($(CONFIG_FFMPEG_STATIC),yes)
ALL-yes   += ${BUILDDIR}/libffmpeg_stamp
endif
ifeq ($(CONFIG_HDHOMERUN_STATIC),yes)
ALL-yes   += ${BUILDDIR}/libhdhomerun_stamp
endif

SRCS += build.c timestamp.c

#
# Build Rules
#

# Default
.PHONY: all
all: $(ALL-yes) ${PROG}

# Check configure output is valid
.config.mk: configure
	@echo "./configure output is old, please re-run"
	@false

# Recreate configuration
.PHONY: reconfigure
reconfigure:
	$(ROOTDIR)/configure $(CONFIGURE_ARGS)

# Binary
${PROG}: .config.mk make_webui $(OBJS)
	$(pCC) -o $@ $(OBJS) $(CFLAGS) $(LDFLAGS)

# Object
${BUILDDIR}/%.o: %.c
	@mkdir -p $(dir $@)
	$(pCC) -MD -MP $(CFLAGS) -c -o $@ $<

# Add-on
${BUILDDIR}/%.so: ${SRCS_EXTRA}
	@mkdir -p $(dir $@)
	${pCC} -O -fbuiltin -fomit-frame-pointer -fPIC -shared -o $@ $< -ldl

# Clean
.PHONY: clean
clean:
	rm -rf ${BUILDDIR}/src ${BUILDDIR}/bundle* ${BUILDDIR}/build.o ${BUILDDIR}/timestamp.* \
	       src/tvh_locale_inc.c
	find . -name "*~" | xargs rm -f
	$(MAKE) -f Makefile.webui clean

# Distclean
.PHONY: distclean
distclean: clean
	rm -rf ${ROOTDIR}/build.*
	rm -rf ${ROOTDIR}/debian/.debhelper
	rm -rf ${ROOTDIR}/data/dvb-scan
	rm -f ${ROOTDIR}/.config.mk

# Create version
$(BUILDDIR)/src/version.o: $(ROOTDIR)/src/version.c
$(ROOTDIR)/src/version.c: FORCE
	@$(ROOTDIR)/support/version $@ > /dev/null
FORCE:

# Include dependency files if they exist.
ifeq ($(filter clean distclean, $(MAKECMDGOALS)),)
-include $(DEPS)
endif

# Some hardcoded deps
src/webui/extjs.c: make_webui

# Include OS specific targets
include ${ROOTDIR}/support/${OSENV}.mk

# Build files
DATE_FMT = %Y-%m-%dT%H:%M:%S%z
ifdef SOURCE_DATE_EPOCH
	BUILD_DATE ?= $(shell date -u -d "@$(SOURCE_DATE_EPOCH)" "+$(DATE_FMT)"  2>/dev/null || date -u -r "$(SOURCE_DATE_EPOCH)" "+$(DATE_FMT)" 2>/dev/null || date -u "+$(DATE_FMT)")
else
	BUILD_DATE ?= $(shell date "+$(DATE_FMT)")
endif
$(BUILDDIR)/timestamp.c: FORCE
	@mkdir -p $(dir $@)
	@echo '#include "build.h"' > $@
	@echo 'const char* build_timestamp = "'$(BUILD_DATE)'";' >> $@

$(BUILDDIR)/timestamp.o: $(BUILDDIR)/timestamp.c
	$(pCC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/build.o: $(BUILDDIR)/build.c
	@mkdir -p $(dir $@)
	$(pCC) $(CFLAGS) -c -o $@ $<

# Documentation
$(BUILDDIR)/docs-timestamp: $(I18N-DOCS) support/doc/md_to_c.py
	@-rm -f src/docs_inc.c src/docs_inc.c.new
	@$(MD-TO-C) --batch --list="$(MD-ROOT)" \
	            --inpath="docs/markdown/%s.md" \
	            --name="tvh_doc_root_%s" \
	            --out="src/docs_inc.c.new"
	@$(MD-TO-C) --batch --list="$(MD-CLASS)" \
	            --inpath="docs/class/%s.md" \
	            --name="tvh_doc_%s_class" \
	            --out="src/docs_inc.c.new"
	@$(MD-TO-C) --batch --list="$(MD-PROP)" \
	            --inpath="docs/property/%s.md" \
	            --name="tvh_doc_%s_property" \
	            --out="src/docs_inc.c.new"
	@$(MD-TO-C) --batch --list="$(MD-WIZARD)" \
	            --inpath="docs/wizard/%s.md" \
	            --name="tvh_doc_wizard_%s" \
	            --out="src/docs_inc.c.new"
	@$(MD-TO-C) --pages="$(MD-ROOT)" >> src/docs_inc.c.new
	@mv src/docs_inc.c.new src/docs_inc.c
	@touch $@

src/docs_inc.c: $(BUILDDIR)/docs-timestamp

src/docs_inc.h: $(BUILDDIR)/docs-timestamp

src/docs.c: src/docs_inc.c src/docs_inc.h

$(BUILDDIR)/src/docs.o: $(BUILDDIR)/docs-timestamp $(I18N-DOCS) support/doc/md_to_c.py

# Internationalization
.PHONY: intl
intl:
	@printf "Building tvheadend.pot\n"
	@$(XGETTEXT2) -o intl/tvheadend.pot.new $(I18N-C)
	@sed -e 's/^"Language: /"Language: en/' < intl/tvheadend.pot.new > intl/tvheadend.pot
	$(MAKE) -f Makefile.webui LANGUAGES="$(LANGUAGES)" WEBUI=std intl
	@printf "Building docs/tvheadend.doc.pot\n"
	@$(XGETTEXT2) -o intl/docs/tvheadend.doc.pot.new $(I18N-C-DOCS)
	@sed -e 's/^"Language: /"Language: en/' < intl/docs/tvheadend.doc.pot.new > intl/docs/tvheadend.doc.pot
	$(MAKE)


intl/tvheadend.pot:

$(BUILDDIR)/src/tvh_locale.o: src/tvh_locale_inc.c
src/tvh_locale_inc.c: $(PO-FILES)
	@printf "Building $@\n"
	@$(POC_PY) --in="$(PO-FILES)" > $@.new
	@mv $@.new $@

# Bundle files
$(BUILDDIR)/bundle.o: $(BUILDDIR)/bundle.c
	@mkdir -p $(dir $@)
	$(pCC) $(CFLAGS) -I${ROOTDIR}/src -c -o $@ $<

$(BUILDDIR)/bundle.c: $(DVBSCAN-yes) make_webui
	@mkdir -p $(dir $@)
	$(pMKBUNDLE) -o $@ -d ${BUILDDIR}/bundle.d $(BUNDLE_FLAGS) $(BUNDLES:%=$(ROOTDIR)/%)

.PHONY: make_webui
make_webui:
	$(MAKE) -f Makefile.webui LANGUAGES="$(LANGUAGES)" all

# Static FFMPEG

ifeq ($(CONFIG_FFMPEG_STATIC),yes)
src/libav.h ${SRCS-LIBAV} ${DEPS-LIBAV}: ${BUILDDIR}/libffmpeg_stamp
endif

${BUILDDIR}/libffmpeg_stamp: ${BUILDDIR}/ffmpeg/build/ffmpeg/lib/libavcodec.a
	@touch $@

${BUILDDIR}/ffmpeg/build/ffmpeg/lib/libavcodec.a: Makefile.ffmpeg
ifeq ($(CONFIG_PCLOUD_CACHE),yes)
	$(MAKE) -f Makefile.ffmpeg libcacheget
	$(MAKE) -f Makefile.ffmpeg build
	$(MAKE) -f Makefile.ffmpeg libcacheput
else
	$(MAKE) -f Makefile.ffmpeg build
endif

# Static HDHOMERUN library

ifeq ($(CONFIG_HDHOMERUN_STATIC),yes)
src/input/mpegts/tvhdhomerun/tvhdhomerun_private.h ${SRCS-HDHOMERUN}: ${BUILDDIR}/libhdhomerun_stamp
endif

${BUILDDIR}/libhdhomerun_stamp: ${BUILDDIR}/hdhomerun/libhdhomerun/libhdhomerun.a
	@touch $@

${BUILDDIR}/hdhomerun/libhdhomerun/libhdhomerun.a: Makefile.hdhomerun
ifeq ($(CONFIG_PCLOUD_CACHE),yes)
	$(MAKE) -f Makefile.hdhomerun libcacheget
	$(MAKE) -f Makefile.hdhomerun build
	$(MAKE) -f Makefile.hdhomerun libcacheput
else
	$(MAKE) -f Makefile.hdhomerun build
endif

.PHONY: ffmpeg_rebuild
ffmpeg_rebuild:
	-rm ${BUILDDIR}/ffmpeg/build/ffmpeg/lib/libavcodec.a
	-rm ${BUILDDIR}/libffmpeg_stamp
	-rm ${BUILDDIR}/ffmpeg/ffmpeg-*/.tvh_build
	$(MAKE) all

# linuxdvb git tree
$(ROOTDIR)/data/dvb-scan/.stamp:
	@echo "Receiving data/dvb-scan from https://github.com/tvheadend/dtv-scan-tables.git#tvheadend"
	@rm -rf $(ROOTDIR)/data/dvb-scan/*
	@$(ROOTDIR)/support/getmuxlist $(ROOTDIR)/data/dvb-scan
	@touch $@

.PHONY: check_dvb_scan
check_dvb_scan: $(ROOTDIR)/data/dvb-scan/.stamp

# dvb-s / enigma2 / satellites.xml
$(ROOTDIR)/data/dvb-scan/dvb-s/.stamp: $(ROOTDIR)/data/satellites.xml \
                                       $(ROOTDIR)/data/dvb-scan/.stamp
	@echo "Generating data/dvb-scan/dvb-s from data/satellites.xml"
	@if ! test -s $(ROOTDIR)/data/satellites.xml ; then echo "Put your satellites.xml file to $(ROOTDIR)/data/satellites.xml"; exit 1; fi
	@if ! test -d $(ROOTDIR)/data/dvb-scan/dvb-s ; then mkdir $(ROOTDIR)/data/dvb-scan/dvb-s ; fi
	@rm -rf $(ROOTDIR)/data/dvb-scan/dvb-s/*
	@PYTHONIOENCODING=utf-8 $(PYTHON) $(ROOTDIR)/support/sat_xml_scan.py \
		$(ROOTDIR)/data/satellites.xml $(ROOTDIR)/data/dvb-scan/dvb-s
	@touch $(ROOTDIR)/data/dvb-scan/dvb-s/.stamp

.PHONY: satellites_xml
satellites_xml: $(ROOTDIR)/data/dvb-scan/dvb-s/.stamp

#
# perf
#

PERF_DATA = /tmp/tvheadend.perf.data
PERF_SLEEP ?= 30

$(PERF_DATA): FORCE
	perf record -F 16000 -g -p $$(pidof tvheadend) -o $(PERF_DATA) sleep $(PERF_SLEEP)

.PHONY: perf-record
perf-record: $(PERF_DATA)

.PHONY: perf-graph
perf-graph:
	perf report --stdio -g graph -i $(PERF_DATA)

.PHONY: perf-report
perf-report:
	perf report --stdio -g none -i $(PERF_DATA)
