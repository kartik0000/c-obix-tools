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
 * Defines the interface to the XML storage.
 * @todo Add error codes enumeration for storage errors.
 *
 * @author Andrey Litvinov
 */
#ifndef XML_STORAGE_H_
#define XML_STORAGE_H_

#include <ixml_ext.h>

// TODO think whether we can use only DOM structures and no char arrays

/** @name URIs of various system objects.
 * These objects are not accessible for a client. They are used by server to
 * generate quickly XML data structures.
 * @{ */
/** URI of Watch object stub. */
extern const char* OBIX_SYS_WATCH_STUB;
/** URI of Error object stub. */
extern const char* OBIX_SYS_ERROR_STUB;
/** URI of WatchOut object stub. */
extern const char* OBIX_SYS_WATCH_OUT_STUB;
/** @} */

/** Name of meta tag. This tag is used to store meta variables for any object
 * in storage, which is not visible for clients. */
extern const char* OBIX_META;

/** Name of meta variable, which is used to define handler functions for
 * oBIX operations. */
extern const char* OBIX_META_VAR_HANDLER_ID;

/**
 * Initializes storage. Should be executed only once on startup.
 *
 * @param serverAddr Address of the server. Storage should know it in cases
 *        when requested URI contains full address. If @a NULL is
 *        provided than address will be retrieved from the Lobby
 *        object.
 *
 * @return error code or @a 0 on success.
 */
int xmldb_init(const char* serverAddr);

/**
 * Stops work of the storage and releases all resources.
 */
void xmldb_dispose();

/**
 * Retrieves XML node with specified URI from the storage.
 *
 * @param href Address of the XML node to be retrieved.
 * @param slashFlag Slash flag is returned here. This flag shows whether
 * 			requested URI differs from URI of returned object in trailing slash.
 * 			@li @a 0 if both URI had the same ending symbol;
 * 			@li @a 1 if the object in the storage had trailing slash but
 * 					requested URI hadn't;
 *			@li @a -1 if requested URI had trailing slash, but the object
 *					hadn't.
 * @return XML data in plain text format or NULL on error.
 */
char* xmldb_get(const char* href, int* slashFlag);

/**
 * Retrieves DOM structure of XML node with specified URI from the storage.
 *
 * @param href Address of the XML node to be retrieved.
 * @param slashFlag Slash flag is returned here. This flag shows whether
 * 			requested URI differs from URI of returned object in trailing slash.
 * 			@li @a 0 if both URI had the same ending symbol;
 * 			@li @a 1 if the object in the storage had trailing slash but
 * 					requested URI hadn't;
 *			@li @a -1 if requested URI had trailing slash, but the object
 *					hadn't.
 * @return XML data as a DOM structure NULL on error.
 */
IXML_Element* xmldb_getDOM(const char* href, int* slashFlag);

/**
 * Retrieves DOM structure of system object from the storage.
 *
 * @param objType URI of the system object (e.g. #OBIX_SYS_WATCH_STUB).
 */
IXML_Element* xmldb_getObixSysObject(const char* objType);

/**
 * Adds new XML node to the storage. The node should contain
 * @a href attribute. If node with the same URI already exists in
 * the storage then new node is not added.
 *
 * @param data XML node represented in plain text format.
 * @return Error code or @a 0 on success.
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
 * @param slashFlag Slash flag is returned here. This flag shows whether
 * 			requested URI differs from URI of returned object in trailing slash.
 * 			@li @a 0 if both URI had the same ending symbol;
 * 			@li @a 1 if the object in the storage had trailing slash but
 * 					requested URI hadn't;
 *			@li @a -1 if requested URI had trailing slash, but the object
 *					hadn't.
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
 * @param href Address of the node to be deleted.
 */
int xmldb_delete(const char* href);

/**
 * Loads XML file to the storage.
 *
 * @param filename Name of the XML file to load.
 * @return Error code or 0 on success.
 */
int xmldb_loadFile(const char* filename);

/**
 * Prints to standard log the current contents of the storage.
 */
void xmldb_printDump();

/**
 * Returns string representation of current storage contents.
 * @note Don't forget to free memory allocated for the dump after usage.
 */
char* xmldb_getDump();

/**
 * Checks that provided URI starts with server's URI
 * @return @a 0 if provided URI starts with server's URI.
 */
int xmldb_compareServerAddr(const char* uri);

/**
 * Creates full URI consisting of server's URI + @a absUri +/- trailing slash.
 *
 * @param slashFlag Defines whether trailing slash should be added or removed
 * 			from @a absUri
 * 			@li @a 0 nothing to be done;
 * 			@li @a 1 add trailing slash;
 *			@li @a -1 remove trailing slash.
 * @note Don't forget to clear memory after usage of the string.
 */
char* xmldb_getFullUri(const char* absUri, int slashFlag);

/**
 * Returns server's URI.
 * @see xmldb_getServerAddressLength
 */
const char* xmldb_getServerAddress();

/**
 * Returns length of server's URI.
 * @see xmldb_getServerAddress
 */
int xmldb_getServerAddressLength();

/**
 * Adds new meta variable to the provided element in the storage.
 *
 * @param name Name of the meta variable.
 * @param value Value of meta variable.
 * @return Link to the meta value node.
 */
IXML_Node* xmldb_putMetaVariable(IXML_Element* element,
                                 const char* name,
                                 const char* value);

/**
 * Deletes meta variable.
 *
 * @return @a 0 on success; @a -1 on error.
 */
int xmldb_deleteMetaVariable(IXML_Node* meta);

/**
 * Sets new value for provided meta variable.
 *
 * @return @a 0 on success; error code otherwise.
 */
int xmldb_changeMetaVariable(IXML_Node* meta, const char* newValue);

/**
 * Returns meta variable of the provided object.
 *
 * @note Method assumes that all variables have unique names. Otherwise it
 * 		returns the first found variable with specified name.
 *
 * @return @a NULL on error.
 */
IXML_Node* xmldb_getMetaVariable(IXML_Element* obj, const char* name);

/**
 * Returns value of meta variable of the provided object.
 *
 * @note Method assumes that all variables have unique names. Otherwise it
 * 		returns value of the first found variable with specified name.
 *
 * @return @a NULL on error.
 */
const char* xmldb_getMetaVariableValue(IXML_Element* obj, const char* name);

/**
 * Returns meta tag of the object.
 * @param doc Object, whose meta data should be retrieved.
 * @return Meta tag if exists, @a NULL otherwise.
 */
IXML_Element* xmldb_getMetaInfo(IXML_Element* doc);

/**
 * Removes all #OBIX_META tags from the document.
 */
void xmldb_deleteMetaInfo(IXML_Element* doc);

#endif /*XML_STORAGE_H_*/
