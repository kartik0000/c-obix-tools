/** @file
 * @todo add description here
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#ifndef DOCTREE_H_
#define DOCTREE_H_

#include <ixml_ext.h>

IXML_Element* doctree_get(const char* uri);

int doctree_put(const char* uri, IXML_Element* data);

#endif /* DOCTREE_H_ */
