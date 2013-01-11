MAN = man/tvheadend.1
ICON = support/gnome/tvheadend.svg

INSTICON= ${DESTDIR}$(prefix)/share/icons/hicolor/scalable/apps


install: ${PROG} ${MAN}
	install -D ${PROG} ${DESTDIR}${bindir}/tvheadend
	install -D ${MAN} ${DESTDIR}${mandir}/tvheadend.1

	for bundle in ${BUNDLES}; do \
		mkdir -p ${DESTDIR}${datadir}/tvheadend/$$bundle ;\
		cp -r $$bundle/*  ${DESTDIR}${datadir}/tvheadend/$$bundle ;\
	done

	find ${DESTDIR} -name .git -exec rm -rf {} \;

uninstall:
	rm -f ${DESTDIR}${bindir)/tvheadend
	rm -f ${DESTDIR}${mandir)/tvheadend.1
