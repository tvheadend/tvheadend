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

/**
 * XML parser, written according to this spec:
 *
 * http://www.w3.org/TR/2006/REC-xml-20060816/
 *
 * Parses of UTF-8 and ISO-8859-1 (Latin 1) encoded XML and output as
 * htsmsg's with UTF-8 encoded payloads
 *
 *  Supports:                             Example:
 *
 *  Comments                              <!--  a comment               -->
 *  Processing Instructions               <?xml                          ?>
 *  CDATA                                 <![CDATA[  <litteraly copied> ]]>
 *  Label references                      &amp;
 *  Character references                  &#65;
 *  Empty tags                            <tagname/>
 *
 *
 *  Not supported:
 *
 *  UTF-16 (mandatory by standard)
 *  Intelligent parsing of <!DOCTYPE>
 *  Entity declarations
 *
 */


#include <assert.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "tvheadend.h"

#include "htsmsg_xml.h"
#include "htsbuf.h"

TAILQ_HEAD(cdata_content_queue, cdata_content);

LIST_HEAD(xmlns_list, xmlns);

typedef struct xmlns {
  LIST_ENTRY(xmlns) xmlns_global_link;
  LIST_ENTRY(xmlns) xmlns_scope_link;

  char *xmlns_prefix;
  unsigned int xmlns_prefix_len;

  char *xmlns_norm;
  unsigned int xmlns_norm_len;

} xmlns_t;

typedef struct xmlparser {
  enum {
    XML_ENCODING_UTF8,
    XML_ENCODING_8859_1,
  } xp_encoding;

  char xp_errmsg[128];

  int xp_srcdataused;

  struct xmlns_list xp_namespaces;

} xmlparser_t;

#define xmlerr(xp, fmt...) \
 snprintf((xp)->xp_errmsg, sizeof((xp)->xp_errmsg), fmt)


typedef struct cdata_content {
  TAILQ_ENTRY(cdata_content) cc_link;
  char *cc_start, *cc_end; /* end points to byte AFTER last char */
  int cc_encoding;
  char cc_buf[0];
} cdata_content_t;

static char *htsmsg_xml_parse_cd(xmlparser_t *xp,
				 htsmsg_t *parent, char *src);

/**
 *
 */
static void
add_unicode(struct cdata_content_queue *ccq, int c)
{
  cdata_content_t *cc;
  int r;

  cc = malloc(sizeof(cdata_content_t) + 6);
  r = put_utf8(cc->cc_buf, c);
  if(r == 0) {
    free(cc);
    return;
  }

  cc->cc_encoding = XML_ENCODING_UTF8;
  TAILQ_INSERT_TAIL(ccq, cc, cc_link);
  cc->cc_start = cc->cc_buf;
  cc->cc_end   = cc->cc_buf + r;
}

/**
 *
 */
static int
decode_character_reference(char **src)
{
  int v = 0;
  char c;

  if(**src == 'x') {
    /* hexadecimal */
    (*src)++;

    /* decimal */
    while(1) {
      c = **src;
      switch(c) {
      case '0' ... '9':
	v = v * 0x10 + c - '0';
	break;
      case 'a' ... 'f':
	v = v * 0x10 + c - 'a' + 10;
	break;
      case 'A' ... 'F':
	v = v * 0x10 + c - 'A' + 10;
	break;
      case ';':
	(*src)++;
	return v;
      default:
	return 0;
      }
      (*src)++;
    }

  } else {

    /* decimal */
    while(1) {
      c = **src;
      switch(c) {
      case '0' ... '9':
	v = v * 10 + c - '0';
	(*src)++;
	break;
      case ';':
	(*src)++;
	return v;
      default:
	return 0;
      }
    }
  }
}

/**
 *
 */
static inline int
is_xmlws(char c)
{
  return c > 0 && c <= 32;
  //  return c == 32 || c == 9 || c = 10 || c = 13;
}


/**
 *
 */
static void
xmlns_destroy(xmlns_t *ns)
{
  LIST_REMOVE(ns, xmlns_global_link);
  LIST_REMOVE(ns, xmlns_scope_link);
  free(ns->xmlns_prefix);
  free(ns->xmlns_norm);
  free(ns);
}

/**
 *
 */
