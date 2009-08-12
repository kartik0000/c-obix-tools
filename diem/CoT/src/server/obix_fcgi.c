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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <syslog.h>
#include <fcgiapp.h>

#include <log_utils.h>
#include <obix_utils.h>
#include <xml_config.h>
#include <ptask.h>
#include "xml_storage.h"
#include "server.h"
#include "request.h"
#include "obix_fcgi.h"

#define LISTENSOCK_FILENO 0
#define LISTENSOCK_FLAGS 0

#define MAX_PARALLEL_REQUEST_DEFAULT 10

static const char* CONFIG_FILE = "server_config.xml";

static const char* CT_HOLD_REQUEST_MAX = "hold-request-max";

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
struct _Request
{
    FCGX_Request r;
    int id;
    BOOL canWait;
    struct _Request* next;
};

static Request* _requestList;
static int _requestsInUse = 0;
static int _requestIds = 0;
static int _requestMaxCount = MAX_PARALLEL_REQUEST_DEFAULT + 1;
pthread_mutex_t _requestListMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t _requestListNew = PTHREAD_COND_INITIALIZER;

static void obixRequest_release(Request* request);
static Request* obixRequest_get();
static void obixRequest_free(Request* request);

static char* parseArguments(int argc, char** argv)
{
    void printUsageNotice(char* programName)
    {
        log_debug("Usage:\n"
                  " %s [-syslog] [res_dir]\n"
                  "where  -syslog - Forces to use syslog for logging during\n"
                  "                 server initialization (before\n"
                  "                 configuration file is read);\n"
                  "       res_dir - Address of the folder with server\n"
                  "                 resources.\n"
                  "Both these arguments are optional.",
                  argv[0]);
    }

    // the call string should be:
    // obix.fcgi [-syslog] [resource_dir]
    int i = 1;

    if (argc > i)
    {
        if (argv[i][0] == '-')
        {
            // we have some kind of argument first
            if (strcmp(argv[i], "-syslog") == 0)
            {	// switch log to syslog. It can be changed back during
                // configuration loading
                log_useSyslog(LOG_USER);
            }
            else
            {	// some unknown argument
                log_warning("Unknown argument (ignored): %s", argv[i]);
                printUsageNotice(argv[0]);
            }
            // go to next argument if any
            i++;
        }
    }

	// check how many arguments remained
    if (argc > (i+1))
    {
        log_warning("Wrong number of arguments provided.");
        printUsageNotice(argv[0]);
    }

    if (argc > i)
    {
        // this is probably resource folder path
        return argv[i];
    }
    // no resource folder found
    return NULL;
}

/**
* Entry point of FCGI script.
*/
int main(int argc, char** argv)
{
    Request* request;
    // parse input arguments
    char* resourceDir = parseArguments(argc, argv);

    log_debug("Starting oBIX server...");

    if (resourceDir == NULL)
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
        // get free request object
        request = obixRequest_get();
        log_debug("Waiting for the request.. (hadler #%d)", request->id);
        int error = FCGX_Accept_r(&(request->r));
        if (error)
        {
            log_warning("Stopping the server: FCGX_Accept_r returned %d", error);
            break;
        }

        log_debug("Request accepted.. (hadler #%d)", request->id);
        obix_fcgi_handleRequest(request);
        log_debug("Request handled. (hadler #%d)", request->id);
    }

    // shut down
    obix_fcgi_shutdown();
    return 0;
}

IXML_Element* obix_fcgi_loadConfig(char* resourceDir)
{
    config_setResourceDir(resourceDir);

    IXML_Element* settings = config_loadFile(CONFIG_FILE);
    if (settings == NULL)
    {
        // failed to load settings
        return NULL;
    }
    // load log configuration
    int error = config_log(settings);
    if (error != 0)
    {
        ixmlElement_freeOwnerDocument(settings);
        return NULL;
    }

    // load optional parameter defining maximum number of requests
    IXML_Element* configTag = config_getChildTag(settings,
                              CT_HOLD_REQUEST_MAX,
                              FALSE);
    if (configTag != NULL)
    {
        _requestMaxCount = config_getTagAttrIntValue(
                               configTag,
                               CTA_VALUE,
                               FALSE,
                               MAX_PARALLEL_REQUEST_DEFAULT) + 1;
    }

    return settings;
}

int obix_fcgi_init(char* resourceDir)
{
    // register callback for handling responses
    obixResponse_setListener(&obix_fcgi_sendResponse);

    IXML_Element* settings = obix_fcgi_loadConfig(resourceDir);
    if (settings == NULL)
    {
        return -1;
    }

    // call before Accept in multithreaded apps
    int error = FCGX_Init();
    if (error)
    {
        log_error("Unable to start the server. FCGX_Init returned: %d", error);
        config_finishInit(settings, FALSE);
        return -1;
    }

    // configure server
    error = obix_server_init(settings);
    config_finishInit(settings, error == 0);
    return error;
}

void obix_fcgi_shutdown(FCGX_Request* request)
{
    obix_server_shutdown();

    pthread_mutex_lock(&_requestListMutex);
    if (_requestList != NULL)
    {
        obixRequest_free(_requestList);
    }
    pthread_mutex_unlock(&_requestListMutex);
    FCGX_ShutdownPending();
}

