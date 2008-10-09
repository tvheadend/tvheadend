-include ../config.mak

SRCS = main.c access.c dtable.c tcp.c http.c notify.c epg.c xmltv.c spawn.c

SRCS += packet.c streaming.c teletext.c

VPATH += dvr
SRCS += dvr_db.c dvr_rec.c dvr_autorec.c

SRCS += htsp.c

SRCS += channels.c subscriptions.c transports.c

SRCS += psi.c parsers.c parser_h264.c tsdemux.c bitstream.c

VPATH += dvb
SRCS += dvb.c dvb_support.c dvb_fe.c dvb_tables.c \
	diseqc.c dvb_adapter.c dvb_multiplex.c dvb_transport.c dvb_preconf.c

SRCS += serviceprobe.c

SRCS += cwc.c krypt.c
VPATH += ffdecsa
SRCS += FFdecsa.c

#
# Primary web interface
#

VPATH  += webui
SRCS   += webui.c comet.c extjs.c

PROGPATH = $(HTS_BUILD_ROOT)/tvheadend
PROG = tvheadend
MAN  = tvheadend.1
CFLAGS += -g -Wall -Werror -O2 -mmmx
CFLAGS += -I$(INCLUDES_INSTALL_BASE) $(HTS_CFLAGS) -I$(CURDIR)
CFLAGS += -Wno-deprecated-declarations -Wmissing-prototypes
CFLAGS += -D_LARGEFILE64_SOURCE
CFLAGS += -DENABLE_INPUT_IPTV -DENABLE_INPUT_DVB -DENABLE_INPUT_V4L
LDFLAGS += -L$(LIBS_INSTALL_BASE)

SLIBS += ${LIBHTS_SLIBS}
DLIBS += ${LIBHTS_DLIBS} -lcrypt

#
# ffmpeg

DLIBS  += $(FFMPEG_DLIBS)
SLIBS  += $(FFMPEG_SLIBS)
CFLAGS += $(FFMPEG_CFLAGS)

DLIBS += -lpthread -lm -lz

include ../build/prog.mk


#
# Install
#
prefix ?= $(INSTALLPREFIX)
INSTBIN= $(prefix)/bin
INSTMAN= $(prefix)/share/man1
INSTSHARE= $(prefix)/share/hts/tvheadend

install: ${PROG}
	mkdir -p $(INSTBIN)
	cd $(.OBJDIR) && install -s ${PROG} $(INSTBIN)

	mkdir -p $(INSTMAN)
	cd man && install ${MAN} $(INSTMAN)

	mkdir -p $(INSTSHARE)/docs/html
	cp  docs/html/*.html $(INSTSHARE)/docs/html

	mkdir -p $(INSTSHARE)/docs/docresources
	cp  docs/docresources/*.png $(INSTSHARE)/docs/docresources

	find webui/static/ -type d |grep -v .svn | awk '{print "$(INSTSHARE)/"$$0}' | xargs mkdir -p 
	find webui/static/ -type f |grep -v .svn | awk '{print $$0 " $(INSTSHARE)/"$$0}' | xargs -n2 cp
