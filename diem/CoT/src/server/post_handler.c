/**
 * @file
 * Contains handlers for various oBIX invoke commands.
 *
 * The following oBIX commands are implemented:
 * - watchService.make	Creates new Watch object.
 * - Watch.add 			Adds object to the watch list.
 * - Watch.remove		Removes object from the watch list.
 * - Watch.pollChanges	Returns objects which are changed since last poll.
 * - Watch.pollRefresh	Returns all objects from the watch list.
 * - Watch.delete		Deletes the Watch object.
 * - signUp				Adds new device data to the server.
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

// handler definitions
void handlerError(Response* response,
                  const char* uri,
                  IXML_Document* input);
void handlerWatchServiceMake(Response* response,
                             const char* uri,
                             IXML_Document* input);
void handlerWatchAdd(Response* response,
                     const char* uri,
                     IXML_Document* input);
void handlerWatchRemove(Response* response,
                        const char* uri,
                        IXML_Document* input);
void handlerWatchPollChanges(Response* response,
                             const char* uri,
                             IXML_Document* input);
void handlerWatchPollRefresh(Response* response,
                             const char* uri,
                             IXML_Document* input);
void handlerWatchDelete(Response* response,
                        const char* uri,
                        IXML_Document* input);
void handlerSignUp(Response* response,
                   const char* uri,
                   IXML_Document* input);
//void handlerBatch(Response* response, const char* uri, IXML_Document* input);

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
        //&handlerBatch				//8 Batch
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

void handlerError(Response* response, const char* uri, IXML_Document* input)
{
    log_debug("Requested operation \"%s\" exists but not implemented.", uri);
    obix_server_generateObixErrorMessage(response, uri, OBIX_HREF_ERR_UNSUPPORTED,
                                         "Unsupported Request",
                                         "The requested operation is not yet implemented.");
    (_responseListener)(response);
}

/**
 * Helper method which is used to send error message when some handler fails.
 * @param response		Response object.
 * @param operationName Name of the failed operation.
 * @param message 		Message which should be sent.
 */
static void sendErrorMessage(Response* response,
                             const char* uri,
                             const char* operationName,
                             const char* message)
{
    log_warning("Unable to process \"%s\" operation. "
                "Returning error message.", operationName);

    char errorName[strlen(operationName) + 7];
    strcpy(errorName, operationName);
    strcat(errorName, " Error");
    // generate error
    obix_server_generateObixErrorMessage(response, uri, NULL, errorName,
                                         message);

    // clean all other parts of the response if anything was generated before
    if (response->next != NULL)
    {
        obixResponse_free(response->next);
        response->next = NULL;
    }
    // send the error message
    (*_responseListener)(response);
}

void handlerWatchServiceMake(Response* response,
                             const char* uri,
                             IXML_Document* input)
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
        sendErrorMessage(response, uri, "Watch Make", message);
        return;
    }

    const char* watchUri = ixmlElement_getAttribute(watchDOM, OBIX_ATTR_HREF);

    obix_server_generateResponse(response,
                                 watchDOM,
                                 watchUri,
                                 TRUE,
                                 FALSE,
                                 0,
                                 TRUE,
                                 TRUE);
    // free Watch DOM structure
    ixmlElement_freeOwnerDocument(watchDOM);

    // send answer
    (*_responseListener)(response);
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

