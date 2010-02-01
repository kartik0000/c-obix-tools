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
 * Interface of Watch module.
 * This module implements oBIX Watch logic at the server.
 *
 * @author Andrey Litvinov
 */

#ifndef WATCH_H_
#define WATCH_H_

#include <pthread.h>

#include <ixml_ext.h>
#include "response.h"

/** @name Values of XML meta attributes.
 * These are used to mark watched objects in the storage.
 * @{ */
extern const char* OBIX_META_WATCH_UPDATED_YES;
extern const char* OBIX_META_WATCH_UPDATED_NO;
/** @} */

/**
 * Name of the meta tag, which stores pointer to a watch item, subscribed for
 * the object.
 */
extern const char* OBIX_META_VAR_WATCHITEM_P;

/**
 * Represents a separate watch item.
 *
 * Watch item is a reference to the object which
 * state is monitored by watch.
 */
typedef struct oBIX_Watch_Item
{
    /**
     * URI of the object which state is monitored.
     */
    char* uri;
    /**
     * Tells whether this watch item monitors operation (<op/>) object.
     */
    BOOL isOperation;
    /**
     * Link to the corresponding object in the storage.
     */
    IXML_Element* doc;
    /**
     * When watch item is subscribed for operation object, this field contains
     * a reference to the operation invocation parameters.
     */
    IXML_Element* input;
    /**
     * Shows whether the object has been updated since last request.
     * In fact it is a link to the updated attribute of meta tag
     * stored at #doc.
     */
    IXML_Node* updated;
    /**
     * Link to the next watch item in a list of items.
     */
    struct oBIX_Watch_Item* next;
}
oBIX_Watch_Item;

/**
 * Represents an oBIX Watch object.
 */
typedef struct oBIX_Watch
{
    /** Id of the watch object. */
    int id;
    /** Id of the timer which removes old unused Watch object. */
    int leaseTimerId;
    /** Id of the scheduled long poll request handler. */
    int pollTaskId;
    /** Mutex for synchronization with scheduled long poll handler. */
    pthread_mutex_t pollTaskMutex;
    /** Condition saying that long poll handler is completed. */
    pthread_cond_t pollTaskCompleted;
    /** Defines whether long poll task is now waiting for max time. */
    BOOL isPollWaitingMax;
    /** Minimum waiting time for long poll requests. */
    long pollWaitMin;
    /** Maximum waiting time for long poll requests. */
    long pollWaitMax;
    /** Pointer to the list of items monitored by this Watch object. */
    oBIX_Watch_Item* items;
}
oBIX_Watch;

/**
 * Prototype of a function, which handles delayed Watch.pollChanges request.
 */
typedef void (*obixWatch_pollHandler)(oBIX_Watch* watch,
                                      Response* response,
                                      const char* uri);

/**
 * Initializes Watch engine.
 * @return @a 0 on success; negative error code otherwise.
 */
int obixWatch_init();

/**
 * Stops Watch engine and releases all allocated memory.
 * @return @a 0 on success; negative error code otherwise.
 */
int obixWatch_dispose();

/**
 * Creates new oBIX Watch instance.
 *
 * @todo Move error codes to the global error enum.
 * @param watch Address where pointer to the new oBIX Watch object will be
 *              written to.
 * @return @li @a >0 - Index of the new watch object;
 *         @li @a -1 - Unable to allocate enough memory for a new object;
 *         @li @a -2 - Maximum number of watches is reached;
 *         @li @a -3 - Storage error - unable to save watch;
 *         @li @a -4 - Internal error.
 */
int obixWatch_create(IXML_Element** watchDOM);

/**
 * Deletes watch instance.
 *
 * @param watch Watch instance to be deleted.
 * @return @a 0 on success, negative error code otherwise.
 */
int obixWatch_delete(oBIX_Watch* watch);

/**
 * Returns Watch object with specified id.
 * @return @a NULL if no object which such id found.
 */
oBIX_Watch* obixWatch_get(int watchId);

/**
 * Generates URI of the provided Watch object.
 * @note Don't forget to free memory after usage.
 */
char* obixWatch_getUri(oBIX_Watch* watch);

/**
 * Returns Watch object, which has provided URI.
 * @return @a NULL if no object with such URI found.
 */
oBIX_Watch* obixWatch_getByUri(const char* uri);

