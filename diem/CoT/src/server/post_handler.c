/**
 * @file
 * Contains handlers for various oBIX invoke commands.
 *
 * The following oBIX commands are implemented:
 * - watchService.make
 * - that's all
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#include <stdlib.h>
#include <string.h>
#include <lwl_ext.h>
#include <obix_utils.h>
#include "xml_storage.h"
#include "watch.h"
#include "server.h"
#include "post_handler.h"


#define WATCH_OUT_PREFIX "<obj is=\"obix:WatchOut\">\r\n  <list name=\"values\" of=\"obix:obj\">\r\n"
#define WATCH_OUT_POSTFIX "\r\n  </list>\r\n</obj>"

#define DEVICE_LIST_URI "/obix/devices/"

#define DEVICE_URI_PREFIX "/obix"
#define DEVICE_URI_PREFIX_LENGTH 5

//// TODO delete me
//typedef struct _post_handler
//{
//    char* uri;
//    obix_server_postHandler handler;
//    struct _post_handler* next;
//}
//post_handler;
//
//static post_handler* _postHandlerList = NULL;

// handler definitions
Response* handlerError(const char* uri, IXML_Document* input);
Response* handlerWatchServiceMake(const char* uri, IXML_Document* input);
Response* handlerWatchAdd(const char* uri, IXML_Document* input);
Response* handlerWatchRemove(const char* uri, IXML_Document* input);
Response* handlerWatchPollChanges(const char* uri, IXML_Document* input);
Response* handlerWatchPollRefresh(const char* uri, IXML_Document* input);
Response* handlerWatchDelete(const char* uri, IXML_Document* input);
Response* handlerSignUp(const char* uri, IXML_Document* input);
//Response* handlerBatch(const char* uri, IXML_Document* input);

static const obix_server_postHandler POST_HANDLER[] =
    {
        &handlerError,				//0 default handler which returns error
        &handlerWatchServiceMake,	//1 watchService.make
        &handlerWatchAdd,			//2 Watch.add
        &handlerWatchRemove,		//3 Watch.remove
        &handlerWatchPollChanges,	//4 Watch.pollChanges
        &handlerWatchPollRefresh,	//5 Watch.pollRefresh
        &handlerWatchDelete,		//6 Watch.delete
        &handlerSignUp				//7 signUp
        //		&handlerBatch				//8 Batch
    };

static const int POST_HANDLERS_COUNT = 8;

obix_server_postHandler obix_server_getPostHandler(int id)
{
    if ((id < 0) || (id >= POST_HANDLERS_COUNT))
    {
        return POST_HANDLER[0];
    }
    else
    {
        return POST_HANDLER[id];
    }
}

Response* handlerError(const char* uri, IXML_Document* input)
{
    log_debug("Requested operation \"%s\" exists but not implemented.", uri);
    return obix_server_getObixErrorMessage(uri, OBIX_HREF_ERR_UNSUPPORTED,
                                           "Unsupported Request",
                                           "The requested operation is not yet implemented.");
}

Response* handlerWatchServiceMake(const char* uri, IXML_Document* input)
{
    log_debug("Creating new watch object.");

    IXML_Element* watchDOM = NULL;
    int watchId = obixWatch_create(&watchDOM);

    if (watchId < 0)
    {
        // can't create Watch object - return error
        char* message;
        switch (watchId)
        {
        case -1:
            message = "Unable to allocate enough memory for a new Watch object.";
            break;
        case -2:
            message = "Maximum number of Watch objects is reached.";
            break;
        case -3:
            message = "Internal server error: Unable to save new Watch object.";
            break;
        case -4:
        default:
            message = "Internal server error.";
            break;
        }
        log_warning("Unable to create Watch object.");
        return obix_server_getObixErrorMessage(uri, NULL,
                                               "Watch Make Error", message);
    }

    const char* watchUri = ixmlElement_getAttribute(watchDOM, OBIX_ATTR_HREF);

    Response* response = obix_server_generateResponse(
                             watchDOM,
                             watchUri,
                             TRUE,
                             FALSE,
                             0,
                             TRUE,
                             TRUE);

    // free Watch DOM structure
    ixmlElement_freeOwnerDocument(watchDOM);

    return response;
}

/**
 * Returns array of unique URI's retrieved from the list of @a <uri/> objects.
 * Some elements of the array can be NULL which mean that they were filtered
 * out.
 *
 * @param nodeList List of @a <uri/> objects.
 * @param uriSet The address where array of URI's will be written to.
 * @return The size of the returned URI set, or negative error code.
 */
