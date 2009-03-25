-include ../config.mak

VPATH += src
SRCS =  main.c \
	access.c \
	dtable.c \
	tcp.c \
	http.c \
	notify.c \
	epg.c \
	xmltv.c \
	spawn.c \
	packet.c \
	streaming.c \
	teletext.c \
	channels.c \
	subscriptions.c \
	transports.c \
	psi.c \
	parsers.c \
	parser_h264.c \
	tsdemux.c \
	bitstream.c \
	htsp.c \
	serviceprobe.c

VPATH += src/dvr
SRCS += dvr_db.c \
	dvr_rec.c \
	dvr_autorec.c

VPATH += src/dvb
SRCS += dvb.c \
	dvb_support.c \
	dvb_fe.c \
	dvb_tables.c \
	diseqc.c \
	dvb_adapter.c \
	dvb_multiplex.c \
	dvb_transport.c \
	dvb_preconf.c


#
# cwc
#
SRCS  += cwc.c krypt.c
VPATH += src/ffdecsa
SRCS  += FFdecsa.c

#
# Primary web interface
#
VPATH += src/webui
SRCS += webui.c \
	comet.c \
	extjs.c

PROGPATH = $(HTS_BUILD_ROOT)/tvheadend
PROG = tvheadend
MAN  = tvheadend.1
CFLAGS += -g -Wall -Werror -funsigned-char -O2 -mmmx
CFLAGS += -I$(INCLUDES_INSTALL_BASE) -I$(CURDIR)/src
CFLAGS += -Wno-deprecated-declarations -Wmissing-prototypes
CFLAGS += -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
CFLAGS += -I$(CURDIR) -I$(INCLUDES_INSTALL_BASE)
LDFLAGS += -L$(LIBS_INSTALL_BASE)

DLIBS  += ${TVHEADEND_DLIBS}  ${HTS_DLIBS} -lcrypt
SLIBS  += ${TVHEADEND_SLIBS}  ${HTS_SLIBS}
CFLAGS += ${TVHEADEND_CFLAGS} ${HTS_CFLAGS}

include ../build/prog.mk


#
# Install
#
prefix ?= $(INSTALLPREFIX)
INSTBIN= $(prefix)/bin
INSTMAN= $(prefix)/share/man/man1
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

uninstall:
	rm -f $(INSTBIN)/${PROG}
	rm -f $(INSTMAN)/${MAN}
	rm -rf $(INSTSHARE)
