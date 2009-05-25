/** @file
 * Defines utility methods for work with XML DOM structure.
 * Expands functionality of IXML library which provides DOM XML
 * parser functionality. IXML is distributed as a part of libupnp
 * (<a href="http://pupnp.sourceforge.net/">http://pupnp.sourceforge.net/</a>).
 */
#ifndef IXML_EXT_H_
#define IXML_EXT_H_

#include <upnp/ixml.h>

/**@name XML node types conversion @{*/
/**
 * Converts node to the element.
 * @param node to be converted.
 * @return NULL if node is not an element (i.e. tag).
 */
IXML_Element* ixmlNode_convertToElement(IXML_Node* node);

/**
 * Converts node to the attribute.
 * @param node to be converted.
 * @return NULL if node is not an attribute.
 */
IXML_Attr* ixmlNode_convertToAttr(IXML_Node* node);

/**
 * Returns node which represents provided element.
 */
IXML_Node* ixmlElement_getNode(IXML_Element* element);

/**
 * Returns node which represents provided document.
 */
IXML_Node* ixmlDocument_getNode(IXML_Document* doc);

/**
 * Returns node which represents provided attribute.
 */
IXML_Node* ixmlAttr_getNode(IXML_Attr* attr);
/**@}*/

/**
 * Returns first element in the documents with provided attribute value.
 *
 * @param doc Document where to search.
 * @param attrName Name of the attribute to check.
 * @param attrValue Attribute value which should be found
 * @return a pointer to the element with matching attribute; NULL if no such
 * element found.
 */
IXML_Element* ixmlDocument_getElementByAttrValue(IXML_Document* doc, const char* attrName, const char* attrValue);

/**
 * Adds new attribute to the element. If attribute with the same name already
 * exists, it's value will be updated. Writes warning message to log on error.
 *
 * @param attrName Name of the attribute to be added.
 * @param attrValue Value of the attribute.
 * @return @a 0 on success or @a 1 on error.
 */
int ixmlElement_setAttributeWithLog(IXML_Element* element, const char* attrName, const char* attrValue);

/**
 * Removes attribute from the provided element.
 * Unlike @a ixmlElement_removeAttribute() the attribute node is removed
 * totally, not only value.
 *
 * @param attrName Name of the attribute to be removed.
 * @return @a 0 on success or @a 1 on error.
 */
int ixmlElement_removeAttributeWithLog(IXML_Element* element, const char* attrName);

/**
 * Duplicates provided element.
 *
 * Creates new instance of @a IXML_Document and copies entire element
 * including all its children to that document.
 *
 * @note Don't forget to free owner document of the clone after usage.
 * @see @a ixmlNode_getOwnerDocument()
 *
 * @param source Element to be copied.
 * @return @a NULL on error, otherwise a pointer to the new copy of the source
 *         element.
 */
IXML_Element* ixmlElement_cloneWithLog(IXML_Element* source);

/**
 * Frees the @a IXML_Document which the provided element belongs to.
 * @note As long as the whole document is freed, all other nodes
 * which belongs to the same document are also freed.
 * @todo implement also ixmlNode_freeOwnerDocument().
 *
 * @param element Element which should be freed with it's owner document.
 */
void ixmlElement_freeOwnerDocument(IXML_Element* element);

int ixmlElement_copyAttributeWithLog(IXML_Element* source,
                                     IXML_Element* target,
                                     const char* attrName,
                                     BOOL obligatory);

IXML_Element* ixmlAttr_getOwnerElement(IXML_Attr* attr);

#endif /* IXML_EXT_H_ */
