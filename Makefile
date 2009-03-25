include config.mak

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
	serviceprobe.c \
	htsmsg.c \
	htsmsg_binary.c \
	htsmsg_json.c \
	htsmsg_xml.c \
	settings.c \
	htsbuf.c \
	parachute.c \
	avg.c \
	htsstr.c \

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

PROGPATH = $(TOPDIR)
PROG = tvheadend
MAN  = tvheadend.1
CFLAGS += -g -Wall -Werror -funsigned-char -O2 -mmmx
CFLAGS += -I$(TOPDIR)/src -I$(TOPDIR)
CFLAGS += -Wno-deprecated-declarations -Wmissing-prototypes
CFLAGS += -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64

LDFLAGS  += -lcrypt -lm -lz -lpthread

SRCS += $(SRCS-yes)
DLIBS += $(DLIBS-yes)
SLIBS += $(SLIBS-yes)

.OBJDIR= obj
DEPFLAG= -M

OBJS=    $(patsubst %.c,  %.o,   $(SRCS))

DEPS= ${OBJS:%.o=%.d}

SRCS += version.c

PROGPATH ?= $(HTS_BUILD_ROOT)/$(PROG)

all:	$(PROG)

.PHONY: version.h

version.h:
	$(TOPDIR)/version.sh $(PROGPATH) $(PROGPATH)/version.h


${PROG}: version.h $(OBJS) Makefile
	cd $(.OBJDIR) && $(CC) -o $@ $(OBJS) $(LDFLAGS) 


.c.o:
	mkdir -p $(.OBJDIR) && cd $(.OBJDIR) && $(CC) -MD $(CFLAGS) -c -o $@ $(CURDIR)/$<

clean:
	rm -rf core* obj version.h
	find . -name "*~" | xargs rm -f

vpath %.o ${.OBJDIR}
vpath %.S ${.OBJDIR}
vpath ${PROG} ${.OBJDIR}
vpath ${PROGBIN} ${.OBJDIR}

# include dependency files if they exist
$(addprefix ${.OBJDIR}/, ${DEPS}): ;
-include $(addprefix ${.OBJDIR}/, ${DEPS})



#
# Install
#
INSTBIN= $(prefix)/bin
INSTMAN= $(prefix)/share/man/man1
INSTSHARE= $(prefix)/share/hts/tvheadend

install: ${PROG}
	mkdir -p $(INSTBIN)
	cd $(.OBJDIR) && install -s ${PROG} $(INSTBIN)

	mkdir -p $(INSTMAN)
	cd src/man && install ${MAN} $(INSTMAN)

	mkdir -p $(INSTSHARE)/docs/html
	cp  src/docs/html/*.html $(INSTSHARE)/docs/html

	mkdir -p $(INSTSHARE)/docs/docresources
	cp  src/docs/docresources/*.png $(INSTSHARE)/docs/docresources

	cd src && find webui/static/ -type d |grep -v .svn | awk '{print "$(INSTSHARE)/"$$0}' | xargs mkdir -p 
	cd src && find webui/static/ -type f |grep -v .svn | awk '{print $$0 " $(INSTSHARE)/"$$0}' | xargs -n2 cp

uninstall:
	rm -f $(INSTBIN)/${PROG}
	rm -f $(INSTMAN)/${MAN}
	rm -rf $(INSTSHARE)

distclean: clean
	rm -rf ffmpeg config.h config.mak
