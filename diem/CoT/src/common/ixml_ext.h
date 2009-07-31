/** @file
 * Defines utility methods for work with XML DOM structure.
 * Expands functionality of IXML library which provides DOM XML
 * parser functionality. IXML is distributed as a part of libupnp
 * (http://pupnp.sourceforge.net/).
 */
#ifndef IXML_EXT_H_
#define IXML_EXT_H_

#include <upnp/ixml.h>

/**@name XML node types conversion
 * @{
 */
/**
 * Returns node which represents provided document.
 * @param doc Document whose node representation is needed.
 * @return Node corresponding to the provided document.
 */
IXML_Node* ixmlDocument_getNode(IXML_Document* doc);

/**
 * Converts node to the element.
 *
 * @param Node to be converted.
 * @return NULL if node is not an element (i.e. tag).
 */
IXML_Element* ixmlNode_convertToElement(IXML_Node* node);

/**
 * Converts node to the attribute.
 *
 * @param Node to be converted.
 * @return NULL if node is not an attribute.
 */
IXML_Attr* ixmlNode_convertToAttr(IXML_Node* node);

/**
 * Returns node which represents provided element.
 *
 * @param element Element whose node representation is needed.
 * @return Node corresponding to the provided element.
 */
IXML_Node* ixmlElement_getNode(IXML_Element* element);

/**
 * Returns node which represents provided attribute.
 *
 * @param attr Attribute whose node representation is needed.
 * @return Node corresponding to the provided attribute.
 */
IXML_Node* ixmlAttr_getNode(IXML_Attr* attr);
/**@}*/

/**
 * Returns root element (root tag) of the XML document.
 *
 * @param doc Document, whose root element should be retrieved.
 * @return Root element or @a NULL if the document is empty or other error
 * 		   occurred.
 */
IXML_Element* ixmlDocument_getRootElement(IXML_Document* doc);

/**
 * Returns first element in the documents with provided attribute value.
 *
 * @param doc Document where to search.
 * @param attrName Name of the attribute to check.
 * @param attrValue Attribute value which should be found
 * @return A pointer to the element with matching attribute; @a NULL if no such
 * element found.
 */
IXML_Element* ixmlDocument_getElementByAttrValue(
    IXML_Document* doc,
    const char* attrName,
    const char* attrValue);

/**
 * Parses an XML text buffer and returns the parent node of the generated DOM
 * structure.
 * @note Don't forget to free memory allocated for the parsed document, not only
 *       the node (e.g. using #ixmlElement_freeOwnerDocument() ).
 *
 * @param data Text buffer to be parsed.
 * @return Node representing parent tag of the parsed XML.
 */
IXML_Node* ixmlNode_parseBuffer(const char* data);

/**
 * Frees the @a IXML_Document which the provided node belongs to.
 * @note As long as the whole document is freed, all other nodes
 * which belongs to the same document are also freed.
 *
 * @param node Node which should be freed with it's owner document.
 */
void ixmlNode_freeOwnerDocument(IXML_Node* node);

/**
 * Parses an XML text buffer and returns the parent element of the generated DOM
 * structure.
 * @note Don't forget to free memory allocated for the parsed document, not only
 *       the element (e.g. using #ixmlElement_freeOwnerDocument() ).
 *
 * @param data Text buffer to be parsed.
 * @return Element representing parent tag of the parsed XML.
 */
IXML_Element* ixmlElement_parseBuffer(const char* data);

/**
 * Adds new attribute to the element. If attribute with the same name already
 * exists, it's value will be updated. Writes warning message to log on error.
 *
 * @param attrName Name of the attribute to be added.
 * @param attrValue Value of the attribute.
 * @return @a 0 on success or @a 1 on error.
 */
int ixmlElement_setAttributeWithLog(IXML_Element* element,
                                    const char* attrName,
                                    const char* attrValue);

/**
 * Removes attribute from the provided element.
 * Unlike @a ixmlElement_removeAttribute() the attribute node is removed
 * totally, not only value.
 *
 * @param attrName Name of the attribute to be removed.
 * @return @a 0 on success or @a 1 on error.
 */
int ixmlElement_removeAttributeWithLog(IXML_Element* element,
                                       const char* attrName);

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
 *
 * @param element Element which should be freed with it's owner document.
 */
void ixmlElement_freeOwnerDocument(IXML_Element* element);

/**
 * Copies attribute value from one element to another.
 * If the attribute with provided name doesn't exist in the target node, it is
 * created. Method also writes error messages using #lwl_ext.h facilities.
 *
 * @param source Element where the attribute will be copied from.
 * @param target Element which the attribute will be copied to.
 * @param attrName Name of the attribute to be copied.
 * @param obligatory Tells whether the attribute should necessarily present in
 *                   the source tag. If @a TRUE and the attribute is missing
 *                   than the error message will be logged. If it is @a False
 *                   than only an error code will be returned in the same
 *                   situation.
 * @return @a IXML_SUCCESS if everything went well, or one of @a ixml error
 * 		   codes.
 */
int ixmlElement_copyAttributeWithLog(IXML_Element* source,
                                     IXML_Element* target,
                                     const char* attrName,
                                     BOOL obligatory);

/**
 * Returns element which the provided attribute belongs to.
 *
 * @param attr Attribute whose owner element should be returned.
 * @return Owner element.
 */
IXML_Element* ixmlAttr_getOwnerElement(IXML_Attr* attr);

#endif /* IXML_EXT_H_ */
