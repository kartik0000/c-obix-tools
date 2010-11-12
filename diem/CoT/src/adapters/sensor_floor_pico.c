/* *****************************************************************************
 * Copyright (c) 2010 Andrey Litvinov
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
 * This is an adapter for the MariMils (Elsi) Sensor Floor.
 * It uses Pico server HTTP interface to get the data from the floor.
 * Usually, Pico server has data feed at the URL
 * http://<server.name>/<room>/feed
 * The adapter reads data about people positions on the floor and publishes it
 * to the static objects at the oBIX server.
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

#include <log_utils.h>
#include <ixml_ext.h>
#include <xml_config.h>
#include <ptask.h>
#include <obix_utils.h>
#include <obix_client.h>

#include "pico_http_feed_reader.h"

/** Id of the connection to the target oBIX server. */
#define SERVER_CONNECTION 0

/** Target object which describes person's position on the sensor floor. */
typedef struct _Target
{
    /** Constant address of the target object at the oBIX server. */
    char* uri;
    // TODO comment me
    int uriLength;
    /** Id of this target at the sensor floor */
    int id;
    /** Defines whether this target new or not. */
    BOOL new;
    /** If true this target is used to track a person. */
    BOOL active;

    PicoCluster* lastCluster;
    /** Id of the task which is scheduled to remove this target when it is not
     * changed for #TARGET_REMOVE_TIMEOUT milliseconds.*/
    int removeTask;
}
Target;

/** Head of the targets list. */
static Target* _targets;
/** Number of targets in the targets list. */
static int _targetsCount;
/** Defines how many points are shown per target.
 * Sensor floor groups neighbor points into one target.
 * Each point corresponds to the activated sensor, e.g. under left and right
 * feet of a person. */
static int _pointsPerTarget = 0;
/** Timeout after which the target is considered unavailable. */
static long _targetRemoveTimeout = 15000;
/** Id of the device which is registered at the oBIX server. */
static int _deviceId;
/** Thread for scheduling asynchronous tasks. */
static Task_Thread* _taskThread;
/** Mutex for thread synchronization: Targets are changed from one thread and
 * removed from another. */
static pthread_mutex_t _targetMutex = PTHREAD_MUTEX_INITIALIZER;
/** Address of sensor floor HTTP interface (pico server). */
static char* _picoServerAddress = NULL;
/** Name of the room to be monitored at pico server. */
static char* _picoRoomName = NULL;
/** Address at oBIX server where sensor floor data will be published to. */
static char* _obixUrlPrefix = NULL;
/** Flag indicating that driver should exit. */
static BOOL _shutDown = FALSE;
/** Stores amount of closed connections with no new data received. */
static int _closedConnectionCount = 0;

// declaration of the ..
void targetRemoveTask(void* arg);

/** Initializes internal task array. */
static void target_initArray()
{
    _targets = (Target*) calloc(_targetsCount, sizeof(Target));
}

/**
 * Searches for target with specified Id.
 *
 * @param id Id of the target which should be found.
 * @return Target with specified Id. If no such target found, search for a free
 *         target object and assigns provided Id to it. If there are no free
 *         targets, @a NULL is returned.
 */
static Target* target_get(int id)
{
    int i;
    for (i = 0; i < _targetsCount; i++)
    {
        if (_targets[i].id == id)
        {
            return &(_targets[i]);
        }
    }

    // no target with provided id is found
    // return empty target
    for (i = 0; i < _targetsCount; i++)
    {
        if (_targets[i].id == 0)
        {
            pthread_mutex_lock(&_targetMutex);
            Target* target = &_targets[i];
            target->id = id;
            target->new = TRUE;
            target->active = TRUE;
            target->removeTask = ptask_schedule(_taskThread,
                                                &targetRemoveTask,
                                                target,
                                                _targetRemoveTimeout,
                                                1);
            pthread_mutex_unlock(&_targetMutex);
            return target;
        }
    }

    // no free slots
    return NULL;
}

static void target_saveUri(Target* target, const char* uri)
{
    target->uriLength = strlen(uri);
    target->uri = (char*) malloc(target->uriLength + 20);
    strcpy(target->uri, uri);
}

/**
 * Returns uri for the field with the provided name.
 * Field URL is <targetURL><fieldName>
 */
