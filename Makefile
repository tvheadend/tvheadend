#
#  Tvheadend streaming server.
#  Copyright (C) 2007-2009 Andreas Öman
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
PROG    := $(BUILDDIR)/tvheadend
LANGUAGES ?= bg cs de en en_GB es fa fr he hr hu it lv nl pl pt ru sv

#
# Common compiler flags
#

CFLAGS  += -g -O2
ifeq ($(CONFIG_W_UNUSED_RESULT),yes)
CFLAGS  += -Wunused-result
endif
CFLAGS  += -Wall -Werror -Wwrite-strings -Wno-deprecated-declarations
CFLAGS  += -Wmissing-prototypes
CFLAGS  += -fms-extensions -funsigned-char -fno-strict-aliasing
CFLAGS  += -D_FILE_OFFSET_BITS=64
CFLAGS  += -I${BUILDDIR} -I${ROOTDIR}/src -I${ROOTDIR}
ifeq ($(CONFIG_ANDROID),yes)
LDFLAGS += -ldl -lm
else
LDFLAGS += -ldl -lpthread -lm
endif
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

ifeq ($(COMPILER), clang)
CFLAGS  += -Wno-microsoft -Qunused-arguments -Wno-unused-function
CFLAGS  += -Wno-unused-value -Wno-tautological-constant-out-of-range-compare
CFLAGS  += -Wno-parentheses-equality -Wno-incompatible-pointer-types
endif

ifeq ($(CONFIG_LIBFFMPEG_STATIC),yes)
CFLAGS  += -I${ROOTDIR}/libav_static/build/ffmpeg/include
LDFLAGS_FFDIR = ${ROOTDIR}/libav_static/build/ffmpeg/lib
LDFLAGS += ${LDFLAGS_FFDIR}/libavresample.a
LDFLAGS += ${LDFLAGS_FFDIR}/libswresample.a
LDFLAGS += ${LDFLAGS_FFDIR}/libswscale.a
LDFLAGS += ${LDFLAGS_FFDIR}/libavutil.a
LDFLAGS += ${LDFLAGS_FFDIR}/libavformat.a
LDFLAGS += ${LDFLAGS_FFDIR}/libavcodec.a
LDFLAGS += ${LDFLAGS_FFDIR}/libavutil.a
LDFLAGS += ${LDFLAGS_FFDIR}/libvorbisenc.a
LDFLAGS += ${LDFLAGS_FFDIR}/libvorbis.a
LDFLAGS += ${LDFLAGS_FFDIR}/libogg.a
ifeq ($(CONFIG_LIBX264_STATIC),yes)
LDFLAGS += ${LDFLAGS_FFDIR}/libx264.a
else
LDFLAGS += -lx264
endif
ifeq ($(CONFIG_LIBX265),yes)
ifeq ($(CONFIG_LIBX265_STATIC),yes)
LDFLAGS += ${LDFLAGS_FFDIR}/libx265.a -lstdc++
else
LDFLAGS += -lx265
endif
endif
LDFLAGS += ${LDFLAGS_FFDIR}/libvpx.a
CONFIG_LIBMFX_VA_LIBS =
ifeq ($(CONFIG_LIBMFX),yes)
CONFIG_LIBMFX_VA_LIBS += -lva
ifeq ($(CONFIG_VA_DRM),yes)
CONFIG_LIBMFX_VA_LIBS += -lva-drm
endif
ifeq ($(CONFIG_VA_X11),yes)
CONFIG_LIBMFX_VA_LIBS += -lva-x11
endif
ifeq ($(CONFIG_LIBMFX_STATIC),yes)
LDFLAGS += ${LDFLAGS_FFDIR}/libmfx.a -lstdc++
else
LDFLAGS += -lmfx
endif
LDFLAGS += ${CONFIG_LIBMFX_VA_LIBS}
endif
endif

