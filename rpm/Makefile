#
#  RPM build
#  Copyright (C) 2014 Jaroslav Kysela
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

include $(dir $(lastword $(MAKEFILE_LIST)))../.config.mk

SPEC     ?= tvheadend.spec.in

COMMIT   ?= $(shell git rev-parse HEAD)
COMMIT1   = $(shell git rev-parse $(COMMIT))
FVERSION  = $(shell cd .. && git describe --match "v*" $(COMMIT1) | sed "s/^v//")
VERSION   = $(shell echo "$(FVERSION)" | cut -d '-' -f 1)
RELEASE   = $(shell echo "$(FVERSION)" | cut -d '-' -f 2-10 | sed "s/-/~/g")
VERSION  ?= "0.0.0"
RELEASE  ?= 1
ifeq ($(VERSION),$(RELEASE))
RELEASE   = 1
REF       = $(COMMIT)
COMMITR   = $(shell echo $(COMMIT) | sed "s/^v//")
else
REF       = $(COMMIT1)
COMMITR   = $(COMMIT1)
endif

tvheadend.spec: $(SPEC)
	@sed -e 's/@VERSION@/$(VERSION)/g' \
	     -e 's/@RELEASE@/$(RELEASE)/g' \
	     -e 's/@REF@/$(REF)/g' \
	     -e 's/@COMMIT@/$(COMMITR)/g' \
		$< > $@

~/rpmbuild/SOURCES:
	mkdir -p $(foreach f,BUILD RPMS SOURCES SPECS SRPMS,~/rpmbuild/$(f))

.PHONY: srpm
srpm: ~/rpmbuild/SOURCES tvheadend.spec
	spectool --get-files --sourcedir tvheadend.spec
	rpmbuild -bs tvheadend.spec

.PHONY: build
build: ~/rpmbuild/SOURCES tvheadend.spec
	spectool --get-files --sourcedir tvheadend.spec
	rpmbuild -ba tvheadend.spec

.PHONY: clean.
clean:
	rm -f tvheadend.spec
