/** @file
 * TODO add description there
 */
#ifndef OBIX_SERVER_H_
#define OBIX_SERVER_H_

#include <fcgiapp.h>
#include <ixml_ext.h>
#include "response.h"

int obix_server_init(char* resourceDir);

void obix_server_shutdown();

Response* obix_server_getObixErrorMessage(const char* uri,
        const char* type,
        const char* name,
        const char* desc);

Response* obix_server_handleGET(const char* uri);

Response* obix_server_handlePUT(const char* uri, const char* input);

Response* obix_server_handlePOST(const char* uri, const char* input);

Response* obix_server_generateResponse(IXML_Element* doc,
                                       const char* requestUri,
                                       BOOL changeUri,
                                       BOOL useObjectUri,
                                       int slashFlag,
                                       BOOL isMultipart,
                                       BOOL saveChanges);

Response* obix_server_dumpEnvironment(FCGX_Request* request);

#endif /* OBIX_SERVER_H_ */
