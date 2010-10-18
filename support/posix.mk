
prefix ?= $(INSTALLPREFIX)
INSTBIN= $(prefix)/bin
INSTMAN= $(prefix)/share/man1
MAN=man/tvheadend.1

install: ${PROG} ${MAN}
	mkdir -p ${DESTDIR}$(INSTBIN)
	install -s ${PROG} ${DESTDIR}$(INSTBIN)

	mkdir -p ${DESTDIR}$(INSTMAN)
	install ${MAN} ${DESTDIR}$(INSTMAN)

uninstall:
	rm -f ${DESTDIR}$(INSTBIN)/${PROG}
	rm -f ${DESTDIR}$(INSTMAN)/${MAN}
