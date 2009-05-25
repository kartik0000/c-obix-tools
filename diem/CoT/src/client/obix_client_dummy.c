/**
 * @file
 * Dummy implementation of oBIX client library.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "obix_client.h"

// TODO:
// In the real library all arrays may be will be replaced with structures
// It will reduce amount of memory used and remove restriction on maximum
// elements (connections per client, devices per connection and listeners per
// device. On the other hand, it will slow down each request to device or
// listener.
#define MAX_CONNECTIONS 3
#define MAX_DEVICES 20
#define MAX_LISTENERS 5

typedef struct _obix_listener
{
    obix_update_listener listener;
    char* uri;
}
obix_listener;

typedef struct _obix_device
{
    char* data;
    obix_listener listeners[MAX_LISTENERS];
    int listenerCount;
}
obix_device;

typedef struct _obix_connection
{
    char* serverAddress;
    obix_device devices[MAX_DEVICES];
    int deviceCount;
}
obix_connection;

static obix_connection _connections[MAX_CONNECTIONS];
static int _connectionCount = 0;

static obix_connection* getConnection(int connectionId)
{
    if ((connectionId <= 0) || (connectionId > MAX_CONNECTIONS))
    {
        printf("ERROR: Provided connection id %d is wrong.\n", connectionId);
        return NULL;
    }

    if (_connections[connectionId - 1].serverAddress == NULL)
    {
        printf("ERROR: Connection #%d is not found.\n", connectionId);
        return NULL;
    }

    return &(_connections[connectionId - 1]);
}

static obix_device* getDeviceFromConn(obix_connection* connection,
                                      int deviceId)
{
    if ((deviceId < 0) || (deviceId > MAX_DEVICES))
    {
        printf("ERROR: Provided device id %d is wrong.\n", deviceId);
        return NULL;
    }

    // deviceId 0 has empty data field because it reserved to
    // store listeners for external server data changes
    if ((deviceId != 0) && (connection->devices[deviceId].data == NULL))
    {
        printf("ERROR: Device #%d is not found at the server \"%s\".\n",
               deviceId, connection->serverAddress);
        return NULL;
    }

    return &(connection->devices[deviceId]);
}

static obix_device* getDevice(int connectionId, int deviceId)
{
    obix_connection* connection = getConnection(connectionId);
    if (connection == NULL)
        return NULL;

    return getDeviceFromConn(connection, deviceId);
}

static int freeDevice(obix_device* device)
{
    // remove all listeners
    int i;
    for (i = 0; i < device->listenerCount; i++)
    {
        printf("Removing listener for \"%s\".\n", device->listeners[i].uri);
        free(device->listeners[i].uri);
    }
    device->listenerCount = 0;
    // remove device data
    free(device->data);
    device->data = NULL;

    return 0;
}

int obix_openConnection(const char* serverAddress)
{
    if (_connectionCount >= MAX_CONNECTIONS)
    {
        printf("ERROR: Unable to create a connection. Maximum connection number is reached.\n");
        return -1;
    }

    // find for free slot for the connection;
    int connectionId = 0;
    while ((connectionId < MAX_CONNECTIONS) && (_connections[connectionId].serverAddress != NULL))
    {
        connectionId++;
    }

    if (connectionId == MAX_CONNECTIONS)
    {
        // this should never happen
        printf("ERROR: fatal library error. SOS!! :)\n");
        return -1;
    }

    obix_connection* newConn = &(_connections[connectionId]);
    newConn->serverAddress = (char*) malloc(strlen(serverAddress) + 1);
    if (newConn->serverAddress == NULL)
    {
        printf("ERROR: Unable to create connection. Not enough memory.\n");
        return -1;
    }

    strcpy(newConn->serverAddress, serverAddress);
    // reserve devices[0] for storing listeners of external data
    newConn->deviceCount = 1;

    _connectionCount++;
    printf("Connection to \"%s\" is successfully created!\n", newConn->serverAddress);

    return connectionId + 1;
}

int obix_closeConnection(int connectionId)
{
    obix_connection* connection = getConnection(connectionId);
    if (connection == NULL)
    {
        printf("ERROR: Unable to close connection.\n");
        return -1;
    }

    free(connection->serverAddress);
    connection->serverAddress = NULL;

    // free also all devices
    int i;
    for (i = 0; i < connection->deviceCount; i++)
    {
        if (freeDevice(&(connection->devices[i])) != 0)
        {
            printf("ERROR: Unable to close connection #%d.\n", connectionId);
            return -1;
        }
    }

    connection->deviceCount = 0;

    //    // move the rest of the array
    //    for (i = connectionId; i < _connectionCount; i++)
    //    {
    //        _connections[i - 1] = _connections[i];
    //    }

    _connectionCount--;

    printf("Connection #%d is successfully closed!\n", connectionId);
    return 0;
}

int obix_registerDevice(int connectionId, const char* obixData)
{
    obix_connection* connection = getConnection(connectionId);
    if (connection == NULL)
    {
        printf("ERROR: Unable to register new device.\n");
        return -1;
    }

    if (connection->deviceCount >= MAX_DEVICES)
    {
        printf("ERROR: No more devices can be published at \"%s\".\n"
               "There will be no such restriction in the real library.\n",
               connection->serverAddress);
        return -1;
    }

    // search for free slot
    int deviceId = 1;
    while ((deviceId < MAX_DEVICES) && (connection->devices[deviceId].data != NULL))
    {
        deviceId++;
    }
    if (deviceId == MAX_DEVICES)
    {
        // this should never happen
        printf("ERROR: Fatal internal library error! SOS!! :)\n");
        return -1;
    }

    // copy device data
    char* data = (char*) malloc(strlen(obixData));
    if (data == NULL)
    {
        printf("ERROR: Unable to register device. Not enough memory.\n");
        return -1;
    }

    strcpy(data, obixData);
    connection->devices[deviceId].data = data;
    connection->deviceCount++;
    printf("New device is successfully registered at \"%s\":\n%s\n",
           connection->serverAddress,
           data);

    return deviceId;
}

int obix_unregisterDevice(int connectionId, int deviceId)
{
    // do not allow to remove device 0
    // it is reserved for storing listeners of the external data
    if (deviceId == 0)
    {
        printf("ERROR: Unable to unregister device."
               " Device ID should be positive integer.\n");
        return -1;
    }

    // we need connection reference to shift all devices
    // in the device array
    obix_connection* connection = getConnection(connectionId);
    if (connection == NULL)
    {
        printf("ERROR: Unable to unregister device.\n");
        return -1;
    }

    obix_device* device = getDeviceFromConn(connection, deviceId);
    if (device == NULL)
    {
        printf("ERROR: Unable to unregister device.\n");
        return -1;
    }

    if (freeDevice(device) != 0)
    {
        printf("ERROR: Unable to unregister device #%d.\n", deviceId);
        return -1;
    }

    //    // move all other devices in order to fill the gap
    //    int i;
    //    for (i = deviceId + 1; i < connection->deviceCount; i++)
    //    {
    //        connection->devices[i-1] = connection->devices[i];
    //    }
    connection->deviceCount--;

    printf("Device #%d is successfully unregistered from \"%s\".\n",
           deviceId, connection->serverAddress);

    return 0;
}

int obix_writeValue(int connectionId,
                    int deviceId,
                    const char* paramUri,
                    const char* newValue)
{
    if ((paramUri == NULL) || (newValue == NULL))
    {
        printf("ERROR: Unable to write to oBIX server. "
               "Wrong arguments provided.\n");
        return -1;
    }

    obix_connection* connection = getConnection(connectionId);
    if (connection == NULL)
    {
        printf("ERROR: Unable to write to oBIX server.\n");
        return -1;
    }

    if (deviceId == 0)
    {
        // writing to external server object

        // URI should show address from the server root
        if (*paramUri != '/')
        {
            printf("ERROR: Unable to write to oBIX server. "
                   "Wrong URI provided.\n");
            return -1;
        }

        printf("New value is written to \"%s%s\": \"%s\".\n",
               connection->serverAddress, paramUri, newValue);

        return 0;
    }

    if (getDeviceFromConn(connection, deviceId) == NULL)
    {
        printf("ERROR: Unable to write to oBIX server.\n");
        return -1;
    }

    printf("New value for parameter \"%s\" is written to the device #%d at "
           "\"%s\": \"%s\".\n",
           paramUri, deviceId, connection->serverAddress, newValue);

    return 0;
}

int obix_registerListener(int connectionId,
                          int deviceId,
                          const char* paramUri,
                          obix_update_listener listener)
{
    if ((paramUri == NULL) || (listener == NULL))
    {
        printf("ERROR: Unable to register new listener. Wrong arguments.\n");
        return -1;
    }

    // get device
    obix_device* device = getDevice(connectionId, deviceId);
    if (device == NULL)
    {
        printf("ERROR: Unable to register new listener.\n");
        return -1;
    }

    // search for the same listener
    int i;
    for (i = 0; i < device->listenerCount; i++)
    {
        if (strcmp(paramUri, device->listeners[i].uri) == 0)
        {
            printf("Overwriting listener: connection #%d; "
                   "device #%d; parameter \"%s\".\n",
                   connectionId,
                   deviceId,
                   paramUri);
            device->listeners[i].listener = listener;
            return 0;
        }
    }

    // we do not have yet listener for this parameter
    // let's create a new one

    obix_listener* newListener = &(device->listeners[device->listenerCount]);
    newListener->uri = (char*) malloc(strlen(paramUri) + 1);

    if (newListener->uri == NULL)
    {
        printf("ERROR: Unable to register new listener. Not enough memory.\n");
        return -1;
    }

    strcpy(newListener->uri, paramUri);

    newListener->listener = listener;

    device->listenerCount++;

    printf("New listener is created: connection %d; device %d; URI \"%s\".\n",
           connectionId, deviceId, paramUri);

    // calling listener immediately after registering for testing purposes
    printf("Calling listener for testing purposes. "
           "Remove this call if not needed.\n");
    (*newListener->listener)(connectionId, deviceId, paramUri, "new test value");
    return 0;
}

int obix_unregisterListener(int connectionId,
                            int deviceId,
                            const char* paramUri)
{
    obix_device* device = getDevice(connectionId, deviceId);
    if (device == NULL)
    {
        printf("ERROR: Unable to remove listener.\n");
        return -1;
    }

    // search for the listener for provided URI
    int i;
    for (i = 0; i < device->listenerCount; i++)
    {
        if (strcmp(paramUri, device->listeners[i].uri) == 0)
        {
            printf("Removing listener: connection #%d; "
                   "device #%d; parameter \"%s\".\n",
                   connectionId,
                   deviceId,
                   paramUri);
            free(device->listeners[i].uri);

            // move other listeners to fill the gap
            for (++i; i < device->listenerCount; i++)
            {
                device->listeners[i-1] = device->listeners[i];
            }
            device->listenerCount--;

            return 0;
        }
    }

    // we did not find any listener with specified URI
    printf("ERROR: Unable to remove listener. Provided URI is not found.\n");
    return -1;
}