static int getUriSet(IXML_NodeList* nodeList, const char*** uriSet)
{
    int length = ixmlNodeList_length(nodeList);
    int i;
    const char** uri = (const char**) calloc(length, sizeof(char*));
    if (uri == NULL)
    {
        return -2;
    }

    for (i = 0; i < length; i++)
    {
        IXML_Element* element = ixmlNode_convertToElement(
                                    ixmlNodeList_item(nodeList, i));
        if (element == NULL)
        {
            // the node is not element (it can be, for instance, a comment
            // block) - ignore it
            uri[i] = NULL;
            continue;
        }

        const char* itemUri = ixmlElement_getAttribute(element, OBIX_ATTR_VAL);
        if (itemUri == NULL)
        {
            // tag in the URI list doesn't have value, ignore it
            uri[i] = NULL;
        }

        uri[i] = itemUri;
        // check that this URI is not already in the output set
        int j;
        for (j = 0; j < i; j++)
        {
            if ((uri[j] != NULL) && (strcmp(uri[j], itemUri) == 0))
            {
                // such URI is already in the set
                uri[i] = NULL;
            }
        }
    }

    // return the generated set
    *uriSet = uri;
    return length;
}

static int processWatchIn(IXML_Document* input, const char*** uriSet)
{
    // check input
    if (input == NULL)
    {
        return -1;
    }

    // get list of uri
    IXML_Element* listObj = ixmlDocument_getElementById(input, OBIX_OBJ_LIST);
    if (listObj == NULL)
    {
        return -1;
    }
    IXML_NodeList* uriList = ixmlElement_getElementsByTagName(
                                 listObj,
                                 OBIX_OBJ_URI);
    if (uriList == NULL)
    {
        return -1;
    }

    // generate a list of unique URI's from the list of oBIX uri objects
    // and return the length of this list.
    int length = getUriSet(uriList, uriSet);
    ixmlNodeList_free(uriList);

    return length;
}

Response* handlerWatchAdd(const char* uri, IXML_Document* input)
{
    Response* response = NULL;

    // shortcut for releasing resources on error
    Response* onError(char* message)
    {
        if (response != NULL)
        {
            obixResponse_free(response);
        }
        log_warning("Unable to process Watch.add operation. Returning error message.");
        return obix_server_getObixErrorMessage(uri, NULL, "Watch.add Error",
                                               message);
    }

    log_debug("Handling Watch.add \"%s\".", uri);

    // find the corresponding watch object
    oBIX_Watch* watch = obixWatch_getByUri(uri);
    if (watch == NULL)
    {
        return onError("Watch object does not exist.");
    }

    // reset lease timer
    obixWatch_resetLeaseTimer(watch, NULL);

    // prepare response header
    response = obixResponse_createFromString(WATCH_OUT_PREFIX);
    if (response == NULL)
    {
        return onError("Unable to create response object.");
    }

    // iterate through all URIs which should be added to the watch
    const char** uriSet = NULL;
    int i;
    int length = processWatchIn(input, &uriSet);
    if (length < 0)
    {
        switch(length)
        {
        case -1:
            return onError("Input data is corrupted. "
                           "An implementation of obix:WatchIn contract is expected.");
        case -2:
            return onError("Not enough memory.");
        default:
            return onError("Internal server error.");
        }
    }

    Response* rItem = response;

    for (i = 0; i < length; i++)
    {
        if (uriSet[i] == NULL)
        {	// ignore NULL values
            continue;
        }

        // add watch item
        oBIX_Watch_Item* watchItem;
        // TODO: in case of failure we will still have some watch items created
        // but user will not know about it. As a workaround we can add items
        // first to the fake Watch object and then import items to the real
        // Watch object on success.
        int error = obixWatch_createWatchItem(watch, uriSet[i], &watchItem);

        switch (error)
        {
        case 0:
            // add the current object state to the output
            rItem->next = obix_server_generateResponse(
                              watchItem->doc,
                              uriSet[i],
                              FALSE,
                              FALSE, 0,
                              FALSE,
                              FALSE);
            break;
        case -1:
            // object with provided URI is not found
            rItem->next = obix_server_getObixErrorMessage(uriSet[i],
                          OBIX_HREF_ERR_BAD_URI,
                          "Bad URI Error",
                          "Requested URI is not found on the server.");

            break;
        case -2:
            rItem->next = obix_server_getObixErrorMessage(uriSet[i],
                          OBIX_HREF_ERR_BAD_URI,
                          "Bad URI Error",
                          "Adding <op/> objects to Watch is forbidden.");
        case -3:
        default:
            rItem->next = obix_server_getObixErrorMessage(uriSet[i],
                          NULL,
                          "Watch.add Error",
                          "Internal server error.");
        }

        if (rItem->next != NULL)
        {
            rItem = rItem->next;
        }
    }
    free(uriSet);

    // finally add WatchOut postfix
    rItem->next = obixResponse_createFromString(WATCH_OUT_POSTFIX);
    if (rItem->next == NULL)
    {
        return onError("Unable to create response object.");
    }

    return response;
}

