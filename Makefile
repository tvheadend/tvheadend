-include ../config.mak

SRCS = main.c dispatch.c channels.c transports.c teletext.c psi.c \
	subscriptions.c mux.c tsdemux.c buffer.c tcp.c \
	resolver.c tsmux.c parsers.c bitstream.c parser_h264.c spawn.c \
	notify.c intercom.c access.c serviceprobe.c dtable.c

SRCS += http.c

SRCS += htsp.c htsp_muxer.c rpc.c

SRCS += pvr.c autorec.c ffmuxer.c

SRCS += epg.c epg_xmltv.c

VPATH += dvb
SRCS += dvb.c dvb_support.c dvb_dvr.c dvb_preconf.c dvb_fe.c dvb_tables.c \
	diseqc.c dvb_adapter.c dvb_multiplex.c dvb_transport.c

SRCS += iptv_input.c iptv_output.c

SRCS += avgen.c file_input.c

SRCS +=	htsclient.c rtsp.c rtp.c xbmsp.c

SRCS += v4l.c

SRCS += cwc.c krypt.c

VPATH += ffdecsa
SRCS += FFdecsa.c

#
# Primary web interface
#

VPATH  += webui
SRCS   += webui.c extjs.c comet.c

JSSRCS += tvheadend.js extensions.js acleditor.js cwceditor.js \
	dvb.js

CSSSRCS += ext.css

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

# XML

DLIBS 	+= ${LIBXML2_DLIBS}
SLIBS 	+= ${LIBXML2_SLIBS}
CFLAGS 	+= ${LIBXML2_CFLAGS}


DLIBS += -lpthread

include ../build/prog.mk


