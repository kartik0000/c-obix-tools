/* *****************************************************************************
 * Copyright (c) 2009 Andrey Litvinov
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
 * Defines main functions of FastCGI communication interface.
 *
 * @author Andrey Litvinov
 */

#ifndef OBIX_FCGI_H_
#define OBIX_FCGI_H_

#include "request.h"
#include "response.h"

/**
 * Initializes oBIX server.
 *
 * @return @a 0 on success, negative error code otherwise.
 */
int obix_fcgi_init();

/**
 * Stops work of the server.
 */
void obix_fcgi_shutdown();

/**
 * Handles incoming request.
 */
void obix_fcgi_handleRequest(Request* request);

/**
 * Sends response back to the client.
 * @param response Should contain generated response message.
 */
void obix_fcgi_sendResponse(Response* response);

/**
 * Sends default error message as a response to the client.
 * Method should be called only when it is not possible to generate custom
 * message, explaining the error.
 */
void obix_fcgi_sendStaticErrorMessage(Request* request);

/**
 * Reads request input message into character buffer.
 * @return Full input message in a buffer, or @a NULL if error occurred.
 */
char* obix_fcgi_readRequestInput(Request* request);

/**
 * Generates a response object with full dump of server database.
 */
void obix_fcgi_dumpEnvironment(Response* response);

#endif /* OBIX_FCGI_H_ */
