/*
 * $Header: /cvs/tuxbox/apps/misc/libs/libxmltree/xmlinterface.h,v 1.2 2009/02/18 17:51:55 seife Exp $
 *
 * xmlinterface for zapit - d-box2 linux project
 *
 * (C) 2002 by thegoodguy <thegoodguy@berlios.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


 * those files (xmlinterface.cpp and xmlinterface.h) lived at three different places
   in the tuxbox-cvs before, so look there for history information:
   - apps/dvb/zapit/include/zapit/xmlinterface.h
   - apps/dvb/zapit/src/xmlinterface.cpp
   - apps/tuxbox/neutrino/daemons/sectionsd/xmlinterface.cpp
   - apps/tuxbox/neutrino/src/system/xmlinterface.cpp
   - apps/tuxbox/neutrino/src/system/xmlinterface.h
 */

#ifndef __xmlinterface_h__
#define __xmlinterface_h__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>

#ifdef USE_LIBXML
#include <libxml/parser.h>
#define xmlNextNode next
inline char*      xmlGetAttribute     (xmlNodePtr cur, const char * s) { return (char *)xmlGetProp(cur, (const xmlChar *)s); };
inline char*      xmlGetName          (xmlNodePtr cur)                 { return (char *)(cur->name); };

#else  /* use libxmltree */
#include "xmltree.h"
typedef XMLTreeParser* xmlDocPtr;
typedef XMLTreeNode*   xmlNodePtr;
#define xmlChildrenNode GetChild()
#define xmlNextNode     GetNext()
inline xmlNodePtr xmlDocGetRootElement(xmlDocPtr  doc)                 { return doc->RootNode(); }
inline void       xmlFreeDoc          (xmlDocPtr  doc)                 { delete doc; }
inline char*      xmlGetAttribute     (xmlNodePtr cur, const char *s)  { return cur->GetAttributeValue(s); }
inline char*      xmlGetName          (xmlNodePtr cur)                 { return cur->GetType();  }
inline char*      xmlGetData          (xmlNodePtr cur)                 { return cur->GetData();  }
#endif /* USE_LIBXML */


unsigned long xmlGetNumericAttribute  (const xmlNodePtr node, const char *name, const int base);
long xmlGetSignedNumericAttribute     (const xmlNodePtr node, const char *name, const int base);
xmlNodePtr xmlGetNextOccurence        (xmlNodePtr cur, const char * s);

std::string Unicode_Character_to_UTF8(const int character);

std::string convert_UTF8_To_UTF8_XML(const char *s);

xmlDocPtr parseXml(const char *data);
xmlDocPtr parseXmlFile(const char * filename, bool warning_by_nonexistence = true);

#endif /* __xmlinterface_h__ */
