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
 * htsmsg structures. The output format is compatible with the original
 * custom XML parser implementation.
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

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "tvheadend.h"

#include "htsmsg_xml.h"
#include "htsbuf.h"

/**
 * Recursively process an XML node and convert it to htsmsg format.
 * 
 * @param node  The XML node to process
 * @return      htsmsg_t* representing the node
 */
static htsmsg_t *
htsmsg_xml_parse_node(xmlNodePtr node)
{
  htsmsg_t *m, *attrs, *tags;
  xmlNodePtr child;
  xmlAttrPtr attr;
  xmlChar *content;
  char *text_content = NULL;
  size_t text_len = 0;
  
  m = htsmsg_create_map();
  
  /* Process attributes */
  if (node->properties) {
    attrs = htsmsg_create_map();
    for (attr = node->properties; attr; attr = attr->next) {
      if (attr->name && attr->children && attr->children->content) {
        htsmsg_add_str(attrs, (const char *)attr->name, 
                       (const char *)attr->children->content);
      }
    }
    if (TAILQ_FIRST(&attrs->hm_fields) != NULL) {
      htsmsg_add_msg(m, "attrib", attrs);
    } else {
      htsmsg_destroy(attrs);
    }
  }
  
  /* First pass: collect text content and check for child elements */
  tags = NULL;
  for (child = node->children; child; child = child->next) {
    if (child->type == XML_TEXT_NODE || child->type == XML_CDATA_SECTION_NODE) {
      content = xmlNodeGetContent(child);
      if (content) {
        /* Accumulate text content */
        size_t content_len = strlen((const char *)content);
        if (content_len > 0) {
          /* Check if content is not just whitespace */
          const char *p = (const char *)content;
          int has_content = 0;
          while (*p) {
            if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
              has_content = 1;
              break;
            }
            p++;
          }
          if (has_content) {
            size_t new_size = text_len + content_len + 1;
            char *new_text = realloc(text_content, new_size);
            if (new_text) {
              text_content = new_text;
              memcpy(text_content + text_len, content, content_len + 1);
              text_len += content_len;
            }
            /* If realloc fails, we keep the existing text_content and
             * skip this chunk of content. This is acceptable behavior
             * for memory pressure situations. */
          }
        }
        xmlFree(content);
      }
    } else if (child->type == XML_ELEMENT_NODE) {
      /* Child element */
      htsmsg_t *child_msg = htsmsg_xml_parse_node(child);
      if (child_msg) {
        if (!tags) {
          tags = htsmsg_create_map();
        }
        htsmsg_add_msg(tags, (const char *)child->name, child_msg);
      }
    }
  }
  
  /* Add text content if present */
  if (text_content && text_len > 0) {
    htsmsg_add_str(m, "cdata", text_content);
  }
  free(text_content);
  
  /* Add tags if present */
  if (tags && TAILQ_FIRST(&tags->hm_fields) != NULL) {
    htsmsg_add_msg(m, "tags", tags);
  } else if (tags) {
    htsmsg_destroy(tags);
  }
  
  return m;
}

/**
 * Parse an XML document from a string using libxml2
 *
 * @param src        The XML source string (will be freed by this function)
 * @param errbuf     Buffer for error messages
 * @param errbufsize Size of error buffer
 * @return           htsmsg_t* representing the document, or NULL on error
 */
htsmsg_t *
htsmsg_xml_deserialize(char *src, char *errbuf, size_t errbufsize)
{
  xmlDocPtr doc;
  xmlNodePtr root;
  htsmsg_t *m, *tags;
  
  if (!src || !*src) {
    snprintf(errbuf, errbufsize, "Empty XML input");
    free(src);
    return NULL;
  }
  
  /* Parse the XML document */
  doc = xmlReadMemory(src, strlen(src), NULL, NULL, 
                      XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  
  if (!doc) {
    xmlErrorPtr err = xmlGetLastError();
    if (err && err->message) {
      snprintf(errbuf, errbufsize, "XML parse error: %s", err->message);
      /* Remove any odd chars inside of errmsg */
      char *p;
      for (p = errbuf; *p; p++)
        if (*p < ' ')
          *p = ' ';
    } else {
      snprintf(errbuf, errbufsize, "XML parse error");
    }
    free(src);
    return NULL;
  }
  
  /* Get the root element */
  root = xmlDocGetRootElement(doc);
  if (!root) {
    snprintf(errbuf, errbufsize, "Empty XML document");
    xmlFreeDoc(doc);
    free(src);
    return NULL;
  }
  
  /* Create the root message structure */
  m = htsmsg_create_map();
  tags = htsmsg_create_map();
  
  /* Parse the root element */
  htsmsg_t *root_msg = htsmsg_xml_parse_node(root);
  if (root_msg) {
    htsmsg_add_msg(tags, (const char *)root->name, root_msg);
  }
  
  htsmsg_add_msg(m, "tags", tags);
  
  /* Clean up */
  xmlFreeDoc(doc);
  free(src);
  
  return m;
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
