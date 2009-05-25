/** @file
 * @todo add description here
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#include <stdlib.h>
#include <string.h>
#include <ixml_ext.h>
#include <xml_config.h>
#include <lwl_ext.h>
#include <curl_ext.h>
#include <ptask.h>
#include <obix_utils.h>
// TODO is included only for error codes
#include <obix_client.h>
#include "obix_http.h"

#define DEFAULT_POLLING_INTERVAL 500
#define DEFAULT_WATCH_LEASE_PADDING 10000

#define OBIX_WATCH_IN_TEMPLATE_HEADER ("<obj is=\"obix:WatchIn\">\r\n" \
							           "  <list name=\"hrefs\" of=\"obix:Uri\">\r\n")
#define OBIX_WATCH_IN_TEMPLATE_HEADER_LENGTH 62

#define OBIX_WATCH_IN_TEMPLATE_URI "    <uri val=\"%s\"/>\r\n"
#define OBIX_WATCH_IN_TEMPLATE_URI_LENGTH 19

#define OBIX_WATCH_IN_TEMPLATE_FOOTER ("  </list>\r\n" \
							           "</obj>")
#define OBIX_WATCH_IN_TEMPLATE_FOOTER_LENGTH 17

#define OBIX_WRITE_REQUEST_TEMPLATE "<%s href=\"%s\" val=\"%s\"/>"
#define OBIX_WRITE_REQUEST_TEMPLATE_LENGTH 18

static const char** OBIX_DATA_TYPE_NAMES[] =
    {
        &OBIX_OBJ_BOOL,
        &OBIX_OBJ_INT,
        &OBIX_OBJ_REAL,
        &OBIX_OBJ_STR,
        &OBIX_OBJ_ENUM,
        &OBIX_OBJ_ABSTIME,
        &OBIX_OBJ_RELTIME,
        &OBIX_OBJ_URI
    };

static const char* OBIX_WATCH_OUT_VALUES = "values";

const Comm_Stack OBIX_HTTP_COMM_STACK =
    {
        &http_initConnection,
        &http_openConnection,
        &http_closeConnection,
        &http_freeConnection,
        &http_registerDevice,
        &http_unregisterDevice,
        &http_registerListener,
        &http_unregisterListener,
        &http_writeValue
    };

static const char* CT_SERVER_ADDRESS = "server-address";
static const char* CT_POLL_INTERVAL = "poll-interval";
static const char* CT_WATCH_LEASE = "watch-lease";
static const char* CTA_LOBBY = "lobby";

static BOOL _initialized;
static CURL_EXT* _curl_handle;
static CURL_EXT* _curl_watch_handle;

static Task_Thread* _watchThread;

static Http_Connection* getHttpConnection(Connection* connection)
{
    // TODO it would be more safe to check the connection type first
    return (Http_Connection*) connection;
}

static Http_Device* getHttpDevice(Device* device)
{
    return (Http_Device*) device;
}

static const char* removeServerAddress(const char* uri, Http_Connection* c)
{
    if (strncmp(uri, c->serverUri, c->serverUriLength) == 0)
    {
        return (uri + c->serverUriLength);
    }
    else
    {
        return uri;
    }
}

// TODO think about moving it to obix_utils
static char* ixmlElement_getFullHref(IXML_Element* element,
                                     Http_Connection* c, BOOL full)
{
    // helper function for uri concatenation
    char* concatUri(const char* parent, int parentLength, char* child)
    {
        char* newUri = (char*) malloc(strlen(child) + parentLength + 1);
        if (newUri == NULL)
        {
            log_error("Unable to generate full URI for <%s/>: "
                      "Not enough memory.", ixmlElement_getTagName(element));
            free(child);
            return NULL;
        }
        strncpy(newUri, parent, parentLength);
        strcpy(newUri + parentLength, child);
        free(child);
        return newUri;
    }

    const char* attrValue = ixmlElement_getAttribute(element, OBIX_ATTR_HREF);

    // if we do not need to return server address, but href attribute already
    // contains it, than drop it
    if (!full && (strncmp(attrValue, c->serverUri, c->serverUriLength) == 0))
    {
        attrValue += c->serverUriLength;
    }
    char* uri = ixmlCloneDOMString(attrValue);

    if (uri == NULL)
    {
        log_error("Element <%s/> doesn't have \"%s\" attribute.",
                  ixmlElement_getTagName(element), OBIX_ATTR_HREF);
        return NULL;
    }

    // append parent's URI until we have URI starting from '/' or
    // from server address
    IXML_Element* parent = element;
    while((*uri != '/') &&
            (strncmp(c->serverUri, uri, c->serverUriLength) != 0))
    {
        parent = ixmlNode_convertToElement(
                     ixmlNode_getParentNode(
                         ixmlElement_getNode(parent)));
        if (parent == NULL)
        {
            // we do not have enough parent objects in the document in order
            // to create the full URI
            log_error("Unable to generate full URI for <%s/>. "
                      "Output should start with either \"/\" or \"%s\", "
                      "but it is \"%s\".",
                      ixmlElement_getTagName(element), c->serverUri, uri);
            free(uri);
            return NULL;

        }

        const char* parentUri =
            ixmlElement_getAttribute(parent, OBIX_ATTR_HREF);

        if (parentUri == NULL)
        {	// this parent doesn't have href attribute, try next parent
            continue;
        }
        // if we do not need to return server address, but parent contains it
        // than drop it
        if (!full &&
                (strncmp(parentUri, c->serverUri, c->serverUriLength) == 0))
        {
            parentUri += c->serverUriLength;
        }

        int parentUriLength = strlen(parentUri);
        if (parentUriLength == 0)
        {
            log_error("Unable to generate full URI for <%s/>. "
                      "Data is corrupted.", ixmlElement_getTagName(element));
            free(uri);
            return NULL;
        }

        if (parentUri[parentUriLength - 1] != '/')
        {
            // we want to concat parent's uri with ours. But we have to use
            // parents URI until it's last slash. Example:
            // <parent href="/a/b" ><child href="c" /></parent>
            // in that case child's href="/a/c
            for (parentUriLength -= 2;
                    (parentUriLength > 0) && (parentUri[parentUriLength] != '/');
                    parentUriLength--)
                ;

            if (parentUriLength <= 0)
            {
                // there is nothing to be copied from this uri, go to the next
                // parent element
                continue;
            }
            // increase length so that trailing slash is also copied
            parentUriLength++;
        }

        // concatenate our URI with parents
        uri = concatUri(parentUri, parentUriLength, uri);
    }

    if (full && (*uri == '/'))
    {	// add server address if full URI is needed, but it is not a full yet.
        uri = concatUri(c->serverUri, c->serverUriLength, uri);
    }

    return uri;
}

static char* getObjectUri(IXML_Document* doc,
                          const char* objName,
                          Http_Connection* c,
                          BOOL full)
{
    IXML_Element* element = ixmlDocument_getElementByAttrValue(
                                doc,
                                OBIX_ATTR_NAME,
                                objName);
    if (element == NULL)
    {
        log_error("Object \"%s\" is not found in the server response.",
                  objName);
        return NULL;
    }

    char* uri = ixmlElement_getFullHref(element, c, full);
    if (uri == NULL)
    {
        log_error("Unable to retrieve full URI for the object \"%s\".",
                  objName);
    }
    return uri;
}

static void resetWatchUris(Http_Connection* c)
{
    if (c->watchAddUri != NULL)
    {
        free(c->watchAddUri);
        c->watchAddUri = NULL;
    }
    if (c->watchDeleteUri != NULL)
    {
        free(c->watchDeleteUri);
        c->watchDeleteUri = NULL;
    }
    if (c->watchRemoveUri != NULL)
    {
        free(c->watchRemoveUri);
        c->watchRemoveUri = NULL;
    }
    if (c->watchPollChangesFullUri != NULL)
    {
        free(c->watchPollChangesFullUri);
        c->watchPollChangesFullUri = NULL;
    }
}

static char* getWatchInString(const char** paramUri, int count)
{
    // calculate size of WatchIn object
    int size = OBIX_WATCH_IN_TEMPLATE_HEADER_LENGTH;
    int i;
    for (i = 0; i < count; i++)
    {
        size += OBIX_WATCH_IN_TEMPLATE_URI_LENGTH + strlen(paramUri[i]);
    }
    size += OBIX_WATCH_IN_TEMPLATE_FOOTER_LENGTH + 1;

    char* watchIn = (char*) malloc(size);
    if (watchIn == NULL)
    {
        return NULL;
    }

    // print header
    strcpy(watchIn, OBIX_WATCH_IN_TEMPLATE_HEADER);
    size = OBIX_WATCH_IN_TEMPLATE_HEADER_LENGTH;
    // print URIs
    for (i = 0; i < count; i++)
    {
        size += sprintf(watchIn + size, OBIX_WATCH_IN_TEMPLATE_URI, paramUri[i]);
    }
    // print footer
    strcpy(watchIn + size, OBIX_WATCH_IN_TEMPLATE_FOOTER);

    return watchIn;
}

static int writeValue(const char* paramUri,
                      const char* newValue,
                      OBIX_DATA_TYPE dataType,
                      CURL_EXT* curlHandle)
{
    // generate request body
    const char* objName = *(OBIX_DATA_TYPE_NAMES[dataType]);
    char* requestBody = (char*)
                        malloc(OBIX_WRITE_REQUEST_TEMPLATE_LENGTH
                               + strlen(objName)
                               + strlen(paramUri)
                               + strlen(newValue) + 1);
    if (requestBody == NULL)
    {
        log_error("Unable to write to the oBIX server: "
                  "Not enough memory.");
        return OBIX_ERR_NO_MEMORY;
    }
    sprintf(requestBody, OBIX_WRITE_REQUEST_TEMPLATE,
            objName, paramUri, newValue);

    // send request
    curlHandle->outputBuffer = requestBody;
    int error = curl_ext_put(curlHandle, paramUri);
    free(requestBody);
    if (error != 0)
    {
        log_error("Unable to write to the server %s: "
                  "curl_ext_put() returned %d.", paramUri, error);
        return OBIX_ERR_BAD_CONNECTION;
    }

    // we do not use parseResponse here to check for error, because
    // generating DOM structure is quite slow.
    if (*(curlHandle->inputBuffer) == '\0')
    {
        log_warning("Server did not returned anything for PUT request. "
                    "Parameter \"%s\" can be left unchanged.", paramUri);
        return OBIX_ERR_BAD_CONNECTION;
    }

    if (strstr(curlHandle->inputBuffer, "<err") != NULL)
    {
        log_warning("Server's answer for PUT request contains error object. "
                    "Parameter \"%s\" can be left unchanged:\n%s",
                    paramUri, curlHandle->inputBuffer);
        return OBIX_ERR_BAD_CONNECTION;
    }

    return OBIX_SUCCESS;
}

static int addWatchItem(Http_Connection* c,
                        const char** paramUri,
                        int count,
                        IXML_Document** response,
                        CURL_EXT* curlHandle)
{
    char fullWatchAddUri[c->serverUriLength + strlen(c->watchAddUri) + 1];
    strcpy(fullWatchAddUri, c->serverUri);
    strcat(fullWatchAddUri, c->watchAddUri);
    char *requestBody = getWatchInString(paramUri, count);

    if (requestBody == NULL)
    {
        log_error("Unable to register new listener: Not enough memory.");
        return OBIX_ERR_NO_MEMORY;
    }
    curlHandle->outputBuffer = requestBody;
    int error = curl_ext_postDOM(curlHandle, fullWatchAddUri, response);
    free(requestBody);

    if (error != 0)
    {
        return OBIX_ERR_BAD_CONNECTION;
    }
    return OBIX_SUCCESS;
}

static int createWatch(Http_Connection* c, CURL_EXT* curlHandle)
{
    char* watchAddUri = NULL;
    char* watchRemoveUri = NULL;
    char* watchPollChangesFullUri = NULL;
    char* watchDeteleUri = NULL;
    IXML_Document* response = NULL;

    // helper function for memory cleaning on errors
    void cleanup()
    {
        if (watchAddUri != NULL)
            free(watchAddUri);
        if (watchRemoveUri != NULL)
            free(watchRemoveUri);
        if (watchPollChangesFullUri != NULL)
            free(watchPollChangesFullUri);
        if (watchDeteleUri != NULL)
            free(watchDeteleUri);
        if (response != NULL)
            ixmlDocument_free(response);
    }
    // form the URI of watchService.make operation
    char makeUri[c->serverUriLength + strlen(c->watchMakeUri) + 1];
    strcpy(makeUri, c->serverUri);
    strcat(makeUri, c->watchMakeUri);

    curlHandle->outputBuffer = NULL;
    curl_ext_postDOM(curlHandle, makeUri, &response);
    if (response == NULL)
    {
        return OBIX_ERR_BAD_CONNECTION;
    }

    watchAddUri = getObjectUri(response, OBIX_NAME_WATCH_ADD, c, FALSE);
    watchRemoveUri = getObjectUri(response, OBIX_NAME_WATCH_REMOVE, c, FALSE);
    watchDeteleUri = getObjectUri(response, OBIX_NAME_WATCH_DELETE, c, FALSE);
    // we store complete URI of pollChanges operation because we use it more
    // often than others.
    watchPollChangesFullUri = getObjectUri(response, OBIX_NAME_WATCH_POLLCHANGES, c, TRUE);
    if ((watchAddUri == NULL)
            || (watchRemoveUri == NULL)
            || (watchDeteleUri == NULL)
            || (watchPollChangesFullUri == NULL))
    {
        log_error("watchService.make operation at server \"%s\" returned Watch "
                  "object in wrong format:\n"
                  "%s", c->serverUri, curlHandle->inputBuffer);
        cleanup();
        return OBIX_ERR_BAD_CONNECTION;
    }

    // store all these URIs
    c->watchAddUri = watchAddUri;
    c->watchRemoveUri = watchRemoveUri;
    c->watchDeleteUri = watchDeteleUri;
    c->watchPollChangesFullUri = watchPollChangesFullUri;

    // try to set watch lease time
    char* watchLeaseUri = getObjectUri(response, OBIX_NAME_WATCH_LEASE, c, TRUE);
    ixmlDocument_free(response);
    if (watchLeaseUri == NULL)
    {
        log_warning("watchService.make operation at server \"%s\" returned Watch "
                    "object without \'%s\' tag:\n%s", c->serverUri,
                    OBIX_NAME_WATCH_LEASE, curlHandle->inputBuffer);
        free(watchLeaseUri);
        // do not consider this error as a big problem
        return OBIX_SUCCESS;
    }

    char* leaseTime = obix_reltime_fromLong(c->watchLease, RELTIME_SEC);
    if (leaseTime == NULL)
    {
        // this should never happen!
        log_error("Unable to convert Watch.lease time from long (%ld) to "
                  "obix:reltime!", c->watchLease);
        free(watchLeaseUri);
        cleanup();
        return OBIX_ERR_UNKNOWN_BUG;
    }

    int error = writeValue(watchLeaseUri,
                           leaseTime,
                           OBIX_T_RELTIME,
                           curlHandle);
    free(watchLeaseUri);
    free(leaseTime);
    if (error == OBIX_ERR_BAD_CONNECTION)
    {
        // server could return error just because it prevents changes of
        // Watch.lease. So we ignore this error.
        return OBIX_SUCCESS;
    }

    return error;
}

/**
 * Creates new Watch object after a failure of previous one.
 */
