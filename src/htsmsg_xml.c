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
 * XML parser using libxml2
 *
 * This module parses XML documents using libxml2 and converts them to
 * htsmsg structures.
 *
 * Output structure:
 *   - "tags" - a map containing child XML elements (by tag name)
 *   - "attrib" - a map containing element attributes
 *   - "cdata" - string containing the element's text content
 */


#include <assert.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlreader.h>

#include "tvheadend.h"

#include "htsmsg_xml.h"
#include "htsbuf.h"

/**
 * Decode a numeric character reference at *src (just after "&#").
 * Returns the code point, or 0 when it isn't a valid one.
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
 * Decode the character reference at *src (just after "&#") into out.
 * Returns the number of bytes written and moves *src past the reference,
 * or 0 when it isn't a valid one.
 */
static int
htsmsg_xml_decode_attr_numeric(char **src, char *out)
{
  char *p = *src;
  const char *q = p + (*p == 'x');
  int c;
  int n;

  q += strspn(q, "0");
  /* At most 7 digits, so decode_character_reference() can't overflow */
  if (strspn(q, "0123456789abcdefABCDEF") > 7)
    return 0;
  c = decode_character_reference(&p);
  if (c <= 0 || c > 0x10ffff)
    return 0;
  n = put_utf8(out, c);
  if (n <= 0)
    return 0;
  *src = p;
  return n;
}

/**
 * Decode the predefined entity at src (just after '&') into out.
 * Returns the length of its name including ';', or 0 when there is none.
 */
static size_t
htsmsg_xml_decode_attr_named(const char *src, char *out)
{
  static const char *named[] = { "amp;&", "lt;<", "gt;>", "quot;\"", "apos;'" };
  size_t i;
  size_t l;

  for (i = 0; i < ARRAY_SIZE(named); i++) {
    l = strlen(named[i]) - 1;
    if (!strncmp(src, named[i], l)) {
      *out = named[i][l];
      return l;
    }
  }
  return 0;
}

/**
 * Decode the predefined entities and character references in a
 * NUL-terminated attribute value, in place. Anything else, including a
 * bare '&', is kept as is, so values that imported before still import.
 * The decoded value is never longer than the source. Returns its length.
 */
int
htsmsg_xml_decode_attr(char *s)
{
  char *r = s;
  char *w = s;
  char *p;
  size_t l;
  int n;

  while (*r) {
    n = 0;
    if (r[0] == '&' && r[1] == '#') {
      p = r + 2;
      n = htsmsg_xml_decode_attr_numeric(&p, w);
      if (n)
        r = p;
    } else if (r[0] == '&') {
      l = htsmsg_xml_decode_attr_named(r + 1, w);
      if (l) {
        r += l + 1;
        n = 1;
      }
    }
    if (n)
      w += n;
    else
      *w++ = *r++;
  }
  *w = 0;
  return w - s;
}

/**
 * Build the htsmsg key for an element or attribute name.
 *
 * A prefixed element is named namespace URI + local name (e.g.
 * "urn:ses-com:satipX_SATIPCAP"), a prefixed attribute keeps its
 * "prefix:name" form, and unprefixed names (including elements in a
 * default namespace) use the local name.
 */
static char *
htsmsg_xml_node_name(const xmlChar *name, const xmlNs *ns, int is_attr)
{
  const char *a;
  const char *sep;
  size_t alen;
  size_t slen;
  size_t nlen = strlen((const char *)name);
  char *r;

  if (ns == NULL || ns->prefix == NULL)
    return strdup((const char *)name);
  if (is_attr) {
    a = (const char *)ns->prefix;
    sep = ":";
  } else {
    a = ns->href ? (const char *)ns->href : "";
    sep = "";
  }
  alen = strlen(a);
  slen = strlen(sep);
  r = malloc(alen + slen + nlen + 1);
  if (r == NULL)
    return NULL;
  memcpy(r, a, alen);
  memcpy(r + alen, sep, slen);
  memcpy(r + alen + slen, name, nlen + 1);
  return r;
}

/**
 * Entity references are not expanded (no XML_PARSE_NOENT), so libxml2
 * keeps them as separate nodes. Reject them rather than lose their
 * content.
 */
