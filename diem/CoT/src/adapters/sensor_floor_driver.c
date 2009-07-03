/** @file
 * @todo add description here
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

// TODO curl is used for hack checking events, which is has to be done
// because feed doesn't return FALLEN event :(
#include <curl_ext.h>
#include <lwl_ext.h>
#include <ptask.h>
#include <ixml_ext.h>
#include <xml_config.h>
#include <obix_utils.h>
#include <obix_client.h>

#define SENSOR_FLOOR_CONNECTION 0
#define SENSOR_FLOOR_FEED_CONNECTION 1
#define SERVER_CONNECTION 2

#define TARGET_REMOVE_TIMEOUT 10000

#define LAST_EVENT_POOL_PERIOD 3000

typedef struct _Target
{
    char* uri;
    char* x;
    char* y;
    int id;
    BOOL new;
    BOOL changed;
    int removeTask;
}
Target;

IXML_Document* _deviceData;
Target* _targets;
int _targetsCount;
int _deviceId;
Task_Thread* _taskThread;
pthread_mutex_t _targetMutex = PTHREAD_MUTEX_INITIALIZER;

// TODO remove me
CURL_EXT* _curl_handle;

IXML_Document* _testData;

void target_setX(Target* target, const char* newValue)
{
    if (target->x != NULL)
    {
        free(target->x);
    }
    target->x = (char*) malloc(strlen(newValue) + 1);
    strcpy(target->x, newValue);

    target->changed = TRUE;
}

void target_setY(Target* target, const char* newValue)
{
    if (target->y != NULL)
    {
        free(target->y);
    }
    target->y = (char*) malloc(strlen(newValue) + 1);
    strcpy(target->y, newValue);

    target->changed = TRUE;
}

Target* target_get(int id)
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
            _targets[i].id = id;
            _targets[i].new = TRUE;
            return &(_targets[i]);
        }
    }

    // no free slots
    return NULL;
}

int target_sendUpdate(Target* target)
{
    // send new X and Y values to the server
    // generate URI for x coordinate
    int error;
    int targetUriLength = strlen(target->uri);
    char fullUri[targetUriLength + 10];
    strcpy(fullUri, target->uri);

    if (target->new == TRUE)
    {
        // send update of available attribute
        strcat(fullUri, "available");

        error = obix_writeValue(SERVER_CONNECTION,
                                _deviceId,
                                fullUri,
                                XML_TRUE,
                                OBIX_T_BOOL);
        target->new = FALSE;
    }
    else if (target->changed == FALSE)
    {
        // target is being removed
        strcat(fullUri, "available");

        error = obix_writeValue(SERVER_CONNECTION,
                                _deviceId,
                                fullUri,
                                XML_FALSE,
                                OBIX_T_BOOL);
    }

    // send update of X coordinate
    fullUri[targetUriLength] = 'x';
    fullUri[targetUriLength + 1] = '\0';

    error = obix_writeValue(SERVER_CONNECTION,
                            _deviceId,
                            fullUri,
                            target->x,
                            OBIX_T_REAL);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    // send Y update
    fullUri[targetUriLength] = 'y';
    fullUri[targetUriLength + 1] = '\0';

    error = obix_writeValue(SERVER_CONNECTION,
                            _deviceId,
                            fullUri,
                            target->y,
                            OBIX_T_REAL);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    target->changed = FALSE;
    return OBIX_SUCCESS;
}

int parseTarget(IXML_Node* node, int position)
{
    node = ixmlNode_getFirstChild(node);
    // first node is id tag (at least we hope so :)
    const char* attrValue = ixmlElement_getAttribute(
                                ixmlNode_convertToElement(node), OBIX_ATTR_VAL);
    int id = atoi(attrValue);
    if (id <= 0)
    {
        char* buffer = ixmlPrintNode(node);
        log_error("Unable to parse target: "
                  "ID expected in the first child tag, but it is:\n%s",
                  buffer);
        free(buffer);
        return -1;
    }
    // get corresponding target
    Target* target = target_get(id);
    if (target == NULL)
    {
        // there is no free slot to monitor this target - ignore it
        return 0;
    }

    // iterate through the whole list of target params
    while (node != NULL)
    {
        IXML_Element* param = ixmlNode_convertToElement(node);
        if (param == NULL)
        {
            log_error("Target contains something unexpected.");
            return -1;
        }

        const char* paramName = ixmlElement_getAttribute(
                                    param,
                                    OBIX_ATTR_NAME);
        if (paramName == NULL)
        {
            log_error("Target object doesn't have \"%s\" attribute.",
                      OBIX_ATTR_NAME);
            return -1;
        }

        if (strcmp(paramName, "x") == 0)
        {
            const char* newValue = ixmlElement_getAttribute(param, OBIX_ATTR_VAL);
            if (newValue == NULL)
            {
                return -1;
            }
            target_setX(target, newValue);
        }
        else if (strcmp(paramName, "y") == 0)
        {
            const char* newValue = ixmlElement_getAttribute(param, OBIX_ATTR_VAL);
            if (newValue == NULL)
            {
                return -1;
            }
            target_setY(target, newValue);
        }

        node = ixmlNode_getNextSibling(node);
    }

    return 0;
}

void targetRemoveTask(void* arg)
{
    Target* target = (Target*) arg;

    pthread_mutex_lock(&_targetMutex);

    if (target->changed == TRUE)
    {
        // target is updated
        // no need to remove it now
        pthread_mutex_unlock(&_targetMutex);
        return;
    }

    // clean target
    target->id = 0;
    target->new = FALSE;
    target->removeTask = 0;
    target_setX(target, "0");
    target_setY(target, "0");
    target->changed = FALSE;

    // send update to the server
    if (target_sendUpdate(target) != OBIX_SUCCESS)
    {
        log_error("Unable to update the target at oBIX server.");
    }

    pthread_mutex_unlock(&_targetMutex);
}

/**
 * Checks whether targets are updated. If yes - sends updated values to the
 * server, if no - schedules target removal
 * @return 0 on success, -1 on error
 */
