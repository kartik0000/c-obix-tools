/** @file
 * @todo It supposed to be a hashtable, but now it is quite slow.
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#include <string.h>
#include <stdlib.h>
#include <table.h>

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

    // search for the free slot
    int id;
    char** keys = table->keys;
    for (id = 0; keys[id] != NULL; id++)
        ;

    //copy key
    keys[id] = (char*) malloc(strlen(key) + 1);
    if (keys[id] == NULL)
    {
        return -1;
    }
    strcpy(keys[id], key);
    // copy value
    table->values[id] = value;

    table->count++;

    return 0;
}

void* table_get(Table* table, const char* key)
{
    int i;
    char** keys = table->keys;
    for (i = 0; i < table->size; i++)
    {
        if ((keys[i] != NULL) && (strcmp(keys[i], key) == 0))
        {
            return table->values[i];
        }
    }

    // nothing is found
    return NULL;
}

int table_remove(Table* table, const char* key)
{
    int i;
    char** keys = table->keys;
    for (i = 0; i < table->size; i++)
    {
        if ((keys[i] != NULL) && (strcmp(keys[i], key) == 0))
        {
        	free(keys[i]);
            keys[i] = NULL;
            table->count--;
            return 0;
        }
    }

    // nothing is found
    return -1;
}

void table_free(Table* table)
{
    int i;
    char** keys = table->keys;
    for (i = 0; i < table->size; i++)
    {
        if (keys[i] != NULL)
        {
            free(keys[i]);
        }
    }

    free(keys);
    free(table->values);

    free(table);
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