static const char* target_getFieldUri(Target* target, const char* fieldName)
{
    strcpy(target->uri + target->uriLength, fieldName);
    return target->uri;
}

/**
 * Adds to the Batch request a command to update the specified field value at
 * oBIX server.
 * The command is added only in case when field's value has changed since last
 * request.
 */
static int addTargetFieldToBatch(oBIX_Batch* batch,
                                 Target* target,
                                 const char* fieldName,
                                 OBIX_DATA_TYPE fieldType,
                                 const char* oldValue,
                                 const char* newValue)
{
    if ((oldValue == NULL) || (strcmp(oldValue, newValue) != 0))
    {
        const char* fieldUri = target_getFieldUri(target, fieldName);
        return obix_batch_writeValue(batch,
                                     0,
                                     fieldUri,
                                     newValue,
                                     fieldType);
    }

    // nothing to send - previous value is equal to current.
    return 0;
}

/**
 * Adds to the Batch request required commands for updating coordinates of the
 * provided point inside a target.
 * Only coordinates that are changed since last request are sent.
 */
static int addTargetPointToBatch(oBIX_Batch* batch,
                                 Target* target,
                                 int pointId,
                                 const PicoClusterPoint* lastPoint,
                                 const PicoClusterPoint* newPoint)
{
    int error = 0;

    // make URI stub for all point fields
    char fieldUri[20];
    sprintf(fieldUri, "points/%d/", pointId + 1);
    int fieldUriLenght = strlen(fieldUri);

    if (picoClusterPoint_isZero(lastPoint))
    {
        if (picoClusterPoint_isZero(newPoint))
        {	// both old and new clusters are empty - nothing to send
            return 0;
        }

        // send update of active status
        strcpy(fieldUri + fieldUriLenght, "active");
        error += addTargetFieldToBatch(
                     batch, target, fieldUri, OBIX_T_BOOL, NULL, XML_TRUE);
    }
    else if (picoClusterPoint_isZero(newPoint))
    {	// last point was not empty but current one is
        // send update of active status
        strcpy(fieldUri + fieldUriLenght, "active");
        error += addTargetFieldToBatch(
                     batch, target, fieldUri, OBIX_T_BOOL, NULL, XML_FALSE);
    }

    // send point's sensor ID if needed
    strcpy(fieldUri + fieldUriLenght, "id");
    error += addTargetFieldToBatch(
                 batch, target, fieldUri, OBIX_T_INT,
                 lastPoint->id, newPoint->id);
    // send point's X coordinate if needed
    strcpy(fieldUri + fieldUriLenght, "x");
    error += addTargetFieldToBatch(
                 batch, target, fieldUri, OBIX_T_REAL,
                 lastPoint->x, newPoint->x);
    // send point's Y coordinate if needed
    strcpy(fieldUri + fieldUriLenght, "y");
    error += addTargetFieldToBatch(
                 batch, target, fieldUri, OBIX_T_REAL,
                 lastPoint->y, newPoint->y);
    // send point's magnitude if needed
    strcpy(fieldUri + fieldUriLenght, "magnitude");
    error += addTargetFieldToBatch(
                 batch, target, fieldUri, OBIX_T_REAL,
                 lastPoint->magnitude, newPoint->magnitude);

    return error;
}

/**
 * Generates a request to oBIX server with updates of target parameters.
 * The request contains only parameters that has been changed since last
 * request.
 *
 * @param target     Target object, which should be updated at oBIX server.
 * @param newCluster Cluster object containing new coordinates of the target and
 *                   possibly, coordinates of the points inside the target.
 */
