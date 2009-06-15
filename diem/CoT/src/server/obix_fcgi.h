/** @file
 * @todo add description
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#ifndef OBIX_FCGI_H_
#define OBIX_FCGI_H_

#include "request.h"
#include "response.h"

int obix_fcgi_init();

void obix_fcgi_shutdown();

void obix_fcgi_handleRequest(Request* request);

void obix_fcgi_sendResponse(Response* response);

void obix_fcgi_sendStaticErrorMessage(Request* request);

char* obix_fcgi_readRequestInput(Request* request);

void obix_fcgi_dumpEnvironment(Response* response);

#endif /* OBIX_FCGI_H_ */