static int
htsmsg_xml_check_entity(const xmlNode *n, char *errbuf, size_t errbufsize)
{
  if (n->type != XML_ENTITY_REF_NODE)
    return 0;
  snprintf(errbuf, errbufsize, "Unsupported entity reference '&%s;'",
           n->name ? (const char *)n->name : "");
  return -1;
}

/**
 * The document is read with xmlTextReader and turned into htsmsg while
 * reading, so libxml2 never builds a tree of the whole document. Each
 * element still open has a frame on the stack below.
 */
#define HTSMSG_XML_MAX_DEPTH 256

typedef struct htsmsg_xml_frame {
  htsmsg_t *m;
  htsmsg_t *tags;
  char *name;
  char *text;
  size_t text_len;
} htsmsg_xml_frame_t;

typedef struct htsmsg_xml_reader {
  xmlTextReaderPtr r;
  /* streaming mode: children of a root element named root go to cb */
  const char *root;
  htsmsg_xml_item_cb_t cb;
  void *opaque;
  int root_ok;
  int roots;
  int depth;
  char *errbuf;
  size_t errbufsize;
  htsmsg_xml_frame_t st[HTSMSG_XML_MAX_DEPTH + 1];
} htsmsg_xml_reader_t;

static int
htsmsg_xml_reader_fail(htsmsg_xml_reader_t *x, const char *msg)
{
  snprintf(x->errbuf, x->errbufsize, "%s", msg);
  return -1;
}

static int
htsmsg_xml_frame_attrib(htsmsg_xml_reader_t *x, htsmsg_t *attrs,
                        xmlNodePtr node, xmlAttrPtr attr)
{
  xmlChar *content = NULL;
  xmlNodePtr child;
  char *name;

  for (child = attr->children; child; child = child->next)
    if (htsmsg_xml_check_entity(child, x->errbuf, x->errbufsize))
      return -1;
  name = htsmsg_xml_node_name(attr->name, attr->ns, 1);
  /* An attribute without children has an empty value */
  if (name != NULL && attr->children != NULL)
    content = xmlNodeListGetString(node->doc, attr->children, 1);
  if (name == NULL || (attr->children != NULL && content == NULL)) {
    free(name);
    xmlFree(content);
    return htsmsg_xml_reader_fail(x, "Out of memory");
  }
  htsmsg_add_str(attrs, name, content ? (const char *)content : "");
  free(name);
  xmlFree(content);
  return 0;
}

static int
htsmsg_xml_frame_attribs(htsmsg_xml_reader_t *x, htsmsg_xml_frame_t *fr,
                         xmlNodePtr node)
{
  htsmsg_t *attrs = htsmsg_create_map();
  xmlAttrPtr attr;

  if (attrs == NULL)
    return htsmsg_xml_reader_fail(x, "Out of memory");
  for (attr = node->properties; attr; attr = attr->next)
    if (htsmsg_xml_frame_attrib(x, attrs, node, attr)) {
      htsmsg_destroy(attrs);
      return -1;
    }
  htsmsg_add_msg(fr->m, "attrib", attrs);
  return 0;
}

static int
htsmsg_xml_frame_open(htsmsg_xml_reader_t *x, htsmsg_xml_frame_t *fr,
                      xmlNodePtr node)
{
  memset(fr, 0, sizeof(*fr));
  fr->m = htsmsg_create_map();
  fr->name = htsmsg_xml_node_name(node->name, node->ns, 0);
  if (fr->m == NULL || fr->name == NULL)
    return htsmsg_xml_reader_fail(x, "Out of memory");
  if (node->properties == NULL)
    return 0;
  return htsmsg_xml_frame_attribs(x, fr, node);
}

static int
htsmsg_xml_is_blank(const char *s)
{
  return s[strspn(s, " \t\r\n")] == '\0';
}

/*
 * Whitespace-only text before any other text is dropped, so indentation
 * between child elements is not kept. Later whitespace is kept, so mixed
 * content like "Tom<!-- c --> Jerry" keeps its space.
 */
