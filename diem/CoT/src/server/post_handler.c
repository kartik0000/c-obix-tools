/* *****************************************************************************
 * Copyright (c) 2009 Andrey Litvinov
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
#include <log_utils.h>
#include <obix_utils.h>
#include "xml_storage.h"
#include "watch.h"
#include "server.h"
#include "post_handler.h"


#define WATCH_OUT_PREFIX "<obj is=\"obix:WatchOut\">\r\n  <list name=\"values\" of=\"obix:obj\">\r\n"
#define WATCH_OUT_POSTFIX "\r\n  </list>\r\n</obj>\r\n"

#define BATCH_OUT_PREFIX "<list is=\"obix:BatchOut\" of=\"obix:obj\">\r\n"
#define BATCH_OUT_POSTFIX "\r\n</list>\r\n"

#define DEVICE_LIST_URI "/obix/devices/"

// handler definitions
void handlerError(Response* response,
                  const char* uri,
                  IXML_Element* input);
void handlerWatchServiceMake(Response* response,
                             const char* uri,
                             IXML_Element* input);
void handlerWatchAdd(Response* response,
                     const char* uri,
                     IXML_Element* input);
void handlerWatchRemove(Response* response,
                        const char* uri,
                        IXML_Element* input);
void handlerWatchPollChanges(Response* response,
                             const char* uri,
                             IXML_Element* input);
void handlerWatchPollRefresh(Response* response,
                             const char* uri,
                             IXML_Element* input);
void handlerWatchDelete(Response* response,
                        const char* uri,
                        IXML_Element* input);
void handlerSignUp(Response* response,
                   const char* uri,
                   IXML_Element* input);
void handlerBatch(Response* response,
                  const char* uri,
                  IXML_Element* input);

static const obix_server_postHandler POST_HANDLER[] =
    {
        &handlerError,				//0 default handler which returns error
        &handlerWatchServiceMake,	//1 watchService.make
        &handlerWatchAdd,			//2 Watch.add
        &handlerWatchRemove,		//3 Watch.remove
        &handlerWatchPollChanges,	//4 Watch.pollChanges
        &handlerWatchPollRefresh,	//5 Watch.pollRefresh
        &handlerWatchDelete,		//6 Watch.delete
        &handlerSignUp,				//7 signUp
        &handlerBatch				//8 Batch
    };

static const int POST_HANDLERS_COUNT = 9;

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

void handlerError(Response* response,
                  const char* uri,
                  IXML_Element* input)
{
    log_debug("Requested operation \"%s\" exists but not implemented.", uri);
    obix_server_generateObixErrorMessage(
        response,
        uri,
        OBIX_CONTRACT_ERR_UNSUPPORTED,
        "Unsupported Request",
        "The requested operation is not yet implemented.");
    obixResponse_send(response);
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
                "Returning error message \"%s\".", operationName, message);

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
    obixResponse_send(response);
}

void handlerWatchServiceMake(Response* response,
                             const char* uri,
                             IXML_Element* input)
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
                                 TRUE);
    // free Watch DOM structure
    ixmlElement_freeOwnerDocument(watchDOM);

    // send answer
    obixResponse_send(response);
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

static int processWatchIn(IXML_Element* input, const char*** uriSet)
{
    // check input
    if (input == NULL)
    {
        return -1;
    }

    // get list of uri
    IXML_NodeList* uriList = ixmlElement_getElementsByTagName(
                                 input,
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

/**
 * Adds new response part to the tail of the response and sends the error
 * message if creation failed.
 *
 * @param respHead Head of the response (used when the error message is sent).
 * @param respTail Tail of the response to which a new part will be added.
 * @param uri Uri of the operation which invoked this function. It will be used
 * 			  in the error message.
 * @param handlerName Name of the operation handler which will appear in the
 * 					  error message.
 * @return New response object which is a next part of the provided response;
 * 		   @a NULL on error.
 */
static Response* addResponsePart(Response* respHead,
                                 Response* respTail,
                                 const char* uri,
                                 const char* operationName)
{
    Response* newPart = obixResponse_add(respTail);
    if (newPart == NULL)
    {
        log_error("Unable to create multipart response object.");
        sendErrorMessage(respHead,
                         uri,
                         operationName,
                         "Internal server error.");
    }
    return newPart;
}

void handlerWatchAdd(Response* response,
                     const char* uri,
                     IXML_Element* input)
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
        rItem = addResponsePart(response, rItem, uri, "Watch.add");
        if (rItem == NULL)
        {   // error message is already sent
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
                                         FALSE);
            break;
        case -1:
            // object with provided URI is not found
            obix_server_generateObixErrorMessage(
                rItem,
                uriSet[i],
                OBIX_CONTRACT_ERR_BAD_URI,
                "Bad URI Error",
                "Requested URI is not found on the server.");

            break;
        case -2:
            obix_server_generateObixErrorMessage(
                rItem,
                uriSet[i],
                OBIX_CONTRACT_ERR_BAD_URI,
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
    rItem = addResponsePart(response, rItem, uri, "Watch.add");
    if (rItem == NULL)
    {   // error message is already sent
        return;
    }
    obixResponse_setText(rItem, WATCH_OUT_POSTFIX, TRUE);
    obixResponse_send(response);
}

