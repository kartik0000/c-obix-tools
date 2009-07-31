/** @file
 * Contains definitions of oBIX constants: names of objects,
 * contracts, facets, etc.
 * (<a href="http://obix.org/">http://obix.org/</a>)
 */

#ifndef OBIX_UTILS_H_
#define OBIX_UTILS_H_

#include <ixml_ext.h>

//TODO add description to all fields
extern const char* OBIX_HREF_ERR_BAD_URI;
extern const char* OBIX_HREF_ERR_UNSUPPORTED;
extern const char* OBIX_HREF_ERR_PERMISSION;

extern const char* OBIX_OBJ;
extern const char* OBIX_OBJ_REF;
extern const char* OBIX_OBJ_OP;
extern const char* OBIX_OBJ_LIST;
extern const char* OBIX_OBJ_ERR;
extern const char* OBIX_OBJ_BOOL;
extern const char* OBIX_OBJ_INT;
extern const char* OBIX_OBJ_REAL;
extern const char* OBIX_OBJ_STR;
extern const char* OBIX_OBJ_ENUM;
extern const char* OBIX_OBJ_ABSTIME;
extern const char* OBIX_OBJ_RELTIME;
extern const char* OBIX_OBJ_URI;
extern const char* OBIX_OBJ_FEED;

extern const char* OBIX_NAME_SIGN_UP;
extern const char* OBIX_NAME_BATCH;
extern const char* OBIX_NAME_WATCH_SERVICE;
extern const char* OBIX_NAME_WATCH_SERVICE_MAKE;
extern const char* OBIX_NAME_WATCH_ADD;
extern const char* OBIX_NAME_WATCH_REMOVE;
extern const char* OBIX_NAME_WATCH_POLLCHANGES;
extern const char* OBIX_NAME_WATCH_POLLREFRESH;
extern const char* OBIX_NAME_WATCH_DELETE;
extern const char* OBIX_NAME_WATCH_LEASE;
extern const char* OBIX_NAME_WATCH_POLL_WAIT_INTERVAL;
extern const char* OBIX_NAME_WATCH_POLL_WAIT_INTERVAL_MIN;
extern const char* OBIX_NAME_WATCH_POLL_WAIT_INTERVAL_MAX;

extern const char* OBIX_OBJ_ERR_TEMPLATE;
extern const char* OBIX_OBJ_NULL_TEMPLATE;

extern const char* OBIX_ATTR_IS;
extern const char* OBIX_ATTR_NAME;
extern const char* OBIX_ATTR_HREF;
extern const char* OBIX_ATTR_VAL;
extern const char* OBIX_ATTR_WRITABLE;
extern const char* OBIX_ATTR_DISPLAY;
extern const char* OBIX_ATTR_DISPLAY_NAME;

extern const char* XML_TRUE;
extern const char* XML_FALSE;

int obix_reltime_parseToLong(const char* str, long* period);

typedef enum {
    RELTIME_MILLIS,
    RELTIME_SEC,
    RELTIME_MIN,
    RELTIME_HOUR,
    RELTIME_DAY,
    RELTIME_MONTH,
    RELTIME_YEAR
} RELTIME_FORMAT;

char* obix_reltime_fromLong(long period, RELTIME_FORMAT);

BOOL obix_obj_implementsContract(IXML_Element* obj, const char* contract);

#endif /* OBIX_UTILS_H_ */
