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
 * Defines interface of POST Handler module.
 *
 * This module is responsible for handling POST requests, i.e. invoke
 * operations. Each operation has its unique id, which is stored in XML meta
 * data ('op' attribute). When server receives a POST request, it takes id of
 * called operation from its meta tag and then uses POST handler to execute
 * corresponding task.
 *
 * @author Andrey Litvinov
 */

#ifndef POST_HANDLER_H_
#define POST_HANDLER_H_

#include <ixml_ext.h>
#include "response.h"

/**
 * Prototype of a POST Handler function.
 *
 * @param response Response object, which should be used to send operation
 * 					results.
 * @param URI Requested URI.
 * @param input Parsed request input.
 */
typedef void (*obix_server_postHandler)(Response* response,
                                        const char* uri,
                                        IXML_Element* input);

/**
 * Returns handler with specified id.
 * Never returns @a NULL. If there is no handler with specified id, then it
 * returns a handler, which sends error message to the user.
 */
obix_server_postHandler obix_server_getPostHandler(int id);

#endif /* POST_HANDLER_H_ */