static oBIX_Batch* generateObixWriteRequest(Target* target,
        PicoCluster* newCluster)
{
    // we send several values to the server simultaneously, that's why we use
    // Batch object.
    oBIX_Batch* batch = obix_batch_create(SERVER_CONNECTION);
    if (batch == NULL)
    {	// that can hardly ever happen... e.g. when there is no enough memory
        return NULL;
    }

    int error = 0;

    // send update of 'active' and 'id' attributes only once for new target, or
    // when old target is removed
    if (target->new == TRUE)
    {
        error += addTargetFieldToBatch(
                     batch, target, "active", OBIX_T_BOOL, NULL, XML_TRUE);
        error += addTargetFieldToBatch(
                     batch, target, "id", OBIX_T_INT, NULL, newCluster->id);

        target->new = FALSE;
    }
    else if (target->active == FALSE)
    {
        // target is being removed
        error += addTargetFieldToBatch(
                     batch, target, "active", OBIX_T_BOOL, NULL, XML_FALSE);
        error += addTargetFieldToBatch(
                     batch, target, "id", OBIX_T_INT, NULL, newCluster->id);
    }

    PicoCluster* lastCluster = target->lastCluster;

    // send update of X coordinate if needed
    error +=
        addTargetFieldToBatch(
            batch, target, "x", OBIX_T_REAL, lastCluster->x, newCluster->x);
    // send update of Y coordinate if needed
    error +=
        addTargetFieldToBatch(
            batch, target, "y", OBIX_T_REAL, lastCluster->y, newCluster->y);
    // send update of VX speed if needed
    error +=
        addTargetFieldToBatch(
            batch, target, "vx", OBIX_T_REAL, lastCluster->vx, newCluster->vx);
    // send update of VY speed if needed
    error +=
        addTargetFieldToBatch(
            batch, target, "vy", OBIX_T_REAL, lastCluster->vy, newCluster->vy);
    // send update of magnitude value if needed
    error +=
        addTargetFieldToBatch(
            batch, target, "magnitude", OBIX_T_REAL,
            lastCluster->magnitude, newCluster->magnitude);

    if (_pointsPerTarget > 0)
    {
        // send update of points inside cluster
        int i;
        for (i = 0; i < _pointsPerTarget; i++)
        {
            error += addTargetPointToBatch(
                         batch, target, i,
                         lastCluster->points[i], newCluster->points[i]);
        }
    }

    if (error < 0)
    {
        log_error("Unable to generate oBIX Batch object.");
        obix_batch_free(batch);
        return NULL;
    }

    return batch;
}


/**
 * Sends update of the target's values to the oBIX server.
 *
 * @param target Target object which should be sent.
 * @param newCluster Object with new target coordinates
 * @return #OBIX_SUCCESS if data is successfully sent, error code otherwise.
 */
static int target_sendUpdate(Target* target, PicoCluster* newCluster)
{
    // generate request to the server
    oBIX_Batch* batch = generateObixWriteRequest(target, newCluster);
    if (batch == NULL)
    {
        return -1;
    }
    // send the batch object which contains all required write operations
    int error = obix_batch_send(batch);
    // and don't forget to free the batch object after use
    obix_batch_free(batch);
    if (error != OBIX_SUCCESS)
    {
        log_error("Unable to send coordinates update to the oBIX server (%d). "
                  "Some data from sensor floor is ignored.",
                  error);
        picoCluster_free(newCluster);
        return -1;
    }

    // store data we sent
    picoCluster_free(target->lastCluster);
    target->lastCluster = newCluster;

    return 0;
}

/** Sends zeros to all parameters of all targets at oBIX server. */
static int target_resetValuesAtServer()
{
    // set NULL coordinates to all targets and then send zero coordinates
    // to server
    int i;
    int error = 0;
    for (i = 0; i < _targetsCount; i++)
    {
        _targets[i].lastCluster = picoCluster_getEmpty();

        error += target_sendUpdate(&(_targets[i]), picoCluster_getZero());
    }

    return error;
}

/**
 * Task which clears target object at the oBIX server if it is not changed for
 * #TARGET_REMOVE_TIMEOUT milliseconds.
 * It is scheduled for each target object. Implements #periodic_task prototype.
 *
 * @param arg Reference to the target object which should be removed.
 */
void targetRemoveTask(void* arg)
{
    Target* target = (Target*) arg;

    pthread_mutex_lock(&_targetMutex);

    // clean target
    target->id = 0;
    target->new = FALSE;
    target->removeTask = 0;
    target->active = FALSE;

    // send update to the server
    if (target_sendUpdate(target, picoCluster_getZero()) != 0)
    {
        log_error("Unable to send target update to the oBIX server.");
    }

    pthread_mutex_unlock(&_targetMutex);
}

/** Generates oBIX data representing a point object and adds it to
 * pointList object. */
