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
 * Implementation of HTTP communication layer.
 * Deals will all kind of oBIX over HTTP requests.
 * Encapsulates also the work with oBIX Watch objects.
 *
 * @see obix_http.h
 *
 * @author Andrey Litvinov
 */

#include <stdlib.h>
#include <string.h>
#include <ixml_ext.h>
#include <xml_config.h>
#include <log_utils.h>
#include <curl_ext.h>
#include <ptask.h>
#include <obix_utils.h>
#include <table.h>
// TODO obix_client.h is included only for error codes
#include "obix_client.h"
#include "obix_batch.h"
#include "obix_http.h"

/** Default poll interval for Watch objects. */
#define DEFAULT_POLLING_INTERVAL 500
/** Default difference between Watch poll interval and Watch.lease time. */
#define DEFAULT_WATCH_LEASE_PADDING 20000

/**
 * @name Templates of some oBIX objects, used in communication with the server.
 * These templates are represented as strings in order to improve processing
 * speed. There is also pre-calculated length for each template. This length
 * defines size of the static part of template. Thus the full size of
 * resulting object would be (static_length + length of all added strings).
 * @{
 */
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


#define OBIX_BATCH_TEMPLATE_HEADER "<list is=\"obix:BatchIn\" of=\"obix:uri\">\r\n"
#define OBIX_BATCH_TEMPLATE_HEADER_LENGTH 40
#define OBIX_BATCH_TEMPLATE_FOOTER "</list>"
#define OBIX_BATCH_TEMPLATE_FOOTER_LENGTH 7
#define OBIX_BATCH_TEMPLATE_CMD_READ " <uri is=\"obix:Read\" val=\"%s\" />\r\n"
#define OBIX_BATCH_TEMPLATE_CMD_READ_LENGTH 32
#define OBIX_BATCH_TEMPLATE_CMD_WRITE (" <uri is=\"obix:Write\" val=\"%s\" >\r\n" \
									   "  <%s name=\"in\" val=\"%s\"/>\r\n" \
									   " </uri>\r\n")
#define OBIX_BATCH_TEMPLATE_CMD_WRITE_LENGTH 65
#define OBIX_BATCH_TEMPLATE_CMD_INVOKE (" <uri is=\"obix:Invoke\" val=\"%s\" >\r\n" \
									    "  %s\r\n" \
									    " </uri>\r\n")
#define OBIX_BATCH_TEMPLATE_CMD_INVOKE_LENGTH 46
/** @} */

/** Name of child object of WatchOut, which contains list of updates. */
static const char* OBIX_WATCH_OUT_VALUES = "values";

/** Name of the extended Watch contract. Watch object, which implements this
 * contract, supports long polling feature. */
static const char* OBIX_CONTRACT_LONG_POLL_WATCH = "LongPollWatch";

/**Names of SSL configuration tags (from settings file).*/
static const char* CT_SSL = "ssl";
static const char* CT_SSL_VERIFY_PEER = "verify-peer";
static const char* CT_SSL_VERIFY_HOST = "verify-host";
static const char* CT_SSL_CA_FILE = "ca-file";

/**
 * Defines HTTP communication stack.
 * @see Comm_Stack
 */
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
        &http_read,
        &http_readValue,
        &http_writeValue,
        &http_invoke,
        &http_sendBatch,
        &http_getServerAddress
    };

/** @name Names of tags and attributes in XML configuration file
 * @{ */
static const char* CT_SERVER_ADDRESS = "server-address";
static const char* CT_POLL_INTERVAL = "poll-interval";
static const char* CT_WATCH_LEASE = "watch-lease";
static const char* CT_LONG_POLL = "long-poll";
static const char* CT_LONG_POLL_MIN = "min-interval";
static const char* CT_LONG_POLL_MAX = "max-interval";
static const char* CTA_LOBBY = "lobby";
/** @} */

/** Global initialization flag. */
static BOOL _initialized;
/** Handle for all requests excluding Watch polling requests.*/
static CURL_EXT* _curl_handle;
/** Handle for Watch poll requests.
 * The reason for this is that used CURL handle is not thread safe, but Watch
 * objects are polling in the separate thread.
 */
static CURL_EXT* _curl_watch_handle;

/** Thread used for Watch polling cycle. */
static Task_Thread* _watchThread;

// definitions of asynchronous tasks implemented later in this file
void watchPollTaskResume(void* arg);
void watchPollTask(void* arg);

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

/**
 * Returns full URI of specified element.
 * Full URI is generated by adding URIs of parent objects.
 * @param full  If #TRUE, than returned URI should contain server address.
 * 				Otherwise, returned URI is relative to server root.
 *
 * @todo Think about moving ixmlElement_getFullHref to obix_utils
 */
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
                      "Resulting URI should start with either \"/\" or \"%s\", "
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

/** Returns URI of oBIX object with specified name. */
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
        log_error("Unable to retrieve full URI for the object \"%s\" from the "
                  "server response. Make sure that the server is replying with "
                  "the same address in \'href\' attributes which is used to "
                  "contact it (%s).",
                  objName, c->serverUri);
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
    if (c->watchAddOperationUri != NULL)
    {
        free(c->watchAddOperationUri);
        c->watchAddOperationUri = NULL;
    }
    if (c->watchOperationResponseUri != NULL)
    {
        free(c->watchOperationResponseUri);
        c->watchOperationResponseUri = NULL;
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

/**
 * Generates string representation of WatchIn object.
 *
 * @param paramUri List of URIs which should be added to WatchIn object.
 * @param count Number of elements in @a paramUri list.
 * @return Generated WatchIn object, or @a NULL if error occurred.
 */
static char* getStrWatchIn(const char** paramUri, int count)
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

/** Performs write request to oBIX server.
 * @return #OBIX_SUCCESS, or one of error codes defined by #OBIX_ERRORCODE.
 */
static int writeValue(const char* paramUri,
                      const char* newValue,
                      OBIX_DATA_TYPE dataType,
                      CURL_EXT* curlHandle)
{
    // generate request body
    const char* objName = obix_getDataTypeName(dataType);
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
        return OBIX_ERR_SERVER_ERROR;
    }

    return OBIX_SUCCESS;
}


/** Checks whether response message from the server is error object. */
static int checkResponseElement(IXML_Element* element)
{
    if (strcmp(ixmlElement_getTagName(element), OBIX_OBJ_ERR) == 0)
    {
        char* text = ixmlPrintNode(ixmlElement_getNode(element));
        log_error("Server replied with error:\n"
                  "%s", text);
        free(text);
        return OBIX_ERR_SERVER_ERROR;
    }

    return OBIX_SUCCESS;
}

/** Checks parsed response for following errors:
 * @li Response is not @a NULL;
 * @li Response contains some object;
 * @li Response object is not error.
 *
 * @param respDoc   Parsed response, which should be checked.
 * @param respElem  Reference to the root object of the response is returned
 * 					here.
 * @return #OBIX_SUCCESS, or one of error codes defined by #OBIX_ERRORCODE.
 */
static int checkResponseDoc(IXML_Document* respDoc, IXML_Element** respElem)
{
    if (respDoc == NULL)
    {
        return OBIX_ERR_BAD_CONNECTION;
    }

    IXML_Element* element = ixmlDocument_getRootElement(respDoc);
    if (element == NULL)
    {
        char* text = ixmlPrintDocument(respDoc);
        log_error("Server response doesn't contain any oBIX objects:\n"
                  "%s", text);
        free(text);
        return OBIX_ERR_BAD_CONNECTION;
    }
    if (respElem != NULL)
    {
        *respElem = element;
    }

    return checkResponseElement(element);
}

