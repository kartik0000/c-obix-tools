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
 * Contains implementation of the part of C oBIX Client API, which deals with
 * Batch operations.
 *
 * @see obix_batch.h
 *
 * @author Andrey Litvinov
 */

#include <stdlib.h>
#include <string.h>
#include <log_utils.h>
#include <obix_utils.h>
#include "obix_comm.h"
#include "obix_client.h"
#include "obix_batch.h"

#define OBIX_BATCH_EMPTY_RESULT 1

/** Releases memory allocated for Batch command.*/
static void obix_batch_commandFree(oBIX_BatchCmd* command)
{
    if (command->uri != NULL)
    {
        free(command->uri);
    }
    if (command->input != NULL)
    {
        free(command->input);
    }
    free(command);
}

/**
 * Cleans stored results of Batch execution
 *
 * @param totally If #TRUE, then results array is deleted completely. Otherwise,
 * 				only values of results array elements are cleared.
 */
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

/**
 * Creates results array for the provided Batch object.
 * @return #OBIX_SUCCESS on success, negative error code otherwise.
 */
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
            result[i].obj = NULL;
            result[i].value = NULL;
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

/**
 * Makes a copy of string and returns a pointer to it.
 * If source string is @a NULL, result copy is also @a NULL.
 *
 * @param dest A link to the copied string will be returned here.
 * @param source String to be copied.
 * @return @a 0 on success; -1 if there is not enough memory.
 */
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

/** Adds new command to the Batch object*/
static int obix_batch_addCommand(oBIX_Batch* batch,
                                 OBIX_BATCH_CMD_TYPE cmdType,
                                 int deviceId,
                                 const char* uri,
                                 const char* input,
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
        if (uri == NULL)
        {
            return OBIX_ERR_INVALID_ARGUMENT;
        }
    }

    oBIX_BatchCmd* command = (oBIX_BatchCmd*) malloc(sizeof(oBIX_BatchCmd));
    char* uriCopy;
    char* inputCopy;
    error = strnullcpy(&uriCopy, uri);
    error += strnullcpy(&inputCopy, input);
    if ((command == NULL) || (error != 0))
    {
        log_error("Unable to add a command to the Batch object: "
                  "Not enough memory.");
        return OBIX_ERR_NO_MEMORY;
    }

    command->type = cmdType;
    command->id = (batch->commandCounter)++;
    command->device = device;
    command->uri = uriCopy;
    // next two fields are used only by write operation
    command->input = inputCopy;
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

int obix_batch_invoke(oBIX_Batch* batch,
                      int deviceId,
                      const char* operationUri,
                      const char* input)
{
    // special case with no input arguments
    if (input == NULL)
    {
        return obix_batch_addCommand(batch,
                                     OBIX_BATCH_INVOKE,
                                     deviceId,
                                     operationUri,
                                     NULL,
                                     OBIX_T_BOOL); // this is not used
    }

    IXML_Element* inputXML = ixmlElement_parseBuffer(input);
    if (inputXML == NULL)
    {
        log_error("obix_batch_invoke: Unable to parse \"input\" object. "
                  "Check XML format.");
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    int error = obix_batch_invokeXML(batch, deviceId, operationUri, inputXML);
    ixmlElement_freeOwnerDocument(inputXML);
    return error;
}

int obix_batch_invokeXML(oBIX_Batch* batch,
                         int deviceId,
                         const char* operationUri,
                         IXML_Element* input)
{
    char* inputString;

    if (input == NULL)
    {
        inputString = NULL;
    }
    else
    {
        // add name attribute ("in"). see obix:Invoke contract.
        // TODO this action is HTTP protocol specific -
        // it should be moved to obix_http.c
        ixmlElement_setAttributeWithLog(input, OBIX_ATTR_NAME, "in");
        inputString = ixmlPrintNode(ixmlElement_getNode(input));
    }

    int error = obix_batch_addCommand(batch,
                                      OBIX_BATCH_INVOKE,
                                      deviceId,
                                      operationUri,
                                      inputString,
                                      OBIX_T_BOOL); // this is not used
    free(inputString);
    return error;
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
