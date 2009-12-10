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
/** @file
 * Implementation of HTTP client interface.
 *
 * @author Andrey Litvinov
 */

#include <stdlib.h>
#include <string.h>
#include <log_utils.h>
#include "curl_ext.h"

#define DEF_INPUT_BUFFER_SIZE 2048
#define REQUEST_HTTP_PUT 0
#define REQUEST_HTTP_POST 1

static int _defaultInputBufferSize = DEF_INPUT_BUFFER_SIZE;

static struct curl_slist* _header;

/**
 * libcurl write callback function. Called each time when
 * something is received to write down the received data.
 */
static size_t inputWriter(char* inputData,
                          size_t size,
                          size_t nmemb,
                          void* arg)
{
    CURL_EXT* handle = (CURL_EXT*) arg;
    size_t newDataSize = size * nmemb;

    log_debug("CURL inputWriter: New data block size %d, free %d, total %d.",
              newDataSize, handle->inputBufferFree, handle->inputBufferSize);

    if (newDataSize > handle->inputBufferFree)
    {
        log_warning("CURL inputWriter: allocating memory for input buffer. "
                    "Change default buffer size if you often see this message!");

        while (newDataSize > handle->inputBufferFree)
        {
            handle->inputBufferSize += _defaultInputBufferSize;
            handle->inputBufferFree += _defaultInputBufferSize;
        }

        handle->inputBuffer = (char*) realloc(handle->inputBuffer,
                                              handle->inputBufferSize);
        if (handle->inputBuffer == NULL)
        {
            log_error("CURL inputWriter: Unable to allocate new space "
                      "for input buffer.");
            return 0;
        }
    }
    // append data to the end of buffer
    strncat(handle->inputBuffer, inputData, newDataSize);
    handle->inputBufferFree -= newDataSize;
    return newDataSize;
}

/**
 * libcurl read callback function. Called each time when
 * something is sent to read the sending buffer.
 */
static size_t outputReader(char *outputData,
                           size_t size,
                           size_t nmemb,
                           void* arg)
{
    CURL_EXT* handle = (CURL_EXT*) arg;
    //calculate total available place at outputData
    size = size * nmemb;
    //number of bytes waiting to be sent
    size_t bytesToSend = (handle->outputSize - handle->outputPos);
    log_debug("CURL outputReader: sending new data portion. pos=%d; total=%d.",
              handle->outputPos, handle->outputSize);
    if (size < bytesToSend)
    {
        bytesToSend = size;
    }

    memcpy(outputData,
           (handle->outputBuffer + handle->outputPos),
           bytesToSend);

    handle->outputPos += bytesToSend;

    return bytesToSend;
}

static void curl_ext_freeMemory(CURL_EXT* handle)
{
    if (handle != NULL)
    {
        if (handle->curl != NULL)
        {
            curl_easy_cleanup(handle->curl);
        }
        if (handle->errorBuffer != NULL)
        {
            free(handle->errorBuffer);
        }
        if (handle->inputBuffer != NULL)
        {
            free(handle->inputBuffer);
        }
        free(handle);
    }
}

/**
 * Allocates memory for a new extended CURL handle and sets all fields to
 * default values.
 * @return NULL on error.
 */
static CURL_EXT* curl_ext_allocateMemory()
{
    // allocate space for new handle
    CURL_EXT* handle = (CURL_EXT*) malloc(sizeof(CURL_EXT));
    if (handle == NULL)
    {
        return NULL;
    }
    // initialize default values
    handle->curl = NULL;
    handle->errorBuffer = NULL;
    handle->inputBuffer = NULL;
    handle->inputBufferFree = _defaultInputBufferSize - 1;
    handle->inputBufferSize = _defaultInputBufferSize;
    handle->outputBuffer = NULL;
    handle->outputPos = 0;
    handle->outputSize = 0;

    // allocate space for buffers
    handle->errorBuffer = (char*) malloc(CURL_ERROR_SIZE);
    if (handle->errorBuffer == NULL)
    {
        curl_ext_freeMemory(handle);
        return NULL;
    }
    *(handle->errorBuffer) = '\0';

    handle->inputBuffer = (char*) malloc(_defaultInputBufferSize);
    if (handle->inputBuffer == NULL)
    {
        curl_ext_freeMemory(handle);
        return NULL;
    }

    return handle;
}