/**
 * Subscribes Watch object to receive updates of provided URI.
 * @param uri Uri, which should be added to the watch list.
 * @param isOperation Tells whether this watch item is created for operation
 * 				object, or not.
 * @param watchItem Reference to the created WatchItem is returned here.
 * @return @li @a  0 - Watch item is added successfully.
 *         @li @a -1 - URI not found/
 *         @li @a -2 - URI is an @a <op/> object, but should be a value object;
 *         				or vice versa: The object should be @a <op/>, but it is
 *         				not.
 *         @li @a -3 - Internal error.
 *         @li @a -4 - URI is an @a <op/> object, which already has assigned
 *         			    handler. Thus it can't be monitored.
 */
int obixWatch_createWatchItem(oBIX_Watch* watch,
                              const char* uri,
                              BOOL isOperation,
                              oBIX_Watch_Item** watchItem);

/**
 * Removes provided URI from subscribed items.
 * @return @a 0 on success; @a -1 on error.
 */
int obixWatch_deleteWatchItem(oBIX_Watch* watch, const char* watchItemUri);

/**
 * Tells whether provided Watch Item has beed updated or not.
 */
BOOL obixWatchItem_isUpdated(oBIX_Watch_Item* item);

/**
 * Changes updated state of provided Watch Item.
 * @return @a 0 on success; error code otherwise.
 */
int obixWatchItem_setUpdated(oBIX_Watch_Item* item, BOOL isUpdated);

/**
 * Deletes saved operation input. Should be used after input has been sent to
 * remote operation handler.
 *
 * @param watchItem Watch item subscribed to an operation, which should be
 * cleaned.
 */
void obixWatchItem_clearOperationInput(oBIX_Watch_Item* watchItem);

/**
 * Saves parameters of remote operation invocation.
 *
 * @param watchItem Watch item subscribed for this operation.
 * @param uri URI of invoked operation.
 * @param response Response object, which will be used later to send back
 * 			operation results.
 * @param input Input arguments for the operation.
 */
int obixWatchItem_saveOperationInvocation(
    oBIX_Watch_Item* watchItem,
    const char* uri,
    Response* response,
    IXML_Element* input);

/**
 * Saves response object of operation, which is forwarded to the subscribed
 * user. This response can be later retrieved back with
 * #obixWatch_getSavedOperationInvocation.
 *
 * @param uri URI of invoked operation.
 * @param response Response object, which will be used later to send back
 * 			operation results.
 */
int obixWatchItem_saveRemoteOperationResponse(
    const char* uri,
    Response* response);

/**
 * Returns saved response object of operation, which was forwarded to the
 * subscribed user.
 *
 * @param uri URI of forwarded operation.
 */
Response* obixWatchItem_getSavedRemoteOperationResponse(const char* uri);

/**
 * Sets Watch attributes of the provided meta tag to "updated" state.
 */
void obixWatch_updateMeta(IXML_Element* meta);

/**
 * Checks whether provided URI is an URI of Watch object.
 */
BOOL obixWatch_isWatchUri(const char* uri);

/**
 * Restarts Watch.lease timer. It should be restarted every time when somebody
 * access the Watch.
 * @return @a 0 on success; @a -1 on error.
 */
int obixWatch_resetLeaseTimer(oBIX_Watch* watch);

/**
 * Handles update of the following time configuration variables of a Watch
 * object: @a Watch.lease, @a Watch.pollWaitInterval.min and
 * @a Watch.pollWaitInterval.max
 */
int obixWatch_processTimeUpdates(const char* uri, IXML_Element* element);

/**
 * Checks whether provided Watch object has long poll mode enabled.
 */
BOOL obixWatch_isLongPollMode(oBIX_Watch* watch);

/**
 * Holds processing of the Watch.pollChanges request.
 * Processing is blocked until:
 * @li At least one of Watch Items has been updated and
 * 		@a Watch.pollWaitInterval.min is reached;
 * @li Or Watch.pollWaitInterval.max is reached.
 *
 * @param pollHandler Function, which will be invoked to process the request.
 * @param maxWait If @a FALSE, than the request processing will be delayed for
 * 				@a Watch.pollWaitInterval.min. Otherwise, it will be delayed for
 * 				@a Watch.pollWaitInterval.max, but later this interval can be
 * 				reduced if some of Watch Items are updated.
 */
int obixWatch_holdPollRequest(obixWatch_pollHandler pollHandler,
                              oBIX_Watch* watch,
                              Response* response,
                              const char* uri,
                              BOOL maxWait);

#endif /* WATCH_H_ */
