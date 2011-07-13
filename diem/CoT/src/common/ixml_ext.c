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
 * Implements utility methods for work with XML DOM structure.
 *
 * @see ixml_ext.h
 *
 * @author Andrey Litvinov
 */

#include <string.h>
#include <stdlib.h>
#include "log_utils.h"
#include "ixml_ext.h"

IXML_Element* ixmlNode_convertToElement(IXML_Node* node)
{
    if (node == NULL)
    {
        return NULL;
    }

    if (ixmlNode_getNodeType(node) != eELEMENT_NODE)
    {
        return NULL;
    }

    return (IXML_Element*) node;
}

IXML_Attr* ixmlNode_convertToAttr(IXML_Node* node)
{
    if (node == NULL)
    {
        return NULL;
    }

    if (ixmlNode_getNodeType(node) != eATTRIBUTE_NODE)
    {
        return NULL;
    }

    return (IXML_Attr*) node;
}

IXML_Node* ixmlElement_getNode(IXML_Element* element)
{
    return &(element->n);
}

IXML_Node* ixmlDocument_getNode(IXML_Document* doc)
{
    return &(doc->n);
}

IXML_Node* ixmlAttr_getNode(IXML_Attr* attr)
{
    return &(attr->n);
}

IXML_Element* ixmlDocument_getRootElement(IXML_Document* doc)
{
    IXML_Node* node = ixmlNode_getFirstChild(ixmlDocument_getNode(doc));
    IXML_Element* element = ixmlNode_convertToElement(node);

    // search for element node in all document's child nodes
    while((element == NULL) && (node != NULL))
    {
        node = ixmlNode_getNextSibling(node);
        element = ixmlNode_convertToElement(node);
    }

    return element;
}

int ixmlElement_setAttributeWithLog(IXML_Element* element,
                                    const char* attrName,
                                    const char* attrValue)
{
    int error = ixmlElement_setAttribute(element, attrName, attrValue);
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to add attribute to the XML element (ixml error %d).",
                  error);
        return -1;
    }

    return 0;
}

int ixmlElement_removeAttributeWithLog(IXML_Element* element,
                                       const char* attrName)
{
    IXML_Attr* attr = ixmlElement_getAttributeNode(element, attrName);
    if (attr == NULL)
    {
        log_warning("Unable to remove \'%s\' attribute: "
                    "No attribute is found.", attrName);
        return -1;
    }

    int error = ixmlElement_removeAttributeNode(element, attr, &attr);
    if (error != IXML_SUCCESS)
    {
        log_warning("Unable to remove \'%s\' attribute: error %d",
                    attrName, error);
        return -1;
    }

    ixmlAttr_free(attr);
    return 0;
}

IXML_Element* ixmlElement_cloneWithLog(IXML_Element* source, BOOL deep)
{
    IXML_Document* doc;
    IXML_Node* node;

    if (source == NULL)
    {
        return NULL;
    }

    int error = ixmlDocument_createDocumentEx(&doc);
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to clone XML. ixmlDocument_createDocumentEx() "
                  "returned %d", error);
        return NULL;
    }
    error =
        ixmlDocument_importNode(doc, ixmlElement_getNode(source), deep, &node);
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to clone XML. ixmlDocument_importNode() "
                  "returned %d", error);
        return NULL;
    }
    error = ixmlNode_appendChild(ixmlDocument_getNode(doc), node);
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to clone XML. ixmlNode_appendChild() returned %d",
                  error);
        return NULL;
    }

    return ixmlNode_convertToElement(node);
}

void ixmlNode_freeOwnerDocument(IXML_Node* node)
{
    ixmlDocument_free(ixmlNode_getOwnerDocument(node));
}

void ixmlElement_freeOwnerDocument(IXML_Element* element)
{
    ixmlDocument_free(ixmlNode_getOwnerDocument(ixmlElement_getNode(element)));
}