ifeq ($(CONFIG_HDHOMERUN_STATIC),yes)
CFLAGS  += -I${ROOTDIR}/libhdhomerun_static
LDFLAGS += -L${ROOTDIR}/libhdhomerun_static/libhdhomerun \
           -Wl,-Bstatic -lhdhomerun -Wl,-Bdynamic
endif

vpath %.c $(ROOTDIR)
vpath %.h $(ROOTDIR)

#
# Other config
#

BUNDLE_FLAGS-${CONFIG_ZLIB} = -z
BUNDLE_FLAGS = ${BUNDLE_FLAGS-yes}

#
# Binaries/Scripts
#

MKBUNDLE = $(PYTHON) $(ROOTDIR)/support/mkbundle
XGETTEXT2 ?= $(XGETTEXT) --language=C --add-comments=/ -k_ -kN_ -s
MSGMERGE ?= msgmerge

#
# Debug/Output
#

ifndef V
ECHO   = printf "%-16s%s\n" $(1) $(2)
BRIEF  = CC MKBUNDLE CXX
MSG    = $(subst $(BUILDDIR)/,,$@)
$(foreach VAR,$(BRIEF), \
	$(eval $(VAR) = @$$(call ECHO,$(VAR),$$(MSG)); $($(VAR))))
endif

#
# Core
#
SRCS-1 = \
	src/version.c \
	src/uuid.c \
	src/main.c \
	src/tvhlog.c \
	src/idnode.c \
	src/prop.c \
	src/utils.c \
	src/wrappers.c \
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
	src/streaming.c \
	src/channels.c \
	src/subscriptions.c \
	src/service.c \
	src/htsp_server.c \
	src/htsmsg.c \
	src/htsmsg_binary.c \
	src/htsmsg_json.c \
	src/htsmsg_xml.c \
	src/misc/dbl.c \
	src/misc/json.c \
	src/settings.c \
	src/htsbuf.c \
	src/trap.c \
	src/avg.c \
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
	src/fsmonitor.c \
	src/cron.c \
	src/esfilter.c \
	src/intlconv.c \
	src/profile.c \
	src/bouquet.c \
	src/lock.c
SRCS = $(SRCS-1)
I18N-C = $(SRCS-1)

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
	src/api/api_timeshift.c

SRCS-2 += \
	src/parsers/parsers.c \
	src/parsers/bitstream.c \
	src/parsers/parser_h264.c \
	src/parsers/parser_hevc.c \
	src/parsers/parser_latm.c \
	src/parsers/parser_avc.c \
	src/parsers/parser_teletext.c \

SRCS-2 += \
	src/epggrab/module.c\
	src/epggrab/channel.c\
	src/epggrab/module/pyepg.c\
	src/epggrab/module/xmltv.c\

SRCS-2 += \
	src/plumbing/tsfix.c \
	src/plumbing/globalheaders.c

SRCS-2 += \
	src/dvr/dvr_db.c \
	src/dvr/dvr_rec.c \
	src/dvr/dvr_autorec.c \
	src/dvr/dvr_timerec.c \
	src/dvr/dvr_config.c \
	src/dvr/dvr_cutpoints.c \

SRCS-2 += \
	src/webui/webui.c \
	src/webui/comet.c \
	src/webui/extjs.c \
	src/webui/simpleui.c \
	src/webui/statedump.c \
	src/webui/html.c\
	src/webui/webui_api.c\

SRCS-2 += \
	src/muxer.c \
	src/muxer/muxer_pass.c \
	src/muxer/ebml.c \
	src/muxer/muxer_mkv.c

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
	src/input/mpegts/dvb_psi_lib.c \
	src/input/mpegts/mpegts_network.c \
	src/input/mpegts/mpegts_mux.c \
	src/input/mpegts/mpegts_service.c \
	src/input/mpegts/mpegts_table.c \
	src/input/mpegts/dvb_support.c \
	src/input/mpegts/dvb_charset.c \
	src/input/mpegts/dvb_psi.c \
	src/input/mpegts/fastscan.c \
	src/input/mpegts/mpegts_mux_sched.c \
        src/input/mpegts/mpegts_network_scan.c
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
        src/input/mpegts/iptv/iptv_pipe.c
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