void handlerWatchAdd(Response* response, const char* uri, IXML_Document* input)
{
    log_debug("Handling Watch.add \"%s\".", uri);

    // find the corresponding watch object
    oBIX_Watch* watch = obixWatch_getByUri(uri);
    if (watch == NULL)
    {
        sendErrorMessage(response,
                         uri,
                         "Watch.add",
                         "Watch object does not exist.");
        return;
    }

    // reset lease timer
    obixWatch_resetLeaseTimer(watch, OBIX_WATCH_LEASE_NO_CHANGE);

    // prepare response header
    obixResponse_setText(response, WATCH_OUT_PREFIX, TRUE);

    // iterate through all URIs which should be added to the watch
    const char** uriSet = NULL;
    int i;
    int length = processWatchIn(input, &uriSet);
    if (length < 0)
    {
        switch(length)
        {
        case -1:
            sendErrorMessage(
                response,
                uri,
                "Watch.add",
                "Input data is corrupted. "
                "An implementation of obix:WatchIn contract is expected.");
        case -2:
            sendErrorMessage(response, uri, "Watch.add", "Not enough memory.");
        default:
            sendErrorMessage(response,
                             uri,
                             "Watch.add",
                             "Internal server error.");
        }
        // stop operation processing because of error
        return;
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

        // Create new response part
        rItem = obixResponse_add(rItem);
        if (rItem == NULL)
        {
            log_error("Unable to create multipart response object.");
            sendErrorMessage(response,
                             uri,
                             "Watch.add",
                             "Internal server error.");
            return;
        }

        switch (error)
        {
        case 0:
            // add the current object state to the output
            obix_server_generateResponse(rItem,
                                         watchItem->doc,
                                         uriSet[i],
                                         FALSE,
                                         FALSE, 0,
                                         FALSE,
                                         FALSE);
            break;
        case -1:
            // object with provided URI is not found
            obix_server_generateObixErrorMessage(
                rItem,
                uriSet[i],
                OBIX_HREF_ERR_BAD_URI,
                "Bad URI Error",
                "Requested URI is not found on the server.");

            break;
        case -2:
            obix_server_generateObixErrorMessage(
                rItem,
                uriSet[i],
                OBIX_HREF_ERR_BAD_URI,
                "Bad URI Error",
                "Adding <op/> objects to Watch is forbidden.");
        case -3:
        default:
            obix_server_generateObixErrorMessage(rItem,
                                                 uriSet[i],
                                                 NULL,
                                                 "Watch.add Error",
                                                 "Internal server error.");
        }
    }
    free(uriSet);

    // finally add WatchOut postfix
    rItem = obixResponse_add(rItem);
    obixResponse_setText(rItem, WATCH_OUT_POSTFIX, TRUE);
    if (rItem == NULL)
    {
        log_error("Unable to create multipart response object.");
        sendErrorMessage(response, uri, "Watch.add", "Internal server error.");
        return;
    }

    (*_responseListener)(response);
}

void handlerWatchRemove(Response* response,
                        const char* uri,
                        IXML_Document* input)
{
    log_debug("Handling Watch.remove \"%s\".", uri);

    // find the corresponding watch object
    oBIX_Watch* watch = obixWatch_getByUri(uri);
    if (watch == NULL)
    {
        sendErrorMessage(response,
                         uri,
                         "Watch.remove",
                         "Watch object does not exist.");
        return;
    }

    // reset lease timer
    obixWatch_resetLeaseTimer(watch, OBIX_WATCH_LEASE_NO_CHANGE);

    // get list of URI's to be removed
    const char** uriSet = NULL;
    int i;
    int length = processWatchIn(input, &uriSet);
    if (length < 0)
    {
        switch(length)
        {
        case -1:
            sendErrorMessage(
                response,
                uri,
                "Watch.remove",
                "Input data is corrupted. "
                "An implementation of obix:WatchIn contract is expected.");
        case -2:
            sendErrorMessage(response,
                             uri,
                             "Watch.remove",
                             "Not enough memory.");
        default:
            sendErrorMessage(response,
                             uri,
                             "Watch.remove",
                             "Internal server error.");
        }
        // stop operation processing because of error
        return;
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
    obixResponse_setText(response, OBIX_OBJ_NULL_TEMPLATE, TRUE);
    (*_responseListener)(response);
}

/**
 * Common function to handle both pollChanges and pollRefresh calls.
 */
static void handlerWatchPollHelper(Response* response,
                                   const char* uri,
                                   IXML_Document* input,
                                   BOOL changedOnly)
{
    char* functionName = changedOnly ?
                         "Watch.pollChanges" : "Watch.pollRefresh";
    // this produces to much of log
    //    log_debug("Handling %s of watch \"%s\".", functionName, uri);

    // find the corresponding watch object
    oBIX_Watch* watch = obixWatch_getByUri(uri);
    if (watch == NULL)
    {
        sendErrorMessage(response,
                         uri,
                         functionName,
                         "Watch object doesn't exist.");
        return;
    }

    // reset lease timer
    obixWatch_resetLeaseTimer(watch, OBIX_WATCH_LEASE_NO_CHANGE);

    //prepare response header
    obixResponse_setText(response, WATCH_OUT_PREFIX, TRUE);

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

            // create new response part
            respPart = obixResponse_add(respPart);
            if (respPart == NULL)
            {
                log_error("Unable to create multipart response object.");
                sendErrorMessage(response,
                                 uri,
                                 functionName,
                                 "Internal server error");
                return;
            }

            obix_server_generateResponse(respPart,
                                         watchItem->doc,
                                         watchItem->uri,
                                         FALSE,
                                         FALSE, 0,
                                         TRUE,
                                         FALSE);

        }
        // iterate to the next watch item
        watchItem = watchItem->next;
    }

    //complete response
    respPart = obixResponse_add(respPart);
    if (respPart == NULL)
    {
        log_error("Unable to create multipart response object.");
        sendErrorMessage(response, uri, functionName, "Internal server error");
        return;
    }
    obixResponse_setText(respPart, WATCH_OUT_POSTFIX, TRUE);

    // send response
    (*_responseListener)(response);
}