static int recreateWatch(Http_Connection* c,
                         IXML_Document** response,
                         CURL_EXT* curlHandle)
{
    log_warning("Trying to create new Watch object...\nIf you often see this "
                "message, try to set/increase <%s/> value in connection "
                "settings (will help only if your oBIX server supports "
                "changing Watch.lease), or reduce <%s/> value.",
                CT_WATCH_LEASE, CT_POLL_INTERVAL);
    // reset old watch uri's.
    resetWatchUris(c);
    // create new watch
    int error = createWatch(c, curlHandle);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }
    // add all watch items that we have in the list
    const char** uri;
    int count = table_getKeys(c->watchTable, &uri);

    return addWatchItem(c, uri, count, response, curlHandle);
}

static int parseResponse(IXML_Document* respDoc, IXML_Element** respElem)
{
    IXML_Node* node = ixmlNode_getFirstChild(ixmlDocument_getNode(respDoc));
    // iterate until we find first xml element
    while((node != NULL) && (ixmlNode_getNodeType(node) != eELEMENT_NODE))
    {
        node = ixmlNode_getNextSibling(node);
    }

    if (node == NULL)
    {
        char* text = ixmlPrintDocument(respDoc);
        log_error("Server response doesn't contain any oBIX objects:\n"
                  "%s", text);
        free(text);
        return OBIX_ERR_BAD_CONNECTION;
    }

    IXML_Element* element = ixmlNode_convertToElement(node);
    if (respElem != NULL)
    {
        *respElem = element;
    }

    if (strcmp(ixmlElement_getTagName(element), OBIX_OBJ_ERR) == 0)
    {
        char* text = ixmlPrintDocument(respDoc);
        log_error("Server replied with error:\n"
                  "%s", text);
        free(text);
        return OBIX_ERR_INVALID_STATE;
    }

    return OBIX_SUCCESS;
}

