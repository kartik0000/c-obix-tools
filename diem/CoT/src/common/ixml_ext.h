/* *****************************************************************************
 * Copyright (c) 2009, 2010 Andrey Litvinov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * ****************************************************************************/
/** @file
 * Defines utility methods for work with XML DOM structure.
 * Expands functionality of @a ixml library which provides DOM XML
 * parser functionality. @a ixml is distributed as a part of @a libupnp
 * (http://pupnp.sourceforge.net/).
 *
 * @author Andrey Litvinov
 */
#ifndef IXML_EXT_H_
#define IXML_EXT_H_

#include <upnp/ixml.h>
/**
 * ixml.h already contains the definition of BOOL type, thus we consider
 * that bool.h is already added.
 */
#define BOOL_H_

/**@name XML node types conversion
 * @{
 */
/**
 * Returns node which represents provided document.
 *
 * @param doc Document whose node representation is needed.
 * @return Node corresponding to the provided document.
 */
IXML_Node* ixmlDocument_getNode(IXML_Document* doc);

/**
 * Converts node to the element.
 *
 * @param node Node which should be converted.
 * @return NULL if node is not an element (i.e. tag).
 */
IXML_Element* ixmlNode_convertToElement(IXML_Node* node);

/**
 * Converts node to the attribute.
 *
 * @param node Node which should be converted.
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
 * Returns first element in the document with provided attribute value.
 *
 * @param doc Document where to search.
 * @param attrName Name of the attribute to check.
 * @param attrValue Attribute value which should be found.
 * @return A pointer to the element with matching attribute; @a NULL if no such
 * element found.
 */
IXML_Element* ixmlDocument_getElementByAttrValue(
    IXML_Document* doc,
    const char* attrName,
    const char* attrValue);

/**
 * Returns first child element with provided attribute value.
 *
 * @param element The search will be performed among children of this element.
 * @param attrName Name of the attribute to check.
 * @param attrValue Attribute value which should be found.
 * @return A pointer to the element with matching attribute; @a NULL if no such
 * element found.
 */
IXML_Element* ixmlElement_getChildElementByAttrValue(
    IXML_Element* element,
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
 * @param node Node which should be freed together with it's owner document.
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
 * exists, it's value will be updated. Writes warning message to log (using
 * log_utils.h) on error.
 *
 * @param element Element to which the attribute should be added.
 * @param attrName Name of the attribute to be added.
 * @param attrValue Value of the attribute.
 * @return @a 0 on success or @a -1 on error.
 */
int ixmlElement_setAttributeWithLog(IXML_Element* element,
                                    const char* attrName,
                                    const char* attrValue);

/**
 * Removes attribute from the provided element.
 * Unlike @a ixmlElement_removeAttribute() the attribute node is removed
 * totally, not only value. Also writes warning message to log (using
 * log_utils.h) on error.
 *
 * @param element Element from which the attribute should be removed.
 * @param attrName Name of the attribute to be removed.
 * @return @a 0 on success or @a -1 on error.
 */
int ixmlElement_removeAttributeWithLog(IXML_Element* element,
                                       const char* attrName);

/**
 * Duplicates provided element.
 *
 * Creates new instance of @a IXML_Document and copies entire element
 * including all its children to that document. Also writes message to
 * log (using log_utils.h) on error.
 *
 * @note Don't forget to free owner document of the clone after usage.
 * @see @a ixmlNode_getOwnerDocument() at @a ixml.h
 *
 * @param source Element to be copied.
 * @return @a NULL on error, otherwise a pointer to the new copy of the source
 *         element.
 */
IXML_Element* ixmlElement_cloneWithLog(IXML_Element* source, BOOL deep);

/**
 * Frees the @a IXML_Document which the provided element belongs to.
 * @note As long as the whole document is freed, all other nodes
 * which belongs to the same document are also freed.
 *
 * @param element Element which should be freed together with it's owner
 *                document.
 */
void ixmlElement_freeOwnerDocument(IXML_Element* element);

/**
 * Copies attribute value from one element to another.
 * If the attribute with provided name doesn't exist in the target node, it is
 * created. Method also writes error messages using log_utils.h facilities.
 *
 * @param source Element where the attribute will be copied from.
 * @param target Element where the attribute will be copied to.
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
 * Creates new element (tag) and appends as a child to the provided parent
 * element.
 *
 * @param parent Element to which the new tag will be added.
 * @param childTagName Name of the new tag.
 * @return A reference to the new element, or @a NULL on error.
 */
IXML_Element* ixmlElement_createChildElementWithLog(
    IXML_Element* parent,
    const char* childTagName);

/**
 * Creates a copy of provided element and inserts it as a child tag.
 *
 * @param parent Element to which the child tag should be inserted.
 * @param childSource Element, which should be inserted to the parent.
 * @param createdChild If not @a NULL, a reference to the created child tag is
 * 				returned here.
 * @return @a IXML_SUCCESS if everything went well, or one of @a ixml error
 * 		   codes.
 */
int ixmlElement_putChildWithLog(IXML_Element* parent,
                                IXML_Element* childSource,
                                IXML_Element** createdChild);

/**
 * Removes child element from the parent tag and releases memory allocated by
 * the child.
 *
 * @param parent Element, from which child should be removed.
 * @param child Element to be deleted.
 * @return @a IXML_SUCCESS if everything went well, or one of @a ixml error
 * 		   codes.
 */
int ixmlElement_freeChildElement(IXML_Element* parent, IXML_Element* child);

/**
 * Returns element which the provided attribute belongs to.
 *
 * @param attr Attribute whose owner element should be returned.
 * @return Owner element.
 */
IXML_Element* ixmlAttr_getOwnerElement(IXML_Attr* attr);

/**
 * Returns the attribute value. If the attribute is not found, logs an error
 * message.
 *
 * @param element Element whose attribute should be returned;
 * @param attrName The name of the attribute.
 * @return Value of the specified attribute or @a NULL if the attribute is not
 *         found.
 */
const char* ixmlElement_getObligarotyAttr(
    IXML_Element* element,
    const char* attrName);

/**
 * Returns first child tag of the specified element.
 *
 * @param element Element whose child should be returned.
 * @return First child tag of the specified element, or @a NULL if the element
 *         does not have nested tags.
 */
IXML_Element* ixmlElement_getFirstChild(IXML_Element* element);

/**
 * Chooses elements from the input list that have specified value of the
 * specified attribute. Input list remains unchanged.
 *
 * @param filteredList List that contains only elements with specified
 *        attribute value;
 * @param list List of XML elements;
 * @param attrName Name of the attribute to check;
 * @param attrValue Desired attribute value;
 * @return @a IXML_SUCCESS if everything went well, or one of @a ixml error
 * 		   codes.
 */
int ixmlNodeList_filterListByAttrValue(
    IXML_NodeList** filteredList,
    IXML_NodeList* list,
    const char* attrName,
    const char* attrValue);

#endif /* IXML_EXT_H_ */
