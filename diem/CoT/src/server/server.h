/* *****************************************************************************
 * Copyright (c) 2009, 2010 Andrey Litvinov
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
 * Interface of the server's request processing engine.
 * This module deals with everything in request processing, which is not related
 * to the transport layer. So it takes received and parsed request, and the
 * output is generated response message.
 *
 * @author Andrey Litvinov
 */
#ifndef OBIX_SERVER_H_
#define OBIX_SERVER_H_

#include <ixml_ext.h>
#include "response.h"

/**
 * Initializes request processing engine.
 * @return @a 0 on success; @a -1 on failure.
 */
int obix_server_init();

/**
 * Stops request processing engine and releases all allocated memory.
 */
void obix_server_shutdown();

/**
 * Generates oBIX error object with provided attributes as a response message.
 * @param uri URI, which was requested by client.
 * @param type Type of error if applicable. oBIX specification defines following
 * 			error contracts: Bad URI error (#OBIX_CONTRACT_ERR_BAD_URI),
 * 			Permission error (#OBIX_CONTRACT_ERR_PERMISSION) and
 * 			Unsupported error (#OBIX_CONTRACT_ERR_UNSUPPORTED). If no of those
 * 			contracts suit for the particular error, than @a NULL should be
 * 			provided.
 * @param name Short name of the error.
 * @param desc Error description.
 */
void obix_server_generateObixErrorMessage(
    Response* response,
    const char* uri,
    const char* type,
    const char* name,
    const char* desc);

/**
 * Handles GET request and sends response back to the client.
 * @param response Response object, which should be used to generate an answer.
 * @param uri URI, which was requested by client.
 */
void obix_server_handleGET(Response* response, const char* uri);

/**
 * Handles read request. Puts response into the provided response object.
 */
void obix_server_read(Response* response, const char* uri);

/**
 * Handles PUT request and sends response back to the client.
 * @param response Response object, which should be used to generate an answer.
 * @param uri URI, which was requested by client.
 * @param input Clients message (body of the PUT request).
 */
void obix_server_handlePUT(Response* response,
                           const char* uri,
                           const char* input);

/**
 * Handles write request. Puts response into the provided response object.
 */
void obix_server_write(Response* response,
                       const char* uri,
                       IXML_Element* input);

/**
 * Handles POST request and sends response back to the client.
 * @param response Response object, which should be used to generate an answer.
 * @param uri URI, which was requested by client.
 * @param input Clients message (body of the POST request).
 */
void obix_server_handlePOST(Response* response,
                            const char* uri,
                            const char* input);

/**
 * Handles invoke operation request. Puts operation results into the provided
 * response object.
 */
void obix_server_invoke(Response* response,
                        const char* uri,
                        IXML_Element* input);

/**
 * Generates complete response message and puts it to the provided response
 * object.
 * @param doc Response message XML.
 * @param requestUri URI, which was requested by client.
 * @param slashFlag Flag, which tells whether trailing slash should be added or
 * 					removed from @a requestUri. This flag is returned by
 * 					#xmldb_get, or xmldb_getDOM.
 * @param saveChanges If @a TRUE, than all changes will be saved in @a doc.
 */
void obix_server_generateResponse(Response* response,
                                  IXML_Element* doc,
                                  const char* requestUri,
                                  int slashFlag,
                                  BOOL saveChanges);

#endif /* OBIX_SERVER_H_ */
