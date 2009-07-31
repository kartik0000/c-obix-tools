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

static void obix_batch_commandFree(oBIX_BatchCmd* command)
{
    if (paramUri != NULL)
    {
        free(paramUri);
    }
    if (newValue != NULL)
    {
        free(newValue);
    }
    free(command);
}

oBIX_Batch* obix_batch_create(int connectionId)
{
    Connection* connection;
    int error = connection_get(connectionId, TRUE, &connection);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    oBIX_Batch* batch = (oBIX_Batch*) malloc(sizeof(oBIX_Batch));
    if (batch == NULL)
    {
        log_error("Unable to initialize new Batch instance: "
                  "Not enough memory.");
        return OBIX_ERR_NO_MEMORY;
    }

    // initialize default values
    batch->connection = connection;
    batch->commandCounter = 0;
    batch->command = NULL;
    batch->result = NULL;
    return batch;
}

static int strnullcpy(char** dest, char* source)
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

    Device* device;
    int error = device_get(connection, deviceId, &device);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }
    if (deviceId == 0)
    {
        device == NULL;
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
    command->id = ++(batch->commandCounter);
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
        return command->id;
    }

    // find the last command in the batch
    while (lastCmd->next != NULL)
    {
        lastCmd = lastCmd->next;
    }
    lastCmd->next = command;
    return command->id;
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
    if (batch == NULL)
    {
        return OBIX_ERR_INVALID_ARGUMENT;
    }

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

    if (batch->result != NULL)
    {	// remove results of previous call
        free(batch->result);
    }

    return (batch->connection->comm->sendBatch)(batch);
}

const oBIX_BatchResult* obix_batch_getResult(oBIX_Batch* batch, int commandId)
{
    if (batch == NULL)
    {
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    return batch->result[commandId - 1];
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
