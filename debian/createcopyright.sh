now=`date`
source=`svn info | grep "URL:" | sed "s/URL: //"`
rev=`svn info | grep "Revision:" | sed "s/Revision: //"`
cat <<EOF
This package was automatically debianized on ${now}

It was downloaded from ${source} revision ${rev}

HTS Showtime and HTS Tvheadend is copyright (c) Andreas Öman 2007

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.



The Debian packaging is (C) 2007, Andreas Öman <andreas@lonelycoder.com> and
is licensed under the GPL, see "/usr/share/common-licenses/GPL".

EOF