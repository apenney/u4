/*
 * $Id$
 */

#ifndef XML_H
#define XML_H

#include <libxml/xmlmemory.h>

xmlDocPtr xmlParse(const char *filename);
char *xmlGetPropAsStr(xmlNodePtr node, const xmlChar *name);
int xmlGetPropAsBool(xmlNodePtr node, const xmlChar *name);
int xmlGetPropAsInt(xmlNodePtr node, const xmlChar *name);
int xmlPropCmp(xmlNodePtr node, const xmlChar *name, const char *s);
int xmlPropCaseCmp(xmlNodePtr node, const xmlChar *name, const char *s);

#endif /* XML_H */
