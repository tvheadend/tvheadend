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

CFLAGS  = -Wall -Werror -Wwrite-strings -Wno-deprecated-declarations 
CFLAGS += -Wmissing-prototypes
LDFLAGS += -lrt

#
# Core
#
SRCS =  src/main.c \
	src/wrappers.c \
	src/version.c \
	src/access.c \
	src/dtable.c \
	src/tcp.c \
	src/http.c \
	src/notify.c \
	src/epg.c \
	src/xmltv.c \
	src/spawn.c \
	src/packet.c \
	src/streaming.c \
	src/teletext.c \
	src/channels.c \
	src/subscriptions.c \
	src/transports.c \
	src/psi.c \
	src/parsers.c \
	src/parser_h264.c \
	src/parser_latm.c \
	src/tsdemux.c \
	src/bitstream.c \
	src/rtsp.c \
	src/rtp.c \
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
	src/v4l.c


SRCS += src/dvr/dvr_db.c \
	src/dvr/dvr_rec.c \
	src/dvr/dvr_autorec.c

SRCS += src/dvb/dvb.c \
	src/dvb/dvb_support.c \
	src/dvb/dvb_fe.c \
	src/dvb/dvb_tables.c \
	src/dvb/diseqc.c \
	src/dvb/dvb_adapter.c \
	src/dvb/dvb_multiplex.c \
	src/dvb/dvb_transport.c \
	src/dvb/dvb_preconf.c \
	src/dvb/dvb_satconf.c \

#
# cwc
#
SRCS += src/cwc.c \
	src/capmt.c \
	src/krypt.c \
	src/ffdecsa/ffdecsa_interface.c \
	src/ffdecsa/ffdecsa_int.c

SRCS-${CONFIG_MMX}  += src/ffdecsa/ffdecsa_mmx.c
SRCS-${CONFIG_SSE2} += src/ffdecsa/ffdecsa_sse2.c

LDFLAGS += -lcrypt

${BUILDDIR}/src/ffdecsa/ffdecsa_mmx.o  : CFLAGS = -mmmx
${BUILDDIR}/src/ffdecsa/ffdecsa_sse2.o : CFLAGS = -msse2

#
# Primary web interface
#
SRCS += src/webui/webui.c \
	src/webui/comet.c \
	src/webui/extjs.c \
	src/webui/simpleui.c \

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
OBJDIRS= $(sort $(dir $(OBJS))) $(sort $(dir $(OBJS_EXTRA)))

# File bundles
BUNDLE_SRCS=$(BUNDLES:%=$(BUILDDIR)/bundles/%.c)
BUNDLE_DEPS=$(BUNDLE_SRCS:%.c=%.d)
BUNDLE_OBJS=$(BUNDLE_SRCS:%.c=%.o)
OBJDIRS+= $(sort $(dir $(BUNDLE_OBJS)))
.PRECIOUS: ${BUNDLE_SRCS}

# Common CFLAGS for all files
CFLAGS_com  = -g -funsigned-char -O2 
CFLAGS_com += -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
CFLAGS_com += -I${BUILDDIR} -I${CURDIR}/src -I${CURDIR}

all: ${PROG}

.PHONY:	clean distclean ffmpeg

${PROG}: ${BUILDDIR}/ffmpeg/install $(OBJDIRS) $(OBJS) $(BUNDLE_OBJS) ${OBJS_EXTRA} Makefile
	$(CC) -o $@ $(OBJS) $(BUNDLE_OBJS) $(LDFLAGS) ${LDFLAGS_cfg}

$(OBJDIRS):
	@mkdir -p $@

${BUILDDIR}/%.o: %.c  ${BUILDDIR}/ffmpeg/install
	$(CC) -MD $(CFLAGS_com) $(CFLAGS) $(CFLAGS_cfg) -c -o $@ $(CURDIR)/$<

${BUILDDIR}/ffmpeg/install ffmpeg:
	cd ${BUILDDIR}/ffmpeg/build && ${MAKE} all
	cd ${BUILDDIR}/ffmpeg/build && ${MAKE} install

${BUILDDIR}/%.so: ${SRCS_EXTRA}
	${CC} -O -fbuiltin -fomit-frame-pointer -fPIC -shared -o $@ $< -ldl

clean:
	rm -rf ${BUILDDIR}/src ${BUILDDIR}/bundles
	find . -name "*~" | xargs rm -f

distclean: clean
	rm -rf build.*

# Create tvheadendversion.h
$(BUILDDIR)/tvheadendversion.h:
	$(CURDIR)/support/version.sh $(CURDIR) $(BUILDDIR)/tvheadendversion.h

src/version.c: $(BUILDDIR)/tvheadendversion.h

# Include dependency files if they exist.
-include $(DEPS) $(BUNDLE_DEPS)

# Include OS specific targets
include support/${OSENV}.mk

# Bundle files
$(BUILDDIR)/bundles/%.o: $(BUILDDIR)/bundles/%.c
	$(CC) -I${CURDIR}/src -c -o $@ $<

$(BUILDDIR)/bundles/%.c: % $(CURDIR)/support/mkbundle
	$(CURDIR)/support/mkbundle \
		-o $@ -s $< -d ${BUILDDIR}/bundles/$<.d -p $< -z
