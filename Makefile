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

#
# Common compiler flags
#

CFLAGS  += -Wall -Werror -Wwrite-strings -Wno-deprecated-declarations
CFLAGS  += -Wmissing-prototypes -fms-extensions
CFLAGS  += -g -funsigned-char -O2 
CFLAGS  += -D_FILE_OFFSET_BITS=64
CFLAGS  += -I${BUILDDIR} -I${ROOTDIR}/src -I${ROOTDIR}
LDFLAGS += -lrt -ldl -lpthread -lm

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
SRCS =  src/version.c \
	src/main.c \
	src/tvhlog.c \
	src/idnode.c \
	src/prop.c \
	src/utils.c \
	src/wrappers.c \
	src/access.c \
	src/dtable.c \
	src/tcp.c \
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
	src/config2.c \
	src/lang_codes.c \
	src/lang_str.c \
	src/imagecache.c \
	src/tvhtime.c \
	src/descrambler/descrambler.c \
	src/service_mapper.c \
	src/input.c \
	src/http/http_client.c \
	src/fsmonitor.c \

SRCS += \
	src/api.c \
	src/api/api_status.c \
	src/api/api_idnode.c \
	src/api/api_input.c \
	src/api/api_channel.c \
	src/api/api_service.c \
	src/api/api_mpegts.c \
  src/api/api_epg.c \
  src/api/api_epggrab.c \
  src/api/api_imagecache.c

SRCS += \
	src/parsers/parsers.c \
	src/parsers/bitstream.c \
	src/parsers/parser_h264.c \
	src/parsers/parser_latm.c \
	src/parsers/parser_avc.c \
	src/parsers/parser_teletext.c \

SRCS += src/epggrab/module.c\
	src/epggrab/channel.c\
	src/epggrab/module/pyepg.c\
	src/epggrab/module/xmltv.c\

SRCS += src/plumbing/tsfix.c \
	src/plumbing/globalheaders.c

SRCS += src/dvr/dvr_db.c \
	src/dvr/dvr_rec.c \
	src/dvr/dvr_autorec.c \

SRCS += src/webui/webui.c \
	src/webui/comet.c \
	src/webui/extjs.c \
	src/webui/simpleui.c \
	src/webui/statedump.c \
	src/webui/html.c\
	src/webui/webui_api.c\

SRCS += src/muxer.c \
	src/muxer/muxer_pass.c \
	src/muxer/muxer_tvh.c \
	src/muxer/tvh/ebml.c \
	src/muxer/tvh/mkmux.c \

#
# Optional code
#

# MPEGTS core
SRCS-$(CONFIG_MPEGTS) += \
	src/input/mpegts/mpegts_input.c \
	src/input/mpegts/mpegts_network.c \
	src/input/mpegts/mpegts_mux.c \
	src/input/mpegts/mpegts_service.c \
	src/input/mpegts/mpegts_table.c \
	src/input/mpegts/dvb_support.c \
	src/input/mpegts/dvb_charset.c \
	src/input/mpegts/dvb_psi.c \
	src/input/mpegts/tsdemux.c \

# MPEGTS EPG
SRCS-$(CONFIG_MPEGTS) += \
	src/epggrab/otamux.c\
	src/epggrab/module/eit.c \
	src/epggrab/support/freesat_huffman.c \
	src/epggrab/module/opentv.c \

# DVB
SRCS-${CONFIG_LINUXDVB} += \
        src/input/mpegts/linuxdvb/linuxdvb.c \
        src/input/mpegts/linuxdvb/linuxdvb_device.c \
        src/input/mpegts/linuxdvb/linuxdvb_adapter.c \
        src/input/mpegts/linuxdvb/linuxdvb_frontend.c \
        src/input/mpegts/linuxdvb/linuxdvb_network.c \
        src/input/mpegts/linuxdvb/linuxdvb_mux.c \
        src/input/mpegts/linuxdvb/linuxdvb_service.c \
        src/input/mpegts/linuxdvb/linuxdvb_satconf.c \
        src/input/mpegts/linuxdvb/linuxdvb_lnb.c \
        src/input/mpegts/linuxdvb/linuxdvb_switch.c \
        src/input/mpegts/linuxdvb/linuxdvb_rotor.c \
        src/input/mpegts/linuxdvb/scanfile.c \

