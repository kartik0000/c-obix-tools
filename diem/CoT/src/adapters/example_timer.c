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
 * Simple oBIX Timer device implementation, which demonstrates usage of oBIX
 * client library.
 * It shows the time elapsed after timer was started or reset by user.
 * Device registers itself at oBIX server, regularly updates elapsed time on
 * it and listens to updates of "reset" parameter. If someone changes "reset"
 * to true, than elapsed time is set to 0.
 * Configuration file template can be found at res/example_timer_config.xml
 *
 * @author Andrey Litvinov
 * @version 1.1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <obix_utils.h>
#include <obix_client.h>
#include <ptask.h>
//TODO think about changing printf to log_utils.h

/** ID of the connection which is described in configuration file. */
#define CONNECTION_ID 0
/** Data which is posted to oBIX server. */
const char* DEVICE_DATA =
    "<obj name=\"ExampleTimer\" displayName=\"Example Timer\" href=\"%s\">\r\n"
    "  <reltime name=\"time\" displayName=\"Elapsed Time\" href=\"%stime\" val=\"PT0S\" writable=\"true\"/>\r\n"
    "  <bool name=\"reset\" displayName=\"Reset Timer\" href=\"%sreset\" val=\"false\" writable=\"true\"/>\r\n"
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
            printf("Unable to create Batch object!\n");
            return -1;
        }
        error = obix_batch_writeValue(batch,
                                      deviceId,
                                      "time",
                                      "PT0S",
                                      OBIX_T_RELTIME);
        if (error < 0)
        {
            printf("Unable to create Batch object!\n");
            return error;
        }
        error = obix_batch_writeValue(batch,
                                      deviceId,
                                      "reset",
                                      XML_FALSE,
                                      OBIX_T_BOOL);
        if (error < 0)
        {
            printf("Unable to create Batch object!\n");
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
        printf("Unable to update timer attributes using oBIX Batch.\n");
        return error;
    }
    // Instead of batch we could use 2 calls of obix_writeValue() like this:
    //
    //    // write zero value to the server
    //    error = obix_writeValue(connectionId,
    //                                deviceId,
    //                                "time",
    //                                "PT0S",
    //                                OBIX_T_RELTIME);
    //    if (error != OBIX_SUCCESS)
    //    {
    //        printf("Unable to set timer to zero at the server.\n");
    //    }
    //    // reset also "reset" field of the timer at the oBIX server
    //    error = obix_writeValue(connectionId,
    //                            deviceId,
    //                            "reset",
    //                            "false",
    //                            OBIX_T_BOOL);
    //    if (error != OBIX_SUCCESS)
    //    {
    //        printf("Unable to set \"reset\" field at the oBIX server "
    //               "to \"false\".\n");
    //        return -1;
    //    }

    return OBIX_SUCCESS;
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
        printf("Unable to update timer value at the server.\n");
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
                                + (strlen(deviceUri) - 2)*3 + 1);
    sprintf(data, DEVICE_DATA, deviceUri, deviceUri, deviceUri);
    return data;
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
    if (argc != 3 ||
    		// check that device URI starts and ends with '/'
    		(argv[2][0] != '/' || argv[2][strlen(argv[2]) - 1] != '/'))
    {
        printf("Usage: %s <config_file> <device_uri>\n"
               " where <config_file> - Address of the configuration file;\n"
               "       <device_uri>  - relative URI at which the device will "
               "be registered,\n"
               "                       e.g. \"/obix/ExampleTimer/\".\n",
               argv[0]);
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
    _taskThread = ptask_init();
    if (_taskThread == NULL)
    {
        printf("Unable to start separate thread for timer.\n");
        return -1;
    }
    // start updating time once in a second
    _timerTaskId = ptask_schedule(_taskThread, &timerTask, &deviceId,
                                  1000, EXECUTE_INDEFINITE);

    printf("Example timer is successfully registered at the server\n\n"
           "Press Enter to stop timer and exit...\n");
    // wait for user input
    getchar();

    // shutdown gracefully

    // stop timer task
    if (ptask_cancel(_taskThread, _timerTaskId, TRUE))
    {
        printf("Unable to stop timer task.\n");
        return -1;
    }
    ptask_dispose(_taskThread, TRUE);

    // release all resources allocated by oBIX client library.
    // No need to close connection or unregister listener explicitly - it is
    // done automatically.
    return obix_dispose();
}
