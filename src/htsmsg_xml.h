/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2008 Andreas Öman
 *
 * Functions converting HTSMSGs to/from XML
 */

#ifndef HTSMSG_XML_H_
#define HTSMSG_XML_H_

#include "htsmsg.h"
#include "htsbuf.h"

htsmsg_t *htsmsg_xml_deserialize(char *src, char *errbuf, size_t errbufsize);
const char *htsmsg_xml_get_cdata_str (htsmsg_t *tags, const char *tag);
int htsmsg_xml_get_cdata_u32 (htsmsg_t *tags, const char *tag, uint32_t *u32);
const char *htsmsg_xml_get_attr_str(htsmsg_t *tag, const char *attr);
int htsmsg_xml_get_attr_u32(htsmsg_t *tag, const char *attr, uint32_t *u32);

#endif /* HTSMSG_XML_H_ */
