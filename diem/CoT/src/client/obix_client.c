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
 * Contains implementation of C oBIX Client API front-end.
 *
 * @see obix_client.h
 *
 * @author Andrey Litvinov
 */

#include <stdlib.h>
#include <string.h>
#include <ixml_ext.h>
#include <log_utils.h>
#include <xml_config.h>
#include <obix_http.h>
#include "obix_client.h"

/** Default maximum amount of devices, which can be registered under one
 * connection. */
#define DEFAULT_MAX_DEVICES 10
/** Default maximum amount of listeners, which can be set for each device */
#define DEFAULT_MAX_LISTENERS 10

/** @name Names of tags and attributes in XML configuration file
 * @{ */
static const char* CT_CONNECTION = "connection";
static const char* CTA_CONNECTION_ID = "id";
static const char* CTA_CONNECTION_TYPE = "type";
static const char* CTAV_CONNECTION_TYPE_HTTP = "http";
static const char* CT_MAX_DEVICES = "max-devices";
static const char* CT_MAX_LISTENERS = "max-listeners";
/** @} */

/** Internal storage of all connections. */
static Connection** _connections;
static int _connectionCount;

/** Frees memory allocated for Listener. */
static void listener_free(Listener* listener)
{
    free(listener->paramUri);
    free(listener);
}

/** Creates and registers new listener object for the provided device. */
static int listener_register(Connection* connection,
                             Device* device,
                             int listenerId,
                             const char* paramUri,
                             obix_update_listener paramListener,
                             obix_operation_handler opHandler)
{
    Listener* listener = (Listener*) malloc(sizeof(Listener));
    if (listener == NULL)
    {
        log_error("Unable to register listener: Not enough memory.");
        return OBIX_ERR_NO_MEMORY;
    }

    listener->paramUri = (char*) malloc(strlen(paramUri) + 1);
    if (listener->paramUri == NULL)
    {
        log_error("Unable to register listener: Not enough memory.");
        free(listener);
        return OBIX_ERR_NO_MEMORY;
    }

    // initialize listener values
    strcpy(listener->paramUri, paramUri);
    listener->id = listenerId;
    listener->deviceId = device->id;
    listener->connectionId = connection->id;
    listener->paramListener = paramListener;
    listener->opHandler = opHandler;

    int error = (connection->comm->registerListener)(
                    connection,
                    (device->id == 0) ? NULL : device,
                    &listener);
    if (error != OBIX_SUCCESS)
    {
        listener_free(listener);
        return error;
    }

    device->listeners[listenerId] = listener;
    device->listenerCount++;

    return listenerId;
}

/** Unregisters the specified listener and releases allocated memory. */
static int listener_unregister(Connection* connection,
                               Device* device,
                               int listenerId)
{
    // we do not check listener id there because we assume that
    // it was checked by caller
    Listener* listener = device->listeners[listenerId];

    // call connection type specific method for removing listener
    int error = (connection->comm->unregisterListener)(
                    connection,
                    (device->id == 0) ? NULL : device,
                    listener);

    // free remaining listener fields
    listener_free(listener);

    // update device
    device->listeners[listenerId] = NULL;
    device->listenerCount--;

    return error;
}

/** Frees memory allocated for Device object (including its listeners). */
static void device_free(Device* device)
{
    if (device->listeners != NULL)
    {
        free(device->listeners);
    }
    free(device);
}

/** Creates new device object and registers it at the server. */
static int device_register(Connection* connection, int deviceId, const char* data)
{
    // create new device object
    Device* device = (Device*) malloc(sizeof(Device));
    if (device == NULL)
    {
        log_error("Unable to initialize device object: Not enough memory.");
        return OBIX_ERR_NO_MEMORY;
    }

    // initialize device fields
    device->listeners = (Listener**) calloc(connection->maxListeners,
                                            sizeof(Listener*));
    if (device->listeners == NULL)
    {
        log_error("Unable to initialize new device: Not enough memory.");
        free(device);
        return OBIX_ERR_NO_MEMORY;
    }
    device->id = deviceId;
    device->listenerCount = 0;

    // call connection type specific device initialization
    // but not for the fake device
    if (deviceId != 0)
    {
        int error = (connection->comm->registerDevice)(connection, &device, data);
        if (error != OBIX_SUCCESS)
        {
            free(device->listeners);
            free(device);
            return error;
        }
    }

    // store created device
    connection->devices[deviceId] = device;
    connection->deviceCount++;

    return OBIX_SUCCESS;
}

