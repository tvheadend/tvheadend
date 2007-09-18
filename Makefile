-include ../config.mak

SRCS = main.c dispatch.c channels.c transports.c teletext.c psi.c

SRCS += pvr.c pvr_rec.c

SRCS += epg.c epg_xmltv.c

SRCS += dvb.c dvb_support.c dvb_dvr.c dvb_muxconfig.c

SRCS += iptv_input.c iptv_output.c

SRCS +=	htsclient.c rtsp.c rtp.c

SRCS += v4l.c

PROG = tvheadend
CFLAGS += -g -Wall -Werror -O2
CFLAGS += -I$(INCLUDES_INSTALL_BASE) $(HTS_CFLAGS)
CFLAGS += -Wno-deprecated-declarations
CFLAGS += -D_LARGEFILE64_SOURCE
CFLAGS += -DENABLE_INPUT_IPTV -DENABLE_INPUT_DVB -DENABLE_INPUT_V4L
LDFLAGS += -L$(LIBS_INSTALL_BASE)

SLIBS += ${LIBHTS_SLIBS}
DLIBS += ${LIBHTS_DLIBS}

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


#############################################################################
# 
#

.OBJDIR=        obj
DEPFLAG = -M

OBJS = $(patsubst %.c,%.o, $(SRCS))
OBJS += $(patsubst %.arbfp1,%.o, $(CG_SRCS))
DEPS= ${OBJS:%.o=%.d}

prefix ?= $(INSTALLPREFIX)
INSTDIR= $(prefix)/bin

all:	$(PROG)

install:
	mkdir -p $(INSTDIR)
	cd $(.OBJDIR) && install -s ${PROG} $(INSTDIR)

${PROG}: $(.OBJDIR) $(OBJS) Makefile
	cd $(.OBJDIR) && $(CC) $(LDFLAGS) -o $@ $(OBJS) \
	$(STATIC_LINKFLAGS) $(SLIBS) $(DYNAMIC_LINKFLAGS) $(DLIBS)

$(.OBJDIR):
	mkdir $(.OBJDIR)

.c.o:	Makefile
	cd $(.OBJDIR) && $(CC) -MD $(CFLAGS) -c -o $@ $(CURDIR)/$<

clean:
	rm -rf $(.OBJDIR) *~ core*

vpath %.o ${.OBJDIR}
vpath %.S ${.OBJDIR}
vpath ${PROG} ${.OBJDIR}
vpath ${PROGBIN} ${.OBJDIR}

# include dependency files if they exist
$(addprefix ${.OBJDIR}/, ${DEPS}): ;
-include $(addprefix ${.OBJDIR}/, ${DEPS})