static int
htsmsg_xml_frame_text(htsmsg_xml_reader_t *x, htsmsg_xml_frame_t *fr,
                      const char *s)
{
  size_t l;
  char *n;

  if (fr->text_len == 0 && htsmsg_xml_is_blank(s))
    return 0;
  l = strlen(s);
  n = realloc(fr->text, fr->text_len + l + 1);
  if (n == NULL)
    return htsmsg_xml_reader_fail(x, "Out of memory");
  fr->text = n;
  memcpy(fr->text + fr->text_len, s, l + 1);
  fr->text_len += l;
  return 0;
}

static void
htsmsg_xml_frame_free(htsmsg_xml_frame_t *fr)
{
  if (fr->m)
    htsmsg_destroy(fr->m);
  if (fr->tags)
    htsmsg_destroy(fr->tags);
  free(fr->name);
  free(fr->text);
  memset(fr, 0, sizeof(*fr));
}

/* Finish the innermost element and add it to its parent, or pass it on */
static int
htsmsg_xml_frame_close(htsmsg_xml_reader_t *x)
{
  htsmsg_xml_frame_t *fr = &x->st[x->depth];
  htsmsg_xml_frame_t *parent = &x->st[x->depth - 1];
  int keep = x->cb == NULL || x->depth > 2;

  if (keep && parent->tags == NULL) {
    parent->tags = htsmsg_create_map();
    if (parent->tags == NULL)
      return htsmsg_xml_reader_fail(x, "Out of memory");
  }
  if (fr->text_len)
    htsmsg_add_str(fr->m, "cdata", fr->text);
  if (fr->tags) {
    htsmsg_add_msg(fr->m, "tags", fr->tags);
    fr->tags = NULL;
  }
  if (keep) {
    htsmsg_add_msg(parent->tags, fr->name, fr->m);
  } else if (x->depth == 2 && x->root_ok) {
    x->cb(x->opaque, fr->name, fr->m);
  } else {
    /* streaming mode: the root element is not kept, nor are the
       children of a root element with another name */
    htsmsg_destroy(fr->m);
  }
  fr->m = NULL;
  htsmsg_xml_frame_free(fr);
  x->depth--;
  return 0;
}

static int
htsmsg_xml_reader_element(htsmsg_xml_reader_t *x)
{
  xmlNodePtr node = xmlTextReaderCurrentNode(x->r);

  if (node == NULL)
    return htsmsg_xml_reader_fail(x, "XML parse error");
  if (x->depth >= HTSMSG_XML_MAX_DEPTH)
    return htsmsg_xml_reader_fail(x, "XML nesting too deep");
  if (x->depth == 0)
    x->roots++;
  x->depth++;
  if (htsmsg_xml_frame_open(x, &x->st[x->depth], node))
    return -1;
  if (x->depth == 1 && x->cb != NULL)
    x->root_ok = !strcmp(x->st[1].name, x->root);
  if (xmlTextReaderIsEmptyElement(x->r))
    return htsmsg_xml_frame_close(x);
  return 0;
}

static int
htsmsg_xml_reader_node(htsmsg_xml_reader_t *x)
{
  const xmlChar *v;

  switch (xmlTextReaderNodeType(x->r)) {
  case XML_READER_TYPE_ELEMENT:
    return htsmsg_xml_reader_element(x);
  case XML_READER_TYPE_END_ELEMENT:
    if (x->depth > 0)
      return htsmsg_xml_frame_close(x);
    return 0;
  case XML_READER_TYPE_TEXT:
  case XML_READER_TYPE_CDATA:
  case XML_READER_TYPE_WHITESPACE:
  case XML_READER_TYPE_SIGNIFICANT_WHITESPACE:
    v = xmlTextReaderConstValue(x->r);
    /* streaming mode: the root element's own text is not kept */
    if (x->cb != NULL && x->depth == 1)
      return 0;
    if (x->depth > 0 && v != NULL)
      return htsmsg_xml_frame_text(x, &x->st[x->depth], (const char *)v);
    return 0;
  case XML_READER_TYPE_ENTITY_REFERENCE:
    v = xmlTextReaderConstName(x->r);
    snprintf(x->errbuf, x->errbufsize, "Unsupported entity reference '&%s;'",
             v ? (const char *)v : "");
    return -1;
  default:
    return 0;
  }
}

