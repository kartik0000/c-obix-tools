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
 * TODO add description there
 */
#ifndef OBIX_SERVER_H_
#define OBIX_SERVER_H_

#include <ixml_ext.h>
#include "response.h"

int obix_server_init(IXML_Element* settings);

void obix_server_shutdown();

void obix_server_generateObixErrorMessage(
    Response* response,
    const char* uri,
    const char* type,
    const char* name,
    const char* desc);

void obix_server_handleGET(Response* response, const char* uri);

void obix_server_read(Response* response, const char* uri);

void obix_server_handlePUT(Response* response,
                           const char* uri,
                           const char* input);

void obix_server_write(Response* response,
                       const char* uri,
                       IXML_Element* input);

void obix_server_handlePOST(Response* response,
                            const char* uri,
                            const char* input);

void obix_server_invoke(Response* response,
                        const char* uri,
                        IXML_Element* input);

void obix_server_generateResponse(Response* response,
                                  IXML_Element* doc,
                                  const char* requestUri,
                                  BOOL changeUri,
                                  BOOL useObjectUri,
                                  int slashFlag,
                                  BOOL saveChanges);

#endif /* OBIX_SERVER_H_ */
