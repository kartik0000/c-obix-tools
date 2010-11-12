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
 * This file defines the interface of sensor floor communication module.
 * The module is used by MariMils (Elsi) Sensor Floor adapter.
 * @see sensor_floor_pico.c
 * @author Andrey Litvinov
 */

#ifndef PICO_HTTP_FEED_READER_H_
#define PICO_HTTP_FEED_READER_H_

#include <ixml_ext.h>

/**
 * Structure representing one sensor of the sensor floor.
 */
typedef struct _PicoSensor
{
    char* id;
    char* x;
    char* y;
}
PicoSensor;

/**
 * Structure representing a point at sensor floor. A point is an activated
 * sensor with its id, coordinates, and level of activation (magnitude).
 */
typedef struct _PicoClusterPoint
{
    char* id;
    const char* x;
    const char* y;
    char* magnitude;
}
PicoClusterPoint;

/**
 * Structure representing a cluster object from sensor floor's Pico server.
 * Cluster is a group of points at the sensor floor, which are considered by
 * Pico server as one person (object).
 */
typedef struct _PicoCluster
{
    char* id;
    char* x;
    char* y;
    char* vx;
    char* vy;
    char* magnitude;
    int pointsCount;
    const PicoClusterPoint** points;
}
PicoCluster;

/**
 * Listener method, which receives new cluster objects read from the sensor
 * floor.
 *
 * @param cluster New cluster object, containing coordinates of a cluster. This
 * 				  object will be freed automatically after the listener method
 * 				  ends, so do not store links to received clusters.
 */
typedef void (*pico_cluster_listener)(PicoCluster* cluster);

/**
 * Checks whether the provided cluster point is "zero" point, i.e. all values
 * are zeros.
 *
 * @param point Point object to check.
 * @return @a TRUE if provided point is a "zero" point, FALSE otherwise.
 */
BOOL picoClusterPoint_isZero(const PicoClusterPoint* point);

/**
 * Returns "zero" cluster object, i.e. object will all values set to "0".
 */
PicoCluster* picoCluster_getZero();

/**
 * Returns empty cluster object, i.e. object where all values are NULL.
 */
PicoCluster* picoCluster_getEmpty();

/**
 * Releases memory allocated for the cluster object.
 *
 * @param cluster Object to be freed.
 */
void picoCluster_free(PicoCluster* cluster);

/**
 * Initializes the feed reader and registers cluster listener.
 *
 * @param listener Method, which will be invoked when new cluster data is
 * 				   received from the sensor floor.
 * @param pointsPerCluster Defines how many points per cluster will be parsed.
 * @return @a 0 on success, @a -1 on error.
 */
int pico_initFeedReader(pico_cluster_listener listener, int pointsPerCluster);

/**
 * Tries to stop the feed reader.
 *
 * Takes no effect if no reader is executing right now.
 * It doesn't guarantees that the reader will stop. The reader may
 * be stuck on waiting for data input from the connection. In that case the
 * only way to stop it is to kill it.
 *
 */
void pico_stopFeedReader();

/**
 * Frees all memory allocated for the feed reader.
 */
void pico_disposeReader();

/**
 * Performs the actual reading of the sensor floor HTTP feed, i.e. connects to
 * the provided URL and parses received output.
 *
 * @param serverAddress Address of the pico server.
 * @param roomName Name of the room that should be monitored.
 * @return @a 0 when connection is closed without errors, @a -1 on error.
 */
int pico_readFeed(const char* serverAddress, const char* roomName);

/**
 * Performs the reading of the sensor floor HTTP feed from a file.
 * This method is intended for testing purposes.
 *
 * @param fileName Name of the file to read.
 * @return @a 0 when connection is closed without errors, @a -1 on error.
 */
int pico_readFeedFromFile(const char* fileName);

/**
 * Reads and returns sensors location in the specified room.
 *
 * @param serverAddress Address of the pico server.
 * @param roomName Name of the room, which data should be read.
 * @param sensors Link to the array of sensors is returned here. Position of
 * 				  a sensor in the array corresponds to its id, i.e.
 *                sensor[i]->id = "i". Some of elements of the array can be
 *                empty (NULL).
 * @param sensorCount Size of the returned array is written here.
 * @return @a 0 on success, @a -1 on error.
 */
int pico_readSensorsInfoFromUrl(const char* serverAddress,
                                const char* roomName,
                                const PicoSensor*** sensors,
                                int* sensorCount);

/**
 * Reads info about sensors location in the room from the file.
 *
 * @param fileName Name of the file to read. It should contain XML document with
 *                 the same syntax as provided by pico server HTTP interface
 *                 at the url /<room>/info.
 * @param sensors Link to the array of sensors is returned here. Position of
 * 				  a sensor in the array corresponds to its id, i.e.
 *                sensor[i]->id = "i". Some of elements of the array can be
 *                empty (NULL).
 * @param sensorCount Size of the returned array is written here.
 * @return @a 0 on success, @a -1 on error.
 */
int pico_readSensorInfoFromFile(const char* fileName,
                                const PicoSensor*** sensors,
                                int* sensorCount);

#endif /* PICO_HTTP_FEED_READER_H_ */
