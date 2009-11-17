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
 * Test application which emulates a generic oBIX adapter. It is created for
 * testing how much adapters can run simultaneously on one platform.
 * The application is based on example_timer.c and has almost the same
 * functionality. The differences are:
 * - it creates bigger device record on the server (as usual device would
 *   have more state variables published)
 * - it listens to more values at the oBIX server
 * - it allocates more memory in order to emulate more complicated adapter
 * - it tries to allocate some memory periodically and exits only if memory
 *   allocation failed.
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

#include <obix_utils.h>
#include <obix_client.h>
#include <log_utils.h>
#include <ptask.h>

/** ID of the connection which is described in configuration file. */
#define CONNECTION_ID 0
/** Data which is posted to oBIX server. It has lot's of dummy data*/
const char* DEVICE_DATA =
    "<obj name=\"ExampleTimer\" displayName=\"Example Timer\" href=\"%s\">\r\n"
    "  <reltime name=\"time\" displayName=\"Elapsed Time\" href=\"%stime\" val=\"PT0S\" writable=\"true\"/>\r\n"
    "  <bool name=\"reset\" displayName=\"Reset Timer\" href=\"%sreset\" val=\"false\" writable=\"true\"/>\r\n"
    "  <obj displayName=\"Dummy data\" href=\"%sdummy/\" >\r\n"
    "    <str displayName=\"String 1\" href=\"str1\" val=\"Hello! I am dummy string 1\" writable=\"true\"/>\r\n"
    "    <str displayName=\"String 2\" href=\"str2\" val=\"Hello! I am dummy string 2\" writable=\"true\"/>\r\n"
    "    <str displayName=\"String 3\" href=\"str3\" val=\"Hello! I am dummy string 3\" writable=\"true\"/>\r\n"
    "    <str displayName=\"String 4\" href=\"str4\" val=\"Hello! I am dummy string 4\" writable=\"true\"/>\r\n"
    "    <str displayName=\"String 5\" href=\"str5/\" val=\"Hello! I am dummy string 5\" writable=\"true\" >\r\n"
    "      <int displayName=\"Integer 1\" href=\"int1\" val=\"1\" writable=\"true\"/>\r\n"
    "      <int displayName=\"Integer 2\" href=\"int2\" val=\"2\" writable=\"true\"/>\r\n"
    "      <int displayName=\"Integer 3\" href=\"int3\" val=\"3\" writable=\"true\"/>\r\n"
    "      <int displayName=\"Integer 4\" href=\"int4\" val=\"4\" writable=\"true\"/>\r\n"
    "      <int displayName=\"Integer 5\" href=\"int5\" val=\"5\" writable=\"true\"/>\r\n"
    "      <int displayName=\"Integer 6\" href=\"int6\" val=\"6\" writable=\"true\"/>\r\n"
    "      <int displayName=\"Integer 7\" href=\"int7\" val=\"7\" writable=\"true\"/>\r\n"
    "      <int displayName=\"Integer 8\" href=\"int8\" val=\"8\" writable=\"true\"/>\r\n"
    "      <int displayName=\"Integer 9\" href=\"int9\" val=\"9\" writable=\"true\"/>\r\n"
    "      <int displayName=\"Integer 0\" href=\"int0\" val=\"0\" writable=\"true\"/>\r\n"
    "    </str>\r\n"
    "  </obj>\r\n"
    "</obj>";

/** Elapsed time is stored here. */
long _time;

/**
 * Need mutex for synchronization, because _time variable is accessed
 * from two threads.
 */
pthread_mutex_t _time_mutex = PTHREAD_MUTEX_INITIALIZER;
/** Separate thread for timer value updating. */
Task_Thread* _taskThread;
/** ID of the task which increases time every second. */
int _timerTaskId;

BOOL _shutDown = FALSE;

/**
 * Handles changes of @a "reset" value.
 * The function implements #obix_update_listener() prototype and is registered
 * as a listener of @a "reset" param using #obix_registerListener().
 * If @a "reset" value is changed at oBIX server to @a "true" it will set it
 * back to @a "false" and reset timer.
 *
 * @see obix_update_listener(), obix_registerListener().
 */