static int parseWatchOut(IXML_Document* doc,
                         Http_Connection* c,
                         CURL_EXT* curlHandle)
{
    // usually DOM doc is provided by caller method (and cleaned also in it)
    // but sometimes we generate own one.
    BOOL needToCleanDoc = FALSE;
    IXML_Element* element = NULL;
    int error = parseResponse(doc, &element);
    if (error != OBIX_SUCCESS)
    {
        if (error != OBIX_ERR_INVALID_STATE)
        {
            return error;
        }

        // we received error object instead of WatchOut.
        // in case if it is BadUri than try to create new Watch object.
        const char* errType = ixmlElement_getAttribute(element, OBIX_ATTR_IS);
        if ((errType == NULL) || (strstr(errType, OBIX_HREF_ERR_BAD_URI) == NULL))
        {
            // it is some strange error. no idea what to do with it
            return OBIX_ERR_BAD_CONNECTION;
        }

        // it is badUri error which indicates (most probably it should :) that
        // the Watch object is unexpectedly deleted from oBIX server
        // (unfortunately it can happen :). Try to create new Watch.
        log_warning("It seems like Watch object doesn't exist on oBIX server "
                    "anymore.");
        error = recreateWatch(c, &doc, curlHandle);
        if (error != OBIX_SUCCESS)
        {
            return error;
        }

        error = parseResponse(doc, &element);
        if (error != OBIX_SUCCESS)
        {
            ixmlDocument_free(doc);
            return error;
        }

        // we have now own response generated by recreateWatch(), thus
        // we need to free it after everything is done
        needToCleanDoc = TRUE;
    }


    element = ixmlDocument_getElementByAttrValue(
                  doc,
                  OBIX_ATTR_NAME,
                  OBIX_WATCH_OUT_VALUES);
    if (element == NULL)
    {
        log_warning("WatchOut object returned by server doesn't contain "
                    "\"%s\" list.", OBIX_WATCH_OUT_VALUES);
        // try to get any list object
        element = ixmlDocument_getElementById(doc, OBIX_OBJ_LIST);
        if (element == NULL)
        {
            if (needToCleanDoc)
            {
                ixmlDocument_free(doc);
            }
            return OBIX_ERR_BAD_CONNECTION;
        }
    }

    IXML_Node* node = ixmlNode_getFirstChild(ixmlElement_getNode(element));
    int retVal = OBIX_SUCCESS;
    for (;node != NULL; node = ixmlNode_getNextSibling(node))
    {
        element = ixmlNode_convertToElement(node);
        if (element == NULL)
        {
            char* text = ixmlPrintDocument(doc);
            log_warning("WatchOut object returned by server contains "
                        "something illegal:\n%s", text);
            free(text);
            retVal = OBIX_ERR_BAD_CONNECTION;
            // ignore this node
            continue;
        }
        // check that this is not an error
        const char* tagName = ixmlElement_getTagName(element);
        if (strcmp(tagName, OBIX_OBJ_ERR) == 0)
        {
            char* text = ixmlPrintDocument(doc);
            log_warning("WatchOut contains error object:\n%s", text);
            free(text);
            retVal = OBIX_ERR_BAD_CONNECTION;
            // ignore this node
            continue;
        }
        // get URI of the updated object
        const char* uri = ixmlElement_getAttribute(element, OBIX_ATTR_HREF);
        if (uri == NULL)
        {
            char* text = ixmlPrintDocument(doc);
            log_warning("WatchOut object returned by server contains "
                        "object without \"%s\" attribute:\n%s", OBIX_ATTR_HREF, text);
            free(text);
            retVal = OBIX_ERR_BAD_CONNECTION;
            // ignore this node
            continue;
        }

        // if there is 'val' attribute in the returned object - return it,
        // otherwise, return the whole object
        // TODO fixme somehow
        char* newValue;
        const char* attrValue = ixmlElement_getAttribute(element, OBIX_ATTR_VAL);

        if (attrValue != NULL)
        {	// copy value
            newValue = ixmlCloneDOMString(attrValue);
        }
        else
        {	// return the whole object
            newValue = ixmlPrintNode(ixmlElement_getNode(element));
        }

        //        if (newValue == NULL)
        //        {
        //            char* text = ixmlPrintDocument(doc);
        //            log_warning("WatchOut object returned by server contains updated "
        //                        "object without \"%s\" attribute:\n%s", OBIX_ATTR_VAL, text);
        //            free(text);
        //            retVal = OBIX_ERR_BAD_CONNECTION;
        //            // ignore this node
        //            continue;
        //        }

        // workaround for some oBIX server implementations which return full
        // URI of the updated object, but it should be the same as passed to
        // Watch.add operation.
        if (strncmp(c->serverUri, uri, c->serverUriLength) == 0)
        {
            uri += c->serverUriLength;
        }

        // find corresponding listener of the object
        Listener* listener = (Listener*) table_get(c->watchTable, uri);
        if (listener != NULL)
        {
            // execute callback function
            (listener->callback)(listener->connectionId,
                                 listener->deviceId,
                                 listener->id,
                                 newValue);
        }
        else
        {
            log_error("Unable to find listener for the object with URI \"%s\".", uri);
            retVal = OBIX_ERR_BAD_CONNECTION;
        }
        ixmlFreeDOMString(newValue);
    }

    if (needToCleanDoc)
    {
        ixmlDocument_free(doc);
    }
    return retVal;
}

