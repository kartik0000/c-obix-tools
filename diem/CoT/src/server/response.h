/** @file
 * @todo add description here
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#ifndef RESPONSE_H_
#define RESPONSE_H_

#include "ixml_ext.h"

typedef struct Response
{
    char* body;
    char* uri;
    BOOL error;
    struct Response* next;
}
Response;

Response* obixResponse_create();

Response* obixResponse_createFromString(const char* text);

int obixResponse_setText(Response* response, const char* text);

int obixResponse_setError(Response* response, char* description);

BOOL obixResponse_isError(Response* response);

void obixResponse_free(Response* response);

#endif /* RESPONSE_H_ */
