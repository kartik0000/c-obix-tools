/** @file
 * Defines the interface to the XML storage.
 * TODO: Add error codes enumeration
 */
#ifndef XML_STORAGE_H_
#define XML_STORAGE_H_

#include <ixml_ext.h>

// TODO global refactor - think whether we can use only DOM structures and no char arrays
//TODO add description

extern const char* OBIX_SYS_WATCH_STUB;
extern const char* OBIX_SYS_ERROR_STUB;
extern const char* OBIX_SYS_WATCH_OUT_STUB;

extern const char* OBIX_META;

/**
 * Initializes storage. Should be executed only once on startup.
 *
 * @param serverAddr of the server. Storage should know it in cases
 *        when requested URI contains full address. If NULL is
 *        provided that address will be retrieved from the Lobby
 *        object.
 *
 * @return error code or 0 on success.
 */
int xmldb_init(const char* serverAddr);

/**
 * Stops work of the storage and releases all resources.
 */
void xmldb_dispose();

/**
 * Retrieves XML node with specified URI from the storage.
 *
 * @param href address of the XML node to be retrieved.
 * @return XML data in plain text format or NULL on error.
 */
char* xmldb_get(const char* href, int* slashFlag);

/**
 * Retrieves DOM structure of XML node with specified URI from the storage.
 *
 * @param href address of the XML node to be retrieved.
 * @return XML data as a DOM structure NULL on error.
 */
IXML_Element* xmldb_getDOM(const char* href, int* slashFlag);

IXML_Element* xmldb_getObixSysObject(const char* objType);

/**
 * Returns the remaining slash flag of the last URI comparison operation.
 * URI comparison in database is done ignoring ending slash ('/'). It is done
 * every time when some object is retrieved from the storage.
 * Slash flag shows whether requested URI and URI found in storage contain
 * ending slash.
 *
 * Slash flag is set by following functions:
 * @li #xmldb_get;
 * @li #xmldb_getDOM;
 * @li #xmldb_update;
 * @li #xmldb_put.
 * //@li #xmldb_compareUri.
 * @note The value of the flag can be changed also by other xmldb
 * functions.
 *
 * @return @li @b 0 if both URI had the same ending symbol;
 *         @li @b 1 if the object in the storage had ending slash but requested
 *                  URI hadn't;
 *         @li @b -1 if requested URI had ending slash, but the object hadn't.
 */
//int xmldb_getLastUriCompSlashFlag();

/**
 * Compares two URI ignoring ending slash ('/').
 * This function also sets the slash flag.
 *
 * @see #xmldb_getLastUriCompSlashFlag
 * @param uri1 first URI to compare (object's URI for the slash flag).
 * @param uri2 second URI to compare (request URI for the slash flag).
 * @return @a TRUE is URI match, @a FALSE otherwise.
 */
//BOOL xmldb_compareUri(const char* uri1, const char* uri2);

/**
 * Adds new XML node to the storage. The node should contain
 * @a href attribute. If node with the same URI already exists in
 * the storage then new node is not added.
 *
 * @param data XML node represented in plain text format.
 * @return error code or @a 0 on success.
 */
int xmldb_put(const char* data);

int xmldb_putDOM(IXML_Element* data);

/**
 * Updates XML node to the storage. Only @a val attribute is
 * updated and only for nodes which have @a writable attribute
 * equal to @a true.
 *
 * @param data XML node represented in plain text format.
 *             The node should contain @a val attribute.
 *             Other data is ignored.
 * @param href URI of the object to be updated.
 * @param updatedNode If not @a NULL is provided and update is successful than
 *                    the address of the updated node will be written there.
 * @return @li @b 0 if value is successfully overwritten;
 * 		   @li @b 1 if request is processed but new value is the same;
 * 		   @li @b -1 if request data is corrupted/wrong format;
 * 		   @li @b -2 if object with specified href is not found;
 * 		   @li @b -3 if object is not writable;
 * 		   @li @b -4 if request failed because of internal server error.
 */
int xmldb_updateDOM(IXML_Element* input,
                    const char* href,
                    IXML_Element** updatedNode,
                    int* slashFlag);

/**
 * Removes XML node from the storage.
 *
 * @param href address of the node to be deleted.
 */
int xmldb_delete(const char* href);

/**
 * Loads xml file to the storage.
 *
 * @param filename name of the xml file to load.
 * @return error code or 0 on success.
 */
int xmldb_loadFile(const char* filename);

/**
 * Prints to standard log current contents of the storage.
 */
void xmldb_printDump();

char* xmldb_getDump();

int xmldb_compareServerAddr(const char* uri);

char* xmldb_getFullUri(const char* absUri, int slashFlag);

const char* xmldb_getServerAddress();

const int xmldb_getServerAddressLength();


//TODO rename and describe

IXML_Node* xmldb_putMeta(IXML_Element* element, const char* name, const char* value);

int xmldb_deleteMeta(IXML_Node* attr);

int xmldb_updateMeta(IXML_Node* meta, const char* newValue);

/**
 * Returns meta tag of the object.
 * @param doc Node whose meta data should be retrieved.
 * @return Meta tag if exists, @a NULL otherwise.
 */
IXML_Element* getMetaInfo(IXML_Element* doc);

void removeMetaInfo(IXML_Element* doc);

#endif /*XML_STORAGE_H_*/
