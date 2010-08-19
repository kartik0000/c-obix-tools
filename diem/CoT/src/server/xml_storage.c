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
 * Simple implementation of XML storage.
 * All data is stored in one DOM structure in memory. Nothing is saved on disk.
 *
 * @see xml_storage.h
 *
 * @author Andrey Litvinov
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <obix_utils.h>
#include <ixml_ext.h>
#include <xml_config.h>
#include <log_utils.h>
#include "xml_storage.h"

/** Link to the list of references for each connected device. */
#define DEVICE_LIST_URI "/obix/devices/"

/** Prefix for all paths inside oBIX server. Only system objects have another
 * URI and they are not accessible for a client. */
static const char* DEFAULT_URI_PREFIX = "/obix";
/** Length of #DEFAULT_URI_PREFIX */
static const int DEFAULT_URI_PREFIX_LENGTH = 5;

const char* OBIX_SYS_WATCH_STUB = "/sys/watch-stub/";
const char* OBIX_SYS_ERROR_STUB = "/sys/error-stub/";
const char* OBIX_SYS_WATCH_OUT_STUB = "/sys/watch-out-stub/";

const char* OBIX_META = "meta";

const char* OBIX_META_VAR_HANDLER_ID = "h-id";

/** Files with initial storage contents. */
static const char* OBIX_STORAGE_FILES[] =
    {"server_lobby.xml",		//Lobby object
     "server_about.xml",		//About object
     "server_watch.xml",		//Watch object
     "server_sys_objects.xml",	//system objects
     "server_devices.xml",		//Device list
     "server_def.xml",			//Custom contracts
     "server_test_device.xml"	//Test device object
    };

/** Number of file names in #OBIX_STORAGE_FILES */
static const int OBIX_STORAGE_FILES_COUNT = 7;

/** The place where all data is stored. */
static IXML_Document* _storage = NULL;

/** Prints contents of XML node to debug log. */
static void printXMLContents(IXML_Node* node, const char* title)
{
    DOMString str = ixmlPrintNode(node);
    log_debug("\n%s:\n%s",title, str);
    ixmlFreeDOMString(str);
}

/**
 * Helper function which searches for last slash in the provided string.
 * Search is performed backwards from the provided position.
 * @note Function assumes that there is at least one slash in the string.
 *
 * @param str String where search should be performed.
 * @param startPosition Position from which search should be started.
 * @return Position of found slash.
 */
static int getLastSlashPosition(const char* str, int startPosition)
{
    const char* temp = str + startPosition;
    while ((*temp != '/') && (temp != str))
    {
        temp--;
    }

    return temp - str;
}

/**
 * Compares URIs. Helper function.
 * Assumes that requiredUri already contains correct server address
 * and shows only relative address from the server root.
 * checked - number of symbols in requiredUri which were already
 * compared and match with parent node's address.
 *
 * @return @li @b  0 If currentUri matches with requiredUri assuming that
 *                   first 'checked' symbols already match.
 *         @li @b >0 If currentUri matches with part of requiredUri. In that
 *                   case returning value is a number of symbols already
 *                   checked in requiredUri.
 *         @li @b <0 If currentUri doesn't match with requiredUri.
 */