# Avahi
SRCS-AVAHI = \
	src/avahi.c
SRCS-$(CONFIG_AVAHI) += $(SRCS-AVAHI)
I18N-C += $(SRCS-AVAHI)

# Bonjour
SRCS-BONJOUR = \
	src/bonjour.c
SRCS-$(CONFIG_BONJOUR) = $(SRCS-BONJOUR)
I18N-C += $(SRCS-BONJOUR)

# libav
SRCS-LIBAV = \
	src/libav.c \
	src/muxer/muxer_libav.c \
	src/plumbing/transcoding.c
SRCS-$(CONFIG_LIBAV) += $(SRCS-LIBAV)
I18N-C += $(SRCS-LIBAV)

# Tvhcsa
SRCS-TVHCSA = \
	src/descrambler/tvhcsa.c
SRCS-${CONFIG_TVHCSA} += $(SRCS-TVHCSA)
I18N-C += $(SRCS-TVHCSA)

# CWC
SRCS-CWC = \
	src/descrambler/cwc.c \
	src/descrambler/emm_reass.c
SRCS-${CONFIG_CWC} += $(SRCS-CWC)
I18N-C += $(SRCS-CWC)
	
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
	src/input/mpegts/linuxdvb/linuxdvb_ca.c \
	src/descrambler/dvbcam.c
SRCS-${CONFIG_LINUXDVB_CA} += $(SRCS-DVBCAM)
I18N-C += $(SRCS-DVBCAM)

# TSDEBUGCW
SRCS-TSDEBUG = \
	src/descrambler/tsdebugcw.c
SRCS-${CONFIG_TSDEBUG} += $(SRCS-TSDEBUG)
I18N-C += $(SRCS-TSDEBUG)

# FFdecsa
ifneq ($(CONFIG_DVBCSA),yes)
FFDECSA-$(CONFIG_CAPMT)   = yes
FFDECSA-$(CONFIG_CWC)     = yes
FFDECSA-$(CONFIG_CONSTCW) = yes
endif

ifeq ($(FFDECSA-yes),yes)
SRCS-yes += src/descrambler/ffdecsa/ffdecsa_interface.c \
	    src/descrambler/ffdecsa/ffdecsa_int.c
SRCS-${CONFIG_MMX}  += src/descrambler/ffdecsa/ffdecsa_mmx.c
SRCS-${CONFIG_SSE2} += src/descrambler/ffdecsa/ffdecsa_sse2.c
${BUILDDIR}/src/descrambler/ffdecsa/ffdecsa_mmx.o  : CFLAGS += -mmmx
${BUILDDIR}/src/descrambler/ffdecsa/ffdecsa_sse2.o : CFLAGS += -msse2
endif

# libaesdec
SRCS-${CONFIG_SSL} += src/descrambler/libaesdec/libaesdec.c

# DBUS
SRCS-${CONFIG_DBUS_1}  += src/dbus.c

# File bundles
SRCS-${CONFIG_BUNDLE}     += bundle.c
BUNDLES-yes               += docs/html docs/docresources src/webui/static
BUNDLES-yes               += data/conf
BUNDLES-${CONFIG_DVBSCAN} += data/dvb-scan
BUNDLES                    = $(BUNDLES-yes)
ALL-$(CONFIG_DVBSCAN)     += check_dvb_scan

#
# Internationalization
#
PO-FILES = $(foreach f,$(LANGUAGES),intl/tvheadend.$(f).po)
SRCS += src/tvh_locale.c

POC_PY=PYTHONIOENCODING=utf-8 $(PYTHON) support/poc.py