static char *
htsmsg_xml_parse_attrib
  (xmlparser_t *xp, htsmsg_t *msg, char *src,
   struct xmlns_list *xmlns_scope_list)
{
  char *attribname, *payload;
  int attriblen, payloadlen;
  char quote;
  htsmsg_field_t *f;
  xmlns_t *ns;

  attribname = src;
  /* Parse attribute name */
  while(1) {
    if(*src == 0) {
      xmlerr(xp, "Unexpected end of file during attribute name parsing");
      return NULL;
    }

    if(is_xmlws(*src) || *src == '=')
      break;
    src++;
  }

  attriblen = src - attribname;
  if(attriblen < 1 || attriblen > 65535) {
    xmlerr(xp, "Invalid attribute name");
    return NULL;
  }

  while(is_xmlws(*src))
    src++;

  if(*src != '=') {
    xmlerr(xp, "Expected '=' in attribute parsing");
    return NULL;
  }
  src++;

  while(is_xmlws(*src))
    src++;


  /* Parse attribute payload */
  quote = *src++;
  if(quote != '"' && quote != '\'') {
    xmlerr(xp, "Expected ' or \" before attribute value");
    return NULL;
  }

  payload = src;
  while(1) {
    if(*src == 0) {
      xmlerr(xp, "Unexpected end of file during attribute value parsing");
      return NULL;
    }
    if(*src == quote)
      break;
    src++;
  }

  payloadlen = src - payload;
  if(payloadlen < 0 || payloadlen > 65535) {
    xmlerr(xp, "Invalid attribute value");
    return NULL;
  }

  src++;
  while(is_xmlws(*src))
    src++;

  if(xmlns_scope_list != NULL &&
     attriblen > 6 && !memcmp(attribname, "xmlns:", 6)) {

    attribname += 6;
    attriblen  -= 6;

    ns = malloc(sizeof(xmlns_t));

    ns->xmlns_prefix = malloc(attriblen + 1);
    memcpy(ns->xmlns_prefix, attribname, attriblen);
    ns->xmlns_prefix[attriblen] = 0;
    ns->xmlns_prefix_len = attriblen;

    ns->xmlns_norm = malloc(payloadlen + 1);
    memcpy(ns->xmlns_norm, payload, payloadlen);
    ns->xmlns_norm[payloadlen] = 0;
    ns->xmlns_norm_len = payloadlen;

    LIST_INSERT_HEAD(&xp->xp_namespaces, ns, xmlns_global_link);
    LIST_INSERT_HEAD(xmlns_scope_list,   ns, xmlns_scope_link);
    return src;
  }

  xp->xp_srcdataused = 1;
  attribname[attriblen] = 0;
  payload[payloadlen] = 0;

  f = htsmsg_field_add(msg, attribname, HMF_STR, 0, 0);
  f->hmf_str = payload;
  return src;
}

/**
 *
 */
