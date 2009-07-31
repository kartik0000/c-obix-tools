/** @file
 * @todo add description here
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#ifndef OBIX_BATCH_H_
#define OBIX_BATCH_H_

typedef enum
{
    OBIX_BATCH_READ_VALUE,
    OBIX_BATCH_READ,
    OBIX_BATCH_WRITE_VALUE
} OBIX_BATCH_CMD_TYPE;

typedef struct _oBIX_BatchCmd
{
    OBIX_BATCH_CMD_TYPE type;
    int id;
    Device* device;
    char* paramUri;
    char* newValue;
    OBIX_DATA_TYPE dataType;
    struct _oBIX_BatchCmd* next;
}
oBIX_BatchCmd;

struct _oBIX_Batch
{
    Connection* connection;
    int commandCounter;
    oBIX_BatchCmd* command;
    oBIX_BatchResult* result;
};

#endif /* OBIX_BATCH_H_ */
