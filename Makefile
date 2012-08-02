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

include ${CURDIR}/config.default

BUILDDIR = build.${PLATFORM}

include ${BUILDDIR}/config.mak

PROG=${BUILDDIR}/tvheadend

CFLAGS += -Wall -Werror -Wwrite-strings -Wno-deprecated-declarations
CFLAGS += -Wmissing-prototypes -fms-extensions
LDFLAGS += -lrt -ldl

BUNDLES += docs/html docs/docresources src/webui/static data

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
	src/htsp.c \
	src/serviceprobe.c \
	src/htsmsg.c \
	src/htsmsg_binary.c \
	src/htsmsg_json.c \
	src/htsmsg_xml.c \
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

SRCS-${CONFIG_LINUXDVB} += \
	src/dvb/dvb.c \
	src/dvb/dvb_support.c \
	src/dvb/dvb_fe.c \
	src/dvb/dvb_tables.c \
	src/dvb/diseqc.c \
	src/dvb/dvb_adapter.c \
	src/dvb/dvb_multiplex.c \
	src/dvb/dvb_transport.c \
	src/dvb/dvb_preconf.c \
	src/dvb/dvb_satconf.c \
	src/webui/extjs_dvb.c \

SRCS-${CONFIG_V4L} += \
	src/v4l.c \
	src/webui/extjs_v4l.c \


#
# cwc
#
SRCS += src/cwc.c \
	src/capmt.c \
	src/ffdecsa/ffdecsa_interface.c \
	src/ffdecsa/ffdecsa_int.c

SRCS-${CONFIG_MMX}  += src/ffdecsa/ffdecsa_mmx.c
SRCS-${CONFIG_SSE2} += src/ffdecsa/ffdecsa_sse2.c

${BUILDDIR}/src/ffdecsa/ffdecsa_mmx.o  : CFLAGS = -mmmx
${BUILDDIR}/src/ffdecsa/ffdecsa_sse2.o : CFLAGS = -msse2

#
# Primary web interface
#
SRCS += src/webui/webui.c \
	src/webui/comet.c \
	src/webui/extjs.c \
	src/webui/simpleui.c \
	src/webui/statedump.c \

#
# Extra modules
#
SRCS_EXTRA = src/extra/capmt_ca.c

#
# AVAHI interface
# 

SRCS-$(CONFIG_AVAHI) += src/avahi.c

${BUILDDIR}/src/avahi.o : CFLAGS = \
                      $(shell pkg-config --cflags avahi-client) -Wall -Werror

# Various transformations
SRCS  += $(SRCS-yes)
DLIBS += $(DLIBS-yes)
SLIBS += $(SLIBS-yes)
OBJS=    $(SRCS:%.c=$(BUILDDIR)/%.o)
OBJS_EXTRA = $(SRCS_EXTRA:%.c=$(BUILDDIR)/%.so)
DEPS=    ${OBJS:%.o=%.d}

# File bundles
BUNDLE_SRCS=$(BUILDDIR)/bundle.c
BUNDLE_DEPS=$(BUNDLE_SRCS:%.c=%.d)
BUNDLE_OBJS=$(BUNDLE_SRCS:%.c=%.o)
.PRECIOUS: ${BUNDLE_SRCS}


# Common CFLAGS for all files
CFLAGS_com  = -g -funsigned-char -O2 
CFLAGS_com += -D_FILE_OFFSET_BITS=64
CFLAGS_com += -I${BUILDDIR} -I${CURDIR}/src -I${CURDIR}

MKBUNDLE = $(CURDIR)/support/mkbundle.py

ifndef V
ECHO   = printf "$(1)\t\t%s\n" $(2)
BRIEF  = CC MKBUNDLE CXX
MSG    = $@
$(foreach VAR,$(BRIEF), \
    $(eval $(VAR) = @$$(call ECHO,$(VAR),$$(MSG)); $($(VAR))))
endif


all: ${PROG}

.PHONY:	clean distclean

#
#
#
${PROG}: $(OBJS) $(ALLDEPS)  support/dataroot/wd.c
	$(CC) -o $@ $(OBJS) support/dataroot/wd.c $(LDFLAGS) ${LDFLAGS_cfg}

${PROG}.bundle: $(OBJS) $(BUNDLE_OBJS) $(ALLDEPS)  support/dataroot/bundle.c
	$(CC) -o $@ $(OBJS) support/dataroot/bundle.c $(BUNDLE_OBJS) $(LDFLAGS) ${LDFLAGS_cfg}

${PROG}.datadir: $(OBJS) $(ALLDEPS)  support/dataroot/datadir.c
	$(CC) -o $@ $(OBJS) -iquote${BUILDDIR} support/dataroot/datadir.c $(LDFLAGS) ${LDFLAGS_cfg}

#
#
#
${BUILDDIR}/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -MD -MP $(CFLAGS_com) $(CFLAGS) $(CFLAGS_cfg) -c -o $@ $(CURDIR)/$<

${BUILDDIR}/%.so: ${SRCS_EXTRA}
	@mkdir -p $(dir $@)
	${CC} -O -fbuiltin -fomit-frame-pointer -fPIC -shared -o $@ $< -ldl

clean:
	rm -rf ${BUILDDIR}/src ${BUILDDIR}/bundle*
	find . -name "*~" | xargs rm -f

distclean: clean
	rm -rf build.*

# Create buildversion.h
src/version.c: $(BUILDDIR)/buildversion.h
$(BUILDDIR)/buildversion.h: FORCE
	@$(CURDIR)/support/version.sh $(CURDIR) $@
FORCE:


# Include dependency files if they exist.
-include $(DEPS) $(BUNDLE_DEPS)

# Include OS specific targets
include support/${OSENV}.mk

# Bundle files
$(BUILDDIR)/bundle.o: $(BUILDDIR)/bundle.c
	@mkdir -p $(dir $@)
	$(CC) -I${CURDIR}/src -c -o $@ $<

$(BUILDDIR)/bundle.c:
	@mkdir -p $(dir $@)
	$(MKBUNDLE) -o $@ -d ${BUILDDIR}/bundle.d -z $(BUNDLES)