static void
htsmsg_xml_reader_error(htsmsg_xml_reader_t *x)
{
  const xmlError *err = xmlGetLastError();
  char *p;

  if (err == NULL || err->message == NULL) {
    htsmsg_xml_reader_fail(x, "XML parse error");
    return;
  }
  if (x->errbufsize == 0)
    return;
  snprintf(x->errbuf, x->errbufsize, "XML parse error: %s", err->message);
  for (p = x->errbuf; *p; p++)
    if (*p < ' ')
      *p = ' ';
}

/* Read the whole document. Returns 0 on success, -1 on error. */
static int
htsmsg_xml_reader_run(htsmsg_xml_reader_t *x, const char *src)
{
  size_t len = src ? strlen(src) : 0;
  int ret;

  if (len == 0)
    return htsmsg_xml_reader_fail(x, "Empty XML input");
  if (len > INT_MAX)
    return htsmsg_xml_reader_fail(x, "XML input too large");
  xmlResetLastError();
  x->r = xmlReaderForMemory(src, (int)len, NULL, NULL,
                            XML_PARSE_NONET | XML_PARSE_NOERROR |
                            XML_PARSE_NOWARNING);
  if (x->r == NULL) {
    htsmsg_xml_reader_error(x);
    return -1;
  }
  x->st[0].m = htsmsg_create_map();
  if (x->st[0].m == NULL)
    return htsmsg_xml_reader_fail(x, "Out of memory");
  ret = xmlTextReaderRead(x->r);
  while (ret == 1) {
    if (htsmsg_xml_reader_node(x))
      return -1;
    ret = xmlTextReaderRead(x->r);
  }
  if (ret < 0) {
    htsmsg_xml_reader_error(x);
    return -1;
  }
  if (x->roots == 0)
    return htsmsg_xml_reader_fail(x, "Empty XML document");
  if (x->depth != 0)
    return htsmsg_xml_reader_fail(x, "XML parse error: unexpected end");
  return 0;
}

static htsmsg_xml_reader_t *
htsmsg_xml_reader_create(char *errbuf, size_t errbufsize)
{
  htsmsg_xml_reader_t *x = calloc(1, sizeof(*x));

  if (x == NULL)
    snprintf(errbuf, errbufsize, "Out of memory");
  else {
    x->errbuf = errbuf;
    x->errbufsize = errbufsize;
  }
  return x;
}

static void
htsmsg_xml_reader_destroy(htsmsg_xml_reader_t *x)
{
  int i;

  for (i = 0; i <= x->depth; i++)
    htsmsg_xml_frame_free(&x->st[i]);
  if (x->r)
    xmlFreeTextReader(x->r);
  free(x);
}

htsmsg_t *
htsmsg_xml_deserialize(char *src, char *errbuf, size_t errbufsize)
{
  htsmsg_xml_reader_t *x = htsmsg_xml_reader_create(errbuf, errbufsize);
  htsmsg_t *m = NULL;

  if (x != NULL && htsmsg_xml_reader_run(x, src) == 0) {
    m = x->st[0].m;
    x->st[0].m = NULL;
    if (x->st[0].tags) {
      htsmsg_add_msg(m, "tags", x->st[0].tags);
      x->st[0].tags = NULL;
    }
  }
  if (x != NULL)
    htsmsg_xml_reader_destroy(x);
  free(src);
  return m;
}

/*
 * See htsmsg_xml.h
 */
int
htsmsg_xml_deserialize_cb(const char *src, const char *root,
                          htsmsg_xml_item_cb_t cb, void *opaque,
                          char *errbuf, size_t errbufsize)
{
  htsmsg_xml_reader_t *x = htsmsg_xml_reader_create(errbuf, errbufsize);
  int ret;

  if (x == NULL)
    return -1;
  x->root = root;
  x->cb = cb;
  x->opaque = opaque;
  ret = htsmsg_xml_reader_run(x, src);
  htsmsg_xml_reader_destroy(x);
  return ret;
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

  m = htsmsg_create_map();
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
