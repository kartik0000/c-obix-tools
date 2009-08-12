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
 * @todo add description here
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#ifndef POST_HANDLER_H_
#define POST_HANDLER_H_

#include <ixml_ext.h>
#include "response.h"

/**@todo describe me*/
typedef void (*obix_server_postHandler)(Response* response,
                                        const char* uri,
                                        IXML_Element* input);

obix_server_postHandler obix_server_getPostHandler(int id);

/**@name POST handlers management @{*/
///**
// * Adds new POST request handler for the specified URI.
// * @param uri POST requests with this URI will be forwarded to the specified
// *            handler.
// * @param handlerFunc Function which should handle the POST request and return
// *                    oBIX answer.
// */
//int obix_server_addPostHandler(const char* uri, obix_server_postHandler handlerFunc);
//
///**@todo describe me*/
//obix_server_postHandler obix_server_getPostHandlerByUri(const char* uri);
//int obix_server_deletePostHandler(const char* uri);
/**@}*/

#endif /* POST_HANDLER_H_ */