static int compare_uri(const char* currentUri,
                       const char* requiredUri,
                       int checked,
                       int* slashFlag)
{
    if (*currentUri == '/')
    {
        // currentUri contains path from server root
        // ignore checked
        checked = 0;
    }

    //clear last comparison flag
    *slashFlag = 0;

    int currLength = strlen(currentUri);
    BOOL currUriHasRemaining;
    if (currentUri[currLength - 1] == '/')
    {
        // log_debug("Uri \"%s\" ends with slash", currentUri);
        // ignore ending '/' during comparison
        currUriHasRemaining = FALSE;
        currLength--;
        // see xmldb_get docs for explanation
        (*slashFlag)++;
    }
    else
    {
        // log_debug("Uri \"%s\" doesn't end with slash", currentUri);
        int slashPos = getLastSlashPosition(currentUri, currLength - 1);
        if (slashPos <= 0)
        {
            // no slash found in currentUri, compare it whole
            currUriHasRemaining = FALSE;
        }
        else
        {
            // ignore everything after last '/'
            currUriHasRemaining = TRUE;
            currLength = slashPos;
        }
    }

    if (strncmp(currentUri, requiredUri + checked, currLength) != 0)
    {
        //log_debug("Uri %s and %s doens't match", currentUri, requiredUri + checked);
        // uri's do not match
        return -1;
    }
    //log_debug("Uri %s and %s match at least partially (%d)", currentUri, requiredUri + checked, currLength);
    // currentUri matches with some piece of requiredUri

    // calculate size of requiredUri remaining
    int requiredLength = strlen(requiredUri + checked);
    // ignore last /
    if (requiredUri[checked + requiredLength - 1] == '/')
    {
        requiredLength--;
        // see xmldb_getLastUriCompSlashFlag for explanation
        (*slashFlag)--;
    }
    //log_debug("checked %d, required %d, requiredUri ends with \'%c\'", currLength, requiredLength, requiredUri[checked + requiredLength]);
    if ((currLength == requiredLength) && !currUriHasRemaining)
    {
        //log_debug("full match");
        // currentUri matches with the end of requiredUri
        return 0;
    }

    if (currUriHasRemaining)
    {
        // try to check remainder
        int remainderLength = strlen(currentUri) - currLength;
        if (((currLength + remainderLength) == requiredLength) &&
                (strncmp(currentUri + currLength,
                         requiredUri + checked + currLength,
                         strlen(currentUri) - currLength) == 0))
        {
            // reminders match completely
            return 0;
        }
        // otherwise we just ignore the reminder address because
        // it is not inherited by child nodes
    }

    // some piece of requiredUri left unchecked
    //    log_debug("somePeaceUnchecked %d", checked + currLength + 1);
    // we checked a piece of path + ending '/'
    return checked + currLength + 1;
}

/**
 * Helper function for #getNodeByHref(IXML_Document*,const char*).
 *
 * @param checked specifies number of symbols from href which are already
 * checked (and match) in parent node.
 */
static IXML_Node* getNodeByHrefRecursive(IXML_Node* node, const char* href,
        int checked, int* slashFlag)
{
    if (node == NULL)
    {
        //recursion exit point
        return NULL;
    }

    IXML_Node* match = NULL;
    IXML_Element* element = ixmlNode_convertToElement(node);
    int compareResult = 0;
    // ignore all nodes which are not tags, are reference tags
    // or do not have 'href' attribute
    if (element != NULL)
    {
        const char* currentUri =
            ixmlElement_getAttribute(element, OBIX_ATTR_HREF);
        if ((strcmp(ixmlElement_getTagName(element), OBIX_OBJ_REF) != 0)
                && (currentUri != NULL))
        {
            compareResult = compare_uri(currentUri, href, checked, slashFlag);
            if (compareResult == 0)
            {
                // we found the required node
                return node;
            }
        }
    }

    // check children
    // note that compareResult == 0 means that URI's
    // were not compared at all
    if (compareResult == 0)
    {
        match = getNodeByHrefRecursive(ixmlNode_getFirstChild(node),
                                       href,
                                       checked,
                                       slashFlag);
    }
    else if (compareResult > 0)
    {
        // we found part of the uri on this step
        // continue search in children the remaining address
        match = getNodeByHrefRecursive(ixmlNode_getFirstChild(node),
                                       href,
                                       compareResult,
                                       slashFlag);
    }
    // do not check children if compareResult < 0

    if (match == NULL)
    {
        match = getNodeByHrefRecursive(ixmlNode_getNextSibling(node),
                                       href,
                                       checked,
                                       slashFlag);
    }

    return match;
}

/**
 * Retrieves XML node with provided URI from the storage.
 * @param slashFlag Slash flag is returned here. This flag shows whether
 * 			requested URI differs from URI of returned object in trailing slash.
 * 			@li @a 0 if both URI had the same ending symbol;
 * 			@li @a 1 if the object in the storage had trailing slash but
 * 					requested URI hadn't;
 *			@li @a -1 if requested URI had trailing slash, but the object
 *					hadn't.
 */
static IXML_Node* getNodeByHref(IXML_Document* doc,
                                const char* href,
                                int* slashFlag)
{
    // in case if no slash flag is required, provide a temp variable to the
    // further methods.
    int temp;
    if (slashFlag == NULL)
    {
        slashFlag = &temp;
    }
    return getNodeByHrefRecursive(ixmlDocument_getNode(doc),
                                  href,
                                  0,
                                  slashFlag);
}

/**
 * Checks for all href attributes in the provided piece of XML and inserts
 * @a /obix prefix if necessary to all URI's which are absolute.
 */
