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
 * Implementation of request management functions.
 *
 * @see request.h
 *
 * @author Andrey Litvinov
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <log_utils.h>
#include "request.h"

/** @name FastCGI connection constants
 * They are used during FCGI request instance initialization.
 * @{ */
#define LISTENSOCK_FILENO 0
#define LISTENSOCK_FLAGS 0
/** @} */

/** Storage for request objects. */
static Request* _requestList;
/** Number of request objects which are currently use for request processing. */
static int _requestsInUse = 0;
/** Is used for unique request id generation. */
static int _requestIds = 0;
/** Defines maximum request objects, which can be created.
 * Server handles usual requests consequently (no multithreading), but
 * it can hold long polling requests for delayed execution. In that case,
 * corresponding request object appears to be blocked, so there is a need for
 * more objects to continue handling other requests. Thus, maximum amount of
 * request objects limits the amount of long polling requests which can be hold
 * simultaneously. */
static int _requestMaxCount = REQUEST_MAX_COUNT_DEFAULT + 1;
/** Is used to synchronize access to the request list from several threads. */
pthread_mutex_t _requestListMutex = PTHREAD_MUTEX_INITIALIZER;
/** Condition, which occurs every time, when some request object gets released
 * and thus is returned back to the request list. */
pthread_cond_t _requestListNew = PTHREAD_COND_INITIALIZER;

/** Returns head element from request list.
 * Should be called from synchronized context only. */
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

/** Waits until some request is released.
 * Should be called from synchronized context only. */
static Request* obixRequest_wait()
{
    // wait for some request instance to be free
    pthread_cond_wait(&_requestListNew, &_requestListMutex);
    return obixRequest_getHead();
}

/** Creates new request object. */
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

    request->serverAddress = NULL;

    // store request at head
    request->next = _requestList;
    request->id = _requestIds++;
    _requestList = request;
    return 0;
}

/** Deletes recursively list of request objects. */
static void obixRequest_freeRecursive(Request* request)
{
    if (request == NULL)
    {
        return; //nothing to be cleared.
    }

    // clear recursively all child objects
    obixRequest_freeRecursive(request->next);

    // clear this request
    FCGX_Free(&(request->r), TRUE);
    free(request);
}

// put request to the head of the list
void obixRequest_release(Request* request)
{
    if (request->serverAddress != NULL)
    {
        free(request->serverAddress);
        request->serverAddress = NULL;
    }
    pthread_mutex_lock(&_requestListMutex);
    request->next = _requestList;
    _requestList = request;
    _requestsInUse--;
    pthread_cond_signal(&_requestListNew);
    pthread_mutex_unlock(&_requestListMutex);
}

Request* obixRequest_get()
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

/**
 * Parses server address as it appears in request.
 * This address is than used to return objects with the server address which
 * was used by client to request it (in case if server has several interfaces,
 * e.g. localhost, external ip address, etc.)
 */
static int obixRequest_parseServerAddress(Request* request)
{
    const char* serverHost = FCGX_GetParam("HTTP_HOST", request->r.envp);
    const char* serverPortStr = FCGX_GetParam("SERVER_PORT", request->r.envp);
    if ((serverHost == NULL) || (serverPortStr == NULL))
    {
        log_error("Unable to retrieve server address from the request:\n"
                  "HTTP_HOST = \"%s\"\n"
                  "SERVER_PORT = \"%s\".", serverHost, serverPortStr);
        return -1;
    }

    long serverPort = atol(serverPortStr);
    if (serverPort <=0)
    {
        log_error("Unable to parse server port number: \"%s\".", serverPort);
        return -1;
    }

    int addressLength = strlen(serverHost);
    //allocate space for server address (enough to hold http prefix)
    request->serverAddress = (char*) malloc(addressLength + 9);
    if (request->serverAddress == NULL)
    {
        log_error("Unable to allocate memory for server address. "
                  "Request parameters:\n"
                  "HTTP_HOST = \"%s\"\n"
                  "SERVER_PORT = \"%s\".", serverHost, serverPort);
        return -1;
    }

    if (serverPort == 443)
    {
    	sprintf(request->serverAddress, "https://%s", serverHost);
    	addressLength += 8;
    }
    else
    {
    	sprintf(request->serverAddress, "http://%s", serverHost);
    	addressLength += 7;
    }

    request->serverAddressLength = addressLength;

    return 0;
}

const char* obixRequest_parseAttributes(Request* request)
{
    // get request URI
    const char* uri = FCGX_GetParam("REQUEST_URI", request->r.envp);
    if (uri == NULL)
    {
        log_error("Unable to retrieve URI from the request.");
        return NULL;
    }
    // check that uri is absolute
    if (*uri != '/')
    {
        log_error("Request URI \"%s\" has wrong format: "
                  "Should start with \'/\'.", uri);
        return NULL;
    }

    if (obixRequest_parseServerAddress(request) != 0)
    {
    	return NULL;
    }

    return uri;
}

void obixRequest_freeAll()
{
    pthread_mutex_lock(&_requestListMutex);
    obixRequest_freeRecursive(_requestList);
    pthread_mutex_unlock(&_requestListMutex);
}

void obixRequest_setMaxCount(int maxCount)
{
    _requestMaxCount = maxCount;
}
