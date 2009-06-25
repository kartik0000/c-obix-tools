/** @file
 * This is simple oBIX Timer device implementation, which demonstrates
 * usage of oBIX client library.
 * It shows the time elapsed after timer was started or reset by user.
 * Device registers itself at oBIX Server, regularly updates elapsed time on
 * it and listens to updates of "reset" parameter. If someone changes "reset"
 * to true, that elapsed time is set to 0.
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <ptask.h>
#include <obix_client.h>
//TODO think about changing printf calls to lwl_ext library

/** ID of the connection which is described in configuration file. */
#define CONNECTION_ID 0
/** Data which is posted to oBIX server. */
const char* DEVICE_DATA =
    "<obj name=\"ExampleTimer\" displayName=\"Example Timer\" href=\"%s\">\r\n"
    "  <reltime name=\"time\" displayName=\"Elapsed Time\" href=\"%stime\" val=\"PT0S\" writable=\"true\"/>\r\n"
    "  <bool name=\"reset\" displayName=\"Reset Timer\" href=\"%sreset\" val=\"false\" writable=\"true\"/>\r\n"
    "</obj>";

/** Elapsed time is stored here. */
unsigned long _time;
/** Used for string representation of the timer value. */
char _time_str[32];
/**
 * Need mutex for synchronization, because _time variable is accessed
 * from two threads.
 */
pthread_mutex_t _time_mutex = PTHREAD_MUTEX_INITIALIZER;
/** Separate thread for timer value updating. */
Task_Thread* taskThread;

/**
 * Handles changes of @a "reset" value.
 * The function implements #obix_update_listener() prototype and is registered
 * as a listener of @a "reset" param using #obix_registerListener().
 * If @a "reset" value is changed at oBIX server to @a "true" it will set it
 * back to "false" and reset timer.
 *
 * @see obix_update_listener(), obix_registerListener().
 */
int resetListener(int connectionId,
                  int deviceId,
                  int listenerId,
                  const char* newValue)
{
    if (strcmp(newValue, "false") == 0)
    {
        // ignore this update
        return OBIX_SUCCESS;
    }

    // new value is true, thus we need to reset timer
    pthread_mutex_lock(&_time_mutex);
    _time = 0;
    pthread_mutex_unlock(&_time_mutex);
    printf("Timer is set to 0.\n");

    // reset also "reset" field of the timer at the oBIX server
    int error = obix_writeValue(connectionId, deviceId, "reset", "false", OBIX_T_BOOL);
    if (error != OBIX_SUCCESS)
    {
        printf("Unable to set \"reset\" field at the oBIX server "
               "to \"false\".\n");
        return -1;
    }

    return OBIX_SUCCESS;
}

/**
 * Generates string representation of elapsed time
 * according to @a xs:duration XML data type.
 */
void generateTimeString()
{
    unsigned long hours = _time / 3600;
    int minutes = (_time % 3600) / 60;
    int seconds = _time % 60;

    int length = sprintf(_time_str, "PT");
    if (hours != 0)
    {
        length += sprintf(_time_str + length, "%luH", hours);
    }
    if (minutes != 0)
    {
        length += sprintf(_time_str + length, "%dM", minutes);
    }

    sprintf(_time_str + length, "%dS", seconds);
}

/**
 * Updates timer value and writes it to oBIX server.
 * Implements #periodic_task() prototype and is scheduled using
 * #ptask_schedule(). This method is executed in a separate thread that is why
 * it uses #_time_mutex for synchronization with #resetListener() which sets
 * timer to 0.
 *
 * @see periodic_task(), ptask_schedule().
 * @param arg Assumes that a pointer to the device ID is passed here. Device ID
 *            is used for updating time value at the server.
 */
void timerTask(void* arg)
{
    // lock mutex in order to prevent other threads
    // from accessing _time variable while we update it
    pthread_mutex_lock(&_time_mutex);
    _time++;
    generateTimeString();
    pthread_mutex_unlock(&_time_mutex);

    // send updated time to the server
    int deviceId = *((int*) arg);
    int error = obix_writeValue(CONNECTION_ID, deviceId, "time", _time_str, OBIX_T_RELTIME);
    if (error != OBIX_SUCCESS)
    {
        printf("Unable to update elapsed time at the server.\n");
    }
}

char* getDeviceData(char* deviceUri)
{
    // length of data template +
    // 3 * (length of device URI - length of '%s' which is substituted) +
	// 1 for end character
    char* data = (char*) malloc(strlen(DEVICE_DATA)
                                + (strlen(deviceUri) - 2)*3 + 1);
    sprintf(data, DEVICE_DATA, deviceUri, deviceUri, deviceUri);
    return data;
}

/**
 * Entry point of the Timer application.
 * It takes the name of the configuration file (use
 * @a example_timer_config.xml).
 *
 * @see example_timer_config.xml
 */
int main(int argc, char** argv)
{
    if (argc != 3)
    {
        printf("Usage: example_clock <config_file> <device_uri>\n"
               " where <config_file> - Address of the configuration file;\n"
               "       <device_uri>  - URI at which device will be "
               "registered.\n");
        return -1;
    }

    // load connection settings from file
    int error = obix_loadConfigFile(argv[1]);
    if (error != OBIX_SUCCESS)
    {
        printf("Unable to load configuration file.\n");
        return error;
    }

    // open connection to the server
    error = obix_openConnection(CONNECTION_ID);
    if (error != OBIX_SUCCESS)
    {
        printf("Unable to establish connection with oBIX server.\n");
        return error;
    }

    // register timer device at the server
    char* deviceData = getDeviceData(argv[2]);
    int deviceId = obix_registerDevice(CONNECTION_ID, deviceData);
    free(deviceData);
    if (deviceId < 0)
    {
        printf("Unable to register device at oBIX server.\n");
        return deviceId;
    }

    // register listener of the "reset" field
    // it will be invoked every time when someone changes
    // "reset" field at the server
    int listenerId = obix_registerListener(CONNECTION_ID,
                                           deviceId,
                                           "reset",
                                           &resetListener);
    if (listenerId < 0)
    {
        printf("Unable to register update listener.\n");
        return listenerId;
    }

    // Initialize separate thread for timer
    taskThread = ptask_init();
    if (taskThread == NULL)
    {
        printf("Unable to start separate thread for timer.\n");
        return -1;
    }
    // start updating time once in a second
    int timerTaskId = ptask_schedule(taskThread, &timerTask, &deviceId,
                                     1000, EXECUTE_INDEFINITE);

    printf("Example timer is successfully registered at the server\n\n"
           "Press Enter to stop timer and exit...\n");
    // wait for user input
    getchar();

    // shutdown gracefully

    // stop timer task
    if (ptask_cancel(taskThread, timerTaskId, TRUE))
    {
        printf("Unable to stop timer task.\n");
        return -1;
    }
    ptask_dispose(taskThread);

    // release all resources allocated by oBIX client library.
    // No need to close connection or unregister listener explicitly - it is
    // done automatically.
    obix_dispose();

    return 0;
}
