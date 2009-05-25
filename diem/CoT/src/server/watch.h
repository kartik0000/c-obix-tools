/** @file
 * @todo add description here
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#ifndef WATCH_H_
#define WATCH_H_

#include <ixml_ext.h>

extern const char* OBIX_META_WATCH_UPDATED_YES;
extern const char* OBIX_META_WATCH_UPDATED_NO;

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
    int id;
    /**
     * Id of the timer which removes old unused Watch object.
     */
    int leaseTimer;
    /**
     * Pointer to the list of items monitored by this Watch object.
     */
    oBIX_Watch_Item* items;
}
oBIX_Watch;

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

BOOL obixWatch_isWatchUri(const char* uri);

int obixWatch_resetLeaseTimer(oBIX_Watch* watch, const char* newPeriod);

#endif /* WATCH_H_ */