int checkTargets()
{
    int i;
    for (i = 0; i < _targetsCount; i++)
    {
        Target* target = &(_targets[i]);
        if (target->id == 0)
        {
            // this is empty target slot - ignore it
            continue;
        }

        if (target->changed == TRUE)
        {
            // check whether removing of the task is scheduled
            if (target->removeTask != 0)
            {
                ptask_cancel(_taskThread, target->removeTask, FALSE);
                target->removeTask = 0;
            }

            // send update to the server
            if (target_sendUpdate(target) != OBIX_SUCCESS)
            {
                return -1;
            }
        }
        else
        {
            // schedule target removal
            if (target->removeTask == 0)
            {
                target->removeTask = ptask_schedule(_taskThread,
                                                    &targetRemoveTask,
                                                    target,
                                                    TARGET_REMOVE_TIMEOUT,
                                                    1);
            }
        }
    }

    return 0;
}

int targetsListener(int connectionId,
                    int deviceId,
                    int listenerId,
                    const char* newValue)
{
    //    log_debug("Targets update received:\n%s\n", newValue);
    // parse update
    IXML_Document* doc;
    int error = ixmlParseBufferEx(newValue, &doc);
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to parse received targets update:\n%s", newValue);
        return -1;
    }

    pthread_mutex_lock(&_targetMutex);
    IXML_NodeList* targets = ixmlDocument_getElementsByTagName(doc, OBIX_OBJ);
    if (targets == NULL)
    {
        // no targets - ignore.
    	ixmlDocument_free(doc);
        error = checkTargets();
        pthread_mutex_unlock(&_targetMutex);
        return error;
    }

    int i;
    int targetsCount = ixmlNodeList_length(targets);
    if (targetsCount > _targetsCount)
    {
        log_warning("Sensor Floor returned %d targets, "
                    "but we monitor only %d.", targetsCount, _targetsCount);
        targetsCount = _targetsCount;
    }

    for (i = 0; i < targetsCount; i++)
    {
        if (parseTarget(ixmlNodeList_item(targets, i), i) != 0)
        {
            // error occurred, stop parsing
            log_error("Unable to parse received targets update:\n%s",
                      newValue);
            break;
        }
    }

    ixmlNodeList_free(targets);
    ixmlDocument_free(doc);

    //check targets and send updates if necessary
    checkTargets();
    pthread_mutex_unlock(&_targetMutex);

    return 0;
}

