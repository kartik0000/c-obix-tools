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
 * Definitions of HTTP communication layer.
 *
 * HTTP communication layer provides stack of functions, which handle oBIX
 * communication over HTTP connection.
 *
 * @author Andrey Litvinov
 */

#ifndef OBIX_HTTP_H_
#define OBIX_HTTP_H_

#include <pthread.h>
#include <table.h>
#include <obix_comm.h>

/** Extended Connection object, which stores HTTP specific settings. */
typedef struct _Http_Connection
{
    Connection c;
    // HTTP specific connection properties
    char* serverUri;
    int serverUriLength;
    char* lobbyUri;
    long pollInterval;
    long watchLease;
    long pollWaitMin;
    long pollWaitMax;

    char* signUpUri;
    char* batchUri;
    char* watchMakeUri;
    char* watchPollChangesFullUri;
    char* watchAddUri;
    char* watchRemoveUri;
    char* watchDeleteUri;

    Table* watchTable;
    pthread_mutex_t watchMutex;
    int watchPollTaskId;
}
Http_Connection;

/** Extended Device object, which stores HTTP specific settings. */
typedef struct _Http_Device
{
    Device d;
    // HTTP specific device properties
    char* uri;
    int uriLength;
}
Http_Device;

/** Stack of HTTP communication functions. */
extern const Comm_Stack OBIX_HTTP_COMM_STACK;

/**
 * Initializes HTTP communication layer.
 * Subsequent calls have no effect.
 *
 * @return #OBIX_SUCCESS, or one of error codes defined by #OBIX_ERRORCODE.
 */
int http_init();

/**
 * Frees memory used by HTTP communication layer.
 * When HTTP stack is not initialized calls of this function have no effect.
 *
 * @return #OBIX_SUCCESS, or one of error codes defined by #OBIX_ERRORCODE.
 */
int http_dispose();

/**
 * Implements #comm_initConnection prototype.
 */
int http_initConnection(IXML_Element* connItem,
                        Connection** connection);
/**
 * Implements #comm_openConnection prototype.
 */
int http_openConnection(Connection* connection);
/**
 * Implements #comm_closeConnection prototype.
 */
int http_closeConnection(Connection* connection);
/**
 * Implements #comm_freeConnection prototype.
 */
void http_freeConnection(Connection* connection);
/**
 * Implements #comm_registerDevice prototype.
 */
int http_registerDevice(Connection* connection,
                        Device** device,
                        const char* data);
/**
 * Implements #comm_unregisterDevice prototype.
 */
int http_unregisterDevice(Connection* connection,
                          Device* device);
/**
 * Implements #comm_registerListener prototype.
 */
int http_registerListener(Connection* connection,
                          Device* device,
                          Listener** listener);
/**
 * Implements #comm_unregisterListener prototype.
 */
int http_unregisterListener(Connection* connection,
                            Device* device,
                            Listener* listener);
/**
 * Implements #comm_readValue prototype.
 */
int http_readValue(Connection* connection,
                        Device* device,
                        const char* paramUri,
                        char** output);
/**
 * Implements #comm_read prototype.
 */
int http_read(Connection* connection,
                   Device* device,
                   const char* paramUri,
                   IXML_Element** output);
/**
 * Implements #comm_writeValue prototype.
 */
int http_writeValue(Connection* connection,
                    Device* device, const
                    char* paramUri,
                    const char* newValue,
                    OBIX_DATA_TYPE dataType);
/**
 * Implements #comm_sendBatch prototype.
 */
int http_sendBatch(oBIX_Batch* batch);

#endif /* OBIX_HTTP_H_ */
