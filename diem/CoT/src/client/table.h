/** @file
 * @todo add description here
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#ifndef TABLE_H_
#define TABLE_H_

typedef struct _Table
{
    int size;
    int count;

    char** keys;
    void** values;
}
Table;

Table* table_create(int initialSize);
int table_put(Table* table, const char* key, void* value);
void* table_get(Table* table, const char* key);
int table_remove(Table* table, const char* key);
void table_free(Table* table);
int table_getKeys(Table* table, const char*** keys);
int table_getValues(Table* table, const void*** values);

#endif /* TABLE_H_ */
