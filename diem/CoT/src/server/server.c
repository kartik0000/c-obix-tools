/** @file
 * @todo add description there
 */
#include <stdlib.h>
#include <obix_utils.h>
#include <xml_config.h>
#include <lwl_ext.h>
#include <ixml_ext.h>
#include "xml_storage.h"
#include "post_handler.h"
#include "watch.h"
#include "server.h"

#define LISTENSOCK_FILENO 0
#define LISTENSOCK_FLAGS 0

static const char* CONFIG_FILE = "server_config.xml";

const char* CT_SERVER_ADDRESS = "server-address";

static const char* OBIX_META_ATTR_OP = "op";

static int loadConfigFile()
{
    IXML_Element* settings = config_loadFile(CONFIG_FILE);

    if (settings == NULL)
    {
        // failed to load settings
        return -1;
    }

    // get the server address from settings
    const char* servAddr = config_getTagAttributeValue(
                               config_getChildTag(settings, CT_SERVER_ADDRESS, TRUE),
                               CTA_VALUE,
                               TRUE);
    if (servAddr == NULL)
    {
        // no server address available shut down
        config_finishInit(FALSE);
        return -1;
    }

    //initialize server storage
    int error = xmldb_init(servAddr);
    if (error != 0)
    {
        log_error("Unable to start the server. xmldb_init returned: %d", error);

        config_finishInit(FALSE);
        return -1;
    }

    // initialize Watch mechanism
    // TODO load max watch count from config file
    error = obixWatch_init();
    if (error != 0)
    {
        log_error("Unable to start the server. obixWatch_init returned: %d", error);

        config_finishInit(FALSE);
        return -1;
    }


    return 0;
}

int obix_server_init(char* resourceDir)
{
    setResourceDir(resourceDir);

    // load settings from file
    if (loadConfigFile() != 0)
    {
        // config failed
        return -1;
    }

    // finalize initialization
    config_finishInit(TRUE);

    return 0;
}
//TODO create global function for memory allocation with error logging

//void obix_server_sendObixMessage(FCGX_Request* request,
//                                 IXML_Element* doc,
//                                 const char* requestUri,
//                                 BOOL useObjectUri,
//                                 int slashFlag)
//{
//    if (doc == NULL)
//    {
//        // some big error occurred on the previous step
//        // TODO handle error
//        return;
//    }
//
//    // get the full URI
//    char* fullUri;
//    if (useObjectUri)
//    {
//        fullUri = normalizeUri(requestUri, doc, slashFlag);
//    }
//    else
//    {
//        fullUri = normalizeUri(requestUri, NULL, 0);
//    }
//
//    char* text = normalizeObixDocument(doc, fullUri, FALSE);
//    if (text == NULL)
//    {
//        log_error("Unable to normalize the oBIX document.");
//        // TODO: send error message
//        return;
//    }
//
//    log_debug("Replying with the following message:\n%s\n", text);
//    // send HTTP reply
//    FCGX_FPrintF(request->out, HTTP_STATUS_OK);
//    // check whether we should specify the correct address of the object
//    if (slashFlag != 0)
//    {
//        // if no URI is generated than we should take it from the object
//        if (fullUri == NULL)
//        {
//            FCGX_FPrintF(request->out,
//                         HTTP_CONTENT_LOCATION,
//                         ixmlElement_getAttribute(doc, OBIX_ATTR_HREF));
//        }
//        else // use generated URI
//        {
//            FCGX_FPrintF(request->out, HTTP_CONTENT_LOCATION, fullUri);
//        }
//    }
//    FCGX_FPrintF(request->out, XML_HEADER);
//    FCGX_FPrintF(request->out, text);
//    FCGX_Finish_r(request);
//
//    free(fullUri);
//    free(text);
//}

