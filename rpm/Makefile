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

COMMIT   ?= $(shell git rev-parse HEAD)
COMMIT1   = $(shell git rev-parse $(COMMIT))
VERSION   = $(shell cd .. && git describe --match "v*" $(COMMIT1) | sed -e 's/-/./g' -e 's/^v//g' )

tvheadend.spec: tvheadend.spec.in
	@sed -e 's/@VERSION@/$(VERSION)/g' \
	     -e 's/@COMMIT@/$(COMMIT1)/g' \
		$< > $@

~/rpmbuild/SOURCES:
	mkdir -p $(foreach f,BUILD RPMS SOURCES SPECS SRPMS,~/rpmbuild/$(f))

build: ~/rpmbuild/SOURCES tvheadend.spec
	spectool --get-files --sourcedir tvheadend.spec
	rpmbuild -ba tvheadend.spec