int eventFeedListener(int connectionId,
                      int deviceId,
                      int listenerId,
                      const char* newValue)
{
    // parse update
    IXML_Document* doc;
    int error = ixmlParseBufferEx(newValue, &doc);
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to parse received event feed update:\n%s", newValue);
        return -1;
    }

    //    if (ixmlDocument_getElementById(doc, OBIX_OBJ) != NULL)
    //    {
    // TODO change me to log_debug
    //    if (strncmp(newValue, "<feed", 5) != 0)
    //    {
    //        printf("Received new event:\n%s", newValue);
    //    }
    //    }

    // check whether we have fallen event

    if (ixmlDocument_getElementByAttrValue(doc,
                                           OBIX_ATTR_VAL,
                                           "FALLEN") != NULL
            || ixmlDocument_getElementByAttrValue(doc,
                                                  OBIX_ATTR_VAL,
                                                  "PULSE") != NULL
            || ixmlDocument_getElementByAttrValue(doc,
                                                  OBIX_ATTR_VAL,
                                                  "NOVITALS") != NULL)
    {
        // fall event occurred! notify oBIX server
        // lock the mutex because we use connection to the server which can
        // be used from another thread simultaneously
        pthread_mutex_lock(&_targetMutex);
        printf("Somebody fell down..\n");
        error = obix_writeValue(SERVER_CONNECTION,
                                _deviceId,
                                "fall",
                                XML_TRUE,
                                OBIX_T_BOOL);
        if (error != OBIX_SUCCESS)
        {
            log_error("Unable to notify oBIX server about fall event.");
        }
        pthread_mutex_unlock(&_targetMutex);
    }

    ixmlDocument_free(doc);
    return 0;
}

int loadDeviceData(IXML_Element* deviceConf)
{
    if (deviceConf == NULL)
    {
        return -1;
    }

    // get device object stub
    IXML_Element* deviceData = config_getChildTag(deviceConf, OBIX_OBJ, TRUE);
    if (deviceData == NULL)
    {
        return -1;
    }

    // create a copy of device data
    deviceData = ixmlElement_cloneWithLog(deviceData);
    IXML_Document* doc = ixmlNode_getOwnerDocument(ixmlElement_getNode(deviceData));

    // get target settings
    IXML_Element* target = config_getChildTag(deviceConf, "target", TRUE);
    if (target == NULL)
    {
        ixmlDocument_free(doc);
        return -1;
    }

    int targetsCount = config_getTagIntAttrValue(target, "count", TRUE, 1);
    if (targetsCount <= 0)
    {
        ixmlDocument_free(doc);
        return -1;
    }

    _targets = (Target*) calloc(targetsCount, sizeof(Target));
    _targetsCount = targetsCount;

    // get target stub
    target = config_getChildTag(target, OBIX_OBJ, TRUE);
    if (target == NULL)
    {
        ixmlDocument_free(doc);
        return -1;
    }

    // generate required amount of targets
    int i;
    IXML_Node* node;
    for (i = 0; i < targetsCount; i++)
    {
        ixmlDocument_importNode(doc, ixmlElement_getNode(target), TRUE, &node);
        ixmlNode_appendChild(ixmlElement_getNode(deviceData), node);

        // generate name and href of the new target
        char name[9];
        char displayName[10];
        char* href = (char*) malloc(10);

        sprintf(name, "target%d", i + 1);
        sprintf(displayName, "Target %d", i + 1);
        sprintf(href, "target%d/", i + 1);

        IXML_Element* t = ixmlNode_convertToElement(node);
        ixmlElement_setAttribute(t, OBIX_ATTR_NAME, name);
        ixmlElement_setAttribute(t, OBIX_ATTR_DISPLAY_NAME, displayName);
        ixmlElement_setAttribute(t, OBIX_ATTR_HREF, href);
        // save target
        _targets[i].uri = href;
    }

    _deviceData = doc;
    return 0;
}

int loadSettings(const char* fileName)
{
    IXML_Element* settings = config_loadFile(fileName);
    if (settings == NULL)
    {
        return -1;
    }

    // initialize obix_client library with loaded settings
    int error = obix_loadConfig(settings);
    if (error != OBIX_SUCCESS)
    {
        config_finishInit(FALSE);
        return error;
    }

    // load data which will be posted to oBIX server from settings file
    IXML_Element* element = config_getChildTag(settings, "device-info", TRUE);
    if (loadDeviceData(element) != 0)
    {
        return -1;
    }

    // TODO remove me
    // load test data
    element = config_getChildTag(settings, "test-data", TRUE);
    element = ixmlElement_cloneWithLog(element);
    _testData = ixmlNode_getOwnerDocument(ixmlElement_getNode(element));

    // finish initialization
    config_finishInit(TRUE);
    return 0;
}

void testCycle()
{
    IXML_NodeList* testList = ixmlDocument_getElementsByTagName(_testData, "list");
    int testCount = ixmlNodeList_length(testList);

    int i;
    for (i = 0; i < testCount; i++)
    {
        printf("Test step #%d from %d. Press Enter to continue.\n", i, testCount);
        getchar();

        char* testData = ixmlPrintNode(ixmlNodeList_item(testList, i));
        targetsListener(SENSOR_FLOOR_CONNECTION, 0, 0, testData);
        free(testData);
    }

    ixmlNodeList_free(testList);
    ixmlDocument_free(_testData);
}