static char *
htsmsg_xml_parse_tag(xmlparser_t *xp, htsmsg_t *parent, char *src)
{
  htsmsg_t *m, *attrs;
  struct xmlns_list nslist;
  char *tagname;
  int taglen, empty = 0, i;
  xmlns_t *ns;

  tagname = src;

  LIST_INIT(&nslist);

  while(1) {
    if(*src == 0) {
      xmlerr(xp, "Unexpected end of file during tag name parsing");
      return NULL;
    }
    if(is_xmlws(*src) || *src == '>' || *src == '/')
      break;
    src++;
  }

  taglen = src - tagname;
  if(taglen < 1 || taglen > 65535) {
    xmlerr(xp, "Invalid tag name");
    return NULL;
  }

  attrs = htsmsg_create_map();

  while(1) {

    while(is_xmlws(*src))
      src++;

    if(*src == 0) {
      htsmsg_destroy(attrs);
      xmlerr(xp, "Unexpected end of file in tag");
      return NULL;
    }

    if(src[0] == '/' && src[1] == '>') {
      empty = 1;
      src += 2;
      break;
    }

    if(*src == '>') {
      src++;
      break;
    }

    if((src = htsmsg_xml_parse_attrib(xp, attrs, src, &nslist)) == NULL) {
      htsmsg_destroy(attrs);
      return NULL;
    }
  }

  m = htsmsg_create_map();

  if(TAILQ_FIRST(&attrs->hm_fields) != NULL) {
    htsmsg_add_msg_extname(m, "attrib", attrs);
  } else {
    htsmsg_destroy(attrs);
  }

  if(!empty)
    src = htsmsg_xml_parse_cd(xp, m, src);

  for(i = 0; i < taglen - 1; i++) {
    if(tagname[i] == ':') {

      LIST_FOREACH(ns, &xp->xp_namespaces, xmlns_global_link) {
	if(ns->xmlns_prefix_len == i &&
	   !memcmp(ns->xmlns_prefix, tagname, ns->xmlns_prefix_len)) {

	  int llen = taglen - i - 1;
	  char *n = malloc(ns->xmlns_norm_len + llen + 1);

	  n[ns->xmlns_norm_len + llen] = 0;
	  memcpy(n, ns->xmlns_norm, ns->xmlns_norm_len);
	  memcpy(n + ns->xmlns_norm_len, tagname + i + 1, llen);
	  htsmsg_add_msg(parent, n, m);
	  free(n);
	  goto done;
	}
      }
    }
  }

  xp->xp_srcdataused = 1;
  tagname[taglen] = 0;
  htsmsg_add_msg_extname(parent, tagname, m);

 done:
  while((ns = LIST_FIRST(&nslist)) != NULL)
    xmlns_destroy(ns);
  return src;
}

/**
 *
 */
static char *
htsmsg_xml_parse_pi(xmlparser_t *xp, htsmsg_t *parent, char *src)
{
  htsmsg_t *attrs;
  char *s = src;
  char *piname;
  int l;

  while(1) {
    if(*src == 0) {
      xmlerr(xp, "Unexpected end of file during parsing of "
	         "Processing instructions");
      return NULL;
    }

    if(is_xmlws(*src) || *src == '?')
      break;
    src++;
  }

  l = src - s;
  if(l < 1 || l > 65536) {
    xmlerr(xp, "Invalid 'Processing instructions' name");
    return NULL;
  }
  piname = alloca(l + 1);
  memcpy(piname, s, l);
  piname[l] = 0;

  attrs = htsmsg_create_map();

  while(1) {

    while(is_xmlws(*src))
      src++;

    if(*src == 0) {
      htsmsg_destroy(attrs);
      xmlerr(xp, "Unexpected end of file during parsing of "
	     "Processing instructions");
      return NULL;
    }

    if(src[0] == '?' && src[1] == '>') {
      src += 2;
      break;
    }

    if((src = htsmsg_xml_parse_attrib(xp, attrs, src, NULL)) == NULL) {
      htsmsg_destroy(attrs);
      return NULL;
    }
  }


  if(TAILQ_FIRST(&attrs->hm_fields) != NULL && parent != NULL) {
    htsmsg_add_msg(parent, piname, attrs);
  } else {
    htsmsg_destroy(attrs);
  }
  return src;
}

/**
 *
 */
static char *
xml_parse_comment(xmlparser_t *xp, char *src)
{
  /* comment */
  while(1) {
    if(*src == 0) { /* EOF inside comment is invalid */
      xmlerr(xp, "Unexpected end of file inside a comment");
      return NULL;
    }

    if(src[0] == '-' && src[1] == '-' && src[2] == '>')
      return src + 3;
    src++;
  }
}

/**
 *
 */
static char *
decode_label_reference
  (xmlparser_t *xp, struct cdata_content_queue *ccq, char *src)
{
  char *s = src;
  int l;
  char *label;

  while(*src != 0 && *src != ';')
    src++;
  if(*src == 0) {
    xmlerr(xp, "Unexpected end of file during parsing of label reference");
    return NULL;
  }

  l = src - s;
  if(l < 1 || l > 65535)
    return NULL;
  label = alloca(l + 1);
  memcpy(label, s, l);
  label[l] = 0;
  src++;

  if(!strcmp(label, "amp"))
    add_unicode(ccq, '&');
  else if(!strcmp(label, "gt"))
    add_unicode(ccq, '>');
  else if(!strcmp(label, "lt"))
    add_unicode(ccq, '<');
  else if(!strcmp(label, "apos"))
    add_unicode(ccq, '\'');
  else if(!strcmp(label, "quot"))
    add_unicode(ccq, '"');
  else {
    xmlerr(xp, "Unknown label referense: \"&%s;\"\n", label);
    return NULL;
  }

  return src;
}