define merge-po
	@if ! test -r "$(1)"; then \
	  sed -e 's/Content-Type: text\/plain; charset=CHARSET/Content-Type: text\/plain; charset=utf-8/' < "$(2)" > "$(1).new"; \
	else \
	  $(MSGMERGE) -o $(1).new $(1) $(2); \
	fi
	@mv $(1).new $(1)
endef

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

ifeq ($(CONFIG_LIBFFMPEG_STATIC),yes)
DEPS      += ${BUILDDIR}/libffmpeg_stamp
endif
ifeq ($(CONFIG_HDHOMERUN_STATIC),yes)
DEPS      += ${BUILDDIR}/libhdhomerun_stamp
endif

SRCS += build.c timestamp.c

#
# Build Rules
#

# Default
all: $(ALL-yes) ${PROG}

# Special
.PHONY:	clean distclean check_config reconfigure

# Check configure output is valid
check_config:
	@test $(ROOTDIR)/.config.mk -nt $(ROOTDIR)/configure\
		|| echo "./configure output is old, please re-run"
	@test $(ROOTDIR)/.config.mk -nt $(ROOTDIR)/configure

# Recreate configuration
reconfigure:
	$(ROOTDIR)/configure $(CONFIGURE_ARGS)

# Binary
${PROG}: check_config make_webui $(OBJS)
	$(CC) -o $@ $(OBJS) $(CFLAGS) $(LDFLAGS)

# Object
${BUILDDIR}/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -MD -MP $(CFLAGS) -c -o $@ $<

# Add-on
${BUILDDIR}/%.so: ${SRCS_EXTRA}
	@mkdir -p $(dir $@)
	${CC} -O -fbuiltin -fomit-frame-pointer -fPIC -shared -o $@ $< -ldl

# Clean
clean:
	rm -rf ${BUILDDIR}/src ${BUILDDIR}/bundle* ${BUILDDIR}/build.o ${BUILDDIR}/timestamp.* \
	       src/tvh_locale_inc.c
	find . -name "*~" | xargs rm -f
	$(MAKE) -f Makefile.webui clean

distclean: clean
	rm -rf ${ROOTDIR}/libav_static
	rm -rf ${ROOTDIR}/libhdhomerun_static
	rm -rf ${ROOTDIR}/build.*
	rm -rf ${ROOTDIR}/data/dvb-scan
	rm -f ${ROOTDIR}/.config.mk

# Create version
$(BUILDDIR)/src/version.o: $(ROOTDIR)/src/version.c
$(ROOTDIR)/src/version.c: FORCE
	@$(ROOTDIR)/support/version $@ > /dev/null
FORCE:

# Include dependency files if they exist.
-include $(DEPS)

# Some hardcoded deps
src/webui/extjs.c: make_webui

# Include OS specific targets
include ${ROOTDIR}/support/${OSENV}.mk

# Build files
$(BUILDDIR)/timestamp.c: FORCE
	@mkdir -p $(dir $@)
	@echo '#include "build.h"' > $@
	@echo 'const char* build_timestamp = "'`date -Iseconds`'";' >> $@

$(BUILDDIR)/timestamp.o: $(BUILDDIR)/timestamp.c
	$(CC) -c -o $@ $<

$(BUILDDIR)/build.o: $(BUILDDIR)/build.c
	@mkdir -p $(dir $@)
	$(CC) -c -o $@ $<

# Internationalization
.PHONY: intl
intl:
	@printf "Building tvheadend.pot\n"
	@$(XGETTEXT2) -o intl/tvheadend.pot.new $(I18N-C)
	@sed -e 's/^"Language: /"Language: en/' < intl/tvheadend.pot.new > intl/tvheadend.pot
	$(MAKE) -f Makefile.webui LANGUAGES="$(LANGUAGES)" WEBUI=std intl
	$(MAKE)

intl/tvheadend.pot:

#intl/tvheadend.en_GB.po: intl/tvheadend.pot
#	$(call merge-po,$@,$<)

#intl/tvheadend.de.po: intl/tvheadend.pot
#	$(call merge-po,$@,$<)

#intl/tvheadend.fr.po: intl/tvheadend.pot
#	$(call merge-po,$@,$<)

#intl/tvheadend.cs.po: intl/tvheadend.pot
#	$(call merge-po,$@,$<)

#intl/tvheadend.pl.po: intl/tvheadend.pot
#	$(call merge-po,$@,$<)

#intl/tvheadend.bg.po: intl/tvheadend.pot
#	$(call merge-po,$@,$<)

#intl/tvheadend.he.po: intl/tvheadend.pot
#	$(call merge-po,$@,$<)

#intl/tvheadend.hr.po: intl/tvheadend.pot
#	$(call merge-po,$@,$<)

#intl/tvheadend.it.po: intl/tvheadend.pot
#	$(call merge-po,$@,$<)

$(BUILDDIR)/src/tvh_locale.o: src/tvh_locale_inc.c
src/tvh_locale_inc.c: $(PO-FILES)
	@printf "Building $@\n"
	@$(POC_PY) --in="$(PO-FILES)" > $@.new
	@mv $@.new $@

# Bundle files
$(BUILDDIR)/bundle.o: $(BUILDDIR)/bundle.c
	@mkdir -p $(dir $@)
	$(CC) -I${ROOTDIR}/src -c -o $@ $<

$(BUILDDIR)/bundle.c: check_dvb_scan make_webui
	@mkdir -p $(dir $@)
	$(MKBUNDLE) -o $@ -d ${BUILDDIR}/bundle.d $(BUNDLE_FLAGS) $(BUNDLES:%=$(ROOTDIR)/%)

.PHONY: make_webui
make_webui:
	$(MAKE) -f Makefile.webui LANGUAGES="$(LANGUAGES)" all

# Static FFMPEG

ifeq ($(CONFIG_LIBFFMPEG_STATIC),yes)
${ROOTDIR}/src/libav.h: ${BUILDDIR}/libffmpeg_stamp
${SRCS_LIBAV}: ${BUILDDIR}/libffmpeg_stamp
endif

${BUILDDIR}/libffmpeg_stamp: ${ROOTDIR}/libav_static/build/ffmpeg/lib/libavcodec.a
	@touch $@

${ROOTDIR}/libav_static/build/ffmpeg/lib/libavcodec.a: Makefile.ffmpeg
	$(MAKE) -f Makefile.ffmpeg build

# Static HDHOMERUN library

ifeq ($(CONFIG_LIBHDHOMERUN_STATIC),yes)
${ROOTDIR}/src/input/mpegts/tvhdhomerun/tvhdhomerun_private.h: ${BUILDDIR}/libhdhomerun_stamp
${SRCS_HDHOMERUN}: ${BUILDDIR}/libhdhomerun_stamp
endif

${BUILDDIR}/libhdhomerun_stamp: ${ROOTDIR}/libhdhomerun_static/libhdhomerun/libhdhomerun.a
	@touch $@

${ROOTDIR}/libhdhomerun_static/libhdhomerun/libhdhomerun.a: Makefile.hdhomerun
	$(MAKE) -f Makefile.hdhomerun build

# linuxdvb git tree
$(ROOTDIR)/data/dvb-scan/.stamp:
	@echo "Receiving data/dvb-scan/dvb-t from http://linuxtv.org/git/dtv-scan-tables.git"
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
	@$(ROOTDIR)/support/sat_xml_scan.py \
		$(ROOTDIR)/data/satellites.xml $(ROOTDIR)/data/dvb-scan/dvb-s
	@touch $(ROOTDIR)/data/dvb-scan/dvb-s/.stamp

.PHONY: satellites_xml
satellites_xml: $(ROOTDIR)/data/dvb-scan/dvb-s/.stamp