static int generatePointXML(IXML_Element* pointList, int pointId)
{
    IXML_Element* point;
    char pointUrl[3];
    char name[8];

    sprintf(pointUrl, "%d", pointId + 1);
    sprintf(name, "point%d", pointId + 1);

    int error =
        obix_obj_addChild(pointList, OBIX_OBJ, pointUrl, name, NULL, &point);
    if (error != 0)
    {
        return -1;
    }

    error += obix_obj_addBooleanChild(
                 point, "active", "active", NULL, FALSE, TRUE, NULL);
    error += obix_obj_addIntegerChild(
                 point, "id", "sensorId", NULL, 0, TRUE, NULL);
    error += obix_obj_addRealChild(
                 point, "x", "x", NULL, 0, 0, TRUE, NULL);
    error += obix_obj_addRealChild(
                 point, "y", "y", NULL, 0, 0, TRUE, NULL);
    error += obix_obj_addRealChild(
                 point, "magnitude", "magnitude", NULL, 0, 0, TRUE, NULL);

    if (error != 0)
    {
        return -1;
    }

    return 0;
}

/** Generates oBIX data representing a target object and adds it to
 * targetList object. */
static int generateTargetXML(IXML_Element* targetList,
                             int targetId,
                             int pointsPerTarget)
{
    IXML_Element* target;
    char targetUrl[4];
    char name[10];
    char displayName[11];

    sprintf(targetUrl, "%d", targetId + 1);
    sprintf(name, "target%d", targetId + 1);
    sprintf(displayName, "Target %d", targetId + 1);

    int error = obix_obj_addChild(targetList,
                                  OBIX_OBJ,
                                  targetUrl,
                                  name,
                                  displayName,
                                  &target);
    if (error != 0)
    {
        return -1;
    }

    // save target URL
    target_saveUri(&(_targets[targetId]),
                   ixmlElement_getAttribute(target, OBIX_ATTR_HREF));

    error +=
        obix_obj_addBooleanChild(
            target, "active", "active", "Target is active", FALSE, TRUE, NULL);
    error += obix_obj_addIntegerChild(
                 target, "id", "id", "ID", 0, TRUE, NULL);
    error += obix_obj_addRealChild(
                 target, "x", "x", "X coordinate", 0, 0, TRUE, NULL);
    error += obix_obj_addRealChild(
                 target, "y", "y", "Y coordinate", 0, 0, TRUE, NULL);
    error += obix_obj_addRealChild(
                 target, "vx", "vx", "X speed", 0, 0, TRUE, NULL);
    error += obix_obj_addRealChild(
                 target, "vy", "vy", "Y speed", 0, 0, TRUE, NULL);
    error +=
        obix_obj_addRealChild(
            target, "magnitude", "magnitude", "Magnitude", 0, 0, TRUE, NULL);
    if (error != 0)
    {
        return -1;
    }

    if (pointsPerTarget > 0)
    {
        IXML_Element* pointList;
        error = obix_obj_addChild(target,
                                  OBIX_OBJ_LIST,
                                  "points",
                                  "pointList",
                                  "List of points",
                                  &pointList);
        if (error != 0)
        {
            return -1;
        }
        int i;
        for (i = 0; i < pointsPerTarget; i++)
        {
            error += generatePointXML(pointList, i);
        }
    }

    if (error != 0)
    {
        return -1;
    }

    return 0;
}

/**
 * Generates device data which will be published to oBIX server.
 *
 * return @a 0 on success, @a -1 on error.
 */
static int generateDeviceData(IXML_Document** deviceData)
{
    IXML_Element* deviceTag;

    int error = obix_obj_create(OBIX_OBJ,
                                _obixUrlPrefix,
                                "SensorFloor",
                                "Sensor Floor",
                                deviceData,
                                &deviceTag);
    if (error != 0)
    {
        return -1;
    }

    error += obix_obj_addStringChild(deviceTag,
                                     "room",
                                     "room",
                                     "Room Name",
                                     _picoRoomName,
                                     FALSE,
                                     NULL);
    // create list of targets
    IXML_Element* targetList;
    error += obix_obj_addChild(deviceTag,
                               OBIX_OBJ_LIST,
                               "targets",
                               "TargetList",
                               "List of Targets",
                               &targetList);
    if (error != 0)
    {
        return -1;
    }

    int i;
    for (i = 0; i < _targetsCount; i++)
    {
        error += generateTargetXML(targetList, i, _pointsPerTarget);
    }

    if (error != 0)
    {
        return -1;
    }
    return 0;
}

/**
 * Loads target settings from configuration file and generates oBIX device data.
 *
 * @param picoSettings XML tag with driver settings.
 * return @a 0 on success, @a -1 on error.
 */