Response* obix_server_getObixErrorMessage(const char* uri,
        const char* type,
        const char* name,
        const char* desc)
{
    IXML_Element* errorDOM = xmldb_getObixSysObject(OBIX_SYS_ERROR_STUB);
    if (errorDOM == NULL)
    {
        log_error("Unable to get error object from the storage.");
        return obixResponse_create();
    }

    int error = 0;

    if (type != NULL)
    {
        error += ixmlElement_setAttributeWithLog(errorDOM, OBIX_ATTR_IS, type);
    }
    error += ixmlElement_setAttributeWithLog(errorDOM, OBIX_ATTR_DISPLAY_NAME, name);
    error += ixmlElement_setAttributeWithLog(errorDOM, OBIX_ATTR_DISPLAY, desc);
    if (error != 0)
    {
        ixmlElement_freeOwnerDocument(errorDOM);
        log_error("Unable to generate oBIX error message: modifying attributes failed.");
        return obixResponse_create();
    }

    Response* response = obix_server_generateResponse(
                             errorDOM,
                             uri,
                             TRUE, FALSE, 0,
                             TRUE,
                             TRUE);
    ixmlElement_freeOwnerDocument(errorDOM);
    return response;
}

Response* obix_server_handleGET(const char* uri)
{
    // try to get requested URI from the database
    IXML_Element* oBIXdoc = xmldb_getDOM(uri);
    int slashFlag = xmldb_getLastUriCompSlashFlag();
    if (oBIXdoc == NULL)
    {
        log_debug("Requested URI \"%s\" is not found in the storage", uri);
        return obix_server_getObixErrorMessage(uri, OBIX_HREF_ERR_BAD_URI,
                                               "Bad URI Error",
                                               "Requested URI is not found on the server.");
    }

    // if it is a Watch object than we should reset it's lease timer
    oBIX_Watch* watch = obixWatch_getByUri(uri);
    if (watch != NULL)
    {
        obixWatch_resetLeaseTimer(watch, NULL);
    }

    return obix_server_generateResponse(oBIXdoc, uri, TRUE, TRUE, slashFlag, TRUE, FALSE);
}

/**
 * Updates meta watch attributes recursively for the object and its parents.
 * All these attributes are set to the updated state. So that all subscribed
 * Watches could see that the value was updated.
 * @todo think about the whole conception of meta attributes (how to unify its
 * usage)
 */
static void updateMetaWatch(IXML_Node* node)
{
    // update parent node meta info if any
    IXML_Node* parent = ixmlNode_getParentNode(node);
    if (parent != NULL)
    {
        updateMetaWatch(parent);
    }

    // get meta tag
    IXML_Node* meta = ixmlElement_getNode(getMetaInfo(
                                              ixmlNode_convertToElement(node)));
    if (meta == NULL)
    {
        // object doesn't have any meta tag - ignore it
        return;
    }

    // iterate through all meta attributes, setting all
    // watch attributes to updated state.
    meta = ixmlNode_getFirstChild(meta);

    for ( ;meta != NULL; meta = ixmlNode_getNextSibling(meta))
    {
        IXML_Element* metaElement = ixmlNode_convertToElement(meta);
        if (metaElement == NULL)
        {
            // this piece of meta data is not an element - ignore it
            continue;
        }

        const char* value = ixmlElement_getAttribute(metaElement, OBIX_ATTR_VAL);
        // we compare only one char of the value so there is no need
        // to use strcmp()
        if ((value != NULL) && (*value == *OBIX_META_WATCH_UPDATED_NO))
        {
            int error = ixmlElement_setAttribute(metaElement,
                                                 OBIX_ATTR_VAL,
                                                 OBIX_META_WATCH_UPDATED_YES);
            if (error != IXML_SUCCESS)
            {
                log_error("Unable to update meta information. "
                          "Watches will not work properly.");
            }
        }
    }
}

