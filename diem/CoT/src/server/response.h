/** @file
 * @todo add description here
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#ifndef RESPONSE_H_
#define RESPONSE_H_

#include <fcgiapp.h>

#include "ixml_ext.h"

typedef struct Response
{
    char* body;
    char* uri;
    BOOL error;
    FCGX_Request* request;
    struct Response* next;
}
Response;

Response* obixResponse_create(FCGX_Request* request);

Response* obixResponse_add(Response* response);

int obixResponse_setText(Response* response, const char* text, BOOL copy);

int obixResponse_setError(Response* response, char* description);

void obixResponse_setErrorFlag(Response* response, BOOL error);

BOOL obixResponse_isError(Response* response);

void obixResponse_free(Response* response);

#endif /* RESPONSE_H_ */