void handlerWatchPollChanges(Response* response,
                             const char* uri,
                             IXML_Document* input)
{

    handlerWatchPollHelper(response, uri, input, TRUE);
}

void handlerWatchPollRefresh(Response* response,
                             const char* uri,
                             IXML_Document* input)
{
    handlerWatchPollHelper(response, uri, input, FALSE);
}

void handlerWatchDelete(Response* response,
                        const char* uri,
                        IXML_Document* input)
{
    log_debug("Handling Watch.delete of watch \"%s\".", uri);

    // find the corresponding watch object
    oBIX_Watch* watch = obixWatch_getByUri(uri);
    if (watch == NULL)
    {
        sendErrorMessage(response,
                         uri,
                         "Watch.delete",
                         "Watch object doesn't exist.");
        return;
    }

    int error = obixWatch_delete(watch);

    switch(error)
    {
    case 0:
        {	// everything was OK
            // return empty object
            obixResponse_setText(response, OBIX_OBJ_NULL_TEMPLATE, TRUE);
            (*_responseListener)(response);
        }
        break;
    case -1:
        sendErrorMessage(response,
                         uri,
                         "Watch.delete",
                         "Unable to delete watch from the storage.");
        break;
    case -2:
        sendErrorMessage(response,
                         uri,
                         "Watch.delete",
                         "Internal server error. "
                         "Unable to delete watch object.");
        break;
    case -3:
    default:
        sendErrorMessage(response, uri, "Watch.delete", "Internal server error.");
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

void handlerSignUp(Response* response, const char* uri, IXML_Document* input)
{
    if (input == NULL)
    {
        sendErrorMessage(response, uri, "Sign Up", "Device data is corrupted.");
        return;
    }

    //extract obix:obj element which we are going to store
    IXML_Element* element = ixmlNode_convertToElement(
                                ixmlNode_getFirstChild(
                                    ixmlDocument_getNode(input)));
    if (element == NULL)
    {
        // input document had no element in the beginning
        sendErrorMessage(response,
                         uri,
                         "Sign Up",
                         "Input data has bad format.");
        return;
    }

    // do not allow storing objects with URI not starting with /obix/
    const char* href = checkHrefAttribute(element);
    if (href == NULL)
    {
        sendErrorMessage(response,
                         uri,
                         "Sign Up Error",
                         "Object must have a valid href attribute.");
        return;
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
            sendErrorMessage(response,
                             href,
                             "Sign Up",
                             "Unable to save device data: "
                             "Object with the same URI already exists.");
            return;
        }
        else
        {
            sendErrorMessage(response,
                             uri,
                             "Sign Up",
                             "Unable to save device data.");
            return;
        }
    }

    // add reference to the new device
    error = putDeviceReference(element);
    if (error != 0)
    {
        xmldb_delete(href);
        sendErrorMessage(response,
                         uri,
                         "Sign Up",
                         "Unable to add device to the device list.");
        return;
    }

    log_debug("New object is successfully registered at \"%s\"", href);
    // return saved object
    obix_server_generateResponse(response,
                                 element,
                                 NULL,
                                 TRUE,
                                 TRUE, 0,
                                 FALSE,
                                 TRUE);
    // TODO what about removing this isError at all?
    if (obixResponse_isError(response))
    {
        // we return error, thus we need to roll back all changes
        xmldb_delete(href);
        // TODO delete also record from device list
    }

    // send response
    (*_responseListener)(response);
}