int resetListener(int connectionId,
                  int deviceId,
                  int listenerId,
                  const char* newValue)
{
    static oBIX_Batch* batch = NULL;
    int error;

    if (strcmp(newValue, XML_FALSE) == 0)
    {
        // ignore this update - we are waiting for "reset" to be "true"
        return OBIX_SUCCESS;
    }

    if (batch == NULL)
    {
        // we are going to send two commands to the server:
        // 1. set timer value to 0
        // 2. set "reset" field back to "false"
        // We will generate once the oBIX Batch object which contains both
        // these operations and will send it instead of two separate commands
        // every time when needed.
        batch = obix_batch_create(connectionId);
        if (batch == NULL)
        {
            log_error("Unable to create Batch object!\n");
            return -1;
        }
        error = obix_batch_writeValue(batch,
                                      deviceId,
                                      "time",
                                      "PT0S",
                                      OBIX_T_RELTIME);
        if (error < 0)
        {
        	log_error("Unable to create Batch object!\n");
            return error;
        }
        error = obix_batch_writeValue(batch,
                                      deviceId,
                                      "reset",
                                      XML_FALSE,
                                      OBIX_T_BOOL);
        if (error < 0)
        {
        	log_error("Unable to create Batch object!\n");
            return error;
        }
    }

    // reset timer task so that the next time of execution will be now + 1 sec
    if (_taskThread != NULL)
    {
        ptask_reset(_taskThread, _timerTaskId);
    }
    // new value is true, thus we need to reset timer
    pthread_mutex_lock(&_time_mutex);
    _time = 0;
    pthread_mutex_unlock(&_time_mutex);
    printf("Timer is set to 0.\n");

    error = obix_batch_send(batch);
    if (error != OBIX_SUCCESS)
    {
    	log_error("Unable to update timer attributes using oBIX Batch.\n");
        return error;
    }

    return OBIX_SUCCESS;
}

/**
 * One more listener. Does nothing except logging the received value.
 */
int dummyListener(int connectionId,
                  int deviceId,
                  int listenerId,
                  const char* newValue)
{
    printf("New data: id %d; value \"%s\"\n", listenerId, newValue);
}

/**
 * Updates timer value and writes it to oBIX server.
 * Implements #periodic_task() prototype and is scheduled using
 * #ptask_schedule(). This method is executed in a separate thread that is why
 * it uses #_time_mutex for synchronization with #resetListener() which sets
 * timer to @a 0.
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
    // increase time by one second
    _time += 1000;
    // if time is bigger than 10 days - reset timer.
    if (_time > 864000000)
    {
        _time = 0;
    }
    char* reltime = obix_reltime_fromLong(_time, RELTIME_DAY);
    pthread_mutex_unlock(&_time_mutex);

    // send updated time to the server
    int deviceId = *((int*) arg);
    int error = obix_writeValue(CONNECTION_ID, deviceId, "time", reltime, OBIX_T_RELTIME);
    if (error != OBIX_SUCCESS)
    {
    	log_error("Unable to update timer value at the server.\n");
    }
    free(reltime);
}

/**
 * Generates device data in oBIX format.
 *
 * @param deviceUri Address at the server where device will be stored to. This
 * 					address will be written as the @a href attribute of the root
 * 					object.
 * @return Data which should be posted to the server.
 */
char* getDeviceData(char* deviceUri)
{
    // length of data template +
    // 3 * (length of device URI - length of '%s' which is substituted) +
    // 1 for end character
    char* data = (char*) malloc(strlen(DEVICE_DATA)
                                + (strlen(deviceUri) - 2)*4 + 1);
    sprintf(data, DEVICE_DATA, deviceUri, deviceUri, deviceUri, deviceUri);
    return data;
}

void signalHandler(int signal)
{
    _shutDown = TRUE;
    printf("\nSignal %d is caught, terminating.\n", signal);
}

/**
 * Entry point of the Timer application.
 * It takes the name of the configuration file (use
 * example_timer_config.xml).
 *
 * @see example_timer_config.xml
 */