/**
 *
 */
static char *
htsmsg_xml_parse_cd0
  (xmlparser_t *xp,struct cdata_content_queue *ccq, htsmsg_t *tags,
   htsmsg_t *pis, char *src, int raw)
{
  cdata_content_t *cc = NULL;
  int c;

  while(src != NULL && *src != 0) {

    if(raw && src[0] == ']' && src[1] == ']' && src[2] == '>') {
      if(cc != NULL)
	cc->cc_end = src;
      cc = NULL;
      src += 3;
      break;
    }

    if(*src == '<' && !raw) {

      if(cc != NULL)
	cc->cc_end = src;
      cc = NULL;

      src++;
      if(*src == '?') {
	src++;
	src = htsmsg_xml_parse_pi(xp, pis, src);
	continue;
      }

      if(src[0] == '!') {

	src++;

	if(src[0] == '-' && src[1] == '-') {
	  src = xml_parse_comment(xp, src + 2);
	  continue;
	}

	if(!strncmp(src, "[CDATA[", 7)) {
	  src += 7;
	  src = htsmsg_xml_parse_cd0(xp, ccq, tags, pis, src, 1);
	  continue;
	}
	xmlerr(xp, "Unknown syntatic element: <!%.10s", src);
	return NULL;
      }

      if(*src == '/') {
	/* End-tag */
	src++;
	while(*src != '>') {
	  if(*src == 0) { /* EOF inside endtag */
	    xmlerr(xp, "Unexpected end of file inside close tag");
	    return NULL;
	  }
	  src++;
	}
	src++;
	break;
      }

      src = htsmsg_xml_parse_tag(xp, tags, src);
      continue;
    }

    if(*src == '&' && !raw) {
      if(cc != NULL)
	cc->cc_end = src;
      cc = NULL;

      src++;

      if(*src == '#') {
	src++;
	/* Character reference */
	if((c = decode_character_reference(&src)) != 0)
	  add_unicode(ccq, c);
	else {
	  xmlerr(xp, "Invalid character reference");
	  return NULL;
	}
      } else {
	/* Label references */
	src = decode_label_reference(xp, ccq, src);
      }
      continue;
    }

    if(cc == NULL) {
      if(*src < 32) {
	src++;
	continue;
      }
      cc = malloc(sizeof(cdata_content_t));
      cc->cc_encoding = xp->xp_encoding;
      TAILQ_INSERT_TAIL(ccq, cc, cc_link);
      cc->cc_start = src;
    }
    src++;
  }

  if(cc != NULL) {
    assert(src != NULL);
    cc->cc_end = src;
  }
  return src;
}

/**
 *
 */
