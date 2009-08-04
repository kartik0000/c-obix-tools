/** @file
 * @todo add description here
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#include <stdlib.h>
#include <string.h>
#include <lwl_ext.h>
#include "obix_comm.h"
#include "obix_client.h"
#include "obix_batch.h"

#define OBIX_BATCH_EMPTY_RESULT 1

static void obix_batch_commandFree(oBIX_BatchCmd* command)
{
    if (command->paramUri != NULL)
    {
        free(command->paramUri);
    }
    if (command->newValue != NULL)
    {
        free(command->newValue);
    }
    free(command);
}

static void obix_batch_resultClear(oBIX_Batch* batch, BOOL totally)
{
    if (batch->result != NULL)
    {
        int i;
        for (i = 0; i < batch->commandCounter; i++)
        {
            oBIX_BatchResult result = batch->result[i];
            if (result.value != NULL)
            {
                free(result.value);
            }
            if (result.obj != NULL)
            {
                ixmlElement_freeOwnerDocument(result.obj);
            }

            if (!totally)
            {
                result.status = OBIX_BATCH_EMPTY_RESULT;
                result.obj = NULL;
                result.value = NULL;
            }
        }

        if (totally)
        {
            free(batch->result);
            batch->result = NULL;
        }
    }
}

static int obix_batch_resultInit(oBIX_Batch* batch)
{
    obix_batch_resultClear(batch, FALSE);

    if (batch->result == NULL)
    {
        oBIX_BatchResult* result =
            (oBIX_BatchResult*) calloc(batch->commandCounter,
                                       sizeof(oBIX_BatchResult));
        if (result == NULL)
        {
            log_error("Not enough memory.");
            return OBIX_ERR_NO_MEMORY;
        }

        // init all results as empty.
        int i;
        for (i = 0; i < batch->commandCounter; i++)
        {
            result[i].status = OBIX_BATCH_EMPTY_RESULT;
        }

        batch->result = result;
    }

    return OBIX_SUCCESS;
}

oBIX_Batch* obix_batch_create(int connectionId)
{
    Connection* connection;
    int error = connection_get(connectionId, TRUE, &connection);
    if (error != OBIX_SUCCESS)
    {
        return NULL;
    }

    oBIX_Batch* batch = (oBIX_Batch*) malloc(sizeof(oBIX_Batch));
    if (batch == NULL)
    {
        log_error("Unable to initialize new Batch instance: "
                  "Not enough memory.");
        return NULL;
    }

    // initialize default values
    batch->connection = connection;
    batch->commandCounter = 0;
    batch->command = NULL;
    batch->result = NULL;
    return batch;
}

static int strnullcpy(char** dest, const char* source)
{
    if (source == NULL)
    {
        *dest = NULL;
        return 0;
    }

    *dest = (char*) malloc(strlen(source) + 1);
    if (*dest == NULL)
    {
        return -1;
    }

    strcpy(*dest, source);
    return 0;
}

static int obix_batch_addCommand(oBIX_Batch* batch,
                                 OBIX_BATCH_CMD_TYPE cmdType,
                                 int deviceId,
                                 const char* paramUri,
                                 const char* newValue,
                                 OBIX_DATA_TYPE dataType)
{
    if (batch == NULL)
    {
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    obix_batch_resultClear(batch, TRUE);

    Device* device;
    int error = device_get(batch->connection, deviceId, &device);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }
    if (deviceId == 0)
    {
        device = NULL;
        if (paramUri == NULL)
        {
            return OBIX_ERR_INVALID_ARGUMENT;
        }
    }

    oBIX_BatchCmd* command = (oBIX_BatchCmd*) malloc(sizeof(oBIX_BatchCmd));
    char* paramUriCopy;
    char* newValueCopy;
    error = strnullcpy(&paramUriCopy, paramUri);
    error += strnullcpy(&newValueCopy, newValue);
    if ((command == NULL) || (error != 0))
    {
        log_error("Unable to add a command to the Batch object: "
                  "Not enough memory.");
        return OBIX_ERR_NO_MEMORY;
    }

    command->type = cmdType;
    command->id = (batch->commandCounter)++;
    command->device = device;
    command->paramUri = paramUriCopy;
    // next two field are used only by write operation
    command->newValue = newValueCopy;
    command->dataType = dataType;

    command->next = NULL;

    oBIX_BatchCmd* lastCmd = batch->command;
    if (lastCmd == NULL)
    {	// no commands in batch, put the first one
        batch->command = command;
        return command->id + 1;
    }

    // find the last command in the batch
    while (lastCmd->next != NULL)
    {
        lastCmd = lastCmd->next;
    }
    lastCmd->next = command;
    return command->id + 1;
}

int obix_batch_readValue(oBIX_Batch* batch,
                         int deviceId,
                         const char* paramUri)
{
    return obix_batch_addCommand(batch,
                                 OBIX_BATCH_READ_VALUE,
                                 deviceId,
                                 paramUri,
                                 NULL, OBIX_T_BOOL); // these are not used
}

int obix_batch_read(oBIX_Batch* batch,
                    int deviceId,
                    const char* paramUri)
{
    return obix_batch_addCommand(batch,
                                 OBIX_BATCH_READ,
                                 deviceId,
                                 paramUri,
                                 NULL, OBIX_T_BOOL); // these are not used
}

int obix_batch_writeValue(oBIX_Batch* batch,
                          int deviceId,
                          const char* paramUri,
                          const char* newValue,
                          OBIX_DATA_TYPE dataType)
{
    return obix_batch_addCommand(batch,
                                 OBIX_BATCH_WRITE_VALUE,
                                 deviceId,
                                 paramUri,
                                 newValue,
                                 dataType);
}

int obix_batch_removeCommand(oBIX_Batch* batch, int commandId)
{
    // all command id's are actually 1 less than shown to the user
    commandId--;

    if (batch == NULL)
    {
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    obix_batch_resultClear(batch, TRUE);

    oBIX_BatchCmd* parent = NULL;
    oBIX_BatchCmd* child = batch->command;

    while (child != NULL)
    {
        if (child->id == commandId)
        {
            if (parent == NULL)
            {
                batch->command = child->next;
            }
            else
            {
                parent->next = child->next;
            }
            obix_batch_commandFree(child);
            return OBIX_SUCCESS;
        }
        parent = child;
        child = child->next;
    }

    // if we are here it means that we did not found required command
    return OBIX_ERR_INVALID_STATE;
}

int obix_batch_send(oBIX_Batch* batch)
{
    if (batch == NULL)
    {
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    int error = obix_batch_resultInit(batch);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    return (batch->connection->comm->sendBatch)(batch);
}

const oBIX_BatchResult* obix_batch_getResult(oBIX_Batch* batch, int commandId)
{
    if ((batch == NULL) || (batch->result == NULL))
    {
        return NULL;
    }

    // all command ID's are actually 1 less than are shown to the user
    commandId--;

    if (batch->result[commandId].status == OBIX_BATCH_EMPTY_RESULT)
    {
        return NULL;
    }

    return &(batch->result[commandId]);
}

void obix_batch_free(oBIX_Batch* batch)
{
    if (batch == NULL)
    {
        return;
    }

    oBIX_BatchCmd* command = batch->command;

    // delete all commands
    while (command != NULL)
    {
        oBIX_BatchCmd* next = command->next;
        obix_batch_commandFree(command);
        command = next;
    }

    // delete results
    free(batch->result);

    free(batch);
}