Response* handlerWatchRemove(const char* uri, IXML_Document* input)
{
    // shortcut for releasing resources on error
    Response* onError(char* message)
    {
        log_warning("Unable to process Watch.remove operation. "
                    "Returning error message.");
        return obix_server_getObixErrorMessage(uri, NULL, "Watch.remove Error",
                                               message);
    }

    log_debug("Handling Watch.remove \"%s\".", uri);

    // find the corresponding watch object
    oBIX_Watch* watch = obixWatch_getByUri(uri);
    if (watch == NULL)
    {
        return onError("Watch object does not exist.");
    }

    // reset lease timer
    obixWatch_resetLeaseTimer(watch, NULL);

    // get list of URI's to be removed
    const char** uriSet = NULL;
    int i;
    int length = processWatchIn(input, &uriSet);
    if (length < 0)
    {
        switch(length)
        {
        case -1:
            return onError("Input data is corrupted. "
                           "An implementation of obix:WatchIn contract is expected.");
        case -2:
            return onError("Not enough memory.");
        default:
            return onError("Internal server error.");
        }
    }
    // iterate through all URIs which should removed from the watch
    for (i = 0; i < length; i++)
    {
        if (uriSet[i] == NULL)
        {	// ignore NULL values
            continue;
        }
        // delete watch item ignoring errors
        obixWatch_deleteWatchItem(watch, uriSet[i]);
    }
    free(uriSet);

    // return empty answer
    return obixResponse_createFromString(OBIX_OBJ_NULL_TEMPLATE);
}

/**
 * Common function to handle both pollChanges and pollRefresh calls.
 */
static Response* handlerWatchPollHelper(const char* uri,
                                        IXML_Document* input,
                                        BOOL changedOnly)
{
    char* functionName = changedOnly ? "pollChanges" : "pollRefresh";
    Response* response = NULL;

    // shortcut for releasing resources on error
    Response* onError(char* message)
    {
        if (response != NULL)
        {
            obixResponse_free(response);
        }
        log_warning("Unable to process Watch.%s operation. "
                    "Returning error message.", functionName);
        return obix_server_getObixErrorMessage(uri, NULL,
                                               "Watch.pollChanges Error",
                                               message);
    }

    log_debug("Handling Watch.%s of watch \"%s\".", functionName, uri);

    // find the corresponding watch object
    oBIX_Watch* watch = obixWatch_getByUri(uri);
    if (watch == NULL)
    {
        return onError("Watch object doesn't exist.");
    }

    // reset lease timer
    obixWatch_resetLeaseTimer(watch, NULL);

    //prepare response header
    response = obixResponse_createFromString(WATCH_OUT_PREFIX);
    if (response == NULL)
    {
        return onError("Unable to create response object.");
    }

    oBIX_Watch_Item* watchItem = watch->items;
    Response* respPart = response;

    // iterate through every watched object
    while (watchItem != NULL)
    {
        if (!changedOnly ||
                (obixWatchItem_isUpdated(watchItem) == TRUE))
        {
            // TODO handle case when the object was deleted
            // in that case, the first pollChanges and all pollRefresh
            // should show <err/> object.
            obixWatchItem_setUpdated(watchItem, FALSE);
            respPart->next = obix_server_generateResponse(watchItem->doc,
                             watchItem->uri,
                             FALSE,
                             FALSE, 0,
                             TRUE,
                             FALSE);

            if (respPart->next != NULL)
            {
                respPart = respPart->next;
            }
        }
        // iterate to the next watch item
        watchItem = watchItem->next;
    }

    //complete response object
    respPart->next = obixResponse_createFromString(WATCH_OUT_POSTFIX);
    if (respPart->next == NULL)
    {
        return onError("Unable to create response object.");
    }

    return response;
}