static int loadTargetSettings(IXML_Element* picoSettings)
{
    // address at oBIX server where data will be posted to
    _obixUrlPrefix =
        strdup(config_getChildTagValue(picoSettings, "obix-url", TRUE));
    if (_obixUrlPrefix == NULL)
    {
        return -1;
    }

    IXML_Element* targetSettings =
        config_getChildTag(picoSettings, "target", TRUE);
    if (targetSettings == NULL)
    {
        return -1;
    }

    _targetsCount =
        config_getTagAttrIntValue(targetSettings, "count", TRUE, 1);
    if (_targetsCount < 0)
    {
        return -1;
    }
    _pointsPerTarget =
        config_getTagAttrIntValue(
            targetSettings, "point-count", FALSE, _pointsPerTarget);
    _targetRemoveTimeout =
        config_getTagAttrLongValue(
            targetSettings, "kill-timeout", FALSE, _targetRemoveTimeout);

    return 0;
}

/**
 * Loads driver settings from the specified configuration file.
 *
 * @param fileName Name of the configuration file.
 * @return @a 0 on success, @a -1 on error.
 */
static int loadSettings(const char* fileName)
{
    IXML_Element* settings = config_loadFile(fileName);
    if (settings == NULL)
    {
        return -1;
    }
    // configure log system
    int error = config_log(settings);
    if (error != 0)
    {
        return -1;
    }

    // initialize obix_client library with loaded settings
    error = obix_loadConfig(settings);
    if (error != OBIX_SUCCESS)
    {
        config_finishInit(settings, FALSE);
        return error;
    }

    // load driver specific settings
    IXML_Element* picoSettings =
        config_getChildTag(settings, "pico-settings", TRUE);
    if (picoSettings == NULL)
    {
        config_finishInit(settings, FALSE);
        return -1;
    }

    // pico server URI and room name
    const char* picoServer =
        config_getChildTagValue(picoSettings, "pico-server", TRUE);
    const char* picoRoomName =
        config_getChildTagValue(picoSettings, "room-name", TRUE);
    if ((picoServer == NULL) || (picoRoomName == NULL))
    {
        config_finishInit(settings, FALSE);
        return -1;
    }
    _picoServerAddress = strdup(picoServer);
    _picoRoomName = strdup(picoRoomName);

    // targets configuration
    if (loadTargetSettings(picoSettings) != 0)
    {
        config_finishInit(settings, FALSE);
        return -1;
    }

    // finish initialization
    config_finishInit(settings, TRUE);
    return 0;
}

/** Generates oBIX data representing one sensor object and adds this data to
 * sensorList object. */
static int generateSensorXML(IXML_Element* sensorList,
                             const PicoSensor* sensorData)
{
    IXML_Element* sensorTag;

    int error =
        obix_obj_addChild(
            sensorList, OBIX_OBJ, NULL, "sensor", NULL, &sensorTag);
    if (error != 0)
    {
        return -1;
    }

    error += obix_obj_addValChild(
                 sensorTag, OBIX_OBJ_INT, NULL, "id",
                 NULL, sensorData->id, FALSE, NULL);
    error += obix_obj_addValChild(
                 sensorTag, OBIX_OBJ_REAL, NULL, "x",
                 NULL, sensorData->x, FALSE, NULL);
    error += obix_obj_addValChild(
                 sensorTag, OBIX_OBJ_REAL, NULL, "y",
                 NULL, sensorData->y, FALSE, NULL);

    if (error != 0)
    {
        return -1;
    }

    return 0;
}

/**
 * Loads data about sensors in the floor and adds this data in oBIX format
 * to the deviceData document.
 *
 * @param testRoomInfoFileName If not @a NULL, this parameter represents the
 *                             name of the file with stored sensor floor data.
 */