Response* obix_server_handlePUT(const char* uri, const char* input)
{
    // TODO add permission checking here
    if (input == NULL)
    {
        log_warning("Unable to process PUT request. Input is empty.");
        return obix_server_getObixErrorMessage(uri,
                                               NULL,
                                               "Write Error",
                                               "Unable to read request input.");
    }

    // update node in the storage
    IXML_Element* element = NULL;
    int error = xmldb_update(input, uri, &element);
    int slashFlag = xmldb_getLastUriCompSlashFlag();

    if ((error != 0) || (element == NULL))
    {
        // TODO return different error types according to the error occurred.
        return obix_server_getObixErrorMessage(uri,
                                               OBIX_HREF_ERR_PERMISSION,
                                               "Write Error",
                                               "Updating object value failed.");
    }

    // update meta tags for the object and its parents
    updateMetaWatch(ixmlElement_getNode(element));

    // check whether it is request for overwriting Watch.lease value.
    oBIX_Watch* watch = obixWatch_getByUri(uri);
    if (watch != NULL)
    {
        // TODO checking that input has correct format should be done
        // for all input values. In that case it would be possible to
        // set new watch lease time before storing to database
        error = obixWatch_resetLeaseTimer(
                    watch,
                    ixmlElement_getAttribute(element, OBIX_ATTR_VAL));
        if (error != 0)
        {
            return obix_server_getObixErrorMessage(
                       uri,
                       NULL,
                       "Write Error",
                       "Unable to update Watch lease value. Lease value is "
                       "updated, but the real timeout is left unchanged. That "
                       "is known issue. Please check that you provided correct"
                       " reltime value and try again.");
        }
    }

    // reply with the updated object
    return obix_server_generateResponse(element, uri, TRUE, TRUE, slashFlag, TRUE, FALSE);
}

/**
 * @todo don't forget to clean the document
 * @param input
 * @return
 */
static IXML_Document* parseRequestInput(const char* input)
{
    // read and parse input
    if (input == NULL)
    {
        return NULL;
    }

    IXML_Document* doc;

    int error = ixmlParseBufferEx(input, &doc);
    if (error != IXML_SUCCESS)
    {
        log_warning("Unable to parse request input. Data is corrupted (error %d).\n"
                    "Data:\n%s\n", error, input);
        return NULL;
    }

    return doc;
}

/**
* @todo describe me
* @param uri
*/
Response* obix_server_handlePOST(const char* uri, const char* input)
{
    // try to get requested URI from the database
    IXML_Element* oBIXdoc = xmldb_getDOM(uri);
    if (oBIXdoc == NULL)
    {
        log_debug("Requested URI \"%s\" is not found in the storage.", uri);
        return obix_server_getObixErrorMessage(uri, OBIX_HREF_ERR_BAD_URI,
                                               "Bad URI Error",
                                               "Requested URI is not found on the server.");
    }
    //TODO move it to the handler
    //    int slashFlag = xmldb_getLastUriCompSlashFlag();

    // check that the <op/> object is requested.
    if (strcmp(ixmlElement_getTagName(oBIXdoc), OBIX_OBJ_OP) != 0)
    {
        log_debug("Requested URI doesn't contain <op/> object");
        return obix_server_getObixErrorMessage(uri, OBIX_HREF_ERR_BAD_URI,
                                               "Bad URI Error",
                                               "Requested URI is not an operation.");
    }

    // prepare input object for the operation
    IXML_Document* opIn = parseRequestInput(input);

    // get the corresponding operation handler
    // by default we use 0 handler which returns error message
    int handlerId = 0;
    IXML_Element* meta = getMetaInfo(oBIXdoc);

    if (meta != NULL)
    {
        const char* handlerStr = ixmlElement_getAttribute(meta, OBIX_META_ATTR_OP);
        if (handlerStr != NULL)
        {
            handlerId = atoi(handlerStr);
        }
    }

    obix_server_postHandler handler = obix_server_getPostHandler(handlerId);

    // execute corresponding request handler
    Response* response = (*handler)(uri, opIn);

    if (opIn != NULL)
    {
        ixmlDocument_free(opIn);
    }

    return response;
}

void obix_server_shutdown()
{
    //TODO release post handlers;
    log_debug("Stopping oBIX server...");
    xmldb_dispose();
    config_dispose();
    obixWatch_dispose();
}