# IPTV
SRCS-${CONFIG_IPTV} += \
	src/input/mpegts/iptv/iptv.c \
        src/input/mpegts/iptv/iptv_mux.c \
        src/input/mpegts/iptv/iptv_service.c \
        src/input/mpegts/iptv/iptv_http.c \
        src/input/mpegts/iptv/iptv_udp.c \

# TSfile
SRCS-$(CONFIG_TSFILE) += \
        src/input/mpegts/tsfile/tsfile.c \
        src/input/mpegts/tsfile/tsfile_input.c \
        src/input/mpegts/tsfile/tsfile_mux.c \

# Timeshift
SRCS-${CONFIG_TIMESHIFT} += \
	src/timeshift.c \
	src/timeshift/timeshift_filemgr.c \
	src/timeshift/timeshift_writer.c \
	src/timeshift/timeshift_reader.c \

# Inotify
SRCS-${CONFIG_INOTIFY} += \
	src/dvr/dvr_inotify.c \

# Avahi
SRCS-$(CONFIG_AVAHI) += src/avahi.c

# libav
SRCS-$(CONFIG_LIBAV) += src/libav.c \
	src/muxer/muxer_libav.c \
	src/plumbing/transcoding.c \

# CWC
SRCS-${CONFIG_CWC} += \
	src/descrambler/tvhcsa.c \
	src/descrambler/cwc.c \
	src/descrambler/capmt.c

# FFdecsa
ifneq ($(CONFIG_DVBCSA),yes)
SRCS-${CONFIG_CWC}  += \
	src/descrambler/ffdecsa/ffdecsa_interface.c \
	src/descrambler/ffdecsa/ffdecsa_int.c
ifeq ($(CONFIG_CWC),yes)
SRCS-${CONFIG_MMX}  += src/descrambler/ffdecsa/ffdecsa_mmx.c
SRCS-${CONFIG_SSE2} += src/descrambler/ffdecsa/ffdecsa_sse2.c
endif
${BUILDDIR}/src/descrambler/ffdecsa/ffdecsa_mmx.o  : CFLAGS += -mmmx
${BUILDDIR}/src/descrambler/ffdecsa/ffdecsa_sse2.o : CFLAGS += -msse2
endif

# File bundles
SRCS-${CONFIG_BUNDLE}     += bundle.c
BUNDLES-yes               += docs/html docs/docresources src/webui/static
BUNDLES-yes               += data/conf
BUNDLES-${CONFIG_DVBSCAN} += data/dvb-scan
BUNDLES                    = $(BUNDLES-yes)

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

#
# Build Rules
#

# Default
all: ${PROG}

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
${PROG}: check_config $(OBJS) $(ALLDEPS)
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
	rm -rf ${BUILDDIR}/src ${BUILDDIR}/bundle*
	find . -name "*~" | xargs rm -f

distclean: clean
	rm -rf ${ROOTDIR}/build.*
	rm -f ${ROOTDIR}/.config.mk

# Create version
$(BUILDDIR)/src/version.o: $(ROOTDIR)/src/version.c
$(ROOTDIR)/src/version.c: FORCE
	@$(ROOTDIR)/support/version $@ > /dev/null
FORCE:

# Include dependency files if they exist.
-include $(DEPS)

# Include OS specific targets
include ${ROOTDIR}/support/${OSENV}.mk

# Bundle files
$(BUILDDIR)/bundle.o: $(BUILDDIR)/bundle.c
	@mkdir -p $(dir $@)
	$(CC) -I${ROOTDIR}/src -c -o $@ $<

$(BUILDDIR)/bundle.c:
	@mkdir -p $(dir $@)
	$(MKBUNDLE) -o $@ -d ${BUILDDIR}/bundle.d $(BUNDLE_FLAGS) $(BUNDLES:%=$(ROOTDIR)/%)