/**
 * Performs actual request to the server for adding new watch items.
 *
 * @param paramUri  List of URIs which should be added to Watch object at the
 * 					server.
 * @param count Number of elements in @a paramUri list.
 * @param isOperation Tells whether @a paramUri contains URI(s) of operation
 * 					object(s) or not. If yes, than @a Watch.addOperation is used
 * 					instead of @a Watch.add.
 * @return #OBIX_SUCCESS, or one of error codes defined by #OBIX_ERRORCODE.
 */
static int addWatchItems(Http_Connection* c,
                         const char** paramUri,
                         int count,
                         BOOL isOperation,
                         IXML_Document** response,
                         CURL_EXT* curlHandle)
{
    const char* watchAddUri =
        isOperation ? c->watchAddOperationUri : c->watchAddUri;
    char fullWatchAddUri[c->serverUriLength + strlen(watchAddUri) + 1];
    strcpy(fullWatchAddUri, c->serverUri);
    strcat(fullWatchAddUri, watchAddUri);
    char *requestBody = getStrWatchIn(paramUri, count);

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

    return checkResponseDoc(*response, NULL);
}

/**
 * Helper function, which sets @a reltime parameter of Watch object at oBIX
 * server. Before doing so, it first retrieves URI of that parameter from Watch
 * object.
 */
static int setWatchTimeParam(Http_Connection* c,
                             CURL_EXT* curlHandle,
                             IXML_Document* watchXml,
                             const char* paramName,
                             long paramValue)
{
    char* paramUri = getObjectUri(watchXml, paramName, c, TRUE);
    if (paramUri == NULL)
    {
        log_warning("watchService.make operation at server \"%s\" returned "
                    "Watch object without \'%s\' tag:\n%s", c->serverUri,
                    paramName, curlHandle->inputBuffer);
        free(paramUri);
        return OBIX_ERR_BAD_CONNECTION;
    }

    char* leaseTime = obix_reltime_fromLong(paramValue, RELTIME_SEC);
    if (leaseTime == NULL)
    {
        // this should never happen!
        log_error("Unable to convert time from long (%ld) to obix:reltime!",
                  paramValue);
        free(paramUri);
        return OBIX_ERR_UNKNOWN_BUG;
    }

    int error = writeValue(paramUri,
                           leaseTime,
                           OBIX_T_RELTIME,
                           curlHandle);
    free(paramUri);
    free(leaseTime);
    return error;
}

/**
 * Sets long polling parameters (if available) of Watch object at oBIX server.
 */
static int setWatchPollWaitTime(Http_Connection* c,
                                CURL_EXT* curlHandle,
                                IXML_Document* watchXml)
{
    if (c->pollWaitMax == 0)
    {
        // nothing to be done: wait interval is zero by default
        return OBIX_SUCCESS;
    }
    // check whether server supports long polling (Watch object should implement
    // LongPollWatch contract)
    BOOL longPollSupported = obix_obj_implementsContract(
                                 ixmlDocument_getRootElement(watchXml),
                                 OBIX_CONTRACT_LONG_POLL_WATCH);
    if (!longPollSupported)
    {
        // server doesn't support long polling - switch to the traditional
        c->pollWaitMin = 0;
        c->pollWaitMax = 0;
        log_warning("Server doesn't support long polling feature. Switching to "
                    "the traditional polling.");
        return OBIX_SUCCESS;
    }

    int error = setWatchTimeParam(c,
                                  curlHandle,
                                  watchXml,
                                  OBIX_NAME_WATCH_POLL_WAIT_INTERVAL_MAX,
                                  c->pollWaitMax);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    error = setWatchTimeParam(c,
                              curlHandle,
                              watchXml,
                              OBIX_NAME_WATCH_POLL_WAIT_INTERVAL_MIN,
                              c->pollWaitMin);
    return error;
}

/** Sets Watch.lease time at oBIX server. */
static int setWatchLeaseTime(Http_Connection* c,
                             CURL_EXT* curlHandle,
                             IXML_Document* watchXml)
{
    int error = setWatchTimeParam(c,
                                  curlHandle,
                                  watchXml,
                                  OBIX_NAME_WATCH_LEASE,
                                  c->watchLease);
    if (error == OBIX_ERR_SERVER_ERROR)
    {
        // server could return error just because it prevents changes of
        // Watch.lease.
        // TODO it's not very safe - lease can remain smaller than poll interval
        return OBIX_SUCCESS;
    }

    return error;
}

static void deleteWatchFromServer(Http_Connection* c)
{
    char watchDeleteFullUri[c->serverUriLength
                            + strlen(c->watchDeleteUri) + 1];
    strcpy(watchDeleteFullUri, c->serverUri);
    strcat(watchDeleteFullUri, c->watchDeleteUri);
    _curl_handle->outputBuffer = NULL;
    IXML_Document* response;
    int error = curl_ext_postDOM(_curl_handle,
                                 watchDeleteFullUri,
                                 &response);
    if (error != 0)
    {
        log_error("Unable to delete Watch from the server %s.",
                  watchDeleteFullUri);
        return;
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
        IXML_Element* obixObj = NULL;
        error = checkResponseDoc(response, &obixObj);
        if (error != OBIX_SUCCESS)
        {
            // check if the response contain Bad Uri error which means
            // that Watch object was already removed and thus we can ignore this
            // error
            if (obix_obj_implementsContract(obixObj, OBIX_CONTRACT_ERR_BAD_URI))
            {
                log_warning("The Watch object is already deleted at the "
                            "server. Probably server has dropped it because "
                            "lease time of the object is less than poll "
                            "interval.");
            }
            else
            {
                // some unknown error
                log_error("Unknown error happened while trying to delete Watch "
                          "object (%s) from the server.", watchDeleteFullUri);
            }

        }

        ixmlDocument_free(response);
    }

    return;
}

/** Removes Watch object from the server and also resets corresponding local
 * settings. */
static int removeWatch(Http_Connection* c)
{
    if (table_getCount(c->watchTable) > 0)
    {
        log_warning("Deleting not empty watch object from the oBIX server. "
                    "Some subscribed listeners can stop receiving updates.");
    }

    // change poll task properties so that it will be executed only once
    int error = ptask_reschedule(_watchThread, c->watchPollTaskId, 0, 1, TRUE);
    if (error != 0)
    {
        log_error("Unable to cancel Watch Poll Task:"
                  "ptask_reschedule() returned %d", error);
        return error;
    }

    deleteWatchFromServer(c);

    // stop polling task and wait for it if it is executing right now
    // ignore error - we just want make sure that the task is canceled
    // but it can be canceled earlier
    ptask_cancel(_watchThread, c->watchPollTaskId, TRUE);

    // reset all Watch related variables, because they are no longer valid
    pthread_mutex_lock(&(c->watchMutex));
    resetWatchUris(c);
    pthread_mutex_unlock(&(c->watchMutex));

    return OBIX_SUCCESS;
}