int ixmlElement_copyAttributeWithLog(IXML_Element* source,
                                     IXML_Element* target,
                                     const char* attrName,
                                     BOOL obligatory)
{
    const char* value = ixmlElement_getAttribute(source, attrName);
    if (value == NULL)
    {
        if (obligatory)
        {
            log_error("Unable to copy element attribute. "
                      "Obligatory attribute \"%s\" is not found.");
        }
        return IXML_NOT_FOUND_ERR;
    }

    int error = ixmlElement_setAttribute(target, attrName, value);
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to copy element attribute. "
                  "ixmlElement_setAttribute() returned %d.", error);
    }

    return error;
}

IXML_Element* ixmlAttr_getOwnerElement(IXML_Attr* attr)
{
    // TODO a kind of hack, we rely on internal structure of Attr
    if (attr == NULL)
    {
        return NULL;
    }

    return attr->ownerElement;
}

/**
 * Helper function for #ixmlDocument_getElementByAttrValue.
 * It is not good idea to make it public, because it searches for the required
 * element also among neighbors of provided node
 * (not only among it's children).
 */
static IXML_Element* ixmlNode_getElementByAttrValue(
    IXML_Node* node,
    const char* attrName,
    const char* attrValue)
{
    if (node == NULL)
    {	// exit point from recursion
        return NULL;
    }

    IXML_Element* element = ixmlNode_convertToElement(node);

    if (element != NULL)
    {
        // check the attribute
        const char* val = ixmlElement_getAttribute(element, attrName);
        if ((val != NULL) && (strcmp(val, attrValue) == 0))
        {
            // congratulations, we found the node
            return element;
        }
    }

    // current element doesn't match, let's check it's children and neighbors
    element = ixmlNode_getElementByAttrValue(
                  ixmlNode_getNextSibling(node),
                  attrName,
                  attrValue);
    if (element != NULL)
    {	// required element was found among neighbors
        return element;
    }

    return ixmlNode_getElementByAttrValue(ixmlNode_getFirstChild(node),
                                          attrName,
                                          attrValue);
}

IXML_Element* ixmlElement_getChildElementByAttrValue(
    IXML_Element* element,
    const char* attrName,
    const char* attrValue)
{
    return ixmlNode_getElementByAttrValue(
               ixmlNode_getFirstChild(ixmlElement_getNode(element)),
               attrName,
               attrValue);
}

IXML_Element* ixmlDocument_getElementByAttrValue(
    IXML_Document* doc,
    const char* attrName,
    const char* attrValue)
{
    return ixmlNode_getElementByAttrValue(
               ixmlDocument_getNode(doc),
               attrName,
               attrValue);
}

IXML_Node* ixmlNode_parseBuffer(const char* data)
{
    IXML_Document* doc;

    if (data == NULL)
    {
        return NULL;
    }


    int error = ixmlParseBufferEx(data, &doc);
    if (error != IXML_SUCCESS)
    {
        log_warning("Unable to parse XML (error %d). Input "
                    "data:\n%s", error, data);
        return NULL;
    }

    return ixmlNode_getFirstChild(ixmlDocument_getNode(doc));
}

IXML_Element* ixmlElement_parseBuffer(const char* data)
{
    return ixmlNode_convertToElement(ixmlNode_parseBuffer(data));
}

IXML_Element* ixmlElement_createChildElementWithLog(
    IXML_Element* parent,
    const char* childTagName)
{
    IXML_Element* childTag;
    IXML_Document* ownerDocument =
        ixmlNode_getOwnerDocument(ixmlElement_getNode(parent));

    int error =
        ixmlDocument_createElementEx(ownerDocument, childTagName, &childTag);
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to create tag \"%s\". "
                  "ixmlDocument_createElementEx() returned %d.",
                  childTagName, error);
        return NULL;
    }
    error = ixmlNode_appendChild(ixmlElement_getNode(parent),
                                 ixmlElement_getNode(childTag));
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to create tag \"%s\". "
                  "ixmlNode_appendChild() returned %d.", childTagName, error);
        return NULL;
    }

    return childTag;
}

