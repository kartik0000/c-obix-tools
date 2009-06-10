/** @file
 * Simplest implementation of XML storage.
 * All data is stored in DOM structure in memory.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <obix_utils.h>
#include <ixml_ext.h>
#include <xml_config.h>
#include <lwl_ext.h>
#include "xml_storage.h"

const char* OBIX_SYS_WATCH_STUB = "/sys/watch-stub/";
const char* OBIX_SYS_ERROR_STUB = "/sys/error-stub/";
const char* OBIX_SYS_WATCH_OUT_STUB = "/sys/watch-out-stub/";

const char* OBIX_META = "meta";

static const char* OBIX_LOBBY_FILE = "server_lobby.xml";
static const char* OBIX_ABOUT_FILE = "server_about.xml";
static const char* OBIX_WATCH_FILE = "server_watch.xml";
static const char* OBIX_SYS_OBJECTS_FILE = "server_sys_objects.xml";
static const char* OBIX_DEVICES_FILE = "server_devices.xml";

/**The place where all data is stored.*/
static IXML_Document* storage = NULL;

/**Address of the current server.*/
char* _serverAddress = NULL;
int _serverAddressLength;

static void printXMLContents(IXML_Node* node, const char* title)
{
    DOMString str = ixmlPrintNode(node);
    log_debug("\n%s:\n%s",title, str);
    ixmlFreeDOMString(str);
}

//static int loadServerAddress(IXML_Node* lobby)
//{
//    //TODO: get the server address from settings file and write it to the lobby object
//    const char* lobbyUri = ixmlElement_getAttribute(ixmlNode_convertToElement(lobby), OBIX_ATTR_HREF);
//    if (lobbyUri == NULL)
//    {
//        ixmlElement_setAttribute(NULL, NULL, NULL);
//        logError("Unable to initialize the storage. Unable to find URI in the Lobby object.");
//        return -1;
//    }
//
//    // check that uri is absolute
//    if (strncmp(lobbyUri, "http://", 7) != 0)
//    {
//        logError("Unable to initialize the storage. Lobby object contains wrong URI.");
//        return -1;
//    }
//
//    // extract server name
//    char* relativeUri = strchr(lobbyUri + 8, '/');
//    if (relativeUri == NULL)
//    {
//        logError("Unable to initialize the storage. Lobby object contains wrong URI.");
//        return -1;
//    }
//
//    _serverAddressLength = relativeUri - lobbyUri;
//    _serverAddress = (char*) malloc(_serverAddressLength + 1);
//    strncpy(_serverAddress, lobbyUri, _serverAddressLength);
//    _serverAddress[_serverAddressLength] = '\0';
//
//    return 0;
//}

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
    error = xmldb_put(data);
    free(data);
    if (error != 0)
    {
        log_error("Unable to update storage. File \"%s\" is corrupted (error %d).", xmlFile, error);
        free(xmlFile);
        return error;
    }

    free(xmlFile);
    return 0;
}

int xmldb_init(const char* serverAddr)
{
    if (storage != NULL)
    {
        //TODO: replace with mega log
        log_error("Storage has been already initialized!");
        return -1;
    }

    int error = ixmlDocument_createDocumentEx(&storage);
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to initialize the storage. (error %d).", error);
        return error;
    }

    // load server address
    if (serverAddr == NULL)
    {
        log_error("No server address provided. Storage initialization failed.");
        return -1;
    }
    else
    {
        _serverAddressLength = strlen(serverAddr);
        _serverAddress = (char*) malloc(_serverAddressLength + 1);
        strcpy(_serverAddress, serverAddr);
    }
    log_debug("Storage is initialized with server address: %s", _serverAddress);

    // load lobby object to the storage
    error = xmldb_loadFile(OBIX_LOBBY_FILE);
    if (error != 0)
    {
        return error;
    }

    // load system objects from a separate file
    error = xmldb_loadFile(OBIX_SYS_OBJECTS_FILE);
    if (error != 0)
    {
        return error;
    }

    // load about object from a separate file
    error = xmldb_loadFile(OBIX_ABOUT_FILE);
    if (error != 0)
    {
        return error;
    }
    // load WatchService object from a separate file
    error = xmldb_loadFile(OBIX_WATCH_FILE);
    if (error != 0)
    {
        return error;
    }
    // load devices object from a separate file
    error = xmldb_loadFile(OBIX_DEVICES_FILE);
    if (error != 0)
    {
        return error;
    }

    return 0;
}

