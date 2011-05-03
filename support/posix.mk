
INSTBIN=  ${DESTDIR}${INSTALLPREFIX}/bin
INSTMAN=  ${DESTDIR}${INSTALLPREFIX}/share/man1
INSTDBG=  ${DESTDIR}/usr/lib/debug/${INSTALLPREFIX}/bin
MAN=man/tvheadend.1

install: ${PROG} ${MAN}
	mkdir -p ${INSTBIN}
	mkdir -p ${INSTDBG}
	install -T ${PROG} ${INSTBIN}/tvheadend

	objcopy --only-keep-debug ${INSTBIN}/tvheadend ${INSTDBG}/tvheadend.debug
	strip -g ${INSTBIN}/tvheadend

	objcopy --add-gnu-debuglink=${INSTDBG}/tvheadend.debug ${INSTBIN}/tvheadend


	mkdir -p ${INSTMAN}
	install ${MAN} ${INSTMAN}

uninstall:
	rm -f ${INSTBIN}/tvheadend
	rm -f ${INSTDBG}/tvheadend.debug
	rm -f ${INSTMAN}/tvheadend.1
