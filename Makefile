include ../config.mak

SRCS = main.c dispatch.c channels.c transports.c teletext.c

SRCS += pvr.c pvr_rec.c

SRCS += epg.c epg_xmltv.c

SRCS += dvb_support.c dvb_pmt.c dvb_dvr.c

SRCS += input_dvb.c input_iptv.c #input_v4l.c

SRCS +=	output_client.c output_multicast.c

PROG = tvhead
CFLAGS += -g -Wall -Werror -O2
CFLAGS += -I$(INCLUDES_INSTALL_BASE)
CFLAGS += -Wno-deprecated-declarations
CFLAGS += -D_LARGEFILE64_SOURCE
CFLAGS += -DENABLE_INPUT_IPTV -DENABLE_INPUT_DVB
LDFLAGS += -L$(LIBS_INSTALL_BASE)

DLIBS += -lhts 

#
# ffmpeg

DLIBS += -lavformat -lavcodec -lavutil -lz
DLIBS += $(LIBA52_DLIBS) $(LIBVORBIS_DLIBS) $(LIBOGG_DLIBS) $(LIBFAAD_DLIBS)
SLIBS += $(LIBA52_SLIBS) $(LIBVORBIS_SLIBS) $(LIBOGG_SLIBS) $(LIBFAAD_SLIBS)


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

all:	$(PROG)

install:
	cd $(.OBJDIR) && install ${PROG} $(CURDIR)/../bin

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