static void watchPollTask(void* arg)
{
    Http_Connection* c = (Http_Connection*) arg;

    pthread_mutex_lock(c->watchMutex);
    if (c->watchPollChangesFullUri == NULL)
    {
        log_error("Watch Poll Task: Someone deleted Watch object but did not "
                  "cancel the poll task.");
        if (ptask_cancel(_watchThread, c->watchPollTaskId) != 0)
        {
            log_error("Watch Poll Task: Unable to delete myself.");
        }
        pthread_mutex_unlock(c->watchMutex);
        return;
    }

    log_debug("requesting %s", c->watchPollChangesFullUri);
    IXML_Document* response;

    int error = curl_ext_postDOM(_curl_watch_handle,
                                 c->watchPollChangesFullUri,
                                 &response);
    if (error != 0)
    {
        log_error("Watch Poll Task: "
                  "Unable to poll changes from the server %s.",
                  c->watchPollChangesFullUri);
        pthread_mutex_unlock(c->watchMutex);
        return;
    }

    error = parseWatchOut(response, c, _curl_watch_handle);
    if (error != OBIX_SUCCESS)
    {
        log_error("Watch Poll Task: "
                  "Unable to parse WatchOut object (error %d).", error);
    }

    pthread_mutex_unlock(c->watchMutex);
    ixmlDocument_free(response);
}