static int device_unregisterAllListeners(
    Connection* connection,
    Device* device)
{
    int i;
    int retVal = OBIX_SUCCESS;
    int error;
    for (i = 0; (i < connection->maxListeners)
            && (device->listenerCount > 0); i++)
    {
        if (device->listeners[i] != NULL)
        {
            error = listener_unregister(connection, device, i);
            if (error != OBIX_SUCCESS)
            {
                retVal = error;
            }
        }

    }

    return retVal;
}

/** Unregisters device from the server and completely deletes Device object. */
static int device_unregister(Connection* connection, int deviceId)
{
    // we do not check device id there because we assume that
    // it was checked by caller
    Device* device = connection->devices[deviceId];

    // unregister all listeners
    int retVal = device_unregisterAllListeners(connection, device);

    // call connection type specific method for removing
    int error = (connection->comm->unregisterDevice)(connection, device);
    if (error != OBIX_SUCCESS)
    {
        retVal = error;
    }

    // free remaining device fields
    device_free(device);

    // update connection
    connection->devices[deviceId] = NULL;
    connection->deviceCount--;

    return retVal;
}

/**
 * Returns id of a first free listener slot at provided device object.
 */
static int device_findFreeListenerSlot(Device* device, int maxListeners)
{
    if (device->listenerCount >= maxListeners)
    {
        return OBIX_ERR_LIMIT_REACHED;
    }

    // search for free slot for a new listener
    int id;
    for (id = 0; (id < maxListeners)
            && (device->listeners[id] != NULL); id++)
        ;
    if (id == maxListeners)
    {	// this should never happen
        log_error("Unable to find free slot for a new listener.");
        return OBIX_ERR_UNKNOWN_BUG;
    }

    return id;
}

