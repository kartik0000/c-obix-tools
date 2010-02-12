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
 * Defines types, which are used by C oBIX Client Batch implementation.
 *
 * These are internal for C oBIX Client API implementation. User should not be
 * allowed to see contents of structures defined here.
 *
 * @author Andrey Litvinov
 */

#ifndef OBIX_BATCH_H_
#define OBIX_BATCH_H_

#include "obix_comm.h"

/**
 * List of operations, which can be executed in Batch command using C oBIX
 * Client API.
 */
typedef enum
{
    OBIX_BATCH_READ_VALUE,
    OBIX_BATCH_READ,
    OBIX_BATCH_WRITE_VALUE,
    OBIX_BATCH_INVOKE
} OBIX_BATCH_CMD_TYPE;

/**
 * Represents one command inside Batch request.
 */
typedef struct _oBIX_BatchCmd
{
    OBIX_BATCH_CMD_TYPE type;
    int id;
    Device* device;
    char* uri;
    char* input;
    OBIX_DATA_TYPE dataType;
    struct _oBIX_BatchCmd* next;
}
oBIX_BatchCmd;

/**
 * Represents a Batch object.
 */
struct _oBIX_Batch
{
    Connection* connection;
    int commandCounter;
    oBIX_BatchCmd* command;
    oBIX_BatchResult* result;
};

#endif /* OBIX_BATCH_H_ */