void xmldb_dispose()
{
    ixmlDocument_free(storage);
    storage = NULL;
    if (_serverAddress != NULL)
    {
        free(_serverAddress);
    }
    _serverAddress = NULL;
}

static int getLastSlashPosition(const char* str, int startPosition)
{
    const char* temp = str + startPosition;
    while ((*temp != '/') && (temp != str))
    {
        temp--;
    }

    return temp - str;
}

static int _lastUriCompSlashMatch = 0;

int xmldb_getLastUriCompSlashFlag()
{
    return _lastUriCompSlashMatch;
}

//BOOL xmldb_compareUri(const char* uri1, const char* uri2)
//{
//    _lastUriCompSlashMatch = 0;
//    int pos1 = strlen(uri1) - 1;
//    int pos2 = strlen(uri2) - 1;
//
//    if (uri1[pos1] == '/')
//    {
//        pos1--;
//        _lastUriCompSlashMatch++;
//    }
//
//    if (uri2[pos2] == '/')
//    {
//        pos2--;
//        _lastUriCompSlashMatch--;
//    }
//
//    if (pos1 != pos2)
//        return FALSE;
//
//    // compare URI's in backwards direction, because
//    // most of them have the same beginning
//    for (; pos1 >=0; pos1--)
//    {
//        if (uri1[pos1] != uri2[pos1])
//            return FALSE;
//    }
//
//    return TRUE;
//}

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
static int compare_uri(const char* currentUri, const char* requiredUri, int checked)
{
    //    log_debug("comparing: \"%s\" and \"%s\"", currentUri, requiredUri + checked);
    if (xmldb_compareServerAddr(currentUri) == 0)
    {
        // currentUri is absolute, compare from the beginning
        // ignore checked
        currentUri += _serverAddressLength;
        checked = 0;
    }
    else
    {
        // currentUri is relative.
        if (*currentUri == '/')
        {
            // currentUri contains path from server root
            // ignore checked
            checked = 0;
        }
    }

    //clear last comparison flag
    _lastUriCompSlashMatch = 0;

    int currLength = strlen(currentUri);
    BOOL currUriHasRemaining;
    if (currentUri[currLength - 1] == '/')
    {
        //        log_debug("Uri \"%s\" ends with slash", currentUri);
        // ignore ending '/' during comparison
        currUriHasRemaining = FALSE;
        currLength--;
        // see xmldb_getLastUriCompSlashFlag for explanation
        _lastUriCompSlashMatch++;
    }
    else
    {
        //        log_debug("Uri \"%s\" doesn't end with slash", currentUri);

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
        //        log_debug("Uri %s and %s doens't match", currentUri, requiredUri + checked);
        // uri's do not match
        return -1;
    }
    //    log_debug("Uri %s and %s match at least partially (%d)", currentUri, requiredUri + checked, currLength);
    // currentUri matches with some piece of requiredUri

    // calculate size of requiredUri remaining
    int requiredLength = strlen(requiredUri + checked);
    // ignore last /
    if (requiredUri[checked + requiredLength - 1] == '/')
    {
        requiredLength--;
        // see xmldb_getLastUriCompSlashFlag for explanation
        _lastUriCompSlashMatch--;
    }
    //    log_debug("checked %d, required %d, requiredUri ends with \'%c\'", currLength, requiredLength, requiredUri[checked + requiredLength]);
    if ((currLength == requiredLength) && !currUriHasRemaining)
    {
        //        log_debug("full match");
        // currentUri matches with the end of requiredUri
        return 0;
    }

    if (currUriHasRemaining)
    {
        // try to check remainder
        int remainderLength = strlen(currentUri) - currLength;
        if (((currLength + remainderLength) == requiredLength) &&
                (strncmp(currentUri + currLength, requiredUri + checked + currLength, strlen(currentUri) - currLength) == 0))
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
        int checked)
{
    if (node == NULL)
    {
        //recursion exit point
        return NULL;
    }

    IXML_Node* match = NULL;
    IXML_Element* element = ixmlNode_convertToElement(node);
    const char* currentUri;
    int compareResult = 0;
    // ignore all nodes which are not tags or are reference tags
    // or do not have 'href' attribute
    if ((element != NULL)
            && (strcmp(ixmlElement_getTagName(element), OBIX_OBJ_REF) != 0)
            && ((currentUri = ixmlElement_getAttribute(element, OBIX_ATTR_HREF)) != NULL))
    {

        compareResult = compare_uri(currentUri, href, checked);
        if (compareResult == 0)
        {
            // we found the required node
            return node;
        }
    }

    // check children
    // note that compareResult == 0 means that URI's
    // were not compared at all
    if (compareResult == 0)
    {
        match = getNodeByHrefRecursive(ixmlNode_getFirstChild(node), href, checked);
    }
    else if (compareResult > 0)
    {
        // we found part of the uri on this step
        // continue search in children the remaining address
        match = getNodeByHrefRecursive(ixmlNode_getFirstChild(node), href, compareResult);
    }
    // do not check children if compareResult < 0

    if (match == NULL)
    {
        match = getNodeByHrefRecursive(ixmlNode_getNextSibling(node), href, checked);
    }

    return match;
}

static IXML_Node* getNodeByHref(IXML_Document* doc, const char* href)
{
    // TODO think whether leave it here and remove checks in other places
    // or remove it from here and make a special method which will be
    // used by obix_fcgi_handleRequest and by Watch.add (remove)
    if (xmldb_compareServerAddr(href) == 0)
    {
        href += _serverAddressLength;
    }
    return getNodeByHrefRecursive(ixmlDocument_getNode(doc), href, 0);
}

IXML_Element* xmldb_getDOM(const char* href)
{
    return ixmlNode_convertToElement(getNodeByHref(storage, href));
}

char* xmldb_get(const char* href)
{
    return ixmlPrintNode(getNodeByHref(storage, href));
}

/**
 * Parses input data.
 * @note Don't forget to free memory allocated for the parsed document.
 *
 * @param data input data in plain text format.
 * @return a parsed DOM structure, or @a NULL if parsing fails.
 */
static IXML_Node* processInputData(const char* data)
{
    IXML_Document* doc;

    if (data == NULL)
    {
        return NULL;
    }

    int error = ixmlParseBufferEx(data, &doc);
    if (error != IXML_SUCCESS)
    {
        log_warning("Unable to write to the storage. Data is corrupted (error %d).\n"
                    "Data:\n%s\n", error, data);
        return NULL;
    }

    IXML_Node* node = ixmlNode_getFirstChild(ixmlDocument_getNode(doc));
    printXMLContents(node, "Node to add");
    return node;
}

/**
 * Checks the node to comply with storage standards.
 * @todo Check that all objects in the input document have required attributes
 * and correct URI's.
 *
 * @param node Node to check
 * @return @a href attribute of the parent node, or @a NULL if check fails.
 */
static const char* checkNode(IXML_Node* node)
{
    IXML_Element* element = ixmlNode_convertToElement(node);
    if (element == NULL)
    {
        log_warning("Input document has wrong format.");
        return NULL;
    }

    const char* href = ixmlElement_getAttribute(element, OBIX_ATTR_HREF);

    if (href == NULL)
    {
        log_warning("Unable to write to the storage. No \'href\' attribute found.");
        return NULL;
    }

    if (*href != '/')
    {
        log_warning("Unable to write to the storage. \'href\' attribute should have absolute URI (without server address).");
        return NULL;
    }

    return href;
}

/**
 * Adds XML node to the storage.
 * The data is stored in the root of the document.
 */
int xmldb_put(const char* data)
{
    IXML_Node* node = NULL;
    IXML_Node* newNode = NULL;

    // shortcut for cleaning all resources if error occurres.
    void onError()
    {
        if (node != NULL)
        {
            ixmlDocument_free(ixmlNode_getOwnerDocument(node));
        }
        if (newNode != NULL)
        {
            ixmlNode_free(newNode);
        }
    }

    node = processInputData(data);
    if (node == NULL)
    {
        return -1;
    }

    const char* href = checkNode(node);
    if (href == NULL)
    {
        // error is already logged.
        onError();
        return -1;
    }

    // append node to the storage
    int error = ixmlDocument_importNode(storage, node, TRUE, &newNode);
    if (error != IXML_SUCCESS)
    {
        log_warning("Unable to write to the storage (error %d).", error);
        onError();
        return error;
    }

    // look for available node with the same href
    IXML_Node* nodeInStorage = getNodeByHref(storage, href);
    if (nodeInStorage != NULL)
    {
        log_warning("Unable to write to the storage: The object with the same URI (%s) already exists.", href);
        onError();
        return -2;
        //         overwrite existing node
        //        ixmlNode_replaceChild(ixmlNode_getParentNode(nodeInStorage), newNode, nodeInStorage, &nodeInStorage);
        //        ixmlNode_free(nodeInStorage);
    }

    // append as a new node
    error = ixmlNode_appendChild(ixmlDocument_getNode(storage), newNode);
    if (error != IXML_SUCCESS)
    {
        log_warning("Unable to write to the storage (error %d).", error);
        onError();
        return error;
    }

    // add server address to the node's href attribute
    char* fullHref = xmldb_getFullUri(href, 0);
    if (fullHref == NULL)
    {
        log_error("Unable to write to the storage. Unable to allocate enough memory.");
        ixmlNode_removeChild(ixmlDocument_getNode(storage), newNode, &newNode);
        onError();
        return -1;
    }

    error = ixmlElement_setAttributeWithLog(ixmlNode_convertToElement(newNode),
                                            OBIX_ATTR_HREF,
                                            fullHref);
    free(fullHref);
    if (error != 0)
    {
        log_error("Unable to write to the storage.");
        ixmlNode_removeChild(ixmlDocument_getNode(storage), newNode, &newNode);
        onError();
        return -1;
    }

    // delete old node
    ixmlDocument_free(ixmlNode_getOwnerDocument(node));

    return 0;
}

int xmldb_update(const char* data, const char* href, IXML_Element** updatedNode)
{
    IXML_Node* node = processInputData(data);
    if (node == NULL)
    {
        return -1;
    }

    const char* newValue = ixmlElement_getAttribute(
                               ixmlNode_convertToElement(node), OBIX_ATTR_VAL);
    if (newValue == NULL)
    {
        log_warning("Unable to update the storage: "
                    "Input data doesn't contain \'%s\' attribute.",
                    OBIX_ATTR_VAL);
        ixmlDocument_free(ixmlNode_getOwnerDocument(node));
        return -1;
    }

    // get the object from the storage
    IXML_Element* nodeInStorage = xmldb_getDOM(href);
    if (nodeInStorage == NULL)
    {
        log_warning("Unable to update the storage: "
                    "No object with the URI \"%s\" is found.", href);
        ixmlDocument_free(ixmlNode_getOwnerDocument(node));
        return -2;
    }

    // check that the object is writable
    const char* writable = ixmlElement_getAttribute(nodeInStorage,
                           OBIX_ATTR_WRITABLE);
    if ((writable == NULL) || (strcmp(writable, XML_TRUE) != 0))
    {
        log_warning("Unable to update the storage: "
                    "The object with the URI \"%s\" is not writable.", href);
        ixmlDocument_free(ixmlNode_getOwnerDocument(node));
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
        ixmlDocument_free(ixmlNode_getOwnerDocument(node));
        return 1;
    }

    // overwrite 'val' attribute
    if (ixmlElement_setAttributeWithLog(nodeInStorage, OBIX_ATTR_VAL, newValue) != 0)
    {
        ixmlDocument_free(ixmlNode_getOwnerDocument(node));
        return -4;
    }

    ixmlDocument_free(ixmlNode_getOwnerDocument(node));
    // return address of updated node
    if (updatedNode != NULL)
    {
        *updatedNode = nodeInStorage;
    }
    return 0;
}

int xmldb_delete(const char* href)
{
    IXML_Node* node = getNodeByHref(storage, href);
    if (node == NULL)
    {
        log_warning("Unable to delete data. Provided URI (%s) doesn't exist.", href);
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

void xmldb_printDump()
{
    printXMLContents(ixmlDocument_getNode(storage), "Storage Dump");
}

char* xmldb_getDump()
{
    return ixmlPrintNode(ixmlDocument_getNode(storage));
}

IXML_Element* xmldb_getObixSysObject(const char* objType)
{
    return ixmlElement_cloneWithLog(xmldb_getDOM(objType));
}

/**
 * 1 - add slash
 * -1 - remove slash
 * 0 - do nothing
 * @param absUri
 * @param slashFlag
 * @return
 */
char* xmldb_getFullUri(const char* absUri, int slashFlag)
{
    char* fullUri;

    if ((slashFlag > 1) || (slashFlag < -1))
    {
        log_warning("Wrong usage of xmldb_getFullUri(): "
                    "slashFlag can't be %d, changed to 0.", slashFlag);
        slashFlag = 0;
    }

    // calculate the size of returning URI.
    int size = _serverAddressLength + strlen(absUri);

    // allocate memory, considering trailing slash and NULL character '\0'
    fullUri = (char*) malloc(size + slashFlag + 1);
    if (fullUri == NULL)
    {
        log_error("Unable to generate full URI: not enough memory.");
        return NULL;
    }

    // copy strings
    memcpy(fullUri, _serverAddress, _serverAddressLength);
    memcpy(fullUri + _serverAddressLength, absUri, size - _serverAddressLength);

    if (slashFlag == 1)
    {
        // add trailing slash
        fullUri[size] = '/';
    }

    // finalize string
    fullUri[size + slashFlag] = '\0';

    return fullUri;
}

int xmldb_compareServerAddr(const char* uri)
{
    return strncmp(uri, _serverAddress, _serverAddressLength);
}


const char* xmldb_getServerAddress()
{
    return _serverAddress;
}

int xmldb_getServerAddressLength()
{
    return _serverAddressLength;
}

IXML_Node* xmldb_putMeta(IXML_Element* element, const char* name, const char* value)
{
    int error;
    IXML_Element* meta = getMetaInfo(element);
    if (meta == NULL)
    {
        // create a new meta tag
        error = ixmlDocument_createElementEx(storage, OBIX_META, &meta);
        if (error != IXML_SUCCESS)
        {
            log_error("Unable to create meta tag. "
                      "ixmlDocument_createElementEx() returned %d.", error);
            return NULL;
        }
        error = ixmlNode_appendChild(ixmlElement_getNode(element),
                                     ixmlElement_getNode(meta));
        if (error != IXML_SUCCESS)
        {
            log_error("Unable to create meta tag. "
                      "ixmlNode_appendChild() returned %d.", error);
            return NULL;
        }
    }

    // create new meta item
    IXML_Element* metaItem;
    error = ixmlDocument_createElementEx(storage, name, &metaItem);
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to create meta item. "
                  "ixmlDocument_createElementEx() returned %d.", error);
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
    // insert new meta item to the meta tag
    error = ixmlNode_appendChild(ixmlElement_getNode(meta),
                                 ixmlElement_getNode(metaItem));
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to create meta item. "
                  "ixmlNode_appendChild() returned %d.", error);
        return NULL;
    }

    // return link to the created attribute
    return ixmlAttr_getNode(ixmlElement_getAttributeNode(metaItem, OBIX_ATTR_VAL));
}

int xmldb_deleteMeta(IXML_Node* meta)
{
    IXML_Node* metaItem = ixmlElement_getNode(
                              ixmlAttr_getOwnerElement(
                                  ixmlNode_convertToAttr(meta)));

    int error = ixmlNode_removeChild(ixmlNode_getParentNode(metaItem), metaItem, &metaItem);
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to delete meta data: ixmlNode_removeChild() returned %d", error);
        return -1;
    }

    ixmlNode_free(metaItem);
    return 0;
}

int xmldb_updateMeta(IXML_Node* meta, const char* newValue)
{
    return ixmlNode_setNodeValue(meta, newValue);
}

IXML_Element* getMetaInfo(IXML_Element* doc)
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


/**
 * Removes all #OBIX_META tags from the document.
 * @a For <op/> objects these tags contain id of the operation handler. For any
 * other objects they can contain ids of watches subscribed for those object
 * changes.
 * @param doc Node which should be cleaned.
 */
void removeMetaInfo(IXML_Element* doc)
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
            log_warning("Unable to clean the oBIX object from meta information (error %d).", error);
            ixmlNodeList_free(list);
            return;
        }
        ixmlNode_free(node);
    }

    ixmlNodeList_free(list);
}

