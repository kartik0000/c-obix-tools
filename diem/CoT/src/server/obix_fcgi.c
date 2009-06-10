// gcc -I/usr/include/fastcgi -lfcgi testfastcgi.c -o test.fastcgi
#include <stdlib.h>

#include <lwl_ext.h>
#include <obix_utils.h>
#include <ptask.h>
#include "xml_storage.h"
#include "server.h"
#include "obix_fcgi.h"

#define LISTENSOCK_FILENO 0
#define LISTENSOCK_FLAGS 0

/** Standard header of server answer*/
static const char* HTTP_STATUS_OK = "Status: 200 OK\r\n"
                                    "Content-Type: text/xml\r\n";

static const char* HTTP_CONTENT_LOCATION = "Content-Location: %s\r\n";
//TODO may be move it to obix_def?
static const char* XML_HEADER = "\r\n<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n";
// TODO think about stylesheet
//                               "<?xml-stylesheet type=\'text/xsl\' href=\'/obix/xsl\'?>\r\n";
/**
 * Header of error answer. Note that according oBIX
 * specification, oBIX error messages should still have
 * Status OK header
 */
//static const char* HTTP_STATUS_ERROR = "Status: 200 OK\r\nContent-Type: text/xml\r\n\r\n";

static const char* ERROR_STATIC = "<err displayName=\"Internal Server Error\" "
                                  "display=\"Unable to process the request. "
                                  "This is a static error message which is "
                                  "returned when things go really bad./>\"";

/**
 * Entry point of FCGI script.
 */
int main(int argc, char** argv)
{
    FCGX_Request* request;

    log_debug("Starting oBIX server...");
    char* resourceDir = NULL;

    if (argc >= 2)
    {
        resourceDir = argv[1];
    }
    else
    {
        log_warning("No resource folder provided. Trying use current directory.\n"
                    "Launch string: \"<path>/obix.fcgi <resource_folder/>\".");
        resourceDir = "./";
    }

    // init server
    if (obix_fcgi_init(&request, resourceDir) != 0)
    {
        // initialization failed
        obix_fcgi_shutdown(request);
        return -1;
    }

    // main loop
    while (1)
    {
        log_debug("Waiting for the request..");
        int error = FCGX_Accept_r(request);
        if (error)
        {
            log_warning("Stopping the server: FCGX_Accept_r returned %d", error);
            break;
        }

        const char* clientAddr = FCGX_GetParam("REMOTE_ADDR", request->envp);
        log_debug("Request accepted from \"%s\"", clientAddr);
        obix_fcgi_handleRequest(request);
        log_debug("Request handled");
    }

    // shut down
    obix_fcgi_shutdown(request);
    return 0;
}

int obix_fcgi_init(FCGX_Request** request, char* resourceDir)
{
    // call before Accept in multithreaded apps
    int error = FCGX_Init();
    if (error)
    {
        log_error("Unable to start the server. FCGX_Init returned: %d", error);
        return -1;
    }

    // init request object
    *request = (FCGX_Request*) malloc(sizeof(FCGX_Request));
    error = FCGX_InitRequest(*request, LISTENSOCK_FILENO, LISTENSOCK_FLAGS);
    if (error)
    {
        log_error("Unable to start the server. FCGX_InitRequest failed: %d", error);
        return -1;
    }

    // register callback for handling responses
    obix_server_setResponseListener(&obix_fcgi_sendResponse);

    return obix_server_init(resourceDir);
}

void obix_fcgi_shutdown(FCGX_Request* request)
{
    obix_server_shutdown();

    if (request != NULL)
    {
        FCGX_Free(request, TRUE);
    }
    FCGX_ShutdownPending();
}

void obix_fcgi_handleRequest(FCGX_Request* request)
{
    // get request URI
    const char* uri = FCGX_GetParam("REQUEST_URI", request->envp);
    if (uri == NULL)
    {
        log_error("Unable to retrieve URI from the request.");
        obix_fcgi_sendStaticErrorMessage(request);
        return;
    }
    // truncate uri if it contains server address
    if (xmldb_compareServerAddr(uri) == 0)
    {
        uri += xmldb_getServerAddressLength();
    }
    // check that uri is absolute
    if (*uri != '/')
    {
        log_error("Request URI \"%s\" has wrong format: "
                  "Should start with \'/\'.", uri);
        obix_fcgi_sendStaticErrorMessage(request);
        return;
    }

    // check the type of request
    const char* requestType = FCGX_GetParam("REQUEST_METHOD", request->envp);
    if (requestType == NULL)
    {
        log_error("Unable to get the request type.");
        obix_fcgi_sendStaticErrorMessage(request);
        return;
    }

    // prepare response object

    Response* response = obixResponse_create(request);
    if (response == NULL)
    {
        log_error("Unable to create response object: Not enough memory.");
        obix_fcgi_sendStaticErrorMessage(request);
        return;
    }

    // call corresponding request handler
    if (!strcmp(requestType, "GET"))
    {
        // handle GET request
        if (strcmp(uri, "/obix-dump/") == 0)
        {
            obix_fcgi_dumpEnvironment(response);
        }
        else
        {
            obix_server_handleGET(response, uri);
        }
    }
    else if (!strcmp(requestType, "PUT"))
    {
        // handle PUT request
        char* input = obix_fcgi_readRequestInput(request);
        obix_server_handlePUT(response, uri, input);
        if (input != NULL)
        {
            free(input);
        }
    }
    else if (!strcmp(requestType, "POST"))
    {
        // handle POST request
        char* input = obix_fcgi_readRequestInput(request);
        obix_server_handlePOST(response, uri, input);
        if (input != NULL)
        {
            free(input);
        }
    }
    else
    {
        //TODO fix me
        // unknown HTTP request
        log_error("Unknown request type: %s. Request is ignored.", requestType);
        char* message = (char*) malloc(strlen(requestType) + 42);
        sprintf(message, "%s request is not supported by oBIX server.", requestType);
        obix_server_generateObixErrorMessage(response,
                                        uri,
                                        OBIX_HREF_ERR_UNSUPPORTED,
                                        "Unsupported Request",
                                        message);
        free(message);
        obix_fcgi_sendResponse(response);
    }
}