Response* handlerWatchPollChanges(const char* uri, IXML_Document* input)
{
    return handlerWatchPollHelper(uri, input, TRUE);
}

Response* handlerWatchPollRefresh(const char* uri, IXML_Document* input)
{
    return handlerWatchPollHelper(uri, input, FALSE);
}

Response* handlerWatchDelete(const char* uri, IXML_Document* input)
{
    // shortcut for returning error message on failure
    Response* onError(char* message)
    {
        log_warning("Unable to process Watch.delete operation. "
                    "Returning error message.");
        return obix_server_getObixErrorMessage(uri, NULL,
                                               "Watch.delete Error",
                                               message);
    }

    log_debug("Handling Watch.delete of watch \"%s\".", uri);

    // find the corresponding watch object
    oBIX_Watch* watch = obixWatch_getByUri(uri);
    if (watch == NULL)
    {
        return onError("Watch object doesn't exist.");
    }

    int error = obixWatch_delete(watch);

    switch(error)
    {
    case 0:
        // everything was OK
        // return empty object
        return obixResponse_createFromString(OBIX_OBJ_NULL_TEMPLATE);
        break;
    case -1:
        return onError("Unable to delete watch from the storage.");
        break;
    case -2:
        return onError("Internal server error. "
                       "Unable to delete watch object.");
        break;
    case -3:
    default:
        return onError("Internal server error.");
    }
}

static int putDeviceReference(IXML_Element* newDevice)
{
    IXML_Node* devices = ixmlElement_getNode(xmldb_getDOM("/obix/devices/"));

    if (devices == NULL)
    {
        // database failure
        log_error("Unable to find device list in the storage.");
        return -1;
    }

    //TODO check that there are no links with such address yet

    // create new <ref/> object and copy 'href', 'name', 'display' and
    // 'displayName' attributes to it
    IXML_Document* parent = ixmlNode_getOwnerDocument(devices);

    IXML_Element* ref;
    int error = ixmlDocument_createElementEx(parent, OBIX_OBJ_REF, &ref);
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to add new reference to the device list: "
                  "ixmlDocument_createElementEx() returned %d", error);
        return -1;
    }

    // copy attribute uri
    error = ixmlElement_copyAttributeWithLog(newDevice, ref,
            OBIX_ATTR_HREF,
            TRUE);
    if (error != IXML_SUCCESS)
    {
        ixmlElement_free(ref);
        return -1;
    }

    // copy attribute name
    error = ixmlElement_copyAttributeWithLog(newDevice, ref,
            OBIX_ATTR_NAME,
            TRUE);
    if (error != IXML_SUCCESS)
    {
        ixmlElement_free(ref);
        return -1;
    }

    // copy optional attribute display
    error = ixmlElement_copyAttributeWithLog(newDevice, ref,
            OBIX_ATTR_DISPLAY,
            FALSE);
    if ((error != IXML_SUCCESS) && (error != IXML_NOT_FOUND_ERR))
    {
        ixmlElement_free(ref);
        return -1;
    }

    // copy optional attribute displayName
    error = ixmlElement_copyAttributeWithLog(newDevice, ref,
            OBIX_ATTR_DISPLAY_NAME,
            FALSE);
    if ((error != IXML_SUCCESS) && (error != IXML_NOT_FOUND_ERR))
    {
        ixmlElement_free(ref);
        return -1;
    }

    error = ixmlNode_appendChild(devices, ixmlElement_getNode(ref));
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to add new reference to the device list: "
                  "ixmlNode_appendChild() returned %d", error);
        ixmlElement_free(ref);
        return -1;
    }

    return 0;
}