/**
 * Sets all targets at server to not available and
 * removes all target values.
 */
void resetTargets()
{
    int i;
    for (i = 0; i < _targetsCount; i++)
    {
        if (_targets[i].id != 0)
        {
            targetRemoveTask(&(_targets[i]));
        }

        free(_targets[i].x);
        free(_targets[i].y);
        free(_targets[i].uri);
    }

    free(_targets);
}

// TODO this task checks last floor event and sends it to
void checkEventsTask(void* args)
{
    static BOOL receivedEvent;
    int error = curl_ext_get(_curl_handle, "http://195.156.198.23/obix/elsi/Stok/event/");
    if (error != 0)
    {
        log_error("Unable to read last floor event.");
    }

    if ((strstr(_curl_handle->inputBuffer, "FALLEN") != NULL) ||
            (strstr(_curl_handle->inputBuffer, "PULSE") != NULL) ||
            (strstr(_curl_handle->inputBuffer, "NOVITALS") != NULL))
    {
        if (receivedEvent == FALSE)
        {
            receivedEvent = TRUE;
            eventFeedListener(SENSOR_FLOOR_FEED_CONNECTION, 0, 0, _curl_handle->inputBuffer);
        }
    }
    else
    {
        receivedEvent = FALSE;
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        printf("Usage: sensor_floor_driver <config_file>\n"
               " where <config_file> - Address of the configuration file.\n");
        return -1;
    }

    // load settings from file
    if (loadSettings(argv[1]) != 0)
    {
        log_error("Unable to load settings from file %s.\n", argv[1]);
        return -1;
    }

    // initialize the task thread
    _taskThread = ptask_init();
    if (_taskThread == NULL)
    {
        log_error("Unable to initialize separate thread.");
    }

    // open connection to the Sensor Floor
    int error = obix_openConnection(SENSOR_FLOOR_CONNECTION);
    if (error != OBIX_SUCCESS)
    {
        log_error("Unable to establish connection with the Sensor Floor.\n");
        return error;
    }

    // open second connection to the Sensor Floor (for events)
    error = obix_openConnection(SENSOR_FLOOR_FEED_CONNECTION);
    if (error != OBIX_SUCCESS)
    {
        log_error("Unable to establish second connection with the Sensor "
                  "Floor.\n");
        return error;
    }

    // open connection to oBIX server
    error = obix_openConnection(SERVER_CONNECTION);
    if (error != OBIX_SUCCESS)
    {
        log_error("Unable to establish connection with oBIX server.\n");
        return error;
    }

    // register Sensor Floor at oBIX server
    char* deviceData = ixmlPrintDocument(_deviceData);
    _deviceId = obix_registerDevice(SERVER_CONNECTION, deviceData);
    free(deviceData);
    if (_deviceId < 0)
    {
        log_error("Unable to register Sensor Floor at oBIX server.\n");
        return -1;
    }

    // register listener for targets list of the Sensor Floor
    int targetListenerId = obix_registerListener(SENSOR_FLOOR_CONNECTION,
                           0,
                           "/obix/elsi/Stok/targets/",
                           &targetsListener);
    if (targetListenerId < 0)
    {
        log_error("Unable to register update listener for targets.\n");
        return targetListenerId;
    }

    // register listener of event feed
    int feedListenerId = obix_registerListener(SENSOR_FLOOR_FEED_CONNECTION,
                         0,
                         "/obix/elsi/Stok/eventFeed/",
                         &eventFeedListener);
    if (feedListenerId < 0)
    {
        printf("Unable to register update listener for event feed.\n");
        return feedListenerId;
    }

    // TODO workaround for catching FALLEN event
    curl_ext_create(&_curl_handle);
    int taskId = ptask_schedule(_taskThread,
                                &checkEventsTask,
                                NULL,
                                LAST_EVENT_POOL_PERIOD,
                                EXECUTE_INDEFINITE);


//	testCycle();

    printf("Sensor Floor is successfully registered at the oBIX server\n\n"
           "Press Enter to stop driver...\n");
    // wait for user input
    getchar();

    // remove targets listener (it is automatically done by calling
    // obix_dispose(), but we want to reset target values at the server before
    // exiting, and we need to stop updating values before this
    obix_unregisterListener(SERVER_CONNECTION, _deviceId, targetListenerId);

    // reset and free all targets
    resetTargets();

    // release all resources allocated by oBIX client library.
    // No need to close connection or unregister listener explicitly - it is
    // done automatically.
    obix_dispose();
    ixmlDocument_free(_deviceData);

    ptask_cancel(_taskThread, taskId, FALSE);
    ptask_dispose(_taskThread, TRUE);

    return 0;
}