static int loadRoomSensorInfo(IXML_Document* deviceData,
                              const char* testRoomInfoFileName)
{
    int sensorCount;
    const PicoSensor** sensors;
    int error;

    if (testRoomInfoFileName != NULL)
    {
        error = pico_readSensorInfoFromFile(testRoomInfoFileName,
                                            &sensors, &sensorCount);
    }
    else
    {
        error = pico_readSensorsInfoFromUrl(_picoServerAddress, _picoRoomName,
                                            &sensors, &sensorCount);
    }

    if (error != 0)
    {
        log_error("Unable to read sensor info from pico server. "
                  "Address = \"%s\"; Room name = \"%s\".",
                  _picoServerAddress, _picoRoomName);
        return -1;
    }

    IXML_Element* deviceTag = ixmlDocument_getRootElement(deviceData);
    IXML_Element* sensorList;

    error = obix_obj_addChild(deviceTag,
                              OBIX_OBJ_LIST,
                              "sensors",
                              "sensorList",
                              "Room sensors layout",
                              &sensorList);
    if (error != 0)
    {
        return -1;
    }

    int i;
    for (i = 0; i < sensorCount; i++)
    {
        if (sensors[i] != NULL)
        {
            error += generateSensorXML(sensorList, sensors[i]);
        }
    }

    if (error != 0)
    {
        return -1;
    }

    return 0;
}

/**
 * Sets all targets at the server to 'not available' and
 * frees all target values.
 */
static void disposeTargets()
{
    int i;
    for (i = 0; i < _targetsCount; i++)
    {
        if (_targets[i].id != 0)
        {
            targetRemoveTask(&(_targets[i]));
        }

        picoCluster_free(_targets[i].lastCluster);
        free(_targets[i].uri);
    }

    free(_targets);
}

/**
 * This listener received cluster structures, when people coordinates on the
 * sensor floor change.
 * @param cluster Structure containing coordinates of a person.
 */
void sensorFloorListener(PicoCluster* cluster)
{
    // find the corresponding target object
    int id = atoi(cluster->id);
    if (id <= 0)
    {
        log_error("A cluster with a wrong id received (integer value "
                  "expected): %s", cluster->id);
        return;
    }

    // we have received something valuable - reset error counter
    _closedConnectionCount = 0;

    Target* target = target_get(id);
    if (target == NULL)
    {
        // there is no free slot to monitor this target - ignore it
        log_debug("Sensor floor reports more clusters, than can be "
                  "published at the oBIX server. Adjust target count attribute"
                  "at driver's configuration file if you want to track more "
                  "people.");
        return;
    }

    pthread_mutex_lock(&_targetMutex);
    // send update to the server
    if (target_sendUpdate(target, cluster) != 0)
    {
        pthread_mutex_unlock(&_targetMutex);
        log_error("Unable to send coordinates update to the oBIX server.");
        return;
    }

    // reset timer of the remove task
    ptask_reset(_taskThread, target->removeTask);
    pthread_mutex_unlock(&_targetMutex);
}

/**
 * Frees all memory allocated by application.
 */
static void disposeEverything()
{
    // reset and free all targets
    disposeTargets();

    // release memory allocated for the feed reader
    pico_disposeReader();

    // release all resources allocated by oBIX client library.
    // No need to close connection or unregister listener explicitly - it is
    // done automatically.
    obix_dispose();

    ptask_dispose(_taskThread, TRUE);

    if (_picoServerAddress != NULL)
    {
        free(_picoServerAddress);
    }
    if (_picoRoomName != NULL)
    {
        free(_picoRoomName);
    }
}

/**
 * Forces application to quit.
 */
void killTask(void* arg)
{
    log_warning("Graceful shutdown doesn't work. "
                "Killing the driver..");
    exit(0);
}

/**
 * This method will listen to the interrupt signal
 * (e.g. when user presses Ctrl+C).
 */
void interruptSignalHandler(int signal)
{
    if (_shutDown == FALSE)
    {
        _shutDown = TRUE;
        log_warning("Received Interrupt signal, terminating..");
        printf("\nInterrupt signal is caught, terminating..\n");

        pico_stopFeedReader();
        ptask_schedule(_taskThread, &killTask, NULL, 5000, 1);
    }
    else
    {
        log_warning("Received another interrupt signal.. "
                    "I'm already trying to stop!");
    }
}

/**
 * Main working cycle of the driver. Listens to the pico server HTTP feed for
 * new people coordinates.s
 */
static void feedReadingLoop()
{
    while(!_shutDown)
    {
        int error = pico_readFeed(_picoServerAddress, _picoRoomName);

        if (error == 0)
        {
            log_warning("Connection with the sensor floor closed. "
                        "Connecting again...");
        }
        else
        {
            log_error("Error occurred during reading the feed of the "
                      "sensor floor. Connecting again...");
        }

        _closedConnectionCount++;
        if (_closedConnectionCount == 5)
        {
            log_error("Attempt to connect to the sensor floor failed for "
                      "5 times in a row. Shutting down.");
            _shutDown = TRUE;
        }

    }
}