static char *
htsmsg_xml_parse_cd(xmlparser_t *xp, htsmsg_t *parent, char *src)
{
  struct cdata_content_queue ccq;
  htsmsg_field_t *f;
  cdata_content_t *cc;
  int c = 0, l, y = 0;
  char *x, *body;
  htsmsg_t *tags = htsmsg_create_map();

  TAILQ_INIT(&ccq);
  src = htsmsg_xml_parse_cd0(xp, &ccq, tags, NULL, src, 0);

  /* Assemble body */

  TAILQ_FOREACH(cc, &ccq, cc_link) {

    switch(cc->cc_encoding) {
    case XML_ENCODING_UTF8:
      c += cc->cc_end - cc->cc_start;
      y++;
      break;

    case XML_ENCODING_8859_1:
      l = 0;
      for(x = cc->cc_start; x < cc->cc_end; x++)
	l += 1 + (*x >= 0x80);

      c += l;
      y += 1 + (l != cc->cc_end - cc->cc_start);
      break;
    }
  }

  if(y == 1 && c > 0) {
    /* One segment UTF-8 (or 7bit ASCII),
       use data directly from source */

    cc = TAILQ_FIRST(&ccq);

    assert(cc != NULL);
    assert(TAILQ_NEXT(cc, cc_link) == NULL);

    f = htsmsg_field_add(parent, "cdata", HMF_STR, 0, 0);
    f->hmf_str = cc->cc_start;
    *cc->cc_end = 0;
    free(cc);

  } else if(c > 0) {
    body = malloc(c + 1);
    c = 0;

    while((cc = TAILQ_FIRST(&ccq)) != NULL) {

      switch(cc->cc_encoding) {
      case XML_ENCODING_UTF8:
	l = cc->cc_end - cc->cc_start;
	memcpy(body + c, cc->cc_start, l);
	c += l;
	break;

      case XML_ENCODING_8859_1:
	for(x = cc->cc_start; x < cc->cc_end; x++)
	  c += put_utf8(body + c, *x);
	break;
      }

      TAILQ_REMOVE(&ccq, cc, cc_link);
      free(cc);
    }
    body[c] = 0;

    f = htsmsg_field_add(parent, "cdata", HMF_STR, HMF_ALLOCED, 0);
    f->hmf_str = body;

  } else {

    while((cc = TAILQ_FIRST(&ccq)) != NULL) {
      TAILQ_REMOVE(&ccq, cc, cc_link);
      free(cc);
    }
  }


  if(src == NULL) {
    htsmsg_destroy(tags);
    return NULL;
  }

  if(TAILQ_FIRST(&tags->hm_fields) != NULL) {
    htsmsg_add_msg_extname(parent, "tags", tags);
  } else {
    htsmsg_destroy(tags);
  }

  return src;
}

/**
 *
 */
static char *
htsmsg_parse_prolog(xmlparser_t *xp, char *src)
{
  htsmsg_t *pis = htsmsg_create_map();
  htsmsg_t *xmlpi;
  const char *encoding;

  while(src != NULL && *src != 0) {

    while(is_xmlws(*src))
      src++;

    if(!strncmp(src, "<?", 2)) {
      src += 2;
      src = htsmsg_xml_parse_pi(xp, pis, src);
      continue;
    }

    if(!strncmp(src, "<!--", 4)) {
      src = xml_parse_comment(xp, src + 4);
      continue;
    }

    if(!strncmp(src, "<!DOCTYPE", 9)) {
      while(*src != 0) {
	if(*src == '>') {
	  src++;
	  break;
	}
	src++;
      }
      continue;
    }
    break;
  }

  if((xmlpi = htsmsg_get_map(pis, "xml")) != NULL) {

    if((encoding = htsmsg_get_str(xmlpi, "encoding")) != NULL) {
      if(!strcasecmp(encoding, "iso-8859-1") ||
	 !strcasecmp(encoding, "iso-8859_1") ||
	 !strcasecmp(encoding, "iso_8859-1") ||
	 !strcasecmp(encoding, "iso_8859_1")) {
	xp->xp_encoding = XML_ENCODING_8859_1;
      }
    }
  }

  htsmsg_destroy(pis);

  return src;
}

/**
 *
 */
htsmsg_t *
htsmsg_xml_deserialize(char *src, char *errbuf, size_t errbufsize)
{
  htsmsg_t *m;
  xmlparser_t xp;
  char *src0 = src;

  memset(&xp, 0, sizeof(xp));
  xp.xp_encoding = XML_ENCODING_UTF8;
  LIST_INIT(&xp.xp_namespaces);

  /* check for UTF-8 BOM */
  if(src[0] == 0xef && src[1] == 0xbb && src[2] == 0xbf)
    memmove(src, src + 3, strlen(src) - 2);

  if((src = htsmsg_parse_prolog(&xp, src)) == NULL)
    goto err;

  m = htsmsg_create_map();

  if(htsmsg_xml_parse_cd(&xp, m, src) == NULL) {
    htsmsg_destroy(m);
    goto err;
  }

  if(xp.xp_srcdataused) {
    m->hm_data = src0;
    m->hm_data_size = strlen(src0) + 1;
  } else {
    free(src0);
  }

  return m;

 err:
  free(src0);
  snprintf(errbuf, errbufsize, "%s", xp.xp_errmsg);

  /* Remove any odd chars inside of errmsg */
  for ( ; *errbuf; errbuf++)
    if (*errbuf < ' ')
      *errbuf = ' ';

  return NULL;
}

/*
 * Get cdata string field
 */
