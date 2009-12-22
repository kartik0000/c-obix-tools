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
 * Defines interface for a table storage (like hashtable).
 * The data is stored as key-value pairs, where key is a string and value can be
 * of any type.
 *
 * @author Andrey Litvinov
 */

#ifndef TABLE_H_
#define TABLE_H_

/** Table storage object. */
typedef struct _Table Table;

/**
 * Creates and returns new table storage.
 *
 * @param initialSize Storage size is auto adjusted every time when no more free
 * 					space is left, but it takes time. So it is good idea to
 * 					choose appropriate size from the beginning.
 */
Table* table_create(int initialSize);

/**
 * Adds new data pair to the specified table.
 *
 * @param key It should be unique and not @a NULL. No checks for these rules are
 * 			done, so wrong key will cause unpredicted behavior. The key string
 * 			is copied, so there is no need to keep the original string.
 * @todo At least checking that the key is unique could be implemented.
 * @param value Reference to any type of data.
 * @return @a 0 if data was added successfully, @a -1 if error occurred.
 */
int table_put(Table* table, const char* key, void* value);

/**
 * Retrieves value corresponding to the provided key.
 * @return @a NULL if no such key found.
 */
void* table_get(Table* table, const char* key);

/**
 * Removes key-value pair from the table.
 * @return @a 0 if the pair was removed successfully, @a -1 otherwise.
 */
int table_remove(Table* table, const char* key);

/**
 * Releases memory allocated for the table.
 */
void table_free(Table* table);

/**
 * Returns amount of elements in the table.
 */
int table_getCount(Table* table);

/**
 * Returns array of keys.
 *
 * @param keys Reference to the keys array is returned here.
 * @return Number of elements in the @a keys array.
 */
int table_getKeys(Table* table, const char*** keys);

/**
 * Returns array of values.
 *
 * @param values Reference to the values array is returned here.
 * @return Number of elements in the @a values array.
 */
int table_getValues(Table* table, const void*** values);

#endif /* TABLE_H_ */
