/** @file
 * @todo add description
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#ifndef OBIX_FCGI_H_
#define OBIX_FCGI_H_

#include <fcgiapp.h>

#include "response.h"

int obix_fcgi_init();

void obix_fcgi_shutdown(FCGX_Request* request);

void obix_fcgi_handleRequest(FCGX_Request* request);

void obix_fcgi_sendResponse(Response* response);

void obix_fcgi_sendStaticErrorMessage(FCGX_Request* request);

char* obix_fcgi_readRequestInput(FCGX_Request* request);

void obix_fcgi_dumpEnvironment(Response* response);

#endif /* OBIX_FCGI_H_ */
