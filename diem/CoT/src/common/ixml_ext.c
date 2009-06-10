/** @file
 * Implements utility methods for work with XML DOM structure.
 */

#include <string.h>
#include <stdlib.h>
#include "lwl_ext.h"
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

    //TODO: That's not a good way to do things.
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

///**
// * Does the same with #ixmlDocument_getElementByHrefRecursive(IXML_Document*,const char*,int).
// * The difference is that this implementation uses utility from upnp library
// * to resolve URI. Such solution helps to resolve relative URI starting with '/'.
// */
//static IXML_Node* ixmlDocument_getElementByHrefRecursiveUpnp(IXML_Node* node,
//		const char* href, const char* baseUri)
//{
//	if (node == NULL)
//	{
//		return NULL;
//	}
//
//	IXML_Node* match = NULL;
//	IXML_Element* element = ixmlNode_convertToElement(node);
//	if (element != NULL)
//	{
//		const char* currUri = ixmlElement_getAttribute(element, HREF);
//		if (currUri != NULL)
//		{
//			char* absUri = (char*) malloc((baseUri == NULL) ? 0 : strlen(
//					baseUri) + strlen(currUri) + 2);
//			int error = UpnpResolveURL(baseUri, currUri, absUri);
//			if (error != UPNP_E_SUCCESS)
//			{
//				printf("!!!Wrong uri %s+%s (error %d)\n", baseUri, currUri,
//						error);
//			}
//			else
//			{
//				printf("resolvedUri=%s\n", absUri);
//				int length = strlen(absUri);
//				if (strncmp(absUri, href, length) == 0)
//				{
//					printf("~=(%d) %s, %s\n", length, absUri, href);
//					// we found something
//					if (length == strlen(href))
//					{
//						// we compared the whole href. Now check that
//						// this is not a reference to another object
//						//TODO: remove this when obix documents will be stored
//						// separately
//						if (strcmp("ref", ixmlNode_getNodeName(node)) == 0)
//						{
//							printf("found reference to the object\n");
////							match = ixmlDocument_getElementByHrefRecursiveUpnp(
////												ixmlNode_getNextSibling(node), href, baseUri);
//						}
//						else
//						{
//							printf("fully checked\n");
//							free(absUri);
//							return node;
//						}
//					}
//					else
//					{
//						printf("still not the end (%d from %d)\n", length,
//								strlen(href));
//						// still have to compare href of the child
//						match = ixmlDocument_getElementByHrefRecursiveUpnp(
//								ixmlNode_getFirstChild(node), href, absUri);
//					}
//				}
//				else
//				{
//					printf("!=(%d) %s, %s\n", length, absUri, href);
//					// href doesn't much, let's check other nodes
//					match = ixmlDocument_getElementByHrefRecursiveUpnp(
//							ixmlNode_getNextSibling(node), href, baseUri);
//				}
//				free(absUri);
//			}
//
//		}
//	}
//	else // element == NULL (i.e. node is not an element)
//	{ // check the next node
//		match = ixmlDocument_getElementByHrefRecursiveUpnp(
//				ixmlNode_getFirstChild(node), href, baseUri);
//		if (match == NULL)
//		{
//			match = ixmlDocument_getElementByHrefRecursiveUpnp(
//					ixmlNode_getNextSibling(node), href, baseUri);
//		}
//	}
//
//	return match;
//}

int ixmlElement_setAttributeWithLog(IXML_Element* element, const char* attrName, const char* attrValue)
{
    int error = ixmlElement_setAttribute(element, attrName, attrValue);
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to add attribute to the XML element (ixml error %d).", error);
        return 1;
    }

    return 0;
}

int ixmlElement_removeAttributeWithLog(IXML_Element* element, const char* attrName)
{
    IXML_Attr* attr = ixmlElement_getAttributeNode(element, attrName);
    if (attr == NULL)
    {
        log_warning("Unable to remove \'%s\' attribute: No attribute is found.", attrName);
        return -1;
    }

    int error = ixmlElement_removeAttributeNode(element, attr, &attr);
    if (error != IXML_SUCCESS)
    {
        log_warning("Unable to remove \'%s\' attribute: error %d", attrName, error);
        return -1;
    }

    ixmlAttr_free(attr);
    return 0;
}

IXML_Element* ixmlElement_cloneWithLog(IXML_Element* source)
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
        log_error("Unable to clone XML. ixmlDocument_createDocumentEx() returned %d", error);
        return NULL;
    }
    error = ixmlDocument_importNode(doc, ixmlElement_getNode(source), TRUE, &node);
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to clone XML. ixmlDocument_importNode() returned %d", error);
        return NULL;
    }
    error = ixmlNode_appendChild(ixmlDocument_getNode(doc), node);
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to clone XML. ixmlNode_appendChild() returned %d", error);
        return NULL;
    }

    return ixmlNode_convertToElement(node);
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
