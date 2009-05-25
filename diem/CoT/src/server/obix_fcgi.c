// gcc -I/usr/include/fastcgi -lfcgi testfastcgi.c -o test.fastcgi
#include <stdlib.h>
#include <fcgiapp.h>

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

FCGX_Request* _request;

// TODO remove me
Task_Thread* _removeMe_Thread;

/**
 * Entry point of FCGI script.
 */
int main(int argc, char** argv)
{
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
    if (obix_fcgi_init(resourceDir) != 0)
    {
        // initialization failed
        obix_fcgi_shutdown();
        return -1;
    }

    // main loop
    while (1)
    {
        log_debug("Waiting for the request..");
        int error = FCGX_Accept_r(_request);
        if (error)
        {
            log_warning("Stopping the server: FCGX_Accept_r returned %d", error);
            break;
        }

        log_debug("Request accepted");
        Response* response = obix_fcgi_handleRequest();
        if (response == NULL)
        {
            obix_fcgi_sendStaticErrorMessage();
        }
        else
        {
            obix_fcgi_sendResponse(response);
        }
        log_debug("Request handled");
    }

    // shut down
    obix_fcgi_shutdown();
    return 0;
}

int obix_fcgi_init(char* resourceDir)
{
    // call before Accept in multithreaded apps
    int error = FCGX_Init();
    if (error)
    {
        log_error("Unable to start the server. FCGX_Init returned: %d", error);
        return -1;
    }

    // init request
    _request = (FCGX_Request*) malloc(sizeof(FCGX_Request));
    error = FCGX_InitRequest(_request, LISTENSOCK_FILENO, LISTENSOCK_FLAGS);
    if (error)
    {
        log_error("Unable to start the server. FCGX_InitRequest failed: %d", error);
        return -1;
    }

    _removeMe_Thread = ptask_init();

    return obix_server_init(resourceDir);
}

void obix_fcgi_shutdown()
{
    obix_server_shutdown();
    ptask_dispose(_removeMe_Thread);

    if (_request != NULL)
    {
        FCGX_Free(_request, TRUE);
    }
    FCGX_ShutdownPending();
}

Response* obix_fcgi_handleRequest()
{
    // get request URI
    const char* uri = FCGX_GetParam("REQUEST_URI", _request->envp);
    if (uri == NULL)
    {
        //TODO: handle error by returning static error message.
        log_error("Unable to retrieve URI from the request. Request is ignored.");
        return NULL;
    }
    // truncate uri if it contains server address
    if (xmldb_compareServerAddr(uri) == 0)
    {
        uri += xmldb_getServerAddressLength();
    }
    // check that uri is absolute
    if (*uri != '/')
    {
        //TODO: handle error somehow
        log_error("Request URI \"%s\" has wrong format: Should start with \'/\'.", uri);
        return NULL;
    }

    // check the type of request
    const char* requestType = FCGX_GetParam("REQUEST_METHOD", _request->envp);
    if (requestType == NULL)
    {
        //TODO: handle error somehow
        log_error("Unable to get the request type. Request is ignored.");
        return NULL;
    }

    // call corresponding request handler
    if (!strcmp(requestType, "GET"))
    {
        // handle GET request
        if (strcmp(uri, "/obix-dump/") == 0)
        {
            return obix_server_dumpEnvironment(_request);
        }
        else
        {
            return obix_server_handleGET(uri);
        }
    }
    else if (!strcmp(requestType, "PUT"))
    {
        // handle PUT request
        char* input = obix_fcgi_readRequestInput();
        Response* response = obix_server_handlePUT(uri, input);
        if (input != NULL)
        {
            free(input);
        }
        return response;
    }
    else if (!strcmp(requestType, "POST"))
    {
        // handle POST request
        char* input = obix_fcgi_readRequestInput();
        Response* response = obix_server_handlePOST(uri, input);
        if (input != NULL)
        {
            free(input);
        }
        return response;
    }
    else
    {
        //TODO fix me
        // unknown HTTP request
        log_error("Unknown request type: %s. Request is ignored.", requestType);
        char* message = (char*) malloc(strlen(requestType) + 42);
        sprintf(message, "%s request is not supported by oBIX server.", requestType);
        Response* response = obix_server_getObixErrorMessage(uri,
                             OBIX_HREF_ERR_UNSUPPORTED,
                             "Unsupported Request",
                             message);
        free(message);
        return response;
    }
}

void obix_fcgi_sendStaticErrorMessage()
{
    // send HTTP reply
    FCGX_FPrintF(_request->out, HTTP_STATUS_OK);
    FCGX_FPrintF(_request->out, XML_HEADER);
    FCGX_FPrintF(_request->out, ERROR_STATIC);
    FCGX_Finish_r(_request);
}

void obix_fcgi_sendResponse(Response* response)
{
    // prepare all parts of the response
    Response* iterator = response;

    while (iterator != NULL)
    {
        if (iterator->body == NULL)
        {
            obixResponse_setError(iterator, "Request handler returned empty response.");
            // if even this operation fails
            if (iterator->body == NULL)
            {
                obix_fcgi_sendStaticErrorMessage();
                obixResponse_free(iterator);
                return;
            }
        }
        iterator = iterator->next;
    }

    // send HTTP header
    FCGX_FPrintF(_request->out, HTTP_STATUS_OK);
    // check whether we should specify the correct address of the object
    if (response->uri != NULL)
    {
        FCGX_FPrintF(_request->out, HTTP_CONTENT_LOCATION, response->uri);
    }
    FCGX_FPrintF(_request->out, XML_HEADER);

    // send all parts of the response
    iterator = response;
    while (iterator != NULL)
    {
        FCGX_FPrintF(_request->out, iterator->body);
        iterator = iterator->next;
    }

    FCGX_Finish_r(_request);
    obixResponse_free(response);
}

char* obix_fcgi_readRequestInput()
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

        bytesRead += FCGX_GetStr(buffer + bytesRead, bufferSize - bytesRead, _request->in);

        if (bytesRead == 0)
        {
        	//empty input
        	free(buffer);
        	return NULL;
        }

        error = FCGX_GetError(_request->in);
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