const char *
htsmsg_xml_get_cdata_str(htsmsg_t *tags, const char *name)
{
  htsmsg_t *sub;
  if((sub = htsmsg_get_map(tags, name)) == NULL)
    return NULL;
  return htsmsg_get_str(sub, "cdata");
}

/*
 * Get cdata u32 field
 */
int
htsmsg_xml_get_cdata_u32(htsmsg_t *tags, const char *name, uint32_t *u32)
{
  htsmsg_t *sub;
  if((sub = htsmsg_get_map(tags, name)) == NULL)
    return HTSMSG_ERR_FIELD_NOT_FOUND;
  return htsmsg_get_u32(sub, "cdata", u32);
}

/*
 * Get tag attribute
 */
const char *
htsmsg_xml_get_attr_str(htsmsg_t *tag, const char *name)
{
  htsmsg_t *attr = htsmsg_get_map(tag, "attrib");
  if (attr) return htsmsg_get_str(attr, name);
  return NULL;
}

int
htsmsg_xml_get_attr_u32(htsmsg_t *tag, const char *name, uint32_t *ret)
{
  htsmsg_t *attr = htsmsg_get_map(tag, "attrib");
  if (attr) return htsmsg_get_u32(attr, name, ret);
  return HTSMSG_ERR_FIELD_NOT_FOUND;
}

/**
 * Take an XPath-like string and return a htsmsg object
 * containing the node path and attributes.
 * Currently only supports:
 *    / = node
 *    @ = attribute
 *    [] = condition
 *
 *    //node1/node2[attrX=value]/@attrY
 */
htsmsg_t *
htsmsg_xml_parse_xpath(const char *xpath)
{
  htsmsg_t *m = NULL;    //The whole message
  htsmsg_t *f = NULL;    //Individual fields within the message

  m = htsmsg_create_map();
  f = htsmsg_create_map();

  tvhdebug(LS_XMLTV, "Parsing '%s'", xpath);

  int     xpLen = 0;          //Length of the xpath string
  int     xpType = 0;         //Type of the current xpath character
  int     xpTypeSaved = 0;    //Type of the current xpath item

  int     inPos = 0;          //Current xpath character position

  int     outPos = 0;         //Current output character position
  char    outStr[128];        //Output string holding the current item
  int64_t outType = 0;        //The current item type

  char    condAtt[128];       //The attribute part of the condition item
  char    condVal[128];       //The value part of the condition item

  char    inPosStr[11];       //Input position as a string for a unique key to the returned htsmsg

  xpLen = strlen(xpath);

  //If the XPath string is too long, abort
  if (xpLen > sizeof(outStr) - 1)
  {
    //Formatting note:
    //In 64 bit Ubuntu, sizeof() returns a 'long unsigned int'
    //In 32 bit i386-debian-strech, sizeof() returns an 'unsigned int'
    //This causes cross compile issues.
    //'%zu' is supposed to work for 'size_t' variables in C99.
    
    tvhtrace(LS_XMLTV, "XPath = '%s' too long, max len = %zu.", xpath, (sizeof(outStr) - 1));
    return NULL;
  }

  memset(outStr, 0, sizeof(outStr));

  for(inPos = 0; inPos < xpLen; inPos++)  //Loop through the xpath string
  {
    xpType = 0; //Keep byte
    if(xpath[inPos] == '/')
    {
      xpType = 1; //Node
    }
    else if (xpath[inPos] == '@' && xpTypeSaved != 3)
    {
      xpType = 2; //Attribute
    }
    else if (xpath[inPos] == '[')
    {
      xpType = 3; //Condition
    }

    //Add this byte to the existing item
    if (xpType == 0 && xpath[inPos] != ']')
    {
      outStr[outPos] = xpath[inPos];
      outPos++;
      outStr[outPos] = 0;
      outType = xpTypeSaved;
    }
    else
    {
        xpTypeSaved = xpType;
    }

    if(inPos == (xpLen - 1) || (xpType != 0 && outPos != 0 ))
    {

      if(outType != 0)
      {

        if(outType == 1 && !strcmp(outStr, "text()"))
        {
          outType = 4;
        }

        condAtt[0] = 0;
        condVal[0] = 0;
        if(outType == 3)
        {
          sscanf(outStr, "@%[^'=']=%s", condAtt, condVal);
        }

        f = htsmsg_create_map();

        if(outType == 3)
        {
          htsmsg_add_str(f, "n", condAtt);  //Condition attribute
        }
        else
        {
          htsmsg_add_str(f, "n", outStr);   //Name
        }

        htsmsg_add_s64(f, "t", outType);    //Type
        htsmsg_add_str(f, "v", condVal);    //Condition value

        snprintf(inPosStr, sizeof(inPosStr), "%d", inPos);
        htsmsg_add_msg(m, inPosStr, f);

      }

      outPos = 0;
      outStr[0] = 0;
      outType = 0;
    }

  }//END for loop through string

  return m;

}