void handlerWatchRemove(Response* response,
                        const char* uri,
                        IXML_Element* input)
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
    obixResponse_send(response);
}

static void completeWatchPollResponse(const char* operationName,
                                      Response* respHead,
                                      Response* respTail,
                                      const char* uri)
{
    //complete response
    respTail = addResponsePart(respHead, respTail, uri, operationName);
    if (respTail == NULL)
    {   // error message is already sent
        return;
    }
    obixResponse_setText(respTail, WATCH_OUT_POSTFIX, TRUE);

    // send response
    obixResponse_send(respHead);
}

static Response* pollWatchItemIterator(const char* operationName,
                                       BOOL changedOnly,
                                       oBIX_Watch* watch,
                                       Response* response,
                                       const char* uri)
{
    oBIX_Watch_Item* watchItem = watch->items;
    Response* respPart = response;

    // iterate through every watched object
    while (watchItem != NULL)
    {
        if ((obixWatchItem_isUpdated(watchItem) == TRUE)
                || !changedOnly)
        {
            // TODO handle case when the object was deleted
            // in that case, the first pollChanges and all pollRefresh
            // should show <err/> object.
            obixWatchItem_setUpdated(watchItem, FALSE);

            // create new response part
            respPart = addResponsePart(response, respPart, uri, operationName);
            if (respPart == NULL)
            {   // error message is already sent
                return NULL;
            }

            obix_server_generateResponse(respPart,
                                         watchItem->doc,
                                         watchItem->uri,
                                         FALSE,
                                         FALSE, 0,
                                         FALSE);

        }
        // iterate to the next watch item
        watchItem = watchItem->next;
    }

    // return tail of the response
    return respPart;
}

/**
 * This method is used to perform delayed poll response
 */
void handlerWatchLongPoll(oBIX_Watch* watch,
                          Response* response,
                          const char* uri)
{
    log_debug("Handling long poll request.");
    //iterate through all watch items and generate response
    Response* respTail = pollWatchItemIterator(
                             "Watch.pollChanges",
                             TRUE,
                             watch,
                             response,
                             uri);

    if (respTail != NULL)
    {
        //complete response
        completeWatchPollResponse("Watch.pollChanges", response, respTail, uri);
    }
}

/**
 * Common function to handle both pollChanges and pollRefresh calls.
 */
static void handlerWatchPollHelper(Response* response,
                                   const char* uri,
                                   BOOL changedOnly)
{
    char* operationName = changedOnly ?
                          "Watch.pollChanges" : "Watch.pollRefresh";
    // this produces to much of log
    //    log_debug("Handling %s of watch \"%s\".", operationName, uri);

    // find the corresponding watch object
    oBIX_Watch* watch = obixWatch_getByUri(uri);
    if (watch == NULL)
    {
        sendErrorMessage(response,
                         uri,
                         operationName,
                         "Watch object doesn't exist.");
        return;
    }

    // reset lease timer
    obixWatch_resetLeaseTimer(watch, OBIX_WATCH_LEASE_NO_CHANGE);

    // prepare response header
    obixResponse_setText(response, WATCH_OUT_PREFIX, TRUE);

    //iterate through all watch items and generate response
    Response* respTail = pollWatchItemIterator(
                             operationName,
                             changedOnly,
                             watch,
                             response,
                             uri);

    if (respTail == NULL)
    {	// error is already handled
        return;
    }

    // if we are not processing long poll request of Watch.pollChanges...
    if (!obixWatch_isLongPoll(watch) || !changedOnly)
    {
        // complete and send response
        completeWatchPollResponse(operationName, response, respTail, uri);
        return;
    }

    // process long poll request
    int error;
    // if we didn't add anything to the response except header...
    if (respTail == response)
    {
        // hold the response for max time (or until something happens)
        error = obixWatch_holdPoll(&handlerWatchLongPoll,
                                   watch,
                                   response,
                                   uri,
                                   TRUE);
    }
    else
    {
        // we do have something to send, but we still have to wait
        // for pollWaitInterval/min.
        // clean everything from response except header, because delayed
        // poll handler will create the rest of the answer again
        obixResponse_free(response->next);
        error = obixWatch_holdPoll(&handlerWatchLongPoll,
                                   watch,
                                   response,
                                   uri,
                                   FALSE);
    }

    if (error != 0)
    {
        if (error == -2)
        {
            // we can't hold the request, because all request objects are
            // already reserved.
            sendErrorMessage(response,
                             uri,
                             operationName,
                             "Unable to hold long poll request: "
                             "Check that you are not trying to call "
                             "pollChanges from Batch request. If not, than "
                             "maximum number of requests on hold is reached. "
                             "Ask administrator to increase "
                             "hold-request-max parameter in server settings or "
                             "use traditional polling.");
        }
        else
        {
            // attempt to hold a poll failed - reason unknown
            sendErrorMessage(response,
                             uri,
                             operationName,
                             "Unable to hold long poll request: "
                             "Internal server error.");
        }
    }
    // answer will be sent by scheduled task, so we don't need to do it now
}