static char* obix_server_getDump()
{
    return xmldb_getDump();
}

/**
 * Returns full URI corresponding to the request URI.
 * Adds server address to the requested URI and also adds/removes
 * ending slash ('/') if needed.
 *
 * @param requestUri requested URI. It should be absolute (show address from
 *                   the server root)
 * @param doc requested oBIX document. Object should contain @a href
 *            attribute. If NULL is provided than full URI is created only
 *            based on request URI.
 * @param slashFlag flag indicating difference in ending slash between
 *        requested URI and object's URI.
 * @return Full URI. If the original oBIX document already contains full URI
 *          then @a NULL will be returned. @note Don't forget to free memory
 *          after usage.
 */
static char* normalizeUri(const char* requestUri, IXML_Element* doc, int slashFlag)
{
    const char* origUri = NULL;

    if (doc != NULL)
    {
        origUri = ixmlElement_getAttribute(doc, OBIX_ATTR_HREF);
        if (origUri == NULL)
        {
            // it should never happen because oBIX document is found
            // in storage based on it's href attribute.
            log_error("Requested oBIX document doesn't have href attribute.");
            return NULL;
        }

        if (xmldb_compareServerAddr(origUri) == 0)
        {
            // original URI is already full URI. nothing to be done
            return NULL;
        }

        if (*origUri == '/')
        {
            // original URI is absolute so we can use it
            // instead of requested URI
            return xmldb_getFullUri(origUri, 0);
        }
    }

    if (origUri == NULL)
    {
        // no need to consider trailing '/' of original URI
        // so simply add server address to the request URI
        return xmldb_getFullUri(requestUri, 0);
    }

    // original URI is local one: we should use request URI
    // considering differences in trailing slash
    return xmldb_getFullUri(requestUri, slashFlag);
}

/**
 * Adds standard attributes to the parent node of the document and
 * returns string representation.
 * - Modifies 'href' attribute to contain full URI including server address
 * - Adds following attributes:
 *    - xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
 *    - xsi:schemaLocation="http://obix.org/ns/schema/1.0"
 *    - xmlns="http://obix.org/ns/schema/1.0"
 * - Removes @a handler attribute from @a <op/> nodes.
 *
 * @param oBIXdoc Pointer to the root node of the oBIX document.
 * @param fullUri URI containing address starting from the root of the server.
 * @param addXmlns If true than xmlns attributes are added to the parent tag.
 * @param saveChanges If TRUE saves changes in the original DOM structure,
 *                    otherwise modifies a copy.
 * @return string representation of the document. <b>Don't forget</b> to free
 *         memory after usage.
 */