static int removeWatch(Http_Connection* c)
{
    if (c->watchTable->count > 0)
    {
        log_warning("Deleting not empty watch object from the oBIX server. "
                    "Some subscribed listeners can stop receiving updates.");
    }

    // stop polling task
    int error = ptask_cancel(_watchThread, c->watchPollTaskId);
    if (error != 0)
    {
        log_error("Unable to cancel Watch Poll Task:"
                  "ptask_cancel() returned %d", error);
        return error;
    }

    // delete Watch object from the oBIX server
    char watchDeleteFullUri[c->serverUriLength
                            + strlen(c->watchDeleteUri) + 1];
    strcpy(watchDeleteFullUri, c->serverUri);
    strcat(watchDeleteFullUri, c->watchDeleteUri);
    _curl_handle->outputBuffer = NULL;
    IXML_Document* response;
    error = curl_ext_postDOM(_curl_handle,
                             watchDeleteFullUri,
                             &response);
    if (error != 0)
    {
        log_error("Unable to delete Watch from the server %s.",
                  watchDeleteFullUri);
        return OBIX_ERR_BAD_CONNECTION;
    }

    if (response == NULL)
    {
        // server did not returned anything, but consider that
        // watch is removed
        log_warning("Server did not return anything for Watch.delete "
                    "operation (%s).", watchDeleteFullUri);
    }
    else
    {
        // check the response
        if (parseResponse(response, NULL) != OBIX_SUCCESS)
        {
            ixmlDocument_free(response);
            return OBIX_ERR_BAD_CONNECTION;
        }

        ixmlDocument_free(response);
    }

    // reset all Watch related variables, because they are no longer valid
    resetWatchUris(c);

    return OBIX_SUCCESS;
}

static int addListener(Http_Connection* c,
                       const char* paramUri,
                       Listener* listener)
{
    // save listener to the listeners table
    pthread_mutex_lock(c->watchMutex);
    table_put(c->watchTable, paramUri, listener);

    // check that we already have a watch object for this server
    if (c->watchAddUri == NULL)
    {
        // create new one
        int error = createWatch(c, _curl_handle);
        if (error != OBIX_SUCCESS)
        {
            pthread_mutex_unlock(c->watchMutex);
            return error;
        }

        // schedule polling task
        // schedule new periodic task for watch polling
        int ptaskId = ptask_schedule(_watchThread, &watchPollTask, c,
                                     c->pollInterval, EXECUTE_INDEFINITE);
        if (ptaskId < 0)
        {
            log_error("Unable to schedule Watch Poll Task: Not enough memory.");
            pthread_mutex_unlock(c->watchMutex);
            return OBIX_ERR_NO_MEMORY;
        }
        c->watchPollTaskId = ptaskId;

    }
    pthread_mutex_unlock(c->watchMutex);

    return OBIX_SUCCESS;
}

static int removeListener(Http_Connection* c, const char* paramUri)
{
    // remove listener URI from the listeners table
    pthread_mutex_lock(c->watchMutex);
    table_remove(c->watchTable, paramUri);

    // remove Watch object  from the server completely
    // if there are no more items to watch
    int retVal = OBIX_SUCCESS;
    if (c->watchTable->count == 0)
    {
        retVal = removeWatch(c);
    }
    pthread_mutex_unlock(c->watchMutex);

    return retVal;
}

int http_init()
{
    if (_initialized)
    {
        return OBIX_SUCCESS;
    }

    // initialize curl library
    // TODO get the buffer size from settings file
    int error = curl_ext_init(0);
    if (error != 0)
    {
        return OBIX_ERR_HTTP_LIB;
    }
    // initialize 2 curl handles: one for watch polling thread
    // and one for other calls.
    error = curl_ext_create(&_curl_handle);
    if (error != 0)
    {
        if (error == -2)
        {
            return OBIX_ERR_NO_MEMORY;
        }
        return OBIX_ERR_HTTP_LIB;
    }
    error = curl_ext_create(&_curl_watch_handle);
    if (error != 0)
    {
        if (error == -2)
        {
            return OBIX_ERR_NO_MEMORY;
        }
        return OBIX_ERR_HTTP_LIB;
    }

    //    curl_easy_setopt(_curl_watch_handle->curl, CURLOPT_VERBOSE, 1L);
    //    curl_easy_setopt(_curl_watch_handle->curl, CURLOPT_STDERR, stdout);
    //
    //    curl_easy_setopt(_curl_handle->curl, CURLOPT_VERBOSE, 1L);
    //    curl_easy_setopt(_curl_handle->curl, CURLOPT_STDERR, stdout);

    // initialize Periodic Task thread which will be used for watch polling
    _watchThread = ptask_init();
    if (_watchThread == NULL)
    {
        return OBIX_ERR_HTTP_LIB;
    }

    _initialized = TRUE;
    return OBIX_SUCCESS;
}

void http_dispose()
{
    if (_initialized)
    {
        // destroy curl handles
        curl_ext_free(_curl_handle);
        curl_ext_free(_curl_watch_handle);
        // stop curl library
        curl_ext_dispose();
        // stop Periodic Task thread
        ptask_dispose(_watchThread);
    }

    _initialized = FALSE;
}

