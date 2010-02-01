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
 * Implementation of Response structure utility functions.
 *
 * @see response.h
 *
 * @author Andrey Litvinov
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <log_utils.h>
#include <obix_utils.h>
#include "response.h"

const char* OBIX_OBJ_ERR_TEMPLATE = "<err displayName=\"Internal Server Error\""
                                    " display=\"%s\"/ >";

/** Function which sends server responses to client is stored here. */
static obix_response_listener _responseListener = NULL;

void obixResponse_setListener(obix_response_listener listener)
{
	_responseListener = listener;
}

Response* obixResponse_create(Request* request)
{
    Response* response = (Response*) malloc(sizeof(Response));
    if (response == NULL)
    {
        return NULL;
    }
    // init all values with default values;
    response->request = request;
    response->body = NULL;
    response->uri = NULL;
    response->next = NULL;
    response->error = FALSE;

    return response;
}

Response* obixResponse_getNewPart(Response* response)
{
    // create new response part. Request object is stored only in the head
    // element
    Response* newPart = obixResponse_create(NULL);

    response->next = newPart;
    return newPart;
}

void obixResponse_free(Response* response)
{
    // free recursively the whole response chain
    if (response->next != NULL)
    {
        obixResponse_free(response->next);
    }

    if (response->body != NULL)
    {
        free(response->body);
    }

    if (response->uri != NULL)
    {
        free(response->uri);
    }

    free(response);
}

int obixResponse_setError(Response* response, char* description)
{
    if (response == NULL)
    {
        return -1;
    }

    response->error = TRUE;

    if (response->body != NULL)
    {
        free(response->body);
    }

    char* errorMessage = malloc(strlen(OBIX_OBJ_ERR_TEMPLATE) +
                                strlen(response->body) + 1);
    if (errorMessage == NULL)
    {
        // fail generation of the error message
        log_error("Unable to allocate memory for error message generation.");
        response->body = NULL;
        return -1;
    }

    sprintf(errorMessage, OBIX_OBJ_ERR_TEMPLATE, description);

    response->body = errorMessage;

    return 0;
}

void obixResponse_setErrorFlag(Response* response, BOOL error)
{
    response->error = error;
}

BOOL obixResponse_isError(Response* response)
{
    if (response == NULL)
    {
        return TRUE;
    }
    else
    {
        return response->error;
    }
}

int obixResponse_setText(Response* response, const char* text, BOOL copy)
{
    if (response == NULL)
    {
        return -1;
    }

    if (response->body != NULL)
    {
        free(response->body);
    }

    if (!copy)
    {
        response->body = (char*) text;
    }
    else
    {
        char* newBody = (char*) malloc(strlen(text) + 1);
        if (newBody == NULL)
        {
            log_error("Unable to allocate memory for the response body.");
            response->body = NULL;
            return -1;
        }
        strcpy(newBody, text);

        response->body = newBody;
    }

    return 0;
}

void obixResponse_setRightUri(Response* response,
                              const char* requestUri,
                              int slashFlag)
{
    int uriLength = strlen(requestUri) + slashFlag;
    char* uri = (char*) malloc(uriLength + 1);
    if (uri == NULL)
    {
        log_error("Unable to set right URI to the response: "
                  "Not enough memory.");
        return;
    }
    strncpy(uri, requestUri, uriLength);
    if (slashFlag == 1)
    {
    	uri[uriLength - 1] = '/';
    }
    uri[uriLength] = '\0';
    response->uri = uri;
}

BOOL obixResponse_isHead(Response* response)
{
	return (response->request != NULL) ? TRUE : FALSE;
}

int obixResponse_send(Response* response)
{
	// if it is not a response head than it should not be sent.
	if (obixResponse_isHead(response))
	{
		(*_responseListener)(response);
		return 0;
	}

	return -1;
}

BOOL obixResponse_canWait(Response* response)
{	// if there is no request object, it means that this response object
	// is some middle part of the response chain. This part can't survive
	// without remaining parts and thus can't wait.
	return (response->request == NULL) ? FALSE : response->request->canWait;
}
