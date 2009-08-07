/** @file
 * @todo add description there
 */
#include <stdlib.h>
#include <string.h>

#include <obix_utils.h>
#include <xml_config.h>
#include <lwl_ext.h>
#include "xml_storage.h"
#include "post_handler.h"
#include "watch.h"
#include "server.h"

#define LISTENSOCK_FILENO 0
#define LISTENSOCK_FLAGS 0

static const char* CT_SERVER_ADDRESS = "server-address";

static const char* OBIX_META_ATTR_OP = "op";

int obix_server_init(IXML_Element* settings)
{
    // get the server address from settings
    const char* servAddr = config_getTagAttributeValue(
                               config_getChildTag(settings, CT_SERVER_ADDRESS, TRUE),
                               CTA_VALUE,
                               TRUE);
    if (servAddr == NULL)
    {
        // no server address available - shut down
        return -1;
    }

    //initialize server storage
    int error = xmldb_init(servAddr);
    if (error != 0)
    {
        log_error("Unable to start the server. xmldb_init returned: %d", error);
        return -1;
    }

    // initialize Watch mechanism
    // TODO load max watch count from config file
    error = obixWatch_init();
    if (error != 0)
    {
        log_error("Unable to start the server. obixWatch_init returned: %d", error);
        return -1;
    }

    return 0;
}
//TODO create global function for memory allocation with error logging

void obix_server_generateObixErrorMessage(
    Response* response,
    const char* uri,
    const char* type,
    const char* name,
    const char* desc)
{
    IXML_Element* errorDOM = xmldb_getObixSysObject(OBIX_SYS_ERROR_STUB);
    if (errorDOM == NULL)
    {
        log_error("Unable to get error object from the storage.");
        // no changes to response
        return;
    }

    int error = 0;

    if (type != NULL)
    {
        error += ixmlElement_setAttributeWithLog(
                     errorDOM,
                     OBIX_ATTR_IS,
                     type);
    }
    error += ixmlElement_setAttributeWithLog(
                 errorDOM,
                 OBIX_ATTR_DISPLAY_NAME,
                 name);
    error += ixmlElement_setAttributeWithLog(
                 errorDOM,
                 OBIX_ATTR_DISPLAY,
                 desc);
    if (error != 0)
    {
        ixmlElement_freeOwnerDocument(errorDOM);
        log_error("Unable to generate oBIX error message: "
                  "Attributes modifying failed.");
        return;
    }

    obix_server_generateResponse(response,
                                 errorDOM,
                                 uri,
                                 TRUE,
                                 FALSE,
                                 0,
                                 TRUE);
    obixResponse_setErrorFlag(response, TRUE);

    ixmlElement_freeOwnerDocument(errorDOM);
}

void obix_server_read(Response* response, const char* uri)
{
    // try to get requested URI from the database
    int slashFlag = 0;
    IXML_Element* oBIXdoc = xmldb_getDOM(uri, &slashFlag);
    if (oBIXdoc == NULL)
    {
        log_warning("Requested URI \"%s\" is not found in the storage", uri);
        obix_server_generateObixErrorMessage(
            response,
            uri,
            OBIX_CONTRACT_ERR_BAD_URI,
            "Bad URI Error",
            "Requested URI is not found on the server.");
        return;
    }

    // if it is a Watch object than we should reset it's lease timer
    oBIX_Watch* watch = obixWatch_getByUri(uri);
    if (watch != NULL)
    {
        obixWatch_resetLeaseTimer(watch, OBIX_WATCH_LEASE_NO_CHANGE);
    }

    obix_server_generateResponse(response,
                                 oBIXdoc,
                                 uri,
                                 TRUE,
                                 TRUE,
                                 slashFlag,
                                 FALSE);
}

void obix_server_handleGET(Response* response, const char* uri)
{
    obix_server_read(response, uri);
    obixResponse_send(response);
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
    IXML_Element* meta = getMetaInfo(ixmlNode_convertToElement(node));
    if (meta != NULL)
    {
        obixWatch_updateMeta(meta);
    }
}