int http_initConnection(IXML_Element* connItem, Connection** connection)
{
    char* serverUri = NULL;
    char* lobbyUri = NULL;
    Http_Connection* c;
    Table* table = NULL;
    pthread_mutex_t* mutex = NULL;
    long pollInterval = DEFAULT_POLLING_INTERVAL;
    long watchLease;

    // helper function for releasing resources on error
    void cleanup()
    {
        if (serverUri != NULL)
            free(serverUri);
        if (lobbyUri != NULL)
            free(lobbyUri);
        if (table != NULL)
            free(table);
        if (mutex != NULL)
            free(mutex);
    }

    // load server address
    IXML_Element* element = config_getChildTag(connItem, CT_SERVER_ADDRESS, TRUE);
    if (element == NULL)
    {
        return OBIX_ERR_INVALID_ARGUMENT;
    }
    const char* attrValue = config_getTagAttributeValue(element, CTA_VALUE, TRUE);
    if (attrValue == NULL)
    {
        return OBIX_ERR_INVALID_ARGUMENT;
    }
    // remove trailing slash from server address
    int serverUriLength = strlen(attrValue);
    if (attrValue[serverUriLength - 1] == '/')
    {
        serverUriLength--;
    }
    serverUri = (char*) malloc(serverUriLength + 1);
    if (serverUri == NULL)
    {
        log_error("Unable to initialize HTTP connection: Not enough memory.");
        cleanup();
        return OBIX_ERR_NO_MEMORY;
    }
    strncpy(serverUri, attrValue, serverUriLength);
    serverUri[serverUriLength] = '\0';
    // load Lobby object address
    attrValue = config_getTagAttributeValue(element, CTA_LOBBY, TRUE);
    if (attrValue == NULL)
    {
        cleanup();
        return OBIX_ERR_INVALID_ARGUMENT;
    }
    // we don't want lobby uri contain server address
    if (strncmp(attrValue, serverUri, serverUriLength) == 0)
    {
        attrValue += serverUriLength;
    }
    if (*attrValue != '/')
    {
        log_error("Attribute \"%s\" of configuration tag <%s/> has wrong "
                  "value: It should contain absolute address of the Lobby "
                  "object.", CTA_LOBBY, CT_SERVER_ADDRESS);
        cleanup();
        return OBIX_ERR_INVALID_ARGUMENT;
    }
    lobbyUri = (char*) malloc(strlen(attrValue) + 1);
    if (lobbyUri == NULL)
    {
        log_error("Unable to initialize HTTP connection: Not enough memory.");
        cleanup();
        return OBIX_ERR_NO_MEMORY;
    }
    strcpy(lobbyUri, attrValue);
    // load watch poll interval, this is optional parameter
    element = config_getChildTag(connItem, CT_POLL_INTERVAL, FALSE);
    if (element != NULL)
    {
        pollInterval = config_getTagLongAttrValue(
                           element,
                           CTA_VALUE,
                           FALSE,
                           DEFAULT_POLLING_INTERVAL);
    }
    // load watch lease time, this is also optional parameter
    // by default it is pollInterval + padding
    watchLease = pollInterval + DEFAULT_WATCH_LEASE_PADDING;
    element = config_getChildTag(connItem, CT_WATCH_LEASE, FALSE);
    if (element != NULL)
    {
        watchLease = config_getTagLongAttrValue(
                         element,
                         CTA_VALUE,
                         FALSE,
                         pollInterval + DEFAULT_WATCH_LEASE_PADDING);
    }

    // allocate space for the connection object
    int listenerMaxCount = (*connection)->maxDevices * (*connection)->maxListeners;
    c = (Http_Connection*) realloc(*connection, sizeof(Http_Connection));
    if (c == NULL)
    {
        log_error("Unable to initialize HTTP connection: Not enough memory.");
        cleanup();
        return OBIX_ERR_NO_MEMORY;
    }
    *connection = &(c->c);

    table = table_create(listenerMaxCount);
    mutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
    if ((table == NULL) || (mutex == NULL))
    {
        log_error("Unable to initialize HTTP connection: Not enough memory.");
        cleanup();
        return OBIX_ERR_NO_MEMORY;
    }

    if (pthread_mutex_init(mutex, NULL) != 0)
    {
        log_error("Unable to initialize HTTP connection: Unable to create mutex.");
        cleanup();
        return OBIX_ERR_HTTP_LIB;
    }
    c->watchTable = table;
    c->watchMutex = mutex;

    c->serverUri = serverUri;
    c->serverUriLength = serverUriLength;
    c->lobbyUri = lobbyUri;
    c->pollInterval = pollInterval;
    c->watchLease = watchLease;

    // initialize other values with zeros
    c->signUpUri = NULL;
    c->watchMakeUri = NULL;
    c->watchAddUri = NULL;
    c->watchDeleteUri = NULL;
    c->watchPollChangesFullUri = NULL;
    c->watchRemoveUri = NULL;

    return OBIX_SUCCESS;
}

void http_freeConnection(Connection* connection)
{
    // free all specific attributes of HTTP connection
    Http_Connection* c = getHttpConnection(connection);
    if (c->serverUri != NULL)
        free(c->serverUri);
    if (c->lobbyUri != NULL)
        free(c->lobbyUri);
    if (c->signUpUri != NULL)
        free(c->signUpUri);
    if (c->watchMakeUri != NULL)
        free(c->watchMakeUri);
    resetWatchUris(c);
    if (c->watchMutex != NULL)
    {
        pthread_mutex_destroy(c->watchMutex);
        free(c->watchMutex);
    }
    if (c->watchTable != 0)
    {
        table_free(c->watchTable);
    }
}

