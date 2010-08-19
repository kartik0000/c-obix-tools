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
/**
 * @file
 * Defines Request type.
 * It is a wrapper for FCGX_Request structure, which allows to manage several
 * requests in concurrent mode.
 *
 * @author Andrey Litvinov
 */

#ifndef REQUEST_H_
#define REQUEST_H_

#include <fcgiapp.h>
#include <bool.h>

/** Default value of maximum amount of request instances in the system. */
#define REQUEST_MAX_COUNT_DEFAULT 20

/** Request structure.
 * No field values should be changed outside #request.c. */
typedef struct _Request
{
	/** Wrapped FCGI request instance. */
    FCGX_Request r;
    /** Unique id. */
    int id;
    /** Tells whether this request instance can be used for delayed request
     * processing, or should be released immediately. */
    BOOL canWait;
    /** Server address used by client in the request. */
    char* serverAddress;
    /** Length of the server address string. */
    int serverAddressLength;

    /** Next request instance in the list. For internal usage only. */
    struct _Request* next;
} Request;

/**
 * Should be called when request object is not needed anymore and can be
 * released.
 *
 * Function puts such object back to the list of free requests.
 */
void obixRequest_release(Request* request);

/**
 * Returns free request object.
 * If there are no free objects at the moment, creates a new one.
 * If request limit is reached, then function blocks until some request get
 * released.
 */
Request* obixRequest_get();

/**
 * Parses requested URI and server address.
 * @return Requested URI
 */
const char* obixRequest_parseAttributes(Request* request);

/**
 * Releases memory allocated for all request objects.
 * Note that it is not possible to delete one request object separately (it
 * should be reused instead).
 */
void obixRequest_freeAll();

/**
 * Sets the maximum count of request object which can be created.
 * When this limit is reached, next call to #obixRequest_get would wait for
 * some request object to be released instead of creating new one.
 */
void obixRequest_setMaxCount(int maxCount);

#endif /* REQUEST_H_ */
