/** @file
 * @todo add description here
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <lwl_ext.h>
#include <obix_utils.h>
#include "response.h"

Response* obixResponse_create(FCGX_Request* request)
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

Response* obixResponse_add(Response* response)
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
