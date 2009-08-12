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
 * @todo add description here
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#ifndef RESPONSE_H_
#define RESPONSE_H_

#include "bool.h"
#include "request.h"

typedef struct Response
{
    char* body;
    char* uri;
    BOOL error;
    BOOL canWait;
    Request* request;
    struct Response* next;
}
Response;

typedef void (*obix_response_listener)(Response* response);

Response* obixResponse_create(Request* request, BOOL canWait);

Response* obixResponse_add(Response* response);

int obixResponse_setText(Response* response, const char* text, BOOL copy);

int obixResponse_setError(Response* response, char* description);

void obixResponse_setErrorFlag(Response* response, BOOL error);

BOOL obixResponse_isError(Response* response);

void obixResponse_free(Response* response);

void obixResponse_setRightUri(Response* response,
                              const char* requestUri,
                              int slashFlag);

void obixResponse_setListener(obix_response_listener listener);

BOOL obixResponse_isHead(Response* response);

int obixResponse_send(Response* response);

#endif /* RESPONSE_H_ */
