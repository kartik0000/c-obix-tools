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
 * Defines response object and its utility functions.
 * This object is used to store generated server response and send it to the
 * client.
 *
 * @author Andrey Litvinov
 */

#ifndef RESPONSE_H_
#define RESPONSE_H_

#include "bool.h"
#include "request.h"

/** Response structure.
 * Contains server's response message.
 * The message can consist of several chained response instances
 * (see #obixResponse_getNewPart). */
typedef struct Response
{
    char* body;
    char* uri;
    BOOL error;
    Request* request;
    struct Response* next;
}
Response;

/**
 * Prototype of a response handler.
 * This function should send provided response to the client.
 * @param response Generated response message. After sending the message, the
 * 		 		function is responsible for deleting this response object.
 */
typedef void (*obix_response_listener)(Response* response);

/**
 * Creates new response object for provided request.
 */
Response* obixResponse_create(Request* request);

/**
 * Creates and returns new part of multi-part response.
 * @param response Previous part of multi-part response.
 * @return New empty part of response.
 */
Response* obixResponse_getNewPart(Response* response);

/**
 * Sets text body of the provided response object.
 * @param copy If @a TRUE, than a copy of message in @a text variable will be
 * 			created and thus @a text can be deleted right after function
 * 			invocation. If @a copy is @a FALSE, then @a text buffer will be used
 * 			when response will be sent to client and after that deleted together
 * 			with response object.
 *
 * @return @a 0 on success; @a -1 on error.
 */
int obixResponse_setText(Response* response, const char* text, BOOL copy);

/**
 * Sets an error message for the response.
 * @param Description Text description of the error. This description is copied
 * 			to the response body.
 */
int obixResponse_setError(Response* response, char* description);

/**
 * Marks the response as an error message.
 */
void obixResponse_setErrorFlag(Response* response, BOOL error);

/**
 * Checks whether response is an error or not.
 */
BOOL obixResponse_isError(Response* response);

/**
 * Releases memory allocated for the response.
 * Note that it doesn't release corresponding request object.
 */
void obixResponse_free(Response* response);

/**
 * Adds a header to HTTP response message, telling the correct URI of the
 * object - Content-Location field. This is not required by oBIX specification.
 *
 * Should be used in case when client has requested URI without trailing
 * slash, while the real object's URI has it (and vice versa).
 * @param slashFlag A flag telling whether requested URI has mistake in trailing
 * 				slash (see #xmldb_get and #xmldb_getDOM)
 */
void obixResponse_setRightUri(Response* response,
                              const char* requestUri,
                              int slashFlag);

/**
 * Assigns global listener, which would be invoked everytime when response is
 * sent using #obixResponse_send.
 */
void obixResponse_setListener(obix_response_listener listener);

/**
 * Tells whether provided response object is a first part of multi-part
 * response, or somewhere in the middle of multi-part response chain.
 * If the provided response object is not multi-part than it is a head.
 */
BOOL obixResponse_isHead(Response* response);

/**
 * Sends response to the client.
 * @return @a 0 on success, @a -1 on error.
 */
int obixResponse_send(Response* response);

/**
 * Tells whether processing of this response can be delayed, or should be done
 * immediately.
 */
BOOL obixResponse_canWait(Response* response);

#endif /* RESPONSE_H_ */