static void insertDefaultUriPrefix(IXML_Node* node)
{
    if (node == NULL)
        return;	// exit point for the recursion

    // if this node is element and has 'href' attribute than check it:
    IXML_Element* element = ixmlNode_convertToElement(node);
    if (element != NULL)
    {
        const char* href = ixmlElement_getAttribute(element, OBIX_ATTR_HREF);
        if ((href != NULL) && (*href == '/'))
        {
            // href attribute points to the server root
            // check whether it starts with /obix/
            if (strncmp(href,
                        DEFAULT_URI_PREFIX,
                        DEFAULT_URI_PREFIX_LENGTH) != 0)
            {
                // let's add /obix prefix
                char newHref[strlen(href) + DEFAULT_URI_PREFIX_LENGTH + 1];
                strcpy(newHref, DEFAULT_URI_PREFIX);
                strcpy(newHref + DEFAULT_URI_PREFIX_LENGTH, href);
                int error = ixmlElement_setAttribute(element,
                                                     OBIX_ATTR_HREF,
                                                     newHref);
                if (error != IXML_SUCCESS)
                {
                    log_warning("Unable to update \"%s\" attribute of the object "
                                "before storing it: ixmlElement_setAttribute "
                                "returned %d.", OBIX_ATTR_HREF, error);
                }
            }
        }
    }

    // search also in child and neighbor tags
    insertDefaultUriPrefix(ixmlNode_getFirstChild(node));
    insertDefaultUriPrefix(ixmlNode_getNextSibling(node));
}

/**
 * Checks the node to comply with storage standards.
 * @todo Check that all objects in the input document have required attributes
 * and correct URI's.
 *
 * @param node Node to check
 * @param checkPrefix If @a TRUE than all nodes with absolute URIs will be also
 *                    checked to have /obix prefix.
 * @return @a href attribute of the parent node, or @a NULL if check fails.
 */
static const char* checkNode(IXML_Node* node, BOOL checkPrefix)
{
    // so far it only checks the href attribute from of the parent object :)
    IXML_Element* element = ixmlNode_convertToElement(node);
    if (element == NULL)
    {
        log_warning("Input document has wrong format.");
        return NULL;
    }

    const char* href = ixmlElement_getAttribute(element, OBIX_ATTR_HREF);

    if (href == NULL)
    {
        log_warning("Unable to write to the storage: "
                    "No \'href\' attribute found.");
        return NULL;
    }

    if (*href != '/')
    {
        log_warning("Unable to write to the storage: "
                    "\'href\' attribute should have absolute URI (without "
                    "server address).");
        return NULL;
    }

    if (checkPrefix)
    {
        insertDefaultUriPrefix(node);
    }

    // the href could be changed so we need to get it once more
    return ixmlElement_getAttribute(element, OBIX_ATTR_HREF);
}

/**
 * Adds provided data to the storage.
 * @param checkPrefix If @a TRUE than all nodes with absolute URIs will be
 *                    checked to have /obix prefix.
 * @return @a 0 on success; error code otherwise.
 */
static int xmldb_putDOMHelper(IXML_Element* data, BOOL checkPrefix)
{
    IXML_Node* node = ixmlElement_getNode(data);
    IXML_Node* newNode = NULL;

    // shortcut for cleaning all resources if error occurs.
    void onError()
    {
        if (newNode != NULL)
        {
            ixmlNode_free(newNode);
        }
    }

    const char* href = checkNode(node, checkPrefix);
    if (href == NULL)
    {
        // error is already logged.
        onError();
        return -1;
    }

    // append node to the storage
    int error = ixmlDocument_importNode(_storage, node, TRUE, &newNode);
    if (error != IXML_SUCCESS)
    {
        log_warning("Unable to write to the storage (error %d).", error);
        onError();
        return error;
    }

    // look for available node with the same href
    IXML_Node* nodeInStorage = getNodeByHref(_storage, href, NULL);
    if (nodeInStorage != NULL)
    {
        log_warning("Unable to write to the storage: The object with the same "
                    "URI (%s) already exists.", href);
        onError();
        return -2;
        // overwrite existing node
        //ixmlNode_replaceChild(ixmlNode_getParentNode(nodeInStorage),
        //                      newNode,
        //                      nodeInStorage,
        //                      &nodeInStorage);
        //ixmlNode_free(nodeInStorage);
    }

    // append as a new node
    error = ixmlNode_appendChild(ixmlDocument_getNode(_storage), newNode);
    if (error != IXML_SUCCESS)
    {
        log_warning("Unable to write to the storage (error %d).", error);
        onError();
        return error;
    }

    return 0;
}

