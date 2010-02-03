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
 * Defines communication layer API for oBIX client.
 *
 * Currently only one communication type is supported: oBIX over HTTP. But in
 * future it can be possible to extend C oBIX Client library with additional
 * communication layers (SOAP, or some other custom oBIX server interface).
 *
 * Also contains common definitions of data types and helper functions, which
 * can be used by communication layer implementation.
 *
 * @author Andrey Litvinov
 */

#ifndef OBIX_COMM_H_
#define OBIX_COMM_H_

#include <ixml_ext.h>
#include <obix_client.h>

/** Defined possible communication methods. */
typedef enum
{
    OBIX_HTTP,
} Connection_Type;

/** See #_Comm_Stack */
typedef struct _Comm_Stack Comm_Stack;

/** @name Basic object types
 * @{ */
typedef struct _Listener
{
    int id;
    int deviceId;
    int connectionId;
    char* paramUri;
    obix_update_listener paramListener;
    obix_operation_handler opHandler;
}
Listener;

typedef struct _Device
{
    int id;
    Listener** listeners;
    int listenerCount;
}
Device;

typedef struct _Connection
{
    /**@name Common connection settings
     * @{ */
    const Comm_Stack* comm;
    Connection_Type type;
    BOOL isConnected;
    int maxDevices;
    int maxListeners;

    int id;
    /** @} */

    /**@name List of devices registered using this connection
     * @{*/
    Device** devices;
    int deviceCount;
    /** @} */
}
Connection;

/** @} */

/**
 * Prototype of a function, which should perform communication type-specific
 * initialization of the Connection object.
 * Note that connection should not be opened yet (see #comm_openConnection).
 * For instance, it can initialize some global variables, or read specific
 * settings from configuration file.
 *
 * @param connItem XML element containing connection settings
 * 				(#CT_CONNECTION tag).
 * @param connection Reference to the basic connection object. It can be
 * 				extended with new attributes and new reference should be written
 * 				back.
 * @return Function should return #OBIX_SUCCESS on success. Otherwise - one of
 * 				negative error codes defined by #OBIX_ERRORCODE.
 */
typedef int (*comm_initConnection)(IXML_Element* connItem,
                                   Connection** connection);

/**
 * Prototype of a function, which should open connection with oBIX server using
 * specified Connection object.
 *
 * @return Function should return #OBIX_SUCCESS on success. Otherwise - one of
 * 				negative error codes defined by #OBIX_ERRORCODE.
 */
typedef int (*comm_openConnection)(Connection* connection);

/**
 * Prototype of a function, which should close provided connection with oBIX
 * server.
 *
 * @return Function should return #OBIX_SUCCESS on success. Otherwise - one of
 * 				negative error codes defined by #OBIX_ERRORCODE.
 */
typedef int (*comm_closeConnection)(Connection* connection);

/**
 * Prototype of a function, which should free all resources allocated by
 * communication type-specific settings of the provided Connection object.
 */
typedef void (*comm_freeConnection)(Connection* connection);

/**
 * Prototype of a function, which should register device data at oBIX server
 * using provided connection.
 *
 * @return Function should return #OBIX_SUCCESS on success. Otherwise - one of
 * 				negative error codes defined by #OBIX_ERRORCODE.
 */
typedef int (*comm_registerDevice)(Connection* connection,
                                   Device** device,
                                   const char* data);

/**
 * Prototype of a function, which should unregister device from oBIX server
 * using provided connection.
 *
 * @return Function should return #OBIX_SUCCESS on success. Otherwise - one of
 * 				negative error codes defined by #OBIX_ERRORCODE.
 */
typedef int (*comm_unregisterDevice)(Connection* connection, Device* device);

/**
 * Prototype of a function, which should perform actual registration of provided
 * listener object at oBIX server.
 *
 * @return Function should return #OBIX_SUCCESS on success. Otherwise - one of
 * 				negative error codes defined by #OBIX_ERRORCODE.
 */
