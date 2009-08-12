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

#ifndef WATCH_H_
#define WATCH_H_

#include <pthread.h>

#include <ixml_ext.h>
#include "response.h"

extern const char* OBIX_META_WATCH_UPDATED_YES;
extern const char* OBIX_META_WATCH_UPDATED_NO;

extern const long OBIX_WATCH_LEASE_NO_CHANGE;

/**
 * Represents a separate watch item.
 *
 * Watch item is a reference to the object which
 * state is monitored by watch.
 */
typedef struct oBIX_Watch_Item
{
    //TODO: place here link to the monitored object
    // and link to the value of meta tag 'updated'
    /**
     * URI of the object which state is monitored.
     */
    char* uri;
    /**
     * Link to the corresponding object in the storage.
     */
    IXML_Element* doc;
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

typedef void (*obixWatch_pollHandler)(oBIX_Watch* watch,
                                      Response* response,
                                      const char* uri);

/**@name oBIX Watch utilities @{*//////////////////////////////////////////////
// TODO: what about creating a separate file for these watch utilities?

int obixWatch_init();

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

oBIX_Watch* obixWatch_get(int watchId);

char* obixWatch_getUri(oBIX_Watch* watch);

oBIX_Watch* obixWatch_getByUri(const char* uri);


/**
 * @todo describe me; rename to createWatchItem
 * @param watch
 * @param uri
 * @param watchItem
 * @return @li @a  0 - Watch item is added successfully;
 *         @li @a -1 - URI not found;
 *         @li @a -2 - URI is an @a <op/> object;
 *         @li @a -3 - Internal error.
 */
int obixWatch_createWatchItem(oBIX_Watch* watch,
                              const char* uri,
                              oBIX_Watch_Item** watchItem);


int obixWatch_appendWatchItem(oBIX_Watch* watch, oBIX_Watch_Item* item);

int obixWatch_deleteWatchItem(oBIX_Watch* watch, const char* watchItemUri);

oBIX_Watch_Item* obixWatch_getWatchItem(oBIX_Watch* watch, const char* watchItemUri);

BOOL obixWatchItem_isUpdated(oBIX_Watch_Item* item);

int obixWatchItem_setUpdated(oBIX_Watch_Item* item, BOOL isUpdated);

void obixWatch_updateMeta(IXML_Element* meta);

BOOL obixWatch_isWatchUri(const char* uri);

int obixWatch_resetLeaseTimer(oBIX_Watch* watch, long newPeriod);

int obixWatch_processTimeUpdates(const char* uri, IXML_Element* element);

BOOL obixWatch_isLongPoll(oBIX_Watch* watch);

int obixWatch_holdPoll(obixWatch_pollHandler pollHandler,
                       oBIX_Watch* watch,
                       Response* response,
                       const char* uri,
                       BOOL maxWait);

#endif /* WATCH_H_ */
