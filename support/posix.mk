MAN = man/tvheadend.1
ICON = support/gnome/tvheadend.svg

INSTICON= ${DESTDIR}$(prefix)/share/icons/hicolor/scalable/apps


install: ${PROG}.datadir ${MAN}
	install -D ${PROG}.datadir ${bindir}/tvheadend
	install -D ${MAN} ${mandir}/tvheadend.1

	for bundle in ${BUNDLES}; do \
		mkdir -p ${datadir}/$$bundle ;\
		cp -r $$bundle/*  ${datadir}/$$bundle ;\
	done


uninstall:
	rm -f ${bindir)/tvheadend
	rm -f ${mandir)/tvheadend.1
