/** @file
 * @todo add description
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#ifndef OBIX_FCGI_H_
#define OBIX_FCGI_H_

#include "response.h"

int obix_fcgi_init();

void obix_fcgi_shutdown();

Response* obix_fcgi_handleRequest();

void obix_fcgi_sendResponse(Response* response);

void obix_fcgi_sendStaticErrorMessage();

char* obix_fcgi_readRequestInput();

#endif /* OBIX_FCGI_H_ */
