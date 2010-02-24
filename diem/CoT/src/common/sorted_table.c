/* *****************************************************************************
 * Copyright (c) 2010 Andrey Litvinov
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
 * Table storage in which keys are stored in sorted array. Adding to such array
 * is quite slow, but search is performed faster than in plain table.
 *
 * @see table.h, table.c
 *
 * @author Andrey Litvinov
 */

#include <string.h>
#include <stdlib.h>
#include <table.h>

/** Table storage structure. */
struct _Table
{
    int size;
    int count;

    char** keys;
    void** values;
};

Table* table_create(int initialSize)
{
    Table* table = (Table*) malloc(sizeof(Table));
    if (table == NULL)
    {
        return NULL;
    }

    table->keys = (char**) calloc(initialSize, sizeof(char*));
    table->values = (void**) calloc(initialSize, sizeof(void*));

    if ((table->keys == NULL) || (table->values == NULL))
    {
        return NULL;
    }

    table->size = initialSize;
    table->count = 0;

    return table;
}

/** Extends size of the table. Size is multiplied by 2. */
static int table_extend(Table* table)
{
    // increase table size 2 times
    int newSize = table->size << 1;
    table->keys = (char**) realloc(table->keys, newSize * sizeof(char*));
    table->values = (void**) realloc(table->values, newSize * sizeof(void*));
    if ((table->keys == NULL) || (table->values == NULL))
    {
        return -1;
    }

    // initialize new allocated space
    int i;
    for (i = table->size; i < newSize; i++)
    {
        table->keys[i] = NULL;
    }

    table->size = newSize;

    return 0;
}

static void* binary_search(Table* table, const char* key, int* position)
{
    if (table->count == 0)
    {
        *position = 0;
        return NULL;
    }

    char** keys = table->keys;
    int low = 0;
    int high = table->count - 1;
    int middle;
    int comparison;

    while (low < high)
    {
        middle = (low + high) >> 1;
        comparison = strcmp(keys[middle], key);
        if (comparison < 0) // element in array is smaller than key
        {
            low = middle + 1;
        }
        else if (comparison > 0) // element in array is bigger than key
        {
            high = middle - 1;
        }
        else // key is found
        {
            *position = middle;
            return table->values[middle];
        }
    }

    if (high < 0)
    {
    	*position = 0;
    	return NULL;
    }

    *position = high;

    // key is not yet found
    if (strcmp(keys[high], key) == 0)
    {
        return table->values[high];
    }
    else
    {
        // no such key in the table
        return NULL;
    }
}

int table_put(Table* table, const char* key, void* value)
{
    if (table->count == table->size)
    {
        int error = table_extend(table);
        if (error != 0)
        {
            return error;
        }
    }

    char** keys = table->keys;
    void** values = table->values;

    // search for the correct place in alphabetical order
    int id;
    void* storedValue = binary_search(table, key, &id);
    if (storedValue != NULL)
    {	// such key already exists
        return -2;
    }

    if (table->count > 0)
    {
        // check whether we should store our new key before or after returned id
        if (strcmp(key, keys[id]) > 0)
        {
            id++;
        }
        // move the rest of array to the right
        int i;
        for (i = table->count; i > id; i--)
        {
            keys[i] = keys[i-1];
            values[i] = values[i-1];
        }
    }

    //copy key
    keys[id] = (char*) malloc(strlen(key) + 1);
    if (keys[id] == NULL)
    {
        return -1;
    }
    strcpy(keys[id], key);
    // copy value
    values[id] = value;

    table->count++;

    return 0;
}

void* table_get(Table* table, const char* key)
{
    int pos;
    return binary_search(table, key, &pos);
}

void* table_remove(Table* table, const char* key)
{
    // implement me
	int position;
	void* valueToReturn = binary_search(table, key, &position);
	if (valueToReturn == NULL) // no such key found
	{
		return NULL;
	}

	// delete key and move all remaining elements in arrays in order to close
	// the gap

	char** keys = table->keys;
	void** values = table->values;

	free(keys[position]);

	for (position++; position < table->count; position++)
	{
		keys[position - 1] = keys[position];
		values[position - 1] = values[position];
	}

	table->count--;
	// clean the last slot
	keys[table->count] = NULL;
	values[table->count] = NULL;

	return valueToReturn;
}

void table_free(Table* table)
{
    int i;
    char** keys = table->keys;
    for (i = 0; i < table->count; i++)
    {
        free(keys[i]);
    }

    free(keys);
    free(table->values);

    free(table);
}

int table_getCount(Table* table)
{
    return table->count;
}

int table_getKeys(Table* table, const char*** keys)
{
    *keys = (const char**) (table->keys);
    return table->count;
}

int table_getValues(Table* table, const void*** values)
{
    *values = (const void**) (table->values);
    return table->count;
}