int http_openConnection(Connection* connection)
{
    char* signUpUri = NULL;
    char* watchServiceUri = NULL;
    IXML_Document* response = NULL;

    // helper function for releasing resources on error
    void cleanup()
    {
        if (signUpUri != NULL)
            free(signUpUri);
        if (watchServiceUri != NULL)
            free(watchServiceUri);
        if (response != NULL)
            ixmlDocument_free(response);
    }

    Http_Connection* c = getHttpConnection(connection);
    log_debug("Trying to connect to the oBIX server \"%s\".", c->serverUri);
    // try get Lobby object from the server
    char lobbyFullUri[strlen(c->serverUri) + strlen(c->lobbyUri) + 1];
    strcpy(lobbyFullUri, c->serverUri);
    strcat(lobbyFullUri, c->lobbyUri);
    curl_ext_getDOM(_curl_handle, lobbyFullUri, &response);
    if ((response == NULL) || (parseResponse(response, NULL) != OBIX_SUCCESS))
    {
        log_error("Unable to get Lobby object from the oBIX server \"%s\".",
                  lobbyFullUri);
        cleanup();
        return OBIX_ERR_BAD_CONNECTION;
    }

    // we need to save links to watchService.make and signUp operations.
    signUpUri = getObjectUri(response,
                             OBIX_NAME_SIGN_UP,
                             c, FALSE);
    watchServiceUri = getObjectUri(response,
                                   OBIX_NAME_WATCH_SERVICE,
                                   c, TRUE);
    // we do not fail if there is no signUp object on the server:
    // it means that we can't register new device at this server, but
    // we are still able to register listeners and write data to it.
    if (watchServiceUri == NULL)
    {	// unable to retrieve correct URI from objects
        cleanup();
        return OBIX_ERR_BAD_CONNECTION;
    }
    // now get watchService object
    ixmlDocument_free(response);
    response = NULL;
    curl_ext_getDOM(_curl_handle, watchServiceUri, &response);
    if ((response == NULL) || (parseResponse(response, NULL) != OBIX_SUCCESS))
    {
        log_error("Unable to get watchService object from the oBIX server "
                  "\"%s\".", watchServiceUri);
        cleanup();
        return OBIX_ERR_BAD_CONNECTION;
    }
    // now get URI of watchService.make operation
    free(watchServiceUri);
    watchServiceUri = getObjectUri(response,
                                   OBIX_NAME_WATCH_SERVICE_MAKE,
                                   c, FALSE);
    if (watchServiceUri == NULL)
    {	// unable to retrieve correct URI from objects
        cleanup();
        return OBIX_ERR_BAD_CONNECTION;
    }

    ixmlDocument_free(response);

    c->signUpUri = signUpUri;
    c->watchMakeUri = watchServiceUri;
    return OBIX_SUCCESS;
}

int http_closeConnection(Connection* connection)
{
    Http_Connection* c = getHttpConnection(connection);
    int retVal = OBIX_SUCCESS;

    log_debug("Closing connection to the server %s...", c->serverUri);
    // remove Watch and polling task if they were not removed earlier
    pthread_mutex_lock(c->watchMutex);
    if (c->watchDeleteUri != NULL)
    {
        retVal = removeWatch(c);
    }
    pthread_mutex_unlock(c->watchMutex);

    return retVal;
}

int http_registerDevice(Connection* connection, Device** device, const char* data)
{
    Http_Connection* c = getHttpConnection(connection);
    log_debug("Registering device at the server %s...", c->serverUri);
    if (c->signUpUri == NULL)
    {
        log_error("Unable to register device: "
                  "oBIX server \"%s\" doesn't support signUp feature.",
                  c->serverUri);
        return OBIX_ERR_INVALID_STATE;
    }

    // register new device at the server
    _curl_handle->outputBuffer = data;
    char signUpFullUri[c->serverUriLength + strlen(c->signUpUri) + 1];
    strcpy(signUpFullUri, c->serverUri);
    strcat(signUpFullUri, c->signUpUri);
    IXML_Document* response;
    curl_ext_postDOM(_curl_handle, signUpFullUri, &response);
    if (response == NULL)
    {
        log_error("Unable to register device using service at \"%s\".",
                  signUpFullUri);
        return OBIX_ERR_BAD_CONNECTION;
    }

    // check response
    IXML_Element* element;
    int error = parseResponse(response, &element);
    if (error != OBIX_SUCCESS)
    {
        if (error != OBIX_ERR_INVALID_STATE)
        {
            ixmlDocument_free(response);
            return OBIX_ERR_BAD_CONNECTION;
        }
        // Server returned error object.
        // TODO that is a workaround which works only with oBIX server
        // from same project. When driver is restarted, it appears that
        // data already exists on the server. If it is, than error message
        // would contain URI of the existing data which we will try to use.
        const char* attrValue = ixmlElement_getAttribute(
                                    element,
                                    OBIX_ATTR_HREF);

        if ((attrValue == NULL) || (strstr(attrValue, signUpFullUri) != NULL))
        {
            // server returned unknown error. Nothing can be done.
            ixmlDocument_free(response);
            return OBIX_ERR_BAD_CONNECTION;
        }

        // Server returned error with URI of the object which already exists
        // on it. Thus signUp failed, but required data is already published
        // (at least we hope so :)
        char* uri = ixmlCloneDOMString(attrValue);
        ixmlDocument_free(response);
        response = NULL;
        curl_ext_getDOM(_curl_handle, uri, &response);
        if ((response == NULL) ||
                (parseResponse(response, NULL) != OBIX_SUCCESS))
        {
            // It's a pity but our hopes did not come true :) URI from the
            // error object did not contain any good data
            if (response != NULL)
            {
                ixmlDocument_free(response);
            }
            free(uri);
            return OBIX_ERR_BAD_CONNECTION;
        }

        // It seems that workaround works so far, so let's continue with this
        // received object. But still there is a possibility that the received
        // object is not what we wanted to post on the server :)
        log_warning("signUp operation at oBIX server returned error, telling "
                    "that the object with provided URI already exists. Trying to "
                    "proceed with object with URI \"%s\".", uri);
        free(uri);
    }

    // save actual device URI
    const char* attrValue = ixmlElement_getAttribute(element, OBIX_ATTR_HREF);
    if (attrValue == NULL)
    {
        ixmlDocument_free(response);
        log_error("Object in server response doesn't contain \"%s\" "
                  "attribute:\n%s", OBIX_ATTR_HREF, _curl_handle->inputBuffer);
        return OBIX_ERR_BAD_CONNECTION;
    }
    // remove server address from the uri
    const char* uri = removeServerAddress(attrValue, c);

    // create device object
    Http_Device* d = (Http_Device*) realloc(*device, sizeof(Http_Device));
    if (d == NULL)
    {
        ixmlDocument_free(response);
        log_error("Unable to create device: Not enough memory.");
        return OBIX_ERR_NO_MEMORY;
    }
    *device = &(d->d);

    char* deviceUri = (char*) malloc(strlen(uri) + 1);
    if (deviceUri == NULL)
    {
        ixmlDocument_free(response);
        log_error("Unable to create device: Not enough memory.");
        return OBIX_ERR_NO_MEMORY;
    }

    strcpy(deviceUri, uri);
    ixmlDocument_free(response);
    d->uri = deviceUri;
    d->uriLength = strlen(deviceUri);

    return OBIX_SUCCESS;
}