/** Registers handler of the Interrupt signal. Instead of immediate
 * termination, the driver tries to close all connection and free all
 * resources.*/
static void registerInterruptionHandler()
{
    // read current interrupt signal action
    struct sigaction interruptSignalAction;
    if (sigaction(SIGINT, NULL, &interruptSignalAction))
    {
        log_warning("Unable to register interruption handler. If user "
                    "presses Ctrl+C a default system handler will be used - it "
                    "will kill the application without closing all "
                    "connections");
        return;
    }
    // remove restart flag - it will force all low-level operations like
    // read(), write(), etc. to exit with error, when the interruption signal
    // received.
    interruptSignalAction.sa_flags &= !SA_RESTART;
    //set our own handler method for interruption signal
    interruptSignalAction.sa_handler = &interruptSignalHandler;
    sigaction(SIGINT, &interruptSignalAction, NULL);
}

/**
 * Entry point of the driver.
 */
int main(int argc, char** argv)
{
    BOOL testMode = FALSE;

    if ((argc != 2) && ((argc != 5) || (strcmp(argv[2], "-test") != 0)))
    {
        printf("Usage: sensor_floor_pico <config_file> "
               "[-test <sensor_info> <feed>]\n"
               " where <config_file> - Name of the configuration file.\n"
               "       -test         - Optional parameter force to run the\n"
               "                       adapter in test mode.\n"
               "       <sensor_info> - Name of the file with floor sensor\n"
               "                       data as it is provided by Pico server\n"
               "                       at the URL /<room>/info\n"
               "       <feed>        - Name of the file with sensor floor\n"
               "                       feed data that provided by Pico server\n"
               "                       at the URL /<room>/feed\n");
        return -1;
    }

    if (argc == 5)
    {
        testMode = TRUE;
    }

    // load settings from file
    if (loadSettings(argv[1]) != 0)
    {
        printf("Unable to load settings from file %s.\n", argv[1]);
        return -1;
    }

    // initialize Sensor Floor connection module
    if (pico_initFeedReader(&sensorFloorListener, _pointsPerTarget) != 0)
    {
        log_error("Unable to initialize communication with the sensor floor");
        return -1;
    }

    // initialize internal targets array
    target_initArray();

    // generate oBIX device data based on parsed settings
    IXML_Document* deviceXML;
    if (generateDeviceData(&deviceXML) != 0)
    {
        log_error("Unable to generate oBIX device data");
        return -1;
    }

    // initialize the task thread
    _taskThread = ptask_init();
    if (_taskThread == NULL)
    {
        log_error("Unable to initialize separate thread.");
        return -1;
    }

    // read sensor info from the sensor floor and add it to our oBIX device data
    const char* roomInfoFile = testMode ? argv[3] : NULL;
    if (loadRoomSensorInfo(deviceXML, roomInfoFile) != 0)
    {
        log_error("Unable to load info about room sensors.");
        return -1;
    }

    // open connection to the target oBIX server
    int error = obix_openConnection(SERVER_CONNECTION);
    if (error != OBIX_SUCCESS)
    {
        log_error("Unable to establish connection with oBIX server.\n");
        return error;
    }

    // register Sensor Floor at the target oBIX server
    char* deviceData = ixmlPrintDocument(deviceXML);
    _deviceId = obix_registerDevice(SERVER_CONNECTION, deviceData);
    free(deviceData);
    ixmlDocument_free(deviceXML);
    if (_deviceId < 0)
    {
        log_error("Unable to register Sensor Floor at oBIX server.\n");
        return -1;
    }

    // reset targets at the server in case if some data remains from previous
    // driver execution.
    if (target_resetValuesAtServer() != 0)
    {
        log_error("Unable reset targets at oBIX server.");
        return -1;
    }


    registerInterruptionHandler();
    log_debug("Sensor floor driver is started.");
    printf("Sensor floor driver is started\n\n"
           "Press Ctrl+C to stop driver...\n");

    if (testMode)
    {
        pico_readFeedFromFile(argv[4]);
    }
    else
    {
        feedReadingLoop();
    }

    disposeEverything();

    return 0;
}