/**
 * Saves provided data into storage.
 * The data is stored in the root of the document.
 */
static int xmldb_putHelper(const char* data, BOOL checkPrefix)
{
    IXML_Node* node = ixmlNode_parseBuffer(data);
    if (node == NULL)
    {
        return -1;
    }
    IXML_Element* element = ixmlNode_convertToElement(node);
    if (element == NULL)
    {
        log_error("Input data is not an XML element! "
                  "This should never happen.");
        return -1;
    }

    int error = xmldb_putDOMHelper(element, checkPrefix);
    ixmlDocument_free(ixmlNode_getOwnerDocument(node));

    return error;
}

IXML_Element* xmldb_getDOM(const char* href, int* slashFlag)
{
    return ixmlNode_convertToElement(getNodeByHref(_storage, href, slashFlag));
}

char* xmldb_get(const char* href, int* slashFlag)
{
    return ixmlPrintNode(getNodeByHref(_storage, href, slashFlag));
}

int xmldb_putDOM(IXML_Element* data)
{
    return xmldb_putDOMHelper(data, TRUE);
}

int xmldb_putDeviceReference(IXML_Element* deviceData)
{
    IXML_Element* devices =
        ixmlNode_convertToElement(
            getNodeByHref(_storage, DEVICE_LIST_URI, NULL));
    if (devices == NULL)
    {
        // database failure
        log_error("Unable to find device list in the storage.");
        return -1;
    }

    //TODO check that there are no links with such address yet

    // create new <ref/> object and copy 'href', 'name', 'display' and
    // 'displayName' attributes to it

    IXML_Element* ref =
        ixmlElement_createChildElementWithLog(devices, OBIX_OBJ_REF);
    if (ref == NULL)
    {
        log_error("Unable to add new reference to the device list.");
        return -1;
    }

    // copy attribute uri
    int error = ixmlElement_copyAttributeWithLog(deviceData, ref,
                OBIX_ATTR_HREF,
                TRUE);
    if (error != IXML_SUCCESS)
    {
        ixmlElement_free(ref);
        return -1;
    }

    // copy attribute name
    error = ixmlElement_copyAttributeWithLog(deviceData, ref,
            OBIX_ATTR_NAME,
            FALSE);
    if ((error != IXML_SUCCESS) && (error != IXML_NOT_FOUND_ERR))
    {
        ixmlElement_free(ref);
        return -1;
    }

    // copy optional attribute display
    error = ixmlElement_copyAttributeWithLog(deviceData, ref,
            OBIX_ATTR_DISPLAY,
            FALSE);
    if ((error != IXML_SUCCESS) && (error != IXML_NOT_FOUND_ERR))
    {
        ixmlElement_free(ref);
        return -1;
    }

    // copy optional attribute displayName
    error = ixmlElement_copyAttributeWithLog(deviceData, ref,
            OBIX_ATTR_DISPLAY_NAME,
            FALSE);
    if ((error != IXML_SUCCESS) && (error != IXML_NOT_FOUND_ERR))
    {
        ixmlElement_free(ref);
        return -1;
    }

    return 0;
}

int xmldb_init()
{
    if (_storage != NULL)
    {
        //TODO: replace with mega log
        log_error("Storage has been already initialized!");
        return -1;
    }

    int error = ixmlDocument_createDocumentEx(&_storage);
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to initialize the storage. (error %d).", error);
        return error;
    }

    // load storage contents from files:
    log_debug("Loading server storage data from files..");
    int i;
    for (i = 0; i < OBIX_STORAGE_FILES_COUNT; i++)
    {
        error = xmldb_loadFile(OBIX_STORAGE_FILES[i]);
        if (error != 0)
        {
            return error;
        }
    }

    log_debug("Storage is initialized!");
    return 0;
}

void xmldb_dispose()
{
    ixmlDocument_free(_storage);
    _storage = NULL;
}

int xmldb_put(const char* data)
{
    return xmldb_putHelper(data, TRUE);
}