void obix_server_write(Response* response,
                       const char* uri,
                       IXML_Element* input)
{
    if (input == NULL)
    {
        log_warning("Unable to process write request. Wrong input: %s", input);
        obix_server_generateObixErrorMessage(response,
                                             uri,
                                             NULL,
                                             "Write Error",
                                             "Unable to read request input.");
        return;
    }

    // update node in the storage
    IXML_Element* element = NULL;
    int slashFlag = 0;
    int error = xmldb_updateDOM(input, uri, &element, &slashFlag);

    // parse error code
    switch(error)
    {
    case 0: //everything is ok
        // update meta tags for the object and its parents
        updateMetaWatch(ixmlElement_getNode(element));
    case 1: //ok, but new value is the same as the old one
        {
            // check whether it is request for overwriting Watch.lease value.
            error = obixWatch_processTimeUpdates(uri, element);
            if (error < 0)
            {
                // it was new value for some Watch parameter which failed to
                // be processed
                obix_server_generateObixErrorMessage(
                    response,
                    uri,
                    NULL,
                    "Write Error",
                    "Unable to update Watch parameter. Note: Value is updated "
                    "in storage, but did not affect the behavior of the Watch "
                    "object. That is a known issue. Please check that you have "
                    "provided correct reltime value and try again.");
                return;
            }

            // reply with the updated object
            obix_server_generateResponse(response,
                                         element,
                                         uri,
                                         TRUE,
                                         TRUE,
                                         slashFlag,
                                         FALSE);
        }
        break;
    case -1: // wrong format of the request
        obix_server_generateObixErrorMessage(response,
                                             uri,
                                             NULL,
                                             "Write Error",
                                             "Wrong format of the request.");
        break;
    case -2: // bad uri
        obix_server_generateObixErrorMessage(response,
                                             uri,
                                             OBIX_CONTRACT_ERR_BAD_URI,
                                             "Write Error",
                                             "URI is not found.");
        break;
    case -3: // object is not writable
        obix_server_generateObixErrorMessage(response,
                                             uri,
                                             OBIX_CONTRACT_ERR_PERMISSION,
                                             "Write Error",
                                             "Object is not writable.");
        break;
    case -4:
        obix_server_generateObixErrorMessage(response,
                                             uri,
                                             NULL,
                                             "Write Error",
                                             "Internal server error.");
    }
}

void obix_server_handlePUT(Response* response,
                           const char* uri,
                           const char* input)
{

    // parse request input
    IXML_Element* element = ixmlElement_parseBuffer(input);

    // process write request
    obix_server_write(response, uri, element);
    if (element != NULL)
    {
        ixmlElement_freeOwnerDocument(element);
    }

    // send response
    obixResponse_send(response);
    return;
}

void obix_server_invoke(Response* response,
                        const char* uri,
                        IXML_Element* input)
{
    // try to get requested URI from the database
    int slashFlag = 0;
    IXML_Element* oBIXdoc = xmldb_getDOM(uri, &slashFlag);
    if (oBIXdoc == NULL)
    {
        log_debug("Requested URI \"%s\" is not found in the storage.", uri);
        obix_server_generateObixErrorMessage(
            response,
            uri,
            OBIX_CONTRACT_ERR_BAD_URI,
            "Bad URI Error",
            "Requested URI is not found on the server.");
        obixResponse_send(response);
        return;
    }

    // check that the <op/> object is requested.
    if (strcmp(ixmlElement_getTagName(oBIXdoc), OBIX_OBJ_OP) != 0)
    {
        log_debug("Requested URI doesn't contain <op/> object");
        obix_server_generateObixErrorMessage(
            response, uri, OBIX_CONTRACT_ERR_BAD_URI,
            "Bad URI Error",
            "Requested URI is not an operation.");
        obixResponse_send(response);
        return;
    }

    // get the corresponding operation handler
    // by default we use 0 handler which returns error message
    int handlerId = 0;
    IXML_Element* meta = getMetaInfo(oBIXdoc);

    if (meta != NULL)
    {
        const char* handlerStr = ixmlElement_getAttribute(meta,
                                 OBIX_META_ATTR_OP);
        if (handlerStr != NULL)
        {
            handlerId = atoi(handlerStr);
        }
    }

    // and check whether we need to return also correct URI of the requested
    // operation
    if (slashFlag != 0)
    {
        obixResponse_setRightUri(response, uri, slashFlag);
    }

    obix_server_postHandler handler = obix_server_getPostHandler(handlerId);

    // execute corresponding request handler
    (*handler)(response, uri, input);
}

void obix_server_handlePOST(Response* response,
                            const char* uri,
                            const char* input)
{
    // prepare input object for the operation
    IXML_Element* opIn = ixmlElement_parseBuffer(input);

    obix_server_invoke(response, uri, opIn);

    if (opIn != NULL)
    {
        ixmlElement_freeOwnerDocument(opIn);
    }
}

void obix_server_shutdown()
{
    //TODO release post handlers;
    log_debug("Stopping oBIX server...");
    xmldb_dispose();
    obixWatch_dispose();
    config_dispose();
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

        // this check can be commented because all hrefs starting from server
        // root should have server address included during storing in database
        //        if (*origUri == '/')
        //        {
        //            // original URI is absolute so we can use it
        //            // instead of requested URI
        //            return xmldb_getFullUri(origUri, 0);
        //        }
    }

    if (origUri == NULL)
    {
        // no need to consider trailing '/' of original the URI
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
void obix_server_generateResponse(Response* response,
                                  IXML_Element* doc,
                                  const char* requestUri,
                                  BOOL changeUri,
                                  BOOL useObjectUri,
                                  int slashFlag,
                                  BOOL saveChanges)
{
    if (doc == NULL)
    {
        // some big error occurred on the previous step
        obixResponse_setError(
            response,
            "Request handler did not return any oBIX object.");
        return;
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

    BOOL responseIsHead = obixResponse_isHead(response);
    char* text = normalizeObixDocument(doc, fullUri, responseIsHead, saveChanges);
    if (text == NULL)
    {
        log_error("Unable to normalize the output oBIX document.");
        obixResponse_setError(response,
                              "Unable to normalize the output oBIX document.");
        return;
    }

    obixResponse_setText(response, text, FALSE);

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
}