void obix_fcgi_handleRequest(Request* request)
{
    // get request URI
    const char* uri = FCGX_GetParam("REQUEST_URI", request->r.envp);
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
    const char* requestType = FCGX_GetParam("REQUEST_METHOD", request->r.envp);
    if (requestType == NULL)
    {
        log_error("Unable to get the request type.");
        obix_fcgi_sendStaticErrorMessage(request);
        return;
    }

    // prepare response object

    Response* response = obixResponse_create(request, request->canWait);
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
        // unknown HTTP request
        log_error("Unknown request type: %s. Request is ignored.", requestType);
        char* message = (char*) malloc(strlen(requestType) + 42);
        sprintf(message, "%s request is not supported by oBIX server.", requestType);
        obix_server_generateObixErrorMessage(response,
                                             uri,
                                             OBIX_CONTRACT_ERR_UNSUPPORTED,
                                             "Unsupported Request",
                                             message);
        free(message);
        obix_fcgi_sendResponse(response);
    }
}

void obix_fcgi_sendStaticErrorMessage(Request* request)
{
    // send HTTP reply
    FCGX_FPrintF(request->r.out, HTTP_STATUS_OK);
    FCGX_FPrintF(request->r.out, XML_HEADER);
    FCGX_FPrintF(request->r.out, ERROR_STATIC);
    FCGX_Finish_r(&(request->r));
    obixRequest_release(request);
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

    FCGX_Request* request = &(response->request->r);

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
    obixRequest_release(response->request);
    obixResponse_free(response);
}

char* obix_fcgi_readRequestInput(Request* request)
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

        bytesRead += FCGX_GetStr(buffer + bytesRead,
                                 bufferSize - bytesRead,
                                 request->r.in);

        if (bytesRead == 0)
        {
            //empty input
            free(buffer);
            return NULL;
        }

        error = FCGX_GetError(request->r.in);
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
        for (envp = response->request->r.envp; *envp; ++envp)
        {
            bufferSize += strlen(*envp) + 32;
        }
    }

    log_debug("Allocating %d bytes for debug.", bufferSize);
    buffer = (char*) malloc(bufferSize);
    if (buffer == NULL)
    {
        // can't dump environment - return empty response
        obixResponse_send(response);
        return;
    }

    strcpy(buffer, "<obj name=\"dump\" displayName=\"Server Dump\">\r\n"
           "  <obj name=\"env\" displayName=\"Request Environment\">\r\n");

    if (response->request != NULL)
    {
        for (envp = response->request->r.envp; *envp; ++envp)
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
        obixResponse_send(response);
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
            obixResponse_send(response);
            return;
        }
        nextPart = nextPart->next;
    }

    // finalize output
    if (obixResponse_setText(nextPart, "\r\n  </obj>\r\n</obj>", TRUE) != 0)
    {
        log_error("Unable to create multipart response. Answer is not complete.");
        obixResponse_send(response);
        return;
    }

    log_debug("Dump request completed.");

    // send response
    obixResponse_send(response);
}

// put request to the head of the list
static void obixRequest_release(Request* request)
{
    pthread_mutex_lock(&_requestListMutex);
    request->next = _requestList;
    _requestList = request;
    _requestsInUse--;
    pthread_cond_signal(&_requestListNew);
    pthread_mutex_unlock(&_requestListMutex);
}

static Request* obixRequest_getHead()
{
    Request* request = _requestList;
    _requestList = request->next;
    request->next = NULL;
    // check whether this request object can be used for handling long poll
    // last available request object should not be used for that, because
    // otherwise it will block server from handling other requests.
    if (++_requestsInUse == _requestMaxCount)
    {
        request->canWait = FALSE;
    }
    else
    {
        request->canWait = TRUE;
    }
    return request;
}

static Request* obixRequest_wait()
{
    // wait for some request instance to be free
    pthread_cond_wait(&_requestListNew, &_requestListMutex);
    return obixRequest_getHead();
}

static int obixRequest_create()
{
    Request* request = (Request*) malloc(sizeof(Request));
    if (request == NULL)
    {
        log_error("Unable to create new request instance: "
                  "Not enough memory.");
        return -1;
    }
    // initalize FCGI request
    request->next = NULL;
    int error = FCGX_InitRequest(&(request->r),
                                 LISTENSOCK_FILENO,
                                 LISTENSOCK_FLAGS);
    if (error != 0)
    {
        log_error("Unable to initialize FCGI request: "
                  "FCGX_InitRequest returned %d.", error);
        free(request);
        return -1;
    }

    // store request at head
    request->next = _requestList;
    request->id = _requestIds++;
    _requestList = request;
    return 0;
}

static Request* obixRequest_get()
{
    Request* request;

    pthread_mutex_lock(&_requestListMutex);

    // if there are available request instances in the list...
    if (_requestList != NULL)
    {	// take head of the list
        request = obixRequest_getHead();
        pthread_mutex_unlock(&_requestListMutex);
        return request;
    }
    // there is nothing in the list of requests.. check whether we can create a
    // new one

    if (_requestsInUse == _requestMaxCount)
    {
        log_error("Maximum number of concurrent requests exceeded. "
                  "That should never happen! Waiting for some request object "
                  "to be freed. Please, contact the developer if you see this "
                  "message.");
        request = obixRequest_wait();
        pthread_mutex_unlock(&_requestListMutex);
        return request;
    }

    // we can create a new one
    if (obixRequest_create() != 0)
    {
        request = obixRequest_wait();
        pthread_mutex_unlock(&_requestListMutex);
        return request;
    }

    request = obixRequest_getHead();
    pthread_mutex_unlock(&_requestListMutex);
    return request;
}

static void obixRequest_free(Request* request)
{
    if (request->next != NULL)
    {
        FCGX_Free(&(request->r), TRUE);
        free(request->next);
    }
    free(request);
}