int xmldb_updateDOM(IXML_Element* input,
                    const char* href,
                    IXML_Element** updatedNode,
                    int* slashFlag)
{
    const char* newValue = ixmlElement_getAttribute(input, OBIX_ATTR_VAL);
    if (newValue == NULL)
    {
        log_warning("Unable to update the storage: "
                    "Input data doesn't contain \'%s\' attribute.",
                    OBIX_ATTR_VAL);
        return -1;
    }

    // get the object from the storage
    IXML_Element* nodeInStorage = xmldb_getDOM(href, slashFlag);
    if (nodeInStorage == NULL)
    {
        log_warning("Unable to update the storage: "
                    "No object with the URI \"%s\" is found.", href);
        return -2;
    }

    // check that the object is writable
    const char* writable = ixmlElement_getAttribute(nodeInStorage,
                           OBIX_ATTR_WRITABLE);
    if ((writable == NULL) || (strcmp(writable, XML_TRUE) != 0))
    {
        log_warning("Unable to update the storage: "
                    "The object with the URI \"%s\" is not writable.", href);
        return -3;
    }

    // check the current value of the object in storage
    const char* oldValue = ixmlElement_getAttribute(nodeInStorage,
                           OBIX_ATTR_VAL);
    if ((oldValue != NULL) && (strcmp(oldValue, newValue) == 0))
    {
        // new value is the same as was in storage.
        // return address of the node in the storage
        if (updatedNode != NULL)
        {
            *updatedNode = nodeInStorage;
        }
        return 1;
    }

    // overwrite 'val' attribute
    int error = ixmlElement_setAttributeWithLog(
                    nodeInStorage,
                    OBIX_ATTR_VAL,
                    newValue);
    if (error != 0)
    {
        return -4;
    }

    // return address of updated node
    if (updatedNode != NULL)
    {
        *updatedNode = nodeInStorage;
    }
    return 0;
}

int xmldb_delete(const char* href)
{
    IXML_Node* node = getNodeByHref(_storage, href, NULL);
    if (node == NULL)
    {
        log_warning("Unable to delete data. Provided URI (%s) doesn't "
                    "exist.", href);
        return -1;
    }

    int error = ixmlNode_removeChild(ixmlNode_getParentNode(node), node, &node);
    if (error != IXML_SUCCESS)
    {
        log_warning("Error occurred when deleting data (error %d).", error);
        return -1;
    }

    ixmlNode_free(node);
    return 0;
}

int xmldb_loadFile(const char* filename)
{
    char* xmlFile = config_getResFullPath(filename);

    // open the file
    FILE* file = fopen(xmlFile, "rb");
    if (file == NULL)
    {
        log_error("Unable to access file \"%s\".", xmlFile);
        free(xmlFile);
        return -1;
    }

    // check the file size
    int error = fseek(file, 0, SEEK_END);
    if (error != 0)
    {
        log_error("Error reading file \'%s\' (%d).", xmlFile, error);
        free(xmlFile);
        return error;
    }
    int size = ftell(file);
    if (size <= 0)
    {
        log_error("Error reading file \"%s\".", xmlFile);
        free(xmlFile);
        return -1;
    }
    rewind(file);

    // read the file to the buffer
    char* data = (char*) malloc(size + 1);
    if (data == NULL)
    {
        log_error("Error reading file \"%s\". File is too big.", xmlFile);
        free(xmlFile);
        return -1;
    }
    int bytesRead = fread(data, 1, size, file);
    data[bytesRead] = '\0';

    // put data to the storage
    error = xmldb_putHelper(data, FALSE);
    free(data);
    if (error != 0)
    {
        log_error("Unable to update storage. File \"%s\" is corrupted "
                  "(error %d).", xmlFile, error);
        free(xmlFile);
        return error;
    }

    free(xmlFile);
    return 0;
}

void xmldb_printDump()
{
    printXMLContents(ixmlDocument_getNode(_storage), "Storage Dump");
}

char* xmldb_getDump()
{
    return ixmlPrintNode(ixmlDocument_getNode(_storage));
}

IXML_Element* xmldb_getObixSysObject(const char* objType)
{
    return ixmlElement_cloneWithLog(xmldb_getDOM(objType, NULL), TRUE);
}

IXML_Node* xmldb_putMetaVariable(IXML_Element* element,
                                 const char* name,
                                 const char* value)
{
    int error;
    IXML_Element* meta = xmldb_getMetaInfo(element);
    if (meta == NULL)
    {
        // create a new meta tag
        meta = ixmlElement_createChildElementWithLog(element, OBIX_META);
        if (meta == NULL)
        {
            log_error("Unable to create meta tag.");
            return NULL;
        }
    }

    // create new meta item
    IXML_Element* metaItem =
        ixmlElement_createChildElementWithLog(meta, name);
    if (metaItem == NULL)
    {
        log_error("Unable to create meta item.");
        return NULL;
    }

    // set meta item value
    error = ixmlElement_setAttribute(metaItem, OBIX_ATTR_VAL, value);
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to create meta item. "
                  "ixmlElement_setAttribute() returned %d.", error);
        return NULL;
    }

    // return link to the created attribute
    return ixmlAttr_getNode(
               ixmlElement_getAttributeNode(metaItem, OBIX_ATTR_VAL));
}