void handlerWatchPollChanges(Response* response,
                             const char* uri,
                             IXML_Element* input)
{
    // ignore input
    handlerWatchPollHelper(response, uri, TRUE);
}

void handlerWatchPollRefresh(Response* response,
                             const char* uri,
                             IXML_Element* input)
{
    // ignore input
    handlerWatchPollHelper(response, uri, FALSE);
}

void handlerWatchDelete(Response* response,
                        const char* uri,
                        IXML_Element* input)
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
            obixResponse_send(response);
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
    IXML_Element* devices = xmldb_getDOM("/obix/devices/", NULL);
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
    int error = ixmlElement_copyAttributeWithLog(newDevice, ref,
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
            FALSE);
    if ((error != IXML_SUCCESS) && (error != IXML_NOT_FOUND_ERR))
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

    return 0;
}

void handlerSignUp(Response* response, const char* uri, IXML_Element* input)
{
    if (input == NULL)
    {
        sendErrorMessage(response, uri, "Sign Up", "Device data is corrupted.");
        return;
    }

    int error = xmldb_putDOM(input);
    const char* href = ixmlElement_getAttribute(input, OBIX_ATTR_HREF);
    // in stored object href attribute contains server address, but we don't
    // need it
    href += xmldb_getServerAddressLength();
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
    error = putDeviceReference(input);
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
                                 input,
                                 NULL,
                                 TRUE,
                                 TRUE, 0,
                                 TRUE);
    // TODO what about removing this isError at all?
    if (obixResponse_isError(response))
    {
        // we return error, thus we need to roll back all changes
        xmldb_delete(href);
        // TODO delete also record from device list
    }

    // send response
    obixResponse_send(response);
}

void handlerBatch(Response* response,
                  const char* uri,
                  IXML_Element* input)
{
    // check input
    if (input == NULL)
    {
        sendErrorMessage(response, uri, "Batch",
                         "Input is empty or broken.");
        return;
    }

    // prepare response
    obixResponse_setText(response, BATCH_OUT_PREFIX, TRUE);
    Response* rItem = addResponsePart(response, response, uri, "Batch");
    if (rItem == NULL)
    {   // error message is already sent
        return;
    }

    // iterate through the list of commands
    IXML_Node* node = ixmlNode_getFirstChild(ixmlElement_getNode(input));

    while (node != NULL)
    {
        IXML_Element* command = ixmlNode_convertToElement(node);
        if (command == NULL)
        {
            // this is not a tag, go to the next node in batch command
            node = ixmlNode_getNextSibling(node);
            continue;
        }

        // get uri of the command
        const char* commandUri = ixmlElement_getAttribute(command,
                                 OBIX_ATTR_VAL);
        if (commandUri == NULL)
        {
            // this is some error in the user command, because there should be
            // always some uri
            sendErrorMessage(response, uri, "Batch",
                             "Input contains illegal tag(s).");
            return;
        }

        // find out type of command (read, write or invoke)
        if (obix_obj_implementsContract(command, "Read"))
        {
            obix_server_read(rItem, commandUri);

        }
        else if (obix_obj_implementsContract(command, "Write"))
        {
            obix_server_write(rItem,
                              commandUri,
                              ixmlNode_convertToElement(
                                  ixmlNode_getFirstChild(node)));
        }
        else if (obix_obj_implementsContract(command, "Invoke"))
        {
            obix_server_invoke(rItem,
                               commandUri,
                               ixmlNode_convertToElement(
                                   ixmlNode_getFirstChild(node)));
        }
        else
        {
            // unknown tag
            sendErrorMessage(response, uri, "Batch",
                             "Input contains illegal tag(s).");
            return;
        }

        // the command handler could generate more than one response part, find
        // current response tail
        while (rItem->next != NULL)
        {
            rItem = rItem->next;
        }
        // create new response part for the next command response
        rItem = addResponsePart(response, rItem, uri, "Batch");
        if (rItem == NULL)
        {   // error message is already sent
            return;
        }
        node = ixmlNode_getNextSibling(node);
    }

    // finish and send the response object
    obixResponse_setText(rItem, BATCH_OUT_POSTFIX, TRUE);
    obixResponse_send(response);
}