void obix_fcgi_sendStaticErrorMessage(FCGX_Request* request)
{
    // send HTTP reply
    FCGX_FPrintF(request->out, HTTP_STATUS_OK);
    FCGX_FPrintF(request->out, XML_HEADER);
    FCGX_FPrintF(request->out, ERROR_STATIC);
    FCGX_Finish_r(request);
}

void obix_fcgi_sendResponse(Response* response)
{
    // prepare all parts of the response
    Response* iterator = response;

    while (iterator != NULL)
    {
        if (iterator->body == NULL)
        {
            log_error("Attempt to send empty response.");
            obixResponse_setError(iterator,
                                  "Request handler returned empty response.");
            // if even this operation fails
            if (iterator->body == NULL)
            {
                obix_fcgi_sendStaticErrorMessage(response->request);
                obixResponse_free(response);
                return;
            }
        }
        iterator = iterator->next;
    }

    FCGX_Request* request = response->request;

    // send HTTP header
    FCGX_FPrintF(request->out, HTTP_STATUS_OK);
    // check whether we should specify the correct address of the object
    if (response->uri != NULL)
    {
        FCGX_FPrintF(request->out, HTTP_CONTENT_LOCATION, response->uri);
    }
    FCGX_FPrintF(request->out, XML_HEADER);

    // send all parts of the response
    iterator = response;
    while (iterator != NULL)
    {
        FCGX_FPrintF(request->out, iterator->body);
        iterator = iterator->next;
    }

    FCGX_Finish_r(request);
    obixResponse_free(response);
}

char* obix_fcgi_readRequestInput(FCGX_Request* request)
{
    char* buffer = NULL;
    int bufferSize = 1024;
    int bytesRead = 0;
    int error;

    do
    {
        // we start from buffer 2KB
        // and than double it's size on every iteration
        bufferSize = bufferSize << 1;

        buffer = (char*) realloc(buffer, bufferSize);
        if (buffer == NULL)
        {
            log_error("Not enough memory to read the contents of the request.");
            return NULL;
        }

        bytesRead += FCGX_GetStr(buffer + bytesRead, bufferSize - bytesRead, request->in);

        if (bytesRead == 0)
        {
            //empty input
            free(buffer);
            return NULL;
        }

        error = FCGX_GetError(request->in);
        if (error != 0)
        {
            log_error("Error occurred while reading request input (code %d).", error);
            free(buffer);
            return NULL;
        }

        // repeat until buffer is big enough to store whole input
    }
    while(bytesRead == bufferSize);
    // finalize input string
    buffer[bytesRead] = '\0';
    log_debug("Received request input (size = %d):\n%s\n", bytesRead, buffer);

    return buffer;
}

void obix_fcgi_dumpEnvironment(Response* response)
{
    log_debug("Starting dump environment...");

    char** envp;
    char* buffer;
    int bufferSize = 256;

    if (response->request != NULL)
    {
        for (envp = response->request->envp; *envp; ++envp)
        {
            bufferSize += strlen(*envp) + 32;
        }
    }

    log_debug("Allocating %d bytes for debug.", bufferSize);
    buffer = (char*) malloc(bufferSize);
    if (buffer == NULL)
    {
        // can't dump environment - return empty response
        (*_responseListener)(response);
        return;
    }

    strcpy(buffer, "<obj name=\"dump\" displayName=\"Server Dump\">\r\n"
           "  <obj name=\"env\" displayName=\"Request Environment\">\r\n");

    if (response->request != NULL)
    {
        for (envp = response->request->envp; *envp; ++envp)
        {
            strcat(buffer, "    <str val=\"");
            strcat(buffer, *envp);
            strcat(buffer, "\"/>\r\n");
        }
    }
    strcat(buffer, "</obj>\r\n");
    strcat(buffer, "  <obj name=\"storage\" displayName=\"Storage Dump\">\r\n");

    obixResponse_setText(response, buffer, FALSE);

    Response* nextPart = obixResponse_add(response);
    if (nextPart == NULL)
    {
        log_error("Unable to create multipart response. Answer is not complete.");
        (*_responseListener)(response);
        return;
    }

    // retreive server storage dump
    char* storageDump = xmldb_getDump();
    if (storageDump != NULL)
    {
        nextPart->body = storageDump;
        obixResponse_add(nextPart);
        if (nextPart->next == NULL)
        {
            log_error("Unable to create multipart response. Answer is not complete.");
            (*_responseListener)(response);
            return;
        }
        nextPart = nextPart->next;
    }

    // finalize output
    if (obixResponse_setText(nextPart, "\r\n  </obj>\r\n</obj>", TRUE) != 0)
    {
        log_error("Unable to create multipart response. Answer is not complete.");
        (*_responseListener)(response);
        return;
    }

    log_debug("Dump request completed.");

    // send response
    (*_responseListener)(response);
}