int xmldb_deleteMetaVariable(IXML_Node* meta)
{
    IXML_Node* metaItem = ixmlElement_getNode(
                              ixmlAttr_getOwnerElement(
                                  ixmlNode_convertToAttr(meta)));

    int error = ixmlNode_removeChild(
                    ixmlNode_getParentNode(metaItem), metaItem, &metaItem);
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to delete meta data: ixmlNode_removeChild() "
                  "returned %d", error);
        return -1;
    }

    ixmlNode_free(metaItem);
    return 0;
}

int xmldb_changeMetaVariable(IXML_Node* meta, const char* newValue)
{
    int error = ixmlNode_setNodeValue(meta, newValue);
    if (error != IXML_SUCCESS)
    {
        const char* metaName =
            ixmlElement_getTagName(ixmlAttr_getOwnerElement(
                                       ixmlNode_convertToAttr(meta)));
        log_error("Unable to change value of meta attribute \"%s\" to \"%s\":"
                  "ixmlNode_setNodeValue returned %d.",
                  metaName, newValue, error);
        return -1;
    }
    return 0;
}

IXML_Node* xmldb_getMetaVariable(IXML_Element* obj, const char* name)
{
    IXML_Element* metaInfo = xmldb_getMetaInfo(obj);
    if (metaInfo == NULL)
    {
        return NULL;
    }

    // iterate through all meta tags until we find corresponding name
    IXML_Node* metaNode = ixmlNode_getFirstChild(ixmlElement_getNode(metaInfo));

    for (; metaNode != NULL; metaNode = ixmlNode_getNextSibling(metaNode))
    {
        IXML_Element* metaElement = ixmlNode_convertToElement(metaNode);
        if (metaElement == NULL)
        {
            // this piece of meta data is not an element - ignore it
            continue;
        }

        if (strcmp(ixmlElement_getTagName(metaElement), name) == 0)
        {
            return ixmlAttr_getNode(
                       ixmlElement_getAttributeNode(metaElement, OBIX_ATTR_VAL));
        }
    }

    // no meta tag with such name found
    return NULL;
}

const char* xmldb_getMetaVariableValue(IXML_Element* obj, const char* name)
{
    IXML_Node* metaAttr = xmldb_getMetaVariable(obj, name);
    if (metaAttr == NULL)
    {
        return NULL;
    }

    return ixmlNode_getNodeValue(metaAttr);
}

IXML_Element* xmldb_getMetaInfo(IXML_Element* doc)
{
    IXML_NodeList* list = ixmlElement_getElementsByTagName(doc, OBIX_META);
    if (list == NULL)
    {
        return NULL;
    }

    int length = ixmlNodeList_length(list);
    int i;
    IXML_Node* result = NULL;

    for (i = 0; i < length; i++)
    {
        result = ixmlNodeList_item(list, i);
        // check that we have found meta of exactly this tag but not of
        // meta of it's child
        if (ixmlNode_getParentNode(result) == ixmlElement_getNode(doc))
        {
            ixmlNodeList_free(list);
            return ixmlNode_convertToElement(result);
        }
    }

    // We did not find anything
    ixmlNodeList_free(list);
    return NULL;
}

void xmldb_deleteMetaInfo(IXML_Element* doc)
{
    IXML_NodeList* list = ixmlElement_getElementsByTagName(doc, OBIX_META);
    if (list == NULL)
    {
        log_debug("oBIX object doesn't contain any meta information.");
        return;
    }

    int length = ixmlNodeList_length(list);

    IXML_Node* node;
    int error;
    int i;

    for (i = 0; i < length; i++)
    {
        node = ixmlNodeList_item(list, i);
        error = ixmlNode_removeChild(ixmlNode_getParentNode(node), node, &node);
        if (error != IXML_SUCCESS)
        {
            log_warning("Unable to clean the oBIX object from meta information "
                        "(error %d).", error);
            ixmlNodeList_free(list);
            return;
        }
        ixmlNode_free(node);
    }

    ixmlNodeList_free(list);
}

