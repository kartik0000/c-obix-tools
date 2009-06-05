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
                                        IXML_Document* input);

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