int ixmlElement_putChildWithLog(IXML_Element* parent,
                                IXML_Element* childSource,
                                IXML_Element** createdChild)
{
    if (childSource == NULL)
    {	// nothing to insert
        return 0;
    }

    IXML_Node* importedNode;
    IXML_Document* ownerDoc =
        ixmlNode_getOwnerDocument(ixmlElement_getNode(parent));

    int error = ixmlDocument_importNode(ownerDoc,
                                        ixmlElement_getNode(childSource),
                                        TRUE,
                                        &importedNode);
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to import element to a new document:"
                  "ixmlDocument_importNode() returned %d.", error);
        return error;
    }

    error = ixmlNode_appendChild(ixmlElement_getNode(parent), importedNode);
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to import element to a new document:"
                  "ixmlNode_appendChild() returned %d.", error);
        return error;
    }

    if (createdChild != NULL)
    {
        *createdChild = ixmlNode_convertToElement(importedNode);
    }

    return IXML_SUCCESS;
}

int ixmlElement_freeChildElement(IXML_Element* parent, IXML_Element* child)
{
    IXML_Node* removedNode;
    int error = ixmlNode_removeChild(ixmlElement_getNode(parent),
                                     ixmlElement_getNode(child),
                                     &removedNode);
    if (error != IXML_SUCCESS)
    {
        return error;
    }

    ixmlNode_free(removedNode);

    return IXML_SUCCESS;
}

const char* ixmlElement_getObligarotyAttr(
    IXML_Element* element,
    const char* attrName)
{
    const char* attr = ixmlElement_getAttribute(element, attrName);
    if (attr == NULL)
    {
        log_error("Tag <%s/> does not have obligatory attribute \"%s\".",
                  ixmlElement_getTagName(element), attrName);
    }

    return attr;
}

IXML_Element* ixmlElement_getFirstChild(IXML_Element* element)
{
	IXML_Node* childNode = ixmlNode_getFirstChild(ixmlElement_getNode(element));
	IXML_Element* childElement = NULL;

	while (childNode != NULL)
	{
		IXML_Element* childElement = ixmlNode_convertToElement(childNode);
		if (childElement != NULL)
			break;

		childNode = ixmlNode_getNextSibling(childNode);
	}

	return childElement;
}

static IXML_NodeList* ixmlNodeList_alloc(IXML_Node* node)
{
	IXML_NodeList* output = (IXML_NodeList*) malloc(sizeof(IXML_NodeList));
	if (output == NULL)
	{
		return NULL;
	}

	output->next = NULL;
	output->nodeItem = node;
	return output;
}

int ixmlNodeList_filterListByAttrValue(
    IXML_NodeList** filteredList,
    IXML_NodeList* list,
    const char* attrName,
    const char* attrValue)
{
    IXML_NodeList* iterator = list;
    IXML_NodeList* outputList = NULL;
    IXML_NodeList* outputTail = NULL;

    while (iterator != NULL)
    {
        IXML_Element* element = ixmlNode_convertToElement(iterator->nodeItem);
        if (element == NULL)
        {
            if (outputList != NULL)
                ixmlNodeList_free(outputList);
            return IXML_INVALID_PARAMETER;
        }

        const char* attr = ixmlElement_getAttribute(element, attrName);
        if ((attr != NULL) && (strcmp(attrValue, attr) == 0))
        {
            IXML_NodeList* listElement = ixmlNodeList_alloc(iterator->nodeItem);
            if (listElement == NULL)
            {
                if (outputList != NULL)
                    ixmlNodeList_free(outputList);
                return IXML_INSUFFICIENT_MEMORY;
            }

            if (outputTail == NULL)
            {
            	outputList = listElement;
            }
            else
            {
            	outputTail->next = listElement;
            }

            outputTail = listElement;
        }

        iterator = iterator->next;
    }

    *filteredList = outputList;
    return IXML_SUCCESS;
}