static char* normalizeObixDocument(IXML_Element* oBIXdoc,
                                   char* fullUri,
                                   BOOL addXmlns,
                                   BOOL saveChanges)
{
    if (!saveChanges)
    {
        oBIXdoc = ixmlElement_cloneWithLog(oBIXdoc);
        if (oBIXdoc == NULL)
        {
            log_error("Unable to normalize oBIX document.");
            return NULL;
        }
    }

    int error = 0;
    if (fullUri != NULL)
    {
        // update URI if provided
        error = ixmlElement_setAttributeWithLog(oBIXdoc, OBIX_ATTR_HREF, fullUri);
    }

    if (addXmlns)
    {   // TODO move it to constants?
        error += ixmlElement_setAttributeWithLog(oBIXdoc, "xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
        error += ixmlElement_setAttributeWithLog(oBIXdoc, "xsi:schemaLocation", "http://obix.org/ns/schema/1.0");
        error += ixmlElement_setAttributeWithLog(oBIXdoc, "xmlns", "http://obix.org/ns/schema/1.0");
    }
    if (error != 0)
    {
        log_error("Unable to normalize oBIX object.");
        return NULL;
    }

    removeMetaInfo(oBIXdoc);

    char* text = ixmlPrintNode(ixmlElement_getNode(oBIXdoc));

    if (!saveChanges)
    {
        // free the cloned DOM structure
        ixmlElement_freeOwnerDocument(oBIXdoc);
    }

    return text;
}

// TODO refactor me to reduce the number of parameters
Response* obix_server_generateResponse(IXML_Element* doc,
                                       const char* requestUri,
                                       BOOL changeUri,
                                       BOOL useObjectUri,
                                       int slashFlag,
                                       BOOL isMultipart,
                                       BOOL saveChanges)
{
    Response* response = obixResponse_create();
    if (response == NULL)
    {
        return NULL;
    }

    if (doc == NULL)
    {
        // some big error occurred on the previous step
        obixResponse_setError(response, "Request handler did not return any oBIX object.");
        return response;
    }


    // get the full URI
    char* fullUri;
    if (!changeUri)
    {	// use provided URI with no changes
        fullUri = ixmlCloneDOMString(requestUri);
    }
    else
    {	// normalize URI
        if (useObjectUri)
        {
            fullUri = normalizeUri(requestUri, doc, slashFlag);
        }
        else
        {
            fullUri = normalizeUri(requestUri, NULL, 0);
        }
    }

    char* text = normalizeObixDocument(doc, fullUri, !isMultipart, saveChanges);
    if (text == NULL)
    {
        log_error("Unable to normalize the output oBIX document.");
        obixResponse_setError(response, "Unable to normalize the output oBIX document.");
        return response;
    }

    response->body = text;

    if (slashFlag != 0)
    {
        // if no URI is generated than we should take it from the object
        if (fullUri == NULL)
        {
            response->uri = ixmlCloneDOMString(
                                ixmlElement_getAttribute(doc, OBIX_ATTR_HREF));
        }
        else // use generated URI
        {
            response->uri = ixmlCloneDOMString(fullUri);
        }
    }
    free(fullUri);

    return response;
}

Response* obix_server_dumpEnvironment(FCGX_Request* request)
{
    log_debug("Starting dump environment...");

    char** envp;
    char* buffer;
    int bufferSize = 256;
    Response* response = obixResponse_create();
    if (response == NULL)
    {
        return NULL;
    }

    if (request != NULL)
    {
        for (envp = request->envp; *envp; ++envp)
        {
            bufferSize += strlen(*envp) + 32;
        }
    }

    log_debug("Allocating %d bytes for debug.", bufferSize);
    buffer = (char*) malloc(bufferSize);
    if (buffer == NULL)
    {
        // can't dump environment - return empty response
        return response;
    }

    strcpy(buffer, "<obj name=\"dump\" displayName=\"Server Dump\">\r\n"
           "  <obj name=\"env\" displayName=\"Request Environment\">\r\n");

    if (request != NULL)
    {
        for (envp = request->envp; *envp; ++envp)
        {
            strcat(buffer, "    <str val=\"");
            strcat(buffer, *envp);
            strcat(buffer, "\"/>\r\n");
        }
    }
    strcat(buffer, "</obj>\r\n");
    strcat(buffer, "  <obj name=\"storage\" displayName=\"Storage Dump\">\r\n");

    response->body = buffer;

    Response* nextPart = obixResponse_create();
    if (nextPart == NULL)
    {
        log_error("Unable to create multipart response. Answer is not complete.");
        return response;
    }
    response->next = nextPart;

    // retreive server storage dump
    char* storageDump = obix_server_getDump();
    if (storageDump != NULL)
    {
        nextPart->body = storageDump;
        nextPart->next = obixResponse_create();
        if (nextPart->next == NULL)
        {
            log_error("Unable to create multipart response. Answer is not complete.");
            return response;
        }
        nextPart = nextPart->next;
    }

    // finalize output
    if (obixResponse_setText(nextPart, "\r\n  </obj>\r\n</obj>") != 0)
    {
        log_error("Unable to create multipart response. Answer is not complete.");
        return response;
    }

    log_debug("Dump request completed.");
    return response;
}