/** Creates Watch object at oBIX server. */
static int createWatch(Http_Connection* c, CURL_EXT* curlHandle)
{
    char* watchAddUri = NULL;
    char* watchAddOperationUri = NULL;
    char* watchOperationResponseUri = NULL;
    char* watchRemoveUri = NULL;
    char* watchPollChangesFullUri = NULL;
    char* watchDeteleUri = NULL;
    IXML_Document* response = NULL;

    // helper function for memory cleaning on errors
    void cleanup()
    {
        if (watchAddUri != NULL)
            free(watchAddUri);
        if (watchAddOperationUri)
            free(watchAddOperationUri);
        if (watchOperationResponseUri)
            free(watchOperationResponseUri);
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
    watchAddOperationUri =
        getObjectUri(response, OBIX_NAME_WATCH_ADD_OPERATION, c, FALSE);
    watchOperationResponseUri =
        getObjectUri(response, OBIX_NAME_WATCH_OPERATION_RESPONSE, c, FALSE);
    watchRemoveUri = getObjectUri(response, OBIX_NAME_WATCH_REMOVE, c, FALSE);
    watchDeteleUri = getObjectUri(response, OBIX_NAME_WATCH_DELETE, c, FALSE);
    // we store complete URI of pollChanges operation because we use it more
    // often than others.
    watchPollChangesFullUri = getObjectUri(response, OBIX_NAME_WATCH_POLLCHANGES, c, TRUE);
    if ((watchAddUri == NULL)
            || (watchAddOperationUri == NULL)
            || (watchOperationResponseUri == NULL)
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

    // try to set watch lease time
    int error = setWatchLeaseTime(c, curlHandle, response);
    if (error != OBIX_SUCCESS)
    {
        cleanup();
        return error;
    }

    error = setWatchPollWaitTime(c, curlHandle, response);
    ixmlDocument_free(response);
    response = NULL;
    if (error != OBIX_SUCCESS)
    {
        cleanup();
        return error;
    }

    // store all received URIs
    c->watchAddUri = watchAddUri;
    c->watchAddOperationUri = watchAddOperationUri;
    c->watchOperationResponseUri = watchOperationResponseUri;
    c->watchRemoveUri = watchRemoveUri;
    c->watchDeleteUri = watchDeteleUri;
    c->watchPollChangesFullUri = watchPollChangesFullUri;
    return OBIX_SUCCESS;
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
    const char** uris;
    int count = table_getKeys(c->watchTable, &uris);
    const void** tableValues;
    table_getValues(c->watchTable, &tableValues);
    // separate tableValues variable is created in order to bypass
    // strict-aliasing warning from compiler
    const Listener** listeners = (const Listener**) tableValues;

    // divide all URIs into two groups: URIs of operations and URIs of other
    // objects
    int i;
    int operationsCount = 0;
    int variablesCount = 0;
    const char* operationUris[count];
    const char* variableUris[count];
    for (i = 0; i < count; i++)
    {
        if (listeners[i]->opHandler == NULL)
        {
            // this is classic variable listener
            variableUris[variablesCount++] = uris[i];
        }
        else
        {
            // this is operation listener
            operationUris[operationsCount++] = uris[i];
        }
    }

    // add to Watch separately operations and all other objects.
    if (operationsCount > 0)
    {
        error = addWatchItems(c,
                              operationUris,
                              operationsCount,
                              TRUE,
                              response,
                              curlHandle);
        if (error != OBIX_SUCCESS)
        {
            log_error("Unable to restore Watch items at the server. "
                      "Creation of Watch object failed.");
            deleteWatchFromServer(c);
            return error;
        }
        // we still want to make sure that WatchOut object doesn't contain
        // error objects. If so, than creation of Watch object failed.
        if (ixmlDocument_getElementById(*response, OBIX_OBJ_ERR) != NULL)
        {
            char* buffer = ixmlPrintDocument(*response);
            log_error("WatchOut object contains errors. Creation of Watch "
                      "object failed.\n%s", buffer);
            free(buffer);
            deleteWatchFromServer(c);
            return OBIX_ERR_SERVER_ERROR;
        }
        ixmlDocument_free(*response);
    }

    if (variablesCount > 0)
    {
        error = addWatchItems(c,
                              variableUris,
                              variablesCount,
                              FALSE,
                              response,
                              curlHandle);
        if (error != OBIX_SUCCESS)
        {
            log_error("Unable to restore Watch items at the server. "
                      "Creation of Watch object failed.");
            deleteWatchFromServer(c);
            return error;
        }
        // we still want to make sure that WatchOut object doesn't contain
        // error objects. If so, than creation of Watch object failed.
        if (ixmlDocument_getElementById(*response, OBIX_OBJ_ERR) != NULL)
        {
            char* buffer = ixmlPrintDocument(*response);
            log_error("WatchOut object contains errors. Creation of Watch "
                      "object failed.\n%s", buffer);
            free(buffer);
            deleteWatchFromServer(c);
            return OBIX_ERR_SERVER_ERROR;
        }
    }

    if (error == OBIX_SUCCESS)
    {
        log_warning("Looks like we have successfully recovered Watch object!");
    }
    return error;
}

/**
 * Returns string representation of OperationResponse object containing
 * output of processed operation.
 * @param operationInvocation Invocation request received from the server.
 * @param operationOuput Output returned by operation handler.
 */
static char* prepareOperationResponse(IXML_Element* operationInvocation,
                                      IXML_Element* operationOutput)
{
    // helper function which generates static error message
    char* getStaticResponseMessage(const char* outputTag)
    {
        const char* href =
            ixmlElement_getAttribute(operationInvocation, OBIX_ATTR_HREF);
        log_debug("Generating static OperationResponse object for operation "
                  "\"%s\". Returning object: %s", href, outputTag);

        char* buffer = (char*) malloc(strlen(href) + strlen(outputTag) + 60);
        if (buffer == NULL)
        {
            log_error("Not enough memory for default error message.");
            return NULL;
        }
        sprintf(buffer,
                "<op href=\"%s\" is=\"/obix/def/OperationResponse\" >\r\n"
                "  %s\r\n"
                "</op>", href, outputTag);
        return buffer;
    }

    // take parent element from invocation object
    IXML_Element* operationResponse =
        ixmlElement_cloneWithLog(operationInvocation, FALSE);
    if (operationResponse == NULL)
    {
        return getStaticResponseMessage(
                   "<err name=\"out\" display=\"Internal oBIX Client library "
                   "error: Unable to prepare OperationResponse object.\" />");
    }

    if (operationOutput == NULL)
    {	// operation handler did not returned anything
        return getStaticResponseMessage("<obj name=\"out\" null=\"true\" />");
    }

    IXML_Element* childTag;
    // append operation output as a child
    int error = ixmlElement_putChildWithLog(operationResponse,
                                            operationOutput,
                                            &childTag);
    if (error != 0)
    {
        ixmlElement_freeOwnerDocument(operationResponse);
        return getStaticResponseMessage(
                   "<err name=\"out\" display=\"Internal oBIX Client library "
                   "error: Unable to prepare OperationResponse object.\" />");
    }

    //change attributes to conform with OperationResponse contract
    error = ixmlElement_setAttributeWithLog(operationResponse,
                                            OBIX_ATTR_IS,
                                            "/obix/def/OperationResponse");
    error += ixmlElement_setAttributeWithLog(
                 childTag,
                 OBIX_ATTR_NAME,
                 "out");
    if (error != 0)
    {
        ixmlElement_freeOwnerDocument(operationResponse);
        return getStaticResponseMessage(
                   "<err name=\"out\" display=\"Internal oBIX Client library "
                   "error: Unable to prepare OperationResponse object.\" />");
    }

    char* message = ixmlPrintNode(ixmlElement_getNode(operationResponse));
    ixmlElement_freeOwnerDocument(operationResponse);
    if (message == NULL)
    {
        return getStaticResponseMessage(
                   "<err name=\"out\" display=\"Internal oBIX Client library "
                   "error: Unable to prepare OperationResponse object.\" />");
    }

    return message;
}

/**
 * Sends output of remote operation back to the server.
 */
static int sendOperationResponse(Http_Connection* c,
                                 IXML_Element* operationInvocation,
                                 IXML_Element* operationOutput,
                                 CURL_EXT* curlHandle)
{
    int uriLength =
        c->serverUriLength + strlen(c->watchOperationResponseUri) + 1;
    char fullWatchOperationResponseUri[uriLength];
    strcpy(fullWatchOperationResponseUri, c->serverUri);
    strcat(fullWatchOperationResponseUri, c->watchOperationResponseUri);

    // prepare message. It should be instance of OperationResponse contract
    char* message =
        prepareOperationResponse(operationInvocation, operationOutput);
    if (message == NULL)
    {	// error is already logged
        return OBIX_ERR_NO_MEMORY;
    }

    IXML_Document* serverResponse = NULL;
    curlHandle->outputBuffer = message;
    int error = curl_ext_postDOM(curlHandle,
                                 fullWatchOperationResponseUri,
                                 &serverResponse);
    free(message);
    if (error != 0)
    {
        return OBIX_ERR_BAD_CONNECTION;
    }

    error = checkResponseDoc(serverResponse, NULL);
    if (error != OBIX_SUCCESS)
    {
        char* buffer = ixmlPrintDocument(serverResponse);
        log_error("Unable to send operation response to the server using "
                  "\"%s\" operation. Received answer:\n%s",
                  fullWatchOperationResponseUri, buffer);
        free(buffer);
    }
    ixmlDocument_free(serverResponse);
    return error;
}

/**
 * Checks that provided object implements OperationInvocation contract and
 * returns a reference to the "input" object.
 */
static IXML_Element* parseOperationInvocation(IXML_Element* operationInvocation)
{
    // helper function for printing errors
    void printErrorMessage(const char* message)
    {
        char* buffer = ixmlPrintNode(ixmlElement_getNode(operationInvocation));
        log_error("%s Received object:\n%s", buffer);
        free(buffer);
    }

    if (!obix_obj_implementsContract(operationInvocation,
                                     "OperationInvocation"))
    {
        printErrorMessage("Unable to process remote operation invocation. "
                          "An instance of OperationInvocation contract is "
                          "expected.");
        return NULL;
    }

    IXML_Element* operationInput =
        ixmlElement_getChildElementByAttrValue(operationInvocation,
                                               OBIX_ATTR_NAME,
                                               "in");
    if (operationInput == NULL)
    {
        printErrorMessage("Unable to process remote operation invocation. "
                          "Input object does not contain child element with "
                          "name \"in\".");
        return NULL;
    }

    return operationInput;
}

/**
 * Handles remote operation invocation received from the server.
 * Executes corresponding operation handler and sends execution results back to
 * the server
 * @param operationInvocation Object from WatchOut list sent by server, which
 * 							  implements OperationInvocation contract.
 */
static int handleRemoteOperation(Http_Connection* c,
                                 Listener* listener,
                                 IXML_Element* operationInvocation,
                                 CURL_EXT* curlHandle)
{
    if (listener->opHandler == NULL)
    {
        log_error("Missing handler reference. This should never happen. "
                  "Connection #%d, device #%d, listener #%d, uri \"%s\".",
                  listener->connectionId, listener->deviceId, listener->id,
                  listener->paramUri);
        return OBIX_ERR_UNKNOWN_BUG;
    }

    IXML_Element* input = parseOperationInvocation(operationInvocation);

    IXML_Element* output = (listener->opHandler)(listener->connectionId,
                           listener->deviceId,
                           listener->id,
                           input);

    return sendOperationResponse(c, operationInvocation, output, curlHandle);
}

/**
 * Executes parameter listener callback.
 * @param element Object from WatchOut list containing update of monitored
 *                parameter.
 * @return Execution result of the listener. Should be @a 0 if everything was
 * 		   OK.
 */
static int callParamListener(IXML_Element* element, Listener* listener)
{
    // if there is 'val' attribute in the returned object - return it,
    // otherwise, return the whole object
    // TODO fixme somehow
    char* receivedValue;
    const char* attrValue = ixmlElement_getAttribute(element, OBIX_ATTR_VAL);

    if (attrValue != NULL)
    {	// copy value
        receivedValue = ixmlCloneDOMString(attrValue);
    }
    else
    {	// return the whole object
        receivedValue = ixmlPrintNode(ixmlElement_getNode(element));
    }

    int result = (listener->paramListener)(listener->connectionId,
                                           listener->deviceId,
                                           listener->id,
                                           receivedValue);
    ixmlFreeDOMString(receivedValue);

    return result;
}

/**
 * Parses WatchOut object returned by server.
 * If it contains some updates of subscribed values, the function invokes
 * listener of that value. In case when server's reply contains error instead of
 * WatchOut object, the function tries to recreate Watch object at the server.
 *
 * @param doc Parsed WatchOut object.
 */
static int parseWatchOut(IXML_Document* doc,
                         Http_Connection* c,
                         CURL_EXT* curlHandle)
{
    IXML_Element* element = NULL;
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
            // Still error.. WatchOut object is wrong
            char* buffer = ixmlPrintDocument(doc);
            log_error("WatchOut object has wrong format:\n%s", buffer);
            free(buffer);
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
            retVal = OBIX_ERR_SERVER_ERROR;
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
        // workaround for some oBIX server implementations which return full
        // URI of the updated object, but it should be the same as passed to
        // Watch.add operation.
        if (strncmp(c->serverUri, uri, c->serverUriLength) == 0)
        {
            uri += c->serverUriLength;
        }

        // find corresponding listener of the object
        Listener* listener = (Listener*) table_get(c->watchTable, uri);
        if (listener == NULL)
        {
            log_error("Unable to find listener for the object with URI \"%s\".", uri);
            retVal = OBIX_ERR_BAD_CONNECTION;
        }

        // execute callback function
        if (listener->paramListener != NULL)
        {
            callParamListener(element, listener);
        }
        else
        {
            handleRemoteOperation(c, listener, element, curlHandle);
        }
        // TODO check results
    }

    return retVal;
}

static void resetWatchPollErrorCount(Http_Connection* c)
{
    c->watchPollErrorCount = 0;
}

static void handleWatchPollError(int error, Http_Connection* c)
{
    log_error("Watch Poll Task: "
              "Error occurred while parsing WatchOut object (error %d).",
              error);

    c->watchPollErrorCount++;
    if (c->watchPollErrorCount >= 3)
    {
        log_error("Last 3 poll requests to %s failed. Probably connection with "
                  "the server is lost. "
                  "Server polling will be resumed after 1 minute.",
                  c->serverUri);

        if (ptask_cancel(_watchThread, c->watchPollTaskId, FALSE) != 0)
        {
            log_error("Unable to delete Watch poll task.");
            return;
        }
        int ptaskId =
            ptask_schedule(_watchThread, &watchPollTaskResume, c, 15000, 1);
        if (ptaskId < 0)
        {
            log_error("Internal error: Unable to schedule new Watch poll task. "
                      "Watch polling will not be restarted, i.e. client will "
                      "not receive any new updates!");
            return;
        }
    }

}

static int checkWatchPollResponse(Http_Connection* c,
                                  IXML_Document** serverResponse,
                                  CURL_EXT* curlHandle)
{
    IXML_Element* element = NULL;
    int error = checkResponseDoc(*serverResponse, &element);
    if (error == OBIX_ERR_SERVER_ERROR)
    {
        // we received error object instead of WatchOut.
        // in case if it is BadUri than try to create new Watch object.
        if (!obix_obj_implementsContract(element, OBIX_CONTRACT_ERR_BAD_URI))
        {
            // it is some strange error. no idea what to do with it
            return OBIX_ERR_BAD_CONNECTION;
        }

        // it is badUri error which indicates (most probably it should :) that
        // the Watch object was unexpectedly deleted from oBIX server
        // (unfortunately it can happen :). Try to create a new Watch.
        log_warning("It seems like Watch object doesn't exist on the oBIX "
                    "server anymore.");
        // delete old response
        ixmlDocument_free(*serverResponse);
        *serverResponse = NULL;
        error = recreateWatch(c, serverResponse, curlHandle);
    }

    return error;
}

static int scheduleWatchPollTask(Http_Connection* c)
{
    resetWatchPollErrorCount(c);
    long pollInterval = (c->pollWaitMax == 0) ? c->pollInterval : 0;
    int ptaskId = ptask_schedule(_watchThread, &watchPollTask, c,
                                 pollInterval, EXECUTE_INDEFINITE);
    if (ptaskId < 0)
    {
        log_error("Unable to schedule Watch Poll Task: Not enough memory.");
        return OBIX_ERR_NO_MEMORY;
    }
    c->watchPollTaskId = ptaskId;
    return OBIX_SUCCESS;
}

void watchPollTaskResume(void* arg)
{
    Http_Connection* c = (Http_Connection*) arg;

    int error = scheduleWatchPollTask(c);
    if (error != OBIX_SUCCESS)
    {
        log_error("Watch Poll Task is not scheduled! No updates from the "
                  "server will be received!");
    }
}

/**
 * Calls Watch.pollChanges at the server and handles response.
 */
void watchPollTask(void* arg)
{
    Http_Connection* c = (Http_Connection*) arg;

    pthread_mutex_lock(&(c->watchMutex));
    if (c->watchPollChangesFullUri == NULL)
    {
        log_error("Watch Poll Task: Someone deleted Watch object but did not "
                  "cancel the poll task.");
        if (ptask_cancel(_watchThread, c->watchPollTaskId, FALSE) != 0)
        {
            log_error("Watch Poll Task: Unable to delete myself.");
        }
        pthread_mutex_unlock(&(c->watchMutex));
        return;
    }

    // we copy URI so that we could release mutex before sending HTTP requests
    // if we don't copy URI than someone can be lucky enough to delete watchUri
    // string before curl_ext_postDOM. We can't hold mutex during request
    // because long poll requests can take considerable amount of time.
    char watchPollChangesUri[strlen(c->watchPollChangesFullUri) + 1];
    strcpy(watchPollChangesUri, c->watchPollChangesFullUri);
    pthread_mutex_unlock(&(c->watchMutex));

    log_debug("requesting %s", watchPollChangesUri);
    IXML_Document* response;

    _curl_watch_handle->outputBuffer = NULL;
    int error = curl_ext_postDOM(_curl_watch_handle,
                                 watchPollChangesUri,
                                 &response);
    if (error != 0)
    {
        log_error("Watch Poll Task: "
                  "Unable to poll changes from the server %s.",
                  watchPollChangesUri);
        handleWatchPollError(OBIX_ERR_BAD_CONNECTION, c);
        return;
    }

    error = checkWatchPollResponse(c, &response, _curl_watch_handle);
    if (error != OBIX_SUCCESS)
    {
        if (response != NULL)
        {
            ixmlDocument_free(response);
        }
        handleWatchPollError(error, c);
        return;
    }

    error = parseWatchOut(response, c, _curl_watch_handle);
    if (error != OBIX_SUCCESS)
    {
        ixmlDocument_free(response);
        handleWatchPollError(error, c);
        return;
    }

    // everything was OK
    ixmlDocument_free(response);
    resetWatchPollErrorCount(c);
}

/** Adds provided listener to local database.
 * Also checks that Watch object at the server already exists. */
static int addListener(Http_Connection* c,
                       const char* paramUri,
                       Listener* listener)
{
    // save listener to the listeners table
    pthread_mutex_lock(&(c->watchMutex));
    table_put(c->watchTable, paramUri, listener);

    // check that we already have a watch object for this server
    if (c->watchAddUri == NULL)
    {
        // create new one
        int error = createWatch(c, _curl_handle);
        if (error != OBIX_SUCCESS)
        {
            pthread_mutex_unlock(&(c->watchMutex));
            return error;
        }

        // schedule new periodic task for watch polling
        error = scheduleWatchPollTask(c);
        if (error != OBIX_SUCCESS)
        {
            pthread_mutex_unlock(&(c->watchMutex));
            return error;
        }
    }
    pthread_mutex_unlock(&(c->watchMutex));

    return OBIX_SUCCESS;
}

/** Removes listener from the local database. If no more listeners left, then it
 * also will remove Watch object from the server (as nothing left to watch). */
static int removeListener(Http_Connection* c, const char* paramUri)
{
    // remove listener URI from the listeners table
    pthread_mutex_lock(&(c->watchMutex));
    table_remove(c->watchTable, paramUri);

    // remove Watch object  from the server completely
    // if there are no more items to watch
    int retVal = OBIX_SUCCESS;
    int watchItemCount = table_getCount(c->watchTable);
    pthread_mutex_unlock(&(c->watchMutex));

    if (watchItemCount == 0)
    {
        retVal = removeWatch(c);
    }

    return retVal;
}

/**
 * Combines device's URI and parameter's URI (one of them can be empty), thus
 * obtaining URI relative to the server root.
 */
static char* getRelUri(Device* device, const char* paramUri)
{
    Http_Device* d = NULL;

    int fullUriLength = 1;
    if (paramUri != NULL)
    {
        fullUriLength += strlen(paramUri);
    }
    if (device != NULL)
    {	// need to add device uri also
        d = getHttpDevice(device);
        fullUriLength += d->uriLength;
    }

    char* fullUri = (char*) malloc(fullUriLength);
    if (fullUri == NULL)
    {
        return NULL;
    }

    *fullUri = '\0';
    if (d != NULL)
    {
        strcat(fullUri, d->uri);
    }
    if (paramUri != NULL)
    {
        strcat(fullUri, paramUri);
    }

    return fullUri;
}

/**
 * Combines server's URI, device's URI and parameter's URI (one of the two last
 * can be empty), thus obtaining full URL.
 */
static char* getAbsUri(Http_Connection* connection,
                       Device* device,
                       const char* paramUri)
{
    Http_Device* d = NULL;

    int fullUriLength = connection->serverUriLength + 1;
    if (paramUri != NULL)
    {
        fullUriLength += strlen(paramUri);
    }
    if (device != NULL)
    {	// need to add device uri also
        d = getHttpDevice(device);
        fullUriLength += d->uriLength;
    }

    char* fullUri = (char*) malloc(fullUriLength);
    if (fullUri == NULL)
    {
        return NULL;
    }

    strcpy(fullUri, connection->serverUri);
    if (d != NULL)
    {
        strcat(fullUri, d->uri);
    }
    if (paramUri != NULL)
    {
        strcat(fullUri, paramUri);
    }

    return fullUri;
}

/**
 * Tries to retrieve object's value (i.e. value of 'val' attribute).
 *
 * @param output Reference to the copy of object's value is returned here.
 * @return #OBIX_SUCCESS, or one of error codes defined by #OBIX_ERRORCODE.
 */
static int parseElementValue(IXML_Element* element, char** output)
{
    const char* attr = ixmlElement_getAttribute(element, OBIX_ATTR_VAL);
    if (attr == NULL)
    {
        char* temp = ixmlPrintNode(ixmlElement_getNode(element));
        log_warning("Received object doesn't have \"%s\" attribute:\n%s",
                    OBIX_ATTR_VAL, temp);
        free(temp);
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    char* copy = (char*) malloc(strlen(attr) + 1);
    if (copy == NULL)
    {
        log_error("Unable to allocate enough memory.");
        return OBIX_ERR_NO_MEMORY;
    }
    strcpy(copy, attr);
    *output = copy;
    return OBIX_SUCCESS;
}

static int configureSSL(IXML_Element* settings)
{
    IXML_Element* sslTag = config_getChildTag(settings, CT_SSL, FALSE);
    if (sslTag == NULL)
    {
        log_debug("No SSL settings found (<%s> tag). "
                  "Leaving curl default settings.", CT_SSL);
        return OBIX_SUCCESS;
    }

    IXML_Element* tag = config_getChildTag(sslTag, CT_SSL_VERIFY_PEER, TRUE);
    if (tag == NULL)
    {
        log_error("Either remove <%s> tag completely or add "
                  "child boolean tag <%s>.", CT_SSL, CT_SSL_VERIFY_PEER);
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    int verifyHost = 0;
    const char* caFile = NULL;
    int verifyPeer = config_getTagAttrBoolValue(tag, OBIX_ATTR_VAL, TRUE);
    if (verifyPeer < 0)
    {
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    if (verifyPeer == TRUE)
    {
        tag = config_getChildTag(sslTag, CT_SSL_VERIFY_HOST, TRUE);
        if (tag != NULL)
        {
            verifyHost = config_getTagAttrBoolValue(tag, OBIX_ATTR_VAL, TRUE);
        }

        caFile = config_getChildTagValue(sslTag, CT_SSL_CA_FILE, FALSE);
    }

    // now set parsed settings
    int error = curl_ext_setSSL(_curl_handle, verifyPeer, verifyHost, caFile);
    error +=
        curl_ext_setSSL(_curl_watch_handle, verifyPeer, verifyHost, caFile);

    if (error != 0)
    {
        return OBIX_ERR_UNKNOWN_BUG;
    }

    return OBIX_SUCCESS;
}

int http_init(IXML_Element* settings)
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

    error = configureSSL(settings);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    // uncomment this to get lot's of debug log from CURL
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

int http_dispose()
{
    int retVal = 0;
    if (_initialized)
    {
        // destroy curl handles
        curl_ext_free(_curl_handle);
        curl_ext_free(_curl_watch_handle);
        // stop curl library
        curl_ext_dispose();
        // stop Periodic Task thread
        retVal = ptask_dispose(_watchThread, TRUE);
    }

    _initialized = FALSE;
    return (retVal == 0) ? OBIX_SUCCESS : OBIX_ERR_UNKNOWN_BUG;
}

int http_initConnection(IXML_Element* connItem, Connection** connection)
{
    char* serverUri = NULL;
    char* lobbyUri = NULL;
    Http_Connection* c;
    Table* table = NULL;
    long pollInterval = DEFAULT_POLLING_INTERVAL;
    long watchLease;
    long pollWaitMin = 0;
    long pollWaitMax = 0;

    // helper function for releasing resources on error
    void cleanup()
    {
        if (serverUri != NULL)
            free(serverUri);
        if (lobbyUri != NULL)
            free(lobbyUri);
        if (table != NULL)
            free(table);
    }

    // load server address
    IXML_Element* element = config_getChildTag(connItem,
                            CT_SERVER_ADDRESS,
                            TRUE);
    if (element == NULL)
    {
        return OBIX_ERR_INVALID_ARGUMENT;
    }
    const char* attrValue = config_getTagAttributeValue(element,
                            CTA_VALUE,
                            TRUE);
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
        pollInterval = config_getTagAttrLongValue(
                           element,
                           CTA_VALUE,
                           FALSE,
                           DEFAULT_POLLING_INTERVAL);
    }
    // load long poll intervals which are also optional
    element = config_getChildTag(connItem, CT_LONG_POLL, FALSE);
    if (element != NULL)
    {
        IXML_Element* childTag = config_getChildTag(element,
                                 CT_LONG_POLL_MIN,
                                 TRUE);
        if (childTag == NULL)
        {
            log_error("Configuration tag <%s/> should have correct child tags "
                      "<%s/> and <%s/>.",
                      CT_LONG_POLL, CT_LONG_POLL_MIN, CT_LONG_POLL_MAX);
            cleanup();
            return OBIX_ERR_INVALID_ARGUMENT;
        }
        pollWaitMin = config_getTagAttrLongValue(childTag,
                      OBIX_ATTR_VAL,
                      TRUE,
                      0);
        childTag = config_getChildTag(element,
                                      CT_LONG_POLL_MAX,
                                      TRUE);
        if (childTag == NULL)
        {
            log_error("Configuration tag <%s/> should have correct child tags "
                      "<%s/> and <%s/>.",
                      CT_LONG_POLL, CT_LONG_POLL_MIN, CT_LONG_POLL_MAX);
            cleanup();
            return OBIX_ERR_INVALID_ARGUMENT;
        }
        pollWaitMax = config_getTagAttrLongValue(childTag,
                      OBIX_ATTR_VAL,
                      TRUE,
                      0);
        if ((pollWaitMin < 0) || (pollWaitMax < 0))
        {
            log_error("Configuration tag <%s/> should have correct child tags "
                      "<%s/> and <%s/>.",
                      CT_LONG_POLL, CT_LONG_POLL_MIN, CT_LONG_POLL_MAX);
            cleanup();
            return OBIX_ERR_INVALID_ARGUMENT;
        }
    }

    // load watch lease time, this is also optional parameter
    // by default it is pollInterval (or pollWaitMax) + padding
    watchLease = (pollInterval > pollWaitMax) ? pollInterval : pollWaitMax;
    watchLease += DEFAULT_WATCH_LEASE_PADDING;
    element = config_getChildTag(connItem, CT_WATCH_LEASE, FALSE);
    if (element != NULL)
    {
        watchLease = config_getTagAttrLongValue(
                         element,
                         CTA_VALUE,
                         FALSE,
                         watchLease);
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

    if (pthread_mutex_init(&(c->watchMutex), NULL) != 0)
    {
        log_error("Unable to initialize HTTP connection: Unable to create mutex.");
        cleanup();
        return OBIX_ERR_HTTP_LIB;
    }

    c->watchTable = table;

    c->serverUri = serverUri;
    c->serverUriLength = serverUriLength;
    c->lobbyUri = lobbyUri;
    c->pollInterval = pollInterval;
    c->watchLease = watchLease;
    c->pollWaitMin = pollWaitMin;
    c->pollWaitMax = pollWaitMax;

    // initialize other values with zeros
    c->signUpUri = NULL;
    c->batchUri = NULL;
    c->watchMakeUri = NULL;
    c->watchAddUri = NULL;
    c->watchAddOperationUri = NULL;
    c->watchOperationResponseUri = NULL;
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
    if (c->batchUri != NULL)
        free(c->batchUri);
    if (c->watchMakeUri != NULL)
        free(c->watchMakeUri);
    resetWatchUris(c);
    pthread_mutex_destroy(&(c->watchMutex));
    if (c->watchTable != 0)
    {
        table_free(c->watchTable);
    }
}

int http_openConnection(Connection* connection)
{
    char* signUpUri = NULL;
    char* watchServiceUri = NULL;
    char* batchUri = NULL;
    IXML_Document* response = NULL;

    // helper function for releasing resources on error
    void cleanup()
    {
        if (signUpUri != NULL)
            free(signUpUri);
        if (watchServiceUri != NULL)
            free(watchServiceUri);
        if (batchUri != NULL)
            free(batchUri);
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
    int error = checkResponseDoc(response, NULL);
    if (error != OBIX_SUCCESS)
    {
        log_error("Unable to get Lobby object from the oBIX server \"%s\".",
                  lobbyFullUri);
        cleanup();
        return error;
    }

    // we need to save links to watchService.make and signUp operations.
    signUpUri = getObjectUri(response,
                             OBIX_NAME_SIGN_UP,
                             c, FALSE);
    batchUri = getObjectUri(response,
                            OBIX_NAME_BATCH,
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
    error = checkResponseDoc(response, NULL);
    if (error != OBIX_SUCCESS)
    {
        log_error("Unable to get watchService object from the oBIX server "
                  "\"%s\".", watchServiceUri);
        cleanup();
        return error;
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
    c->batchUri = batchUri;
    c->watchMakeUri = watchServiceUri;
    return OBIX_SUCCESS;
}

int http_closeConnection(Connection* connection)
{
    Http_Connection* c = getHttpConnection(connection);
    int retVal = OBIX_SUCCESS;

    log_debug("Closing connection to the server %s...", c->serverUri);
    // remove Watch and polling task if they were not removed earlier
    if (c->watchDeleteUri != NULL)
    {
        retVal = removeWatch(c);
    }

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
    IXML_Document* response = NULL;
    int error = curl_ext_postDOM(_curl_handle, signUpFullUri, &response);
    if (error != 0 || response == NULL)
    {
        log_error("Unable to register device using service at \"%s\".",
                  signUpFullUri);
        return OBIX_ERR_BAD_CONNECTION;
    }

    // check response
    IXML_Element* element;
    error = checkResponseDoc(response, &element);
    if (error != OBIX_SUCCESS)
    {
        if (error != OBIX_ERR_SERVER_ERROR)
        {
            ixmlDocument_free(response);
            return OBIX_ERR_BAD_CONNECTION;
        }
        // Server returned error object.
        // TODO that is a workaround which works only with oBIX server
        // from same project. The problem is that when driver is restarted, it
        // appears that data already exists on the server. In that case error
        // message from server would contain URI of the existing data which we
        // will try to use.
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
        int error = checkResponseDoc(response, &element);
        if (error != OBIX_SUCCESS)
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
                    "that the object with provided URI already exists. Trying "
                    "to proceed with object with URI \"%s\".", uri);
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

int http_registerListener(Connection* connection,
                          Device* device,
                          Listener** listener)
{
    Http_Connection* c = getHttpConnection(connection);

    // generate full URI for listening parameter
    char* fullParamUri = getRelUri(device, (*listener)->paramUri);
    if (fullParamUri == NULL)
    {
        log_error("Not enough memory.");
        return OBIX_ERR_NO_MEMORY;
    }

    log_debug("Registering listener of object \"%s\" at the server "
              "\"%s\"...", (*listener)->paramUri, c->serverUri);

    int error = addListener(c, fullParamUri, *listener);
    if(error != OBIX_SUCCESS)
    {
        return error;
    }

    // determine listener type
    BOOL isOperationHandler = ((*listener)->opHandler != NULL) ? TRUE : FALSE;

    IXML_Document* response = NULL;
    error = addWatchItems(c,
                          (const char**) (&fullParamUri),
                          1,
                          isOperationHandler,
                          &response,
                          _curl_handle);
    if (error != OBIX_SUCCESS)
    {
        removeListener(c, fullParamUri);
        free(fullParamUri);
        return error;
    }

    error = checkResponseDoc(response, NULL);

    if ((error == OBIX_SUCCESS) && ((*listener)->opHandler == NULL))
    {
        // for simple value listener we need also to parse current value
        error = parseWatchOut(response, c, _curl_handle);
    }

    ixmlDocument_free(response);
    if (error != OBIX_SUCCESS)
    {
        removeListener(c, fullParamUri);
    }
    free(fullParamUri);

    return error;
}

int http_unregisterListener(Connection* connection,
                            Device* device,
                            Listener* listener)
{
    Http_Connection* c = getHttpConnection(connection);

    log_debug("Removing listener of parameter \"%s\" at the server \"%s\"...",
              listener->paramUri, c->serverUri);

    char* fullParamUri = getRelUri(device, listener->paramUri);
    if (fullParamUri == NULL)
    {
        log_error("Not enough memory.");
        return OBIX_ERR_NO_MEMORY;
    }

    // remove generated URI from the Watch object at the oBIX server

    // get URI of Watch.remove operation
    char fullWatchRemoveUri[c->serverUriLength + strlen(c->watchRemoveUri) + 1];
    strcpy(fullWatchRemoveUri, c->serverUri);
    strcat(fullWatchRemoveUri, c->watchRemoveUri);

    // send request
    char* requestBody = getStrWatchIn((const char**) (&fullParamUri), 1);
    if (requestBody == NULL)
    {
        free(fullParamUri);
        log_error("Unable to unregister listener: Not enough memory.");
        return OBIX_ERR_NO_MEMORY;
    }
    _curl_handle->outputBuffer = requestBody;
    IXML_Document* response;
    int error = curl_ext_postDOM(_curl_handle, fullWatchRemoveUri, &response);
    free(requestBody);
    if (error != 0)
    {
        free(fullParamUri);
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
    {
        IXML_Element* obixObj = NULL;
        // check server response
        error = checkResponseDoc(response, &obixObj);
        if (error != OBIX_SUCCESS)
        {
            // if server replied with Bad Uri error, than we can consider
            // that watch item is removed (actually all Watch object is
            // dropped by someone)
            if (!obix_obj_implementsContract(obixObj, OBIX_CONTRACT_ERR_BAD_URI))
            {
                free(fullParamUri);
                ixmlDocument_free(response);
                return OBIX_ERR_BAD_CONNECTION;
            }
        }
        ixmlDocument_free(response);
    }

    error = removeListener(c, fullParamUri);
    free(fullParamUri);
    return error;
}

int http_read(Connection* connection,
              Device* device,
              const char* paramUri,
              IXML_Element** output)
{
    Http_Connection* c = getHttpConnection(connection);
    IXML_Document* response = NULL;

    char* fullUri = getAbsUri(c, device, paramUri);

    if (fullUri == NULL)
    {
        log_error("Not enough memory.");
        return OBIX_ERR_NO_MEMORY;
    }

    curl_ext_getDOM(_curl_handle, fullUri, &response);
    int error = checkResponseDoc(response, NULL);
    if (error != OBIX_SUCCESS)
    {
        log_error("Unable to get object \"%s\" from the oBIX server.",
                  fullUri);
        if (response != NULL)
        {
            ixmlDocument_free(response);
        }
        free(fullUri);
        return error;
    }

    // return the output object
    IXML_Element* element = ixmlDocument_getRootElement(response);
    if (element == NULL)
    {
        log_error("Response from the server (\"%s\") doesn't contain any XML "
                  "tags.", fullUri);
        if (response != NULL)
        {
            ixmlDocument_free(response);
        }
        free(fullUri);
        return OBIX_ERR_BAD_CONNECTION;
    }

    free(fullUri);
    *output = element;
    return OBIX_SUCCESS;
}

int http_readValue(Connection* connection,
                   Device* device,
                   const char* paramUri,
                   char** output)
{
    IXML_Element* element;
    int error = http_read(connection, device, paramUri, &element);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    error = parseElementValue(element, output);
    ixmlElement_freeOwnerDocument(element);
    return error;
}

int http_writeValue(Connection* connection,
                    Device* device,
                    const char* paramUri,
                    const char* newValue,
                    OBIX_DATA_TYPE dataType)
{
    Http_Connection* c = getHttpConnection(connection);
    char* fullUri = getAbsUri(c, device, paramUri);
    if (fullUri == NULL)
    {
        log_error("Not enough memory.");
        return OBIX_ERR_NO_MEMORY;
    }

    log_debug("Performing write operation...");
    int error = writeValue(fullUri, newValue, dataType, _curl_handle);
    free(fullUri);
    return error;
}

int http_invoke(Connection* connection,
                Device* device,
                const char* operationUri,
                const char* input,
                char** output)
{
    Http_Connection* c = getHttpConnection(connection);

    char* fullUri = getAbsUri(c, device, operationUri);

    if (fullUri == NULL)
    {
        log_error("Not enough memory.");
        return OBIX_ERR_NO_MEMORY;
    }

    _curl_handle->outputBuffer = input;
    int error = curl_ext_post(_curl_handle, fullUri);
    free(fullUri);
    if (error != 0)
    {
        log_error("Unable to send invoke request.");
        return OBIX_ERR_HTTP_LIB;
    }
    *output = strdup(_curl_handle->inputBuffer);
    if (*output == NULL)
    {
        log_error("Not enough memory.");
        return OBIX_ERR_NO_MEMORY;
    }

    return OBIX_SUCCESS;
}

/** Generates string representation of Batch object including all commands
 * it contains. */
static char* getStrBatch(oBIX_Batch* batch)
{
    // start with forming correct URI's and calculating size of the final
    // message
    char* uri[batch->commandCounter];

    // helper function to free memory
    void cleanup()
    {
        // free all generated URIs
        int i;
        for (i = 0; i < batch->commandCounter; i++)
        {
            if (uri[i] != NULL)
            {
                free(uri[i]);
            }
        }
    }

    int batchMessageSize = OBIX_BATCH_TEMPLATE_HEADER_LENGTH +
                           OBIX_BATCH_TEMPLATE_FOOTER_LENGTH + 1;

    int i;
    for (i = 0; i < batch->commandCounter; i++)
    {
        uri[i] = NULL;
    }

    oBIX_BatchCmd* command = batch->command;
    while (command != NULL)
    {
        char* fullUri = getRelUri(command->device, command->uri);
        if (fullUri == NULL)
        {
            cleanup();
            return NULL;
        }
        uri[command->id] = fullUri;

        // calculate length of this command
        batchMessageSize += strlen(fullUri);
        switch (command->type)
        {
        case OBIX_BATCH_WRITE_VALUE:
            batchMessageSize +=
                OBIX_BATCH_TEMPLATE_CMD_WRITE_LENGTH +
                strlen(obix_getDataTypeName(command->dataType)) +
                strlen(command->input);
            break;
        case OBIX_BATCH_READ:
        case OBIX_BATCH_READ_VALUE:
            batchMessageSize += OBIX_BATCH_TEMPLATE_CMD_READ_LENGTH;
            break;
        case OBIX_BATCH_INVOKE:
            batchMessageSize +=
                OBIX_BATCH_TEMPLATE_CMD_INVOKE_LENGTH +
                strlen((command->input != NULL) ?
                       command->input : OBIX_OBJ_NULL_TEMPLATE);
            break;
        }

        command = command->next;
    }

    char* batchMessage = (char*) malloc(batchMessageSize);
    if (batchMessage == NULL)
    {
        cleanup();
        return NULL;
    }

    // print batch header
    strcpy(batchMessage, OBIX_BATCH_TEMPLATE_HEADER);
    int size = OBIX_BATCH_TEMPLATE_HEADER_LENGTH;

    // print commands
    command = batch->command;
    while (command != NULL)
    {
        switch (command->type)
        {
        case OBIX_BATCH_WRITE_VALUE:
            size += sprintf(batchMessage + size,
                            OBIX_BATCH_TEMPLATE_CMD_WRITE,
                            uri[command->id],
                            obix_getDataTypeName(command->dataType),
                            command->input);
            break;
        case OBIX_BATCH_READ:
        case OBIX_BATCH_READ_VALUE:
            size += sprintf(batchMessage + size,
                            OBIX_BATCH_TEMPLATE_CMD_READ,
                            uri[command->id]);
            break;
        case OBIX_BATCH_INVOKE:
            size += sprintf(batchMessage + size,
                            OBIX_BATCH_TEMPLATE_CMD_INVOKE,
                            uri[command->id],
                            (command->input != NULL) ?
                            command->input : OBIX_OBJ_NULL_TEMPLATE);
            break;
        }
        free(uri[command->id]);
        command = command->next;
    }

    // print batch footer
    strcpy(batchMessage + size, OBIX_BATCH_TEMPLATE_FOOTER);
    return batchMessage;
}

int http_sendBatch(oBIX_Batch* batch)
{
    // generate batch request
    char* requestBody = getStrBatch(batch);
    if (requestBody == NULL)
    {
        log_error("Unable to generate the Batch object: "
                  "Not enough memory.");
        return OBIX_ERR_NO_MEMORY;
    }

    // get full URI of batch operation
    Http_Connection* c = getHttpConnection(batch->connection);
    char fullBatchUri[c->serverUriLength + strlen(c->batchUri) + 1];
    strcpy(fullBatchUri, c->serverUri);
    strcat(fullBatchUri, c->batchUri);

    // send the batch request
    _curl_handle->outputBuffer = requestBody;
    IXML_Document* response;
    int error = curl_ext_postDOM(_curl_handle, fullBatchUri, &response);
    free(requestBody);
    if (error != 0)
    {
        return OBIX_ERR_BAD_CONNECTION;
    }

    // parse response message
    IXML_Element* list;
    error = checkResponseDoc(response, &list);
    if (error != OBIX_SUCCESS)
    {
        ixmlDocument_free(response);
        return error;
    }

    IXML_Node* node = ixmlNode_getFirstChild(ixmlElement_getNode(list));
    oBIX_BatchCmd* command = batch->command;

    while (node != NULL)
    {
        IXML_Element* commandResponse = ixmlNode_convertToElement(node);

        if (commandResponse != NULL)
        {
            // find corresponding result object
            oBIX_BatchResult* result = &(batch->result[command->id]);

            // check that response doesn't contain error messages
            result->status = checkResponseElement(commandResponse);
            if (result->status == OBIX_SUCCESS)
            {
                // fill other result fields depending on command type
                switch (command->type)
                {
                case OBIX_BATCH_READ:
                    {
                        // save a copy of returned object
                        result->obj =
                            ixmlElement_cloneWithLog(commandResponse, TRUE);
                        if (result->obj == NULL)
                        {
                            result->status = OBIX_ERR_UNKNOWN_BUG;
                        }
                    }
                    break;
                case OBIX_BATCH_READ_VALUE:
                    {
                        // save returned value
                        result->status = parseElementValue(commandResponse,
                                                           &(result->value));
                    }
                    break;
                case OBIX_BATCH_INVOKE:
                    {
                        // save a copy of returned object
                        result->obj =
                            ixmlElement_cloneWithLog(commandResponse, TRUE);
                        if (result->obj == NULL)
                        {
                            result->status = OBIX_ERR_UNKNOWN_BUG;
                        }

                        // save character form of it
                        result->value =
                            ixmlPrintNode(ixmlElement_getNode(commandResponse));
                    }
                    break;
                default:
                    break;
                }
            }

            // switch to next command
            command = command->next;
        }
        // switch to next command response
        node = ixmlNode_getNextSibling(node);
    }
    ixmlDocument_free(response);

    return OBIX_SUCCESS;
}

const char* http_getServerAddress(Connection* connection)
{
    Http_Connection* c = getHttpConnection(connection);
    return c->serverUri;
}
