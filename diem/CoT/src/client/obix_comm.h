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
 * @todo add description here
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#ifndef OBIX_COMM_H_
#define OBIX_COMM_H_

#include <ixml_ext.h>
#include <obix_client.h>

typedef enum
{
    OBIX_HTTP,
} Connection_Type;

typedef struct _Connection Connection;
typedef struct _Device Device;
typedef struct _Listener Listener;

typedef int (*comm_initConnection)(IXML_Element* connItem,
                                   Connection** connection);
typedef int (*comm_openConnection)(Connection* connection);
typedef int (*comm_closeConnection)(Connection* connection);
typedef void (*comm_freeConnection)(Connection* connection);
typedef int (*comm_registerDevice)(Connection* connection,
                                   Device** device,
                                   const char* data);
typedef int (*comm_unregisterDevice)(Connection* connection, Device* device);
typedef int (*comm_registerListener)(Connection* connection,
                                     Device* device,
                                     Listener** listener);
typedef int (*comm_unregisterListener)(Connection* connection,
                                       Device* device,
                                       Listener* listener);
typedef int (*comm_readValue)(Connection* connection,
                              Device* device,
                              const char* paramUri,
                              char** output);
typedef int (*comm_read)(Connection* connection,
                         Device* device,
                         const char* paramUri,
                         IXML_Element** output);
typedef int (*comm_writeValue)(Connection* connection,
                               Device* device,
                               const char* paramUri,
                               const char* newValue,
                               OBIX_DATA_TYPE dataType);
typedef int (*comm_sendBatch)(oBIX_Batch* batch);

typedef struct _Comm_Stack
{
    comm_initConnection initConnection;
    comm_openConnection openConnection;
    comm_closeConnection closeConnection;
    comm_freeConnection freeConnection;
    comm_registerDevice registerDevice;
    comm_unregisterDevice unregisterDevice;
    comm_registerListener registerListener;
    comm_unregisterListener unregisterListener;
    comm_read read;
    comm_readValue readValue;
    comm_writeValue writeValue;
    comm_sendBatch sendBatch;
}
Comm_Stack;

struct _Listener
{
    int id;
    int deviceId;
    int connectionId;
    char* paramUri;
    obix_update_listener callback;
};

struct _Device
{
    int id;
    Listener** listeners;
    int listenerCount;
};

struct _Connection
{
    /**common connection settings*/
    const Comm_Stack* comm;
    Connection_Type type;
    BOOL isConnected;
    int maxDevices;
    int maxListeners;

    int id;

    /**list of devices registered using this connection*/
    Device** devices;
    int deviceCount;
};

int connection_get(int connectionId,
                   BOOL isConnected,
                   Connection** connection);

int device_get(Connection* connection, int deviceId, Device** device);

#endif /* OBIX_COMM_H_ */