typedef int (*comm_registerListener)(Connection* connection,
                                     Device* device,
                                     Listener** listener);

/**
 * Prototype of a function, which should unregister provided listener object
 * from oBIX server.
 *
 * @return Function should return #OBIX_SUCCESS on success. Otherwise - one of
 * 				negative error codes defined by #OBIX_ERRORCODE.
 */
typedef int (*comm_unregisterListener)(Connection* connection,
                                       Device* device,
                                       Listener* listener);

/**
 * Prototype of a function, which should read parameter value of specified
 * device from oBIX server.
 *
 * @param output Reference to the received value should be returned here.
 *
 * @return Function should return #OBIX_SUCCESS on success. Otherwise - one of
 * 				negative error codes defined by #OBIX_ERRORCODE.
 */
typedef int (*comm_readValue)(Connection* connection,
                              Device* device,
                              const char* paramUri,
                              char** output);

/**
 * Prototype of a function, which reads complete object from oBIX server.
 *
 * @param output The whole received oBIX object should be returned here.
 * @return Function should return #OBIX_SUCCESS on success. Otherwise - one of
 * 				negative error codes defined by #OBIX_ERRORCODE.
 */
typedef int (*comm_read)(Connection* connection,
                         Device* device,
                         const char* paramUri,
                         IXML_Element** output);

/**
 * Prototype of a function, which writes new value to the provided object at
 * oBIX server.
 *
 * @return Function should return #OBIX_SUCCESS on success. Otherwise - one of
 * 				negative error codes defined by #OBIX_ERRORCODE.
 */
typedef int (*comm_writeValue)(Connection* connection,
                               Device* device,
                               const char* paramUri,
                               const char* newValue,
                               OBIX_DATA_TYPE dataType);

/**
 * Prototype of a function, which sends provided Batch object to oBIX server.
 *
 * @return Function should return #OBIX_SUCCESS on success. Otherwise - one of
 * 				negative error codes defined by #OBIX_ERRORCODE.
 */
typedef int (*comm_sendBatch)(oBIX_Batch* batch);

/**
 * Defines full set of operations, which should be implemented by any
 * communication layer.
 */
struct _Comm_Stack
{
    /** See #comm_initConnection */
    comm_initConnection initConnection;
    /** See #comm_openConnection */
    comm_openConnection openConnection;
    /** See #comm_closeConnection */
    comm_closeConnection closeConnection;
    /** See #comm_freeConnection */
    comm_freeConnection freeConnection;
    /** See #comm_registerDevice */
    comm_registerDevice registerDevice;
    /** See #comm_unregisterDevice */
    comm_unregisterDevice unregisterDevice;
    /** See #comm_registerListener */
    comm_registerListener registerListener;
    /** See #comm_unregisterListener */
    comm_unregisterListener unregisterListener;
    /** See #comm_read */
    comm_read read;
    /** See #comm_readValue */
    comm_readValue readValue;
    /** See #comm_writeValue */
    comm_writeValue writeValue;
    /** See #comm_sendBatch */
    comm_sendBatch sendBatch;
};

/**
 * Returns Connection object with specified id.
 *
 * @param isConnected Caller of the function should assume that connection is
 * 					either opened or closed. If the real retrieved connection
 * 					doesn't have the assumed state, #OBIX_ERR_INVALID_STATE
 * 					error is returned.
 * @param connection Reference to the retrieved connection object is returned
 * 					here.
 * @return #OBIX_SUCCESS if connection object is retrieved successfully.
 * 					Otherwise, one of error codes specified by #OBIX_ERRORCODE.
 */
int connection_get(int connectionId,
                   BOOL isConnected,
                   Connection** connection);

/**
 * Returns Device object with specified id.
 *
 * @param device Reference to the retrieved object is returned here.
 * @return #OBIX_SUCCESS if device object is retrieved successfully.
 * 					Otherwise, one of error codes specified by #OBIX_ERRORCODE.
 */
int device_get(Connection* connection, int deviceId, Device** device);

#endif /* OBIX_COMM_H_ */