/**
 * Take a htsmsg holding an XML object model
 * and a htsmsg holding an XPath model and
 * try to match the XPath to a node or
 * attribute.
 */
const char *htsmsg_xml_xpath_search(htsmsg_t *message, htsmsg_t *xpath)
{
  htsmsg_t        *temp_msg;
  htsmsg_t        *temp_path;
  int64_t         temp_type;
  htsmsg_field_t  *f;
  htsmsg_t        *attribs;
  htsmsg_t        *tags;
  htsmsg_t        *pass_tags = NULL;
  const char      *value;
  const char      *criteria;
  const char      *str_saved;

  temp_msg = message;
  str_saved = NULL;

  HTSMSG_FOREACH(f, xpath) {

      temp_path = htsmsg_get_map(xpath, htsmsg_field_name(f));
      htsmsg_get_s64(temp_path, "t", &temp_type);
      tvhdebug(LS_XMLTV, "htsmsg_xml_xpath_search '%s' = '%s', '%"PRIu64"', '%s'", htsmsg_field_name(f), htsmsg_get_str(temp_path, "n"), temp_type, htsmsg_get_str(temp_path, "v"));

      if(temp_type == 4)  //This item returns the text of the previous matched XML node
      {
        return str_saved;
      }

      if(temp_type == 1)  //This item deals with an XML node
      {
        str_saved = NULL;

        if((tags = htsmsg_get_map(temp_msg, "tags")) == NULL)
        {
          tvherror(LS_XMLTV, "Failed to find tags");
          return NULL;
        }

        if((pass_tags = htsmsg_get_map(tags, htsmsg_get_str(temp_path, "n"))) == NULL)
        {
          tvherror(LS_XMLTV, "Failed to match '%s'", htsmsg_get_str(temp_path, "n"));
          return NULL;
        }
        else
        {
          tvhdebug(LS_XMLTV, "Matched node '%s'", htsmsg_get_str(temp_path, "n"));
          str_saved = htsmsg_get_str(pass_tags, "cdata");
          temp_msg = pass_tags;
        }
      }//END of node type

      if(temp_type == 2 || temp_type == 3)  //This items deal with an XML attribute.
      {
        if((attribs = htsmsg_get_map(temp_msg, "attrib")) == NULL) return NULL;
        if((value = htsmsg_get_str(attribs, htsmsg_get_str(temp_path, "n"))) == NULL)
        {
          return NULL;
        }
        else
        {
          if(temp_type == 2)  //If this is a simple attribute, return the value.
          {
            tvhdebug(LS_XMLTV, "Returning attribute value '%s'", value);
            return value;
          }//END just return attribute value

          if(temp_type == 3)  //If this is an attribute comparison, compare it.
          {
            if((criteria = htsmsg_get_str(temp_path, "v")) == NULL)
            {
              tvherror(LS_XMLTV, "NO CRITERIA '%s'", htsmsg_get_str(temp_path, "v"));
              return NULL;
            }
            else
            {
              tvhdebug(LS_XMLTV, "COMPARING: '%s' to '%s'", value, criteria);
              if(!strcmp(value, criteria))
              {
                //Continue the search to the next XPath item, but not the next node
                tvhdebug(LS_XMLTV, "MATCHED: '%s' to '%s'", value, criteria);
              }
              else
              {
                //Return an abject failure in disgrace.
                return NULL;
              }
            }
          }//END attribute value comparison
        }//END found the attribute being searched for
      }//END of attribute type

  }//END loop through each XPath item.

  return NULL;

}