int curl_ext_init(int defaultInputBufferSize)
{
    if (defaultInputBufferSize > 0)
    {
        _defaultInputBufferSize = defaultInputBufferSize;
    }

    CURLcode code = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (code != CURLE_OK)
    {
        log_error("Unable to initialize CURL: "
                  "curl_global_init() returned %d.", code);
        return -1;
    }

    // TODO what about reading this from settings file also?
    // all requests are in xml
    _header = curl_slist_append(_header, "Content-Type: text/xml");
    // disable default header for PUT requests "Expect: 100-continue"
    _header = curl_slist_append(_header, "Expect:");

    return 0;
}

void curl_ext_dispose()
{
    curl_global_cleanup();

    // cleanup custom headers
    curl_slist_free_all(_header);
    _header = NULL;
}

int curl_ext_create(CURL_EXT** handle)
{
    // allocate space for new handle
    CURL_EXT* h = curl_ext_allocateMemory();
    if (h == NULL)
    {
        log_error("Unable to allocate memory for CURL handle.");
        return -2;
    }

    // Initialize CURL connection
    CURL* curl = curl_easy_init();
    if (curl == NULL)
    {
        log_error("Unable to initialize CURL handle.");
        curl_ext_freeMemory(h);
        return -1;
    }

    CURLcode code = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, h->errorBuffer);
    if (code != CURLE_OK)
    {
        log_error("Unable to initialize CURL handle: "
                  "Failed to set error buffer (%d).", code);
        curl_ext_freeMemory(h);
        return -1;
    }

    code = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, _header);
    if (code != CURLE_OK)
    {
        log_error("Unable to initialize CURL handle: "
                  "Failed to set custom header (%d).", code);
        curl_ext_freeMemory(h);
        return -1;
    }

    // Set incoming data handler (is called by curl to write down
    // received data)
    code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &inputWriter);
    if (code != CURLE_OK)
    {
        log_error("Unable to initialize CURL handle: "
                  "Failed to set input writer (%d).", code);
        curl_ext_freeMemory(h);
        return -1;
    }

    // argument which will be provided to the input writer
    code = curl_easy_setopt(curl, CURLOPT_WRITEDATA, h);
    if (code != CURLE_OK)
    {
        log_error("Unable to initialize CURL handle: "
                  "Failed to set input buffer (%d).", code);
        curl_ext_freeMemory(h);
        return -1;
    }

    // Set output reader function (is called by curl to read
    // body of request from buffer)
    code = curl_easy_setopt(curl, CURLOPT_READFUNCTION, &outputReader);
    if (code != CURLE_OK)
    {
        log_error("Unable to initialize CURL handle: "
                  "Failed to set output reader (%d).", code);
        curl_ext_freeMemory(h);
        return -1;
    }

    // argument which will be provided to the output reader
    code = curl_easy_setopt(curl, CURLOPT_READDATA, h);
    if (code != CURLE_OK)
    {
        log_error("Unable to initialize CURL handle: "
                  "Failed to set output data (%d).", code);
        curl_ext_freeMemory(h);
        return -1;
    }

    h->curl = curl;
    *handle = h;

    return 0;
}

void curl_ext_free(CURL_EXT* handle)
{
    curl_ext_freeMemory(handle);
}

/**
 * Helper function which performs actual HTTP request
 * assumes that type of request was already set by a caller.
 */
static int sendRequest(CURL_EXT* handle, const char* uri)
{
    CURLcode code = curl_easy_setopt(handle->curl, CURLOPT_URL, uri);
    if (code != CURLE_OK)
    {
        log_error("Unable to initialize HTTP request: "
                  "Failed to set URL (%d).", code);
        return -1;
    }

    // Cleanup input buffer
    *(handle->inputBuffer) = '\0';
    handle->inputBufferFree = handle->inputBufferSize - 1;

    // Retrieve content of the URL
    code = curl_easy_perform(handle->curl);
    // allow server empty responses
    if ((code != CURLE_OK) && (code != CURLE_GOT_NOTHING))
    {
        log_error("HTTP request to \"%s\" failed (%d): %s.",
                  uri, code, handle->errorBuffer);
        return -1;
    }

    log_debug("CURL received input:\n%s", handle->inputBuffer);

    return 0;
}

