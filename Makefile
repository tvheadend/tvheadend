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

include ${CURDIR}/.config.mk
PROG = ${BUILDDIR}/tvheadend

#
# Common compiler flags
#

CFLAGS  += -Wall -Werror -Wwrite-strings -Wno-deprecated-declarations
CFLAGS  += -Wmissing-prototypes -fms-extensions
CFLAGS  += -g -funsigned-char -O2 
CFLAGS  += -D_FILE_OFFSET_BITS=64
CFLAGS  += -I${BUILDDIR} -I${CURDIR}/src -I${CURDIR}
LDFLAGS += -lrt -ldl -lpthread -lm

#
# Other config
#

BUNDLE_FLAGS-${CONFIG_ZLIB} = -z
BUNDLE_FLAGS = ${BUNDLE_FLAGS-yes}

#
# Binaries/Scripts
#

MKBUNDLE = $(PYTHON) $(CURDIR)/support/mkbundle

#
# Debug/Output
#

ifndef V
ECHO   = printf "$(1)\t\t%s\n" $(2)
BRIEF  = CC MKBUNDLE CXX
MSG    = $@
$(foreach VAR,$(BRIEF), \
    $(eval $(VAR) = @$$(call ECHO,$(VAR),$$(MSG)); $($(VAR))))
endif

#
# Core
#
SRCS =  src/main.c \
	src/utils.c \
	src/wrappers.c \
	src/version.c \
	src/access.c \
	src/dtable.c \
	src/tcp.c \
	src/http.c \
	src/notify.c \
	src/file.c \
	src/epg.c \
	src/epgdb.c\
	src/epggrab.c\
	src/spawn.c \
	src/packet.c \
	src/streaming.c \
	src/teletext.c \
	src/channels.c \
	src/subscriptions.c \
	src/service.c \
	src/psi.c \
	src/parsers.c \
	src/parser_h264.c \
	src/parser_latm.c \
	src/tsdemux.c \
	src/bitstream.c \
	src/htsp_server.c \
	src/serviceprobe.c \
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
	src/rawtsinput.c \
	src/iptv_input.c \
	src/avc.c \
  src/huffman.c \
  src/filebundle.c \
  src/muxes.c \
  src/config2.c \
  src/lang_codes.c \
  src/lang_str.c \

SRCS += src/epggrab/module.c\
  src/epggrab/channel.c\
  src/epggrab/otamux.c\
  src/epggrab/module/pyepg.c\
  src/epggrab/module/xmltv.c\
  src/epggrab/module/eit.c \
  src/epggrab/module/opentv.c \
  src/epggrab/support/freesat_huffman.c \

SRCS += src/plumbing/tsfix.c \
	src/plumbing/globalheaders.c \

SRCS += src/dvr/dvr_db.c \
	src/dvr/dvr_rec.c \
	src/dvr/dvr_autorec.c \
	src/dvr/ebml.c \
	src/dvr/mkmux.c \

SRCS += src/webui/webui.c \
	src/webui/comet.c \
	src/webui/extjs.c \
	src/webui/simpleui.c \
	src/webui/statedump.c \
	src/webui/html.c\

SRCS += src/muxer.c \
	src/muxer_pass.c \
	src/muxer_tvh.c \

#
# Optional code
#

# DVB
SRCS-${CONFIG_LINUXDVB} += \
	src/dvb/dvb.c \
	src/dvb/dvb_support.c \
	src/dvb/dvb_charset.c \
	src/dvb/dvb_fe.c \
	src/dvb/dvb_tables.c \
	src/dvb/diseqc.c \
	src/dvb/dvb_adapter.c \
	src/dvb/dvb_multiplex.c \
	src/dvb/dvb_service.c \
	src/dvb/dvb_preconf.c \
	src/dvb/dvb_satconf.c \
	src/dvb/dvb_input_filtered.c \
	src/dvb/dvb_input_raw.c \
	src/webui/extjs_dvb.c \

# V4L
SRCS-${CONFIG_V4L} += \
	src/v4l.c \
	src/webui/extjs_v4l.c \

# CWC
SRCS-${CONFIG_CWC} += src/cwc.c \
	src/capmt.c \
	src/ffdecsa/ffdecsa_interface.c \
	src/ffdecsa/ffdecsa_int.c

# Avahi
SRCS-$(CONFIG_AVAHI) += src/avahi.c

# Optimised code
SRCS-${CONFIG_MMX}  += src/ffdecsa/ffdecsa_mmx.c
SRCS-${CONFIG_SSE2} += src/ffdecsa/ffdecsa_sse2.c
${BUILDDIR}/src/ffdecsa/ffdecsa_mmx.o  : CFLAGS += -mmmx
${BUILDDIR}/src/ffdecsa/ffdecsa_sse2.o : CFLAGS += -msse2

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
.PHONY:	clean distclean

# Binary
${PROG}: $(OBJS) $(ALLDEPS)
	$(CC) -o $@ $(OBJS) $(CFLAGS) $(LDFLAGS)

# Object
${BUILDDIR}/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -MD -MP $(CFLAGS) -c -o $@ $(CURDIR)/$<

# Add-on
${BUILDDIR}/%.so: ${SRCS_EXTRA}
	@mkdir -p $(dir $@)
	${CC} -O -fbuiltin -fomit-frame-pointer -fPIC -shared -o $@ $< -ldl

# Clean
clean:
	rm -rf ${BUILDDIR}/src ${BUILDDIR}/bundle*
	find . -name "*~" | xargs rm -f

distclean: clean
	rm -rf ${CURDIR}/build.*
	rm -f ${CURDIR}/.config.mk

# Create buildversion.h
src/version.c: FORCE
	@$(CURDIR)/support/version $@ > /dev/null
FORCE:

# Include dependency files if they exist.
-include $(DEPS)

# Include OS specific targets
include support/${OSENV}.mk

# Bundle files
$(BUILDDIR)/bundle.o: $(BUILDDIR)/bundle.c
	@mkdir -p $(dir $@)
	$(CC) -I${CURDIR}/src -c -o $@ $<

$(BUILDDIR)/bundle.c:
	@mkdir -p $(dir $@)
	$(MKBUNDLE) -o $@ -d ${BUILDDIR}/bundle.d $(BUNDLE_FLAGS) $(BUNDLES)