int main(int argc, char** argv)
{
    if (argc != 5)
    {
        printf("Usage: %s <config_file> <device_uri> <const_mem> <var_mem>\n"
               " where <config_file> - Address of the configuration file;\n"
               "       <device_uri>  - URI at which the device will be "
               "registered;\n"
               "       <const_mem>   - Additional memory, which will be "
               "allocated all\n"
               "                       the time;\n"
               "       <var_mem>     - Amount of memory which adapter will try "
               "to allocate\n"
               "                       and release periodically.\n", argv[0]);
        return -1;
    }

    // load connection settings from file
    int error = obix_loadConfigFile(argv[1]);
    if (error != OBIX_SUCCESS)
    {
    	log_error("Unable to load configuration file.\n");
        return error;
    }

    // open connection to the server
    error = obix_openConnection(CONNECTION_ID);
    if (error != OBIX_SUCCESS)
    {
    	log_error("Unable to establish connection with oBIX server.\n");
        return error;
    }

    // register timer device at the server
    char* deviceData = getDeviceData(argv[2]);
    int deviceId = obix_registerDevice(CONNECTION_ID, deviceData);
    free(deviceData);
    if (deviceId < 0)
    {
    	log_error("Unable to register device at oBIX server.\n");
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
    	log_error("Unable to register update listener.\n");
        return listenerId;
    }

    // Initialize separate thread for timer
    _taskThread = ptask_init();
    if (_taskThread == NULL)
    {
    	log_error("Unable to start separate thread for timer.\n");
        return -1;
    }
    // start updating time once in a second
    _timerTaskId = ptask_schedule(_taskThread, &timerTask, &deviceId,
                                  1000, EXECUTE_INDEFINITE);

    printf("Test device is successfully registered at the server at the "
           "following address: %s\n", argv[2]);

    // allocate additional memory
    int size = atoi(argv[3]);
    char* additionalMemory = (char*) malloc(size);
    if (additionalMemory == NULL)
    {
        printf("Unable to allocate %d bytes of memory!\n", size);
        log_error("Unable to allocate %d bytes of memory!\n", size);
        return -1;
    }
    // fill allocated memory with very 'sensitive' data :)
    int i;
    for (i = 0; i < size; i++)
    {
        additionalMemory[i] = (char)i;
    }

    printf("%d additional bytes are successfully allocated!\n", size);

    // register more fake listeners
    char* hrefs[] = {"dummy/str1",
                     "dummy/str2",
                     "dummy/str3",
                     "dummy/str4",
                     "dummy/str5/",
                     "dummy/str5/int1",
                     "dummy/str5/int2",
                     "dummy/str5/int3",
                     "dummy/str5/int4",
                     "dummy/str5/int5",
                     "dummy/str5/int6",
                     "dummy/str5/int7",
                     "dummy/str5/int8",
                     "dummy/str5/int9",
                     "dummy/str5/int0"};
    for (i = 0; i < 15; i++)
    {
        error = obix_registerListener(CONNECTION_ID,
                                      deviceId,
                                      hrefs[i],
                                      &dummyListener);
        if (error < 0)
        {
        	log_error("Unable to register the dummy listener number %d!\n", i + 1);
            return -1;
        }
    }

    // register signal handler
    signal(SIGINT, &signalHandler);
    printf("Press Ctrl+C to shutdown.\n");

    // fall into the endless loop
    size = atoi(argv[4]);
    while (_shutDown == FALSE)
    {
        // try to allocate some more memory
        char* buffer = (char*) malloc(size);
        if (buffer == NULL)
        {
            printf("Unable to allocate variable piece of memory - %d bytes!\n",
                   size);
            log_error("Unable to allocate variable piece of memory - "
                      "%d bytes!\n", size);
            return -1;
        }
        buffer[0] = '\0';
        strlen(buffer);
        free(buffer);

        sleep(2);
    }

    // shutdown gracefully
    ptask_dispose(_taskThread, TRUE);

    // release all resources allocated by oBIX client library.
    // No need to close connection or unregister listener explicitly - it is
    // done automatically.
    return obix_dispose();
}
