/** @file
 * @todo add description here
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#ifndef RESPONSE_H_
#define RESPONSE_H_

#include "ixml_ext.h"
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

#endif /* RESPONSE_H_ */
