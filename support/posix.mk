MAN = $(ROOTDIR)/man/tvheadend.1
ICON = $(ROOTDIR)/support/gnome/tvheadend.svg

INSTICON= ${DESTDIR}$(prefix)/share/icons/hicolor/scalable/apps


install: ${PROG} ${MAN}
	install -d ${DESTDIR}${bindir}
	install ${PROG} ${DESTDIR}${bindir}/tvheadend
	install -d ${DESTDIR}${mandir}
	install ${MAN} ${DESTDIR}${mandir}/tvheadend.1

	for bundle in ${BUNDLES}; do \
		mkdir -p ${DESTDIR}${datadir}/tvheadend/$$bundle ;\
		cp -r $(ROOTDIR)/$$bundle/*  ${DESTDIR}${datadir}/tvheadend/$$bundle ;\
	done

	find ${DESTDIR}${datadir}/tvheadend -name .git -exec rm -rf {} \; &>/dev/null || /bin/true

uninstall:
	rm -f ${DESTDIR}${bindir}/tvheadend
	rm -f ${DESTDIR}${mandir}/tvheadend.1
	rm -rf ${DESTDIR}${datadir}/tvheadend
