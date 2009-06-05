/** @file
 * TODO add description there
 */
#ifndef OBIX_SERVER_H_
#define OBIX_SERVER_H_

#include <ixml_ext.h>
#include "response.h"

typedef void (*obix_server_response_listener)(Response* response);

// method which handles server responses is stored here
extern obix_server_response_listener _responseListener;

int obix_server_init(char* resourceDir);

void obix_server_shutdown();

void obix_server_generateObixErrorMessage(
    Response* response,
    const char* uri,
    const char* type,
    const char* name,
    const char* desc);

void obix_server_setResponseListener(obix_server_response_listener listener);

void obix_server_handleGET(Response* response, const char* uri);

void obix_server_handlePUT(Response* response,
                           const char* uri,
                           const char* input);

void obix_server_handlePOST(Response* response,
                            const char* uri,
                            const char* input);

void obix_server_generateResponse(Response* response,
                                  IXML_Element* doc,
                                  const char* requestUri,
                                  BOOL changeUri,
                                  BOOL useObjectUri,
                                  int slashFlag,
                                  BOOL isMultipart,
                                  BOOL saveChanges);

#endif /* OBIX_SERVER_H_ */
