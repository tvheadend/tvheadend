
prefix ?= $(INSTALLPREFIX)
INSTBIN= $(prefix)/bin
INSTMAN= $(prefix)/share/man1
MAN=man/tvheadend.1

install: ${PROG} ${MAN}
	mkdir -p $(INSTBIN)
	install -s ${PROG} $(INSTBIN)

	mkdir -p $(INSTMAN)
	install ${MAN} $(INSTMAN)

uninstall:
	rm -f $(INSTBIN)/${PROG}
	rm -f $(INSTMAN)/${MAN}