static const char* checkHrefAttribute(IXML_Element* element)
{
    const char* href = ixmlElement_getAttribute(element, OBIX_ATTR_HREF);
    if (href == NULL)
    {
        // href is obligatory attribute
        return NULL;
    }

    if (xmldb_compareServerAddr(href) == 0)
    {
        href += xmldb_getServerAddressLength();
        char newHref[strlen(href) + 1];
        strcpy(newHref, href);

        int error = ixmlElement_setAttribute(element, OBIX_ATTR_HREF, newHref);
        if (error != IXML_SUCCESS)
        {
            log_error("Unable to generate URI for input object: "
                      "ixmlElement_setAttribute() returned %d.", error);
            return NULL;
        }
    }

    if (*href != '/')
    {
        // wrong address format
        return NULL;
    }

    if (strncmp(href, DEVICE_URI_PREFIX, DEVICE_URI_PREFIX_LENGTH) != 0)
    {
        // object address should always has such a prefix
        char newHref[strlen(href) + DEVICE_URI_PREFIX_LENGTH + 1];
        memcpy(newHref, DEVICE_URI_PREFIX, DEVICE_URI_PREFIX_LENGTH);
        strcpy(newHref + DEVICE_URI_PREFIX_LENGTH, href);
        int error = ixmlElement_setAttribute(element, OBIX_ATTR_HREF, newHref);
        if (error != IXML_SUCCESS)
        {
            log_error("Unable to generate URI for input object: "
                      "ixmlElement_setAttribute() returned %d.", error);
            return NULL;
        }
    }

    return ixmlElement_getAttribute(element, OBIX_ATTR_HREF);
}

Response* handlerSignUp(const char* uri, IXML_Document* input)
{
    if (input == NULL)
    {
        return obix_server_getObixErrorMessage(uri,
                                               NULL,
                                               "Sign Up Error",
                                               "Device data is corrupted.");
    }

    //extract obix:obj element which we are going to store
    IXML_Element* element = ixmlNode_convertToElement(
                                ixmlNode_getFirstChild(
                                    ixmlDocument_getNode(input)));
    if (element == NULL)
    {
        // input document had no element in the beginning
        return obix_server_getObixErrorMessage(uri,
                                               NULL,
                                               "Sign Up Error",
                                               "Input data has bad format.");
    }

    // do not allow storing objects with URI not starting with /obix/
    const char* href = checkHrefAttribute(element);
    if (href == NULL)
    {
        return obix_server_getObixErrorMessage(uri,
                                               NULL,
                                               "Sign Up Error",
                                               "Object must have a valid href attribute.");
    }

    char* deviceData = ixmlPrintNode(ixmlElement_getNode(element));
    int error = xmldb_put(deviceData);
    if (deviceData != 0)
    {
        free(deviceData);
    }
    if (error != 0)
    {
        // TODO return different description of different errors
        if (error == -2)
        {
            // it means that object with the same URI already exists in the
            // database. return specific error
            return obix_server_getObixErrorMessage(
                       href,
                       NULL,
                       "Sign Up Error",
                       "Unable to save device data: Object with the same URI "
                       "already exists.");
        }
        else
        {
            return obix_server_getObixErrorMessage(
                       uri,
                       NULL,
                       "Sign Up Error",
                       "Unable to save device data.");
        }
    }

    // add reference to the new device
    error = putDeviceReference(element);
    if (error != 0)
    {
        xmldb_delete(href);
        return obix_server_getObixErrorMessage(uri,
                                               NULL,
                                               "Sign Up Error",
                                               "Unable to add device to the device list.");
    }

    // return saved object
    Response* response = obix_server_generateResponse(
                             element,
                             NULL,
                             TRUE,
                             TRUE, 0,
                             FALSE,
                             TRUE);

    if (obixResponse_isError(response))
    {
        // we return error, thus we need to roll back all changes
        xmldb_delete(href);
        // TODO delete also record from device list
    }

    return response;
}