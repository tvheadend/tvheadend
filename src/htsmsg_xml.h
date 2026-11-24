/*
 *  Functions converting HTSMSGs to/from XML
 *  Copyright (C) 2008 Andreas Öman
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HTSMSG_XML_H_
#define HTSMSG_XML_H_

#include "htsmsg.h"
#include "htsbuf.h"

htsmsg_t *htsmsg_xml_deserialize(char *src, char *errbuf, size_t errbufsize);
/*
 * Parse src and pass each child element of a root element named root
 * to cb, one at a time, so the whole document is never held as htsmsg.
 * cb owns m and must destroy it. src is not modified. Returns 0 if the
 * document parsed, -1 on error. On error, cb may already have been
 * called for the items completed before the error.
 */
typedef void (*htsmsg_xml_item_cb_t)(void *opaque, const char *name,
                                     htsmsg_t *m);
int htsmsg_xml_deserialize_cb(const char *src, const char *root,
                              htsmsg_xml_item_cb_t cb, void *opaque,
                              char *errbuf, size_t errbufsize);
int htsmsg_xml_decode_attr(char *s);
const char *htsmsg_xml_get_cdata_str (htsmsg_t *tags, const char *tag);
int htsmsg_xml_get_cdata_u32 (htsmsg_t *tags, const char *tag, uint32_t *u32);
const char *htsmsg_xml_get_attr_str(htsmsg_t *tag, const char *attr);
int htsmsg_xml_get_attr_u32(htsmsg_t *tag, const char *attr, uint32_t *u32);
htsmsg_t *htsmsg_xml_parse_xpath(const char *xpath);
const char *htsmsg_xml_xpath_search(htsmsg_t *tag, htsmsg_t *xpath);

#endif /* HTSMSG_XML_H_ */