int curl_ext_get(CURL_EXT* handle, const char* uri)
{
    // perform HTTP GET operation
    CURLcode code = curl_easy_setopt(handle->curl, CURLOPT_HTTPGET, 1L);
    if (code != CURLE_OK)
    {
        log_error("Unable to initialize HTTP GET request: "
                  "Failed to switch to GET (%d).", code);
        return -1;
    }

    log_debug("Requesting data from %s.", uri);
    return sendRequest(handle, uri);
}

int curl_ext_put(CURL_EXT* handle, const char* uri)
{
    // perform HTTP PUT operation
    CURLcode code = curl_easy_setopt(handle->curl, CURLOPT_UPLOAD, 1L);
    if (code != CURLE_OK)
    {
        log_error("Unable to initialize HTTP PUT request: "
                  "Failed to switch to PUT (%d).", code);
        return -1;
    }

    // Reset output counters
    handle->outputPos = 0;
    if (handle->outputBuffer == NULL)
    {
        // Do not allow empty PUT requests
        log_error("Trying to perform PUT request with empty body.");
        return -1;
    }
    handle->outputSize = strlen(handle->outputBuffer);

    // Set output length
    code = curl_easy_setopt(handle->curl, CURLOPT_INFILESIZE, handle->outputSize);
    if (code != CURLE_OK)
    {
        log_error("Unable to initialize HTTP PUT request: "
                  "Failed to set output size (%d).", code);
        return -1;
    }

    log_debug("CURL sending data:\n%s", handle->outputBuffer);
    return sendRequest(handle, uri);
}

int curl_ext_post(CURL_EXT* handle, const char* uri)
{
    // perform HTTP POST operation
    // have to set explicitly upload mode to 0, otherwise
    // PUT will be used instead of POST
    CURLcode code = curl_easy_setopt(handle->curl, CURLOPT_UPLOAD, 0L);
    if (code != CURLE_OK)
    {
        log_error("Unable to initialize HTTP POST request: "
                  "Failed to switch of upload (%d).", code);
        return -1;
    }

    code = curl_easy_setopt(handle->curl, CURLOPT_POST, 1L);
    if (code != CURLE_OK)
    {
        log_error("Unable to initialize HTTP POST request: "
                  "Failed to switch to POST (%d).", code);
        return -1;
    }

    // Reset output counters
    handle->outputPos = 0;
    // allow empty body of POST request
    if (handle->outputBuffer == NULL)
    {
        handle->outputSize = 0;
    }
    else
    {
        handle->outputSize = strlen(handle->outputBuffer);
    }

    // Set output length
    code = curl_easy_setopt(handle->curl, CURLOPT_POSTFIELDSIZE, handle->outputSize);
    if (code != CURLE_OK)
    {
        log_error("Unable to initialize HTTP POST request: "
                  "Failed to set output size (%d).", code);
        return -1;
    }

    log_debug("CURL sending data to %s:\n%s", uri, handle->outputBuffer);
    return sendRequest(handle, uri);
}

/**
 * Helper function for parsing received response at input buffer of provided
 * handle.
 * @return @a 0 on success; @a -1 on error.
 */
static int parseXmlInput(CURL_EXT* handle, IXML_Document** doc)
{
    if (*(handle->inputBuffer) == '\0')
    {	// we do not consider empty answer as an error
        *doc = NULL;
        return 0;
    }

    int error = ixmlParseBufferEx(handle->inputBuffer, doc);
    if (error != IXML_SUCCESS)
    {
        log_error("Server response is not an XML document (error %d):\n%s",
                  error, handle->inputBuffer);
        return -1;
    }

    return 0;
}

int curl_ext_getDOM(CURL_EXT* handle, const char* uri, IXML_Document** response)
{
    int error = curl_ext_get(handle, uri);
    if (error != 0)
    {
        return error;
    }

    return parseXmlInput(handle, response);
}

int curl_ext_putDOM(CURL_EXT* handle, const char* uri, IXML_Document** response)
{
    int error = curl_ext_put(handle, uri);
    if (error != 0)
    {
        return error;
    }

    return parseXmlInput(handle, response);
}

int curl_ext_postDOM(CURL_EXT* handle, const char* uri, IXML_Document** response)
{
    int error = curl_ext_post(handle, uri);
    if (error != 0)
    {
        return error;
    }

    return parseXmlInput(handle, response);
}