int http_unregisterDevice(Connection* connection, Device* device)
{
    // TODO implement me
    log_debug("Unregistering device from the server %s",
              getHttpConnection(connection)->serverUri);
    log_warning("Unfortunately driver unregistering is not supported yet.");

    Http_Device* d = getHttpDevice(device);
    free(d->uri);
    return 0;
}

int http_registerListener(Connection* connection, Device* device, Listener** listener)
{
    Http_Connection* c = getHttpConnection(connection);
    Http_Device* d = NULL;

    log_debug("Registering listener of parameter \"%s\" at the server "
              "\"%s\"...", (*listener)->paramUri, c->serverUri);
    int fullParamUriLength = strlen((*listener)->paramUri) + 1;
    if (device != NULL)
    {	// need to add device URI
        d = getHttpDevice(device);
        fullParamUriLength += d->uriLength;
    }
    char fullParamUri[fullParamUriLength];
    if (device == NULL)
    {
        strcpy(fullParamUri, (*listener)->paramUri);
    }
    else
    {
        strcpy(fullParamUri, d->uri);
        strcat(fullParamUri, (*listener)->paramUri);
    }
    int error = addListener(c, fullParamUri, *listener);
    if(error != OBIX_SUCCESS)
    {
        return error;
    }

    IXML_Document* response = NULL;
    char* p = fullParamUri;
    error = addWatchItem(c, (const char**) (&p), 1, &response, _curl_handle);
    if (error != OBIX_SUCCESS)
    {
        removeListener(c, fullParamUri);
        return error;
    }

    error = parseWatchOut(response, c, _curl_handle);
    ixmlDocument_free(response);
    if (error != OBIX_SUCCESS)
    {
        removeListener(c, fullParamUri);
    }

    return error;
}

int http_unregisterListener(Connection* connection, Device* device, Listener* listener)
{
    Http_Connection* c = getHttpConnection(connection);
    Http_Device* d = NULL;

    log_debug("Removing listener of parameter \"%s\" at the server \"%s\"...",
              listener->paramUri, c->serverUri);
    int fullParamUriLength = strlen(listener->paramUri) + 1;

    if (device != NULL)
    {	// need to add device URI
        d = getHttpDevice(device);
        fullParamUriLength += d->uriLength;
    }

    // generate full listeners URI
    char fullParamUri[fullParamUriLength];
    if (device == NULL)
    {
        strcpy(fullParamUri, listener->paramUri);
    }
    else
    {
        strcpy(fullParamUri, d->uri);
        strcat(fullParamUri, listener->paramUri);
    }

    // remove that URI from the Watch object at the oBIX server
    // get URI of Watch.remove operation
    char fullWatchRemoveUri[c->serverUriLength + strlen(c->watchRemoveUri) + 1];
    strcpy(fullWatchRemoveUri, c->serverUri);
    strcat(fullWatchRemoveUri, c->watchRemoveUri);

    // send request
    char* p = fullParamUri;
    char* requestBody = getWatchInString((const char**) (&p), 1);
    if (requestBody == NULL)
    {
        log_error("Unable to unregister listener: Not enough memory.");
        return OBIX_ERR_NO_MEMORY;
    }
    _curl_handle->outputBuffer = requestBody;
    IXML_Document* response;
    int error = curl_ext_postDOM(_curl_handle, fullWatchRemoveUri, &response);
    free(requestBody);
    if (error != 0)
    {
        log_error("Unable to remove watch item from the server %s.",
                  fullWatchRemoveUri);
        return OBIX_ERR_BAD_CONNECTION;
    }

    if (response == NULL)
    {	// server did not returned anything, but consider that
        // watch item is removed
        log_warning("Server did not return anything for Watch.remove "
                    "operation (%s).", fullWatchRemoveUri);
    }
    else
    {	// check server response
        if (parseResponse(response, NULL) != OBIX_SUCCESS)
        {
            ixmlDocument_free(response);
            return OBIX_ERR_BAD_CONNECTION;
        }
        ixmlDocument_free(response);
    }

    return removeListener(c, fullParamUri);
}

int http_writeValue(Connection* connection, Device* device, const char* paramUri, const char* newValue, OBIX_DATA_TYPE dataType)
{
    Http_Connection* c = getHttpConnection(connection);
    Http_Device* d = NULL;

    log_debug("Writing new value for the object \"%s\" at the server %s...",
              paramUri, c->serverUri);

    int paramFullUriLength = c->serverUriLength + strlen(paramUri) + 1;
    if (device != NULL)
    {	// need to add device uri also
        d = getHttpDevice(device);
        paramFullUriLength += d->uriLength;
    }


    char paramFullUri[paramFullUriLength];
    strcpy(paramFullUri, c->serverUri);
    if (device != NULL)
    {
        strcat(paramFullUri, d->uri);
    }
    strcat(paramFullUri, paramUri);
    return writeValue(paramFullUri, newValue, dataType, _curl_handle);
}