int device_get(Connection* connection, int deviceId, Device** device)
{
    if ((deviceId < 0)
            || (deviceId >= connection->maxDevices)
            || (connection->devices[deviceId] == NULL))
    {
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    if (device != NULL)
    {
        *device = connection->devices[deviceId];
    }
    return OBIX_SUCCESS;
}

/** Frees memory allocated for the Connection object. */
static int connection_free(Connection* connection)
{
    if (connection->isConnected)
    {
        log_error("Can't delete open connection.");
        return OBIX_ERR_INVALID_STATE;
    }

    // clean connection type specific attributes
    if (connection->comm != NULL)
    {
        (connection->comm->freeConnection)(connection);
    }
    // clean the rest of connection object
    if (connection->devices != NULL)
    {
        if (connection->devices[0] != NULL)
        {
            device_free(connection->devices[0]);
        }
        free(connection->devices);
    }
    free(connection);
    return OBIX_SUCCESS;
}

/**
 * Creates new Connection object based on provided XML configuration.
 * @param connItem XML element containing connection settings
 * 				(#CT_CONNECTION tag).
 */
static int connection_create(IXML_Element* connItem)
{
    Connection* connection;
    int error;
    // parse following parameters from the <connection/> tag
    int id;
    Connection_Type type;
    const Comm_Stack* comm;
    int maxDevices = DEFAULT_MAX_DEVICES;
    int maxListeners = DEFAULT_MAX_LISTENERS;

    // get connection id
    id = config_getTagAttrIntValue(connItem, CTA_CONNECTION_ID, TRUE, 0);
    if (id < 0)
    {	// error occurred and logged
        return OBIX_ERR_INVALID_ARGUMENT;
    }
    if (id >= _connectionCount)
    {
        log_error("Connection id is too big: %d.", id);
        return OBIX_ERR_INVALID_ARGUMENT;
    }
    if (_connections[id] != NULL)
    {
        log_error("Several <%s/> tags have the same \"%s\" "
                  "attribute value: \"%d\".",
                  CT_CONNECTION, CTA_CONNECTION_ID, id);
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    // get connection type
    const char* attrValue = config_getTagAttributeValue(
                                connItem,
                                CTA_CONNECTION_TYPE,
                                TRUE);
    if (attrValue == NULL)
    {
        log_error("Settings parsing for connection id %d failed.", id);
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    if (strcmp(attrValue, CTAV_CONNECTION_TYPE_HTTP) == 0)
    {
        type = OBIX_HTTP;
        comm = &OBIX_HTTP_COMM_STACK;
        // initialize HTTP function stack
        error = http_init();
        if (error != OBIX_SUCCESS)
        {
            log_error("Unable to initialize HTTP communication module (needed "
                      "by connection id %d).", id);
            return error;
        }
    }
    else
    {
        log_error("Wrong connection type \"%s\". Available values: \"%s\".",
                  attrValue, CTAV_CONNECTION_TYPE_HTTP);
        log_error("Settings parsing for connection id %d failed.", id);
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    // load optional attributes
    // load maximum devices count
    IXML_Element* confItem = config_getChildTag(connItem,
                             CT_MAX_DEVICES,
                             FALSE);
    if (confItem != NULL)
    {
        maxDevices = config_getTagAttrIntValue(confItem,
                                               CTA_VALUE,
                                               FALSE,
                                               DEFAULT_MAX_DEVICES);
    }
    // load maximum listeners per device
    confItem = config_getChildTag(connItem,
                                  CT_MAX_LISTENERS,
                                  FALSE);
    if (confItem != NULL)
    {
        maxListeners = config_getTagAttrIntValue(
                           confItem,
                           CTA_VALUE,
                           FALSE,
                           DEFAULT_MAX_LISTENERS);
    }

    connection = (Connection*) malloc(sizeof(Connection));
    if (connection == NULL)
    {
        log_error("Unable to initialize new connection: Not enough memory.");
        return OBIX_ERR_NO_MEMORY;
    }

    // init connection fields
    connection->type = type;
    connection->comm = comm;
    connection->id = id;
    connection->isConnected = FALSE;

    // initialize devices
    // we create one fake device for storing listeners of external objects
    // thus increase maxDevices
    connection->maxListeners = maxListeners;
    connection->maxDevices = ++maxDevices;
    connection->devices = (Device**) calloc(maxDevices, sizeof(Device*));
    connection->deviceCount = 0;
    if (connection->devices == NULL)
    {
        log_error("Unable to initialize connection (id %d): "
                  "Not enough memory.", id);
        connection_free(connection);
        return OBIX_ERR_NO_MEMORY;
    }
    // create fake device
    error = device_register(connection, 0, NULL);
    if (error != OBIX_SUCCESS)
    {
        log_error("Unable to initialize connection (id %d).", id);
        connection_free(connection);
        return error;
    }

    // call type-specific initialization of the connection
    error = (comm->initConnection)(connItem, &connection);
    if (error != OBIX_SUCCESS)
    {
        log_error("Settings parsing for connection id %d failed.", id);
        connection->comm = NULL;
        connection_free(connection);
        return error;
    }

    // store created connection
    _connections[id] = connection;

    return OBIX_SUCCESS;
}

int connection_get(int connectionId,
                   BOOL isConnected,
                   Connection** connection)
{
    if ((connectionId < 0) || (connectionId >= _connectionCount))
    {
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    Connection* c = _connections[connectionId];
    if (c->isConnected != isConnected)
    {
        return OBIX_ERR_INVALID_STATE;
    }

    *connection = _connections[connectionId];
    return OBIX_SUCCESS;
}

int obix_loadConfigFile(const char* fileName)
{
    IXML_Element* settings = config_loadFile(fileName);

    if (settings == NULL)
    {
        // failed to load settings
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    // initialize log settings
    int error = config_log(settings);
    if (error != 0)
    {
        config_finishInit(settings, FALSE);
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    // parse config file
    error = obix_loadConfig(settings);
    if (error != OBIX_SUCCESS)
    {
        config_finishInit(settings, FALSE);
        return error;
    }

    config_finishInit(settings, TRUE);
    return OBIX_SUCCESS;
}

int obix_loadConfig(IXML_Element* config)
{
    // find all connection tags
    IXML_NodeList* connList = ixmlElement_getElementsByTagName(
                                  config,
                                  CT_CONNECTION);
    if (connList == NULL)
    {
        log_error("No <%s/> tags found.", CT_CONNECTION);
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    _connectionCount = ixmlNodeList_length(connList);
    if (_connectionCount == 0)
    {
        log_error("At least one configuration tag <%s/> expected.",
                  CT_CONNECTION);
        ixmlNodeList_free(connList);
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    // initialize connection storage
    _connections = (Connection**) calloc(_connectionCount, sizeof(Connection*));
    if (_connections == NULL)
    {
        log_error("Unable to initialize oBIX client: not enough memory.");
        ixmlNodeList_free(connList);
        return OBIX_ERR_NO_MEMORY;
    }

    // iterate through the list of <connection/> tags
    int i;
    for (i = 0; i < _connectionCount; i++)
    {
        IXML_Element* element = ixmlNode_convertToElement(
                                    ixmlNodeList_item(connList, i));
        if (element == NULL)
        {
            log_error("ixmlElement_getElementsByTagName() "
                      "returned something which is not an element.");
            // ignore and continue to the next list item
            continue;
        }

        // parse <connection/> tag
        int error = connection_create(element);
        if (error != OBIX_SUCCESS)
        {
            obix_dispose();
            ixmlNodeList_free(connList);
            return error;
        }
    }

    ixmlNodeList_free(connList);
    return OBIX_SUCCESS;
}

int obix_openConnection(int connectionId)
{
    Connection* connection;
    int error = connection_get(connectionId, FALSE, &connection);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    // execute connection type specific method
    error = (connection->comm->openConnection)(connection);
    if (error == OBIX_SUCCESS)
    {
        connection->isConnected = TRUE;
    }

    return error;
}

int obix_closeConnection(int connectionId)
{
    Connection* connection;
    int error = connection_get(connectionId, TRUE, &connection);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    // unregister all devices (except zero fake device)
    int i;
    int retVal = OBIX_SUCCESS;
    for (i = 1; (i < connection->maxDevices)
            && (connection->deviceCount > 1); i++)
    {
        if (connection->devices[i] != NULL)
        {
            error = device_unregister(connection, i);
            if (error != OBIX_SUCCESS)
            {
                retVal = error;
            }
        }
    }

    // remove all listeners from fake device
    device_unregisterAllListeners(connection, connection->devices[0]);

    // call connection type specific method
    error = (connection->comm->closeConnection)(connection);
    if (error == OBIX_SUCCESS)
    {
        connection->isConnected = FALSE;
    }
    else
    {
        retVal = error;
    }

    return retVal;
}

int obix_registerDevice(int connectionId, const char* obixData)
{
    Connection* connection;
    int error = connection_get(connectionId, TRUE, &connection);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }
    if (connection->deviceCount >= connection->maxDevices)
    {
        return OBIX_ERR_LIMIT_REACHED;
    }

    // search for free slot for a new device
    int id;
    for (id = 1; (id < connection->maxDevices)
            && (connection->devices[id] != NULL); id++)
        ;
    if (id == connection->maxDevices)
    {	// this should never happen
        log_error("Unable to find free slot for a new device.");
        return OBIX_ERR_UNKNOWN_BUG;
    }

    error = device_register(connection, id, obixData);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    return id;
}

int obix_unregisterDevice(int connectionId, int deviceId)
{
    Connection* connection;
    int error = connection_get(connectionId, TRUE, &connection);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    error = device_get(connection, deviceId, NULL);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    return device_unregister(connection, deviceId);
}

int obix_registerListener(int connectionId,
                          int deviceId,
                          const char* paramUri,
                          obix_update_listener listener)
{
    Connection* connection;
    int error = connection_get(connectionId, TRUE, &connection);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    Device* device;
    error = device_get(connection, deviceId, &device);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    // search for free slot for the new listener
    int id = device_findFreeListenerSlot(device, connection->maxListeners);
    if (id < 0)
    {
        return error;
    }

    error = listener_register(connection, device, id, paramUri, listener, NULL);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    return id;
}

int obix_registerOperationListener(int connectionId,
                                   int deviceId,
                                   const char* operationUri,
                                   obix_operation_handler listener)
{
    if (deviceId == 0)
    {
        log_error("It is allowed to register handlers only for own published "
                  "operations.");
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    Connection* connection;
    int error = connection_get(connectionId, TRUE, &connection);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    Device* device;
    error = device_get(connection, deviceId, &device);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    // search for free slot for the new listener
    int id = device_findFreeListenerSlot(device, connection->maxListeners);
    if (id < 0)
    {
        return error;
    }

    error =
        listener_register(connection, device, id, operationUri, NULL, listener);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    return id;
}

int obix_unregisterListener(int connectionId,
                            int deviceId,
                            int listenerId)
{
    // check connection id
    Connection* connection;
    int error = connection_get(connectionId, TRUE, &connection);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    // check device id
    Device* device;
    error = device_get(connection, deviceId, &device);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    // check listener id
    if ((listenerId < 0)
            || (listenerId >= connection->maxListeners)
            || (device->listeners[listenerId] == NULL))
    {
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    return listener_unregister(connection, device, listenerId);
}

int obix_readValue(int connectionId,
                   int deviceId,
                   const char* paramUri,
                   char** output)
{
    Connection* connection;
    int error = connection_get(connectionId, TRUE, &connection);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    Device* device;
    error = device_get(connection, deviceId, &device);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    if (deviceId == 0)
    {
        device = NULL;
    }

    if ((paramUri == NULL) && (device == NULL))
    {
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    return (connection->comm->readValue)(connection, device, paramUri, output);
}


int obix_read(int connectionId,
              int deviceId,
              const char* paramUri,
              IXML_Element** output)
{
    Connection* connection;
    int error = connection_get(connectionId, TRUE, &connection);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    Device* device;
    error = device_get(connection, deviceId, &device);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    if (deviceId == 0)
    {
        device = NULL;
    }

    if ((paramUri == NULL) && (device == NULL))
    {
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    return (connection->comm->read)(connection, device, paramUri, output);
}

int obix_writeValue(int connectionId,
                    int deviceId,
                    const char* paramUri,
                    const char* newValue,
                    OBIX_DATA_TYPE dataType)
{
    Connection* connection;
    int error = connection_get(connectionId, TRUE, &connection);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    Device* device;
    error = device_get(connection, deviceId, &device);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    if (deviceId == 0)
    {
        device = NULL;
    }

    if ((paramUri == NULL) && (device == NULL))
    {
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    return (connection->comm->writeValue)(connection,
                                          device,
                                          paramUri,
                                          newValue,
                                          dataType);
}

int obix_invoke(int connectionId,
                int deviceId,
                const char* operationUri,
                const char* input,
                char** output)
{
    if (input == NULL)
    {
        log_error("Operation input cannot be NULL. Use oBIX Nil object if "
                  "operation doesn't take any input parameters.");
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    Connection* connection;
    int error = connection_get(connectionId, TRUE, &connection);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    Device* device;
    error = device_get(connection, deviceId, &device);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    if (deviceId == 0)
    {
        device = NULL;
    }

    if ((operationUri == NULL) && (device == NULL))
    {
        return OBIX_ERR_INVALID_ARGUMENT;
    }

    return (connection->comm->invoke)(connection,
                                      device,
                                      operationUri,
                                      input,
                                      output);
}

const char* obix_getServerAddress(int connectionId)
{
    if ((connectionId < 0) || (connectionId >= _connectionCount))
    {
        return NULL;
    }

    Connection* connection = _connections[connectionId];
    return (connection->comm->getServerAddress)(connection);
}

int obix_dispose()
{
    int retVal = OBIX_SUCCESS;
    int error;
    if (_connections != NULL)
    {
        int i;
        for (i = 0; i < _connectionCount; i++)
        {
            if (_connections[i] != NULL)
            {
                if (_connections[i]->isConnected)
                {
                    error = obix_closeConnection(i);
                    if ((error != OBIX_SUCCESS) && (retVal == OBIX_SUCCESS))
                    {
                        retVal = error;
                    }
                }
                error = connection_free(_connections[i]);
                if ((error != OBIX_SUCCESS) && (retVal == OBIX_SUCCESS))
                {
                    retVal = error;
                }
            }
        }
    }
    free(_connections);
    error = http_dispose();
    if ((error != OBIX_SUCCESS) && (retVal == OBIX_SUCCESS))
    {
        retVal = error;
    }

    return retVal;
}
