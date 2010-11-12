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
 * This is a communication module for the MariMils (Elsi) Sensor Floor adapter.
 * It handles the communication with Pico server HTTP feed.
 * @see pico_http_feed_reader.h
 * @author Andrey Litvinov
 */

#include <string.h>
#include <stdio.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <curl/curl.h>
#include <time.h>
#include <pthread.h>

#include <curl_ext.h>
#include <log_utils.h>
#include <ixml_ext.h>
#include "pico_http_feed_reader.h"

/**
 * Default point, which is returned when no data is received from the floor.
 */
static const PicoClusterPoint _zeroPoint =
    {
        "0", "0", "0", "0"
    };
/**
 * Default cluster, which is returned when no data is received from the floor.
 */
static PicoCluster _zeroCluster =
    {
        "0", "0", "0", "0", "0", "0", 0, NULL
    };
/** Empty point constant. */
static const PicoClusterPoint _emptyPoint =
    {
        NULL, NULL, NULL, NULL
    };
/** Empty cluster. */
static PicoCluster _emptyCluster =
    {
        NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL
    };
/** Curl handler for HTTP communication. */
static CURL* _curl = NULL;
/**  XML parser handler. */
static xmlParserCtxtPtr _xmlParser = NULL;
/** Place to store a cluster object during XML parsing. */
static PicoCluster* _currentCluster = NULL;
/** Place where Curl writes error messages. */
static char* _httpErrorBuffer = NULL;
/** Method which will be invoked to handle new parsed cluster objects. */
static pico_cluster_listener _clusterListener = NULL;
/** Defines how many points inside each cluster will be parsed. */
static int _pointsPerCluster = 0;
/** Array of floor sensors, with their ids and coordinates. */
static PicoSensor** _sensors = NULL;
/** Size of the sensors array. */
static int _sensorCount = 0;
/** Flag indicating that current execution of the feed reader should
 * be stopped. */
static BOOL _feedReaderCanceled = FALSE;


/**
 * libcurl write callback function. Called each time when
 * something is received to write down the received data.
 */
static size_t inputWriter(char* inputData,
                          size_t size,
                          size_t nmemb,
                          void* arg)
{
    // check whether request has been canceled
    if (_feedReaderCanceled == TRUE)
    {	// stop doing anything, CURL should handle this as a write error.
        log_warning("Feed reader was forced to close.");
        return 0;
    }

    size_t newDataSize = size * nmemb;

    log_debug("Pico feed reader: New data block size %d.", newDataSize);

    int error = xmlParseChunk(_xmlParser, inputData, newDataSize, 0);
    if (error != 0)
    {
        char receivedChunk[newDataSize + 1];
        memcpy(receivedChunk, inputData, newDataSize);
        receivedChunk[newDataSize] = '\0';
        log_error("Pico feed reader: "
                  "Unable to parse received buffer (error #%d): %s",
                  error, receivedChunk);
        return 0;
    }

    return newDataSize;
}

/**
 * Releases memory allocated for sensor array.
 *
 * @param sensors Link to the sensor array.
 * @param count Size of the array.
 */
static void picoSensors_free(PicoSensor** sensors, int count)
{
    int i;
    for (i = 0; i < count; i++)
    {
        if (sensors[i] != NULL)
        {
            if (sensors[i]->id != NULL)
            {
                free(sensors[i]->id);
            }
            if (sensors[i]->x != NULL)
            {
                free(sensors[i]->x);
            }
            if (sensors[i]->y != NULL)
            {
                free(sensors[i]->y);
            }

            free(sensors[i]);
        }
    }

    free(sensors);
}

/**
 * Creates an array of references to the provided point object.
 */
static const PicoClusterPoint** picoClusterPoint_initArray(const PicoClusterPoint* point)
{
    const PicoClusterPoint** pointArray =
        (const PicoClusterPoint**) calloc(
            _pointsPerCluster, sizeof(PicoClusterPoint*));
    int i;
    for (i = 0; i < _pointsPerCluster; i++)
    {
        pointArray[i] = point;
    }

    return pointArray;
}

/**
 * Initializes constant cluster objects. One has all values == string "0" and
 * the other one - all NULLs.
 */
static void picoCluster_initConstants()
{
    if (_pointsPerCluster > 0)
    {
        // initialize empty cluster objects. they should contain empty points
        _zeroCluster.points = picoClusterPoint_initArray(&_zeroPoint);
        _zeroCluster.pointsCount = _pointsPerCluster;

        _emptyCluster.points = picoClusterPoint_initArray(&_emptyPoint);
        _emptyCluster.pointsCount = _pointsPerCluster;
    }
}

/**
 * Releases memory allocated by cluster object constants.
 */
static void picoCluster_freeConstants()
{
    if (_zeroCluster.points != NULL)
    {
        free(_zeroCluster.points);
        _zeroCluster.points = NULL;
        _zeroCluster.pointsCount = 0;
    }
    if (_emptyCluster.points != NULL)
    {
        free(_emptyCluster.points);
        _emptyCluster.points = NULL;
        _emptyCluster.pointsCount = 0;
    }
}

/**
 * Allocates memory for a new cluster object. Cluster object is also filled
 * with point containing zero values.
 */
static PicoCluster* picoCluster_init()
{
    PicoCluster* cluster = (PicoCluster*) calloc(1, sizeof(PicoCluster));

    if (_pointsPerCluster > 0)
    {
        cluster->points = picoClusterPoint_initArray(&_zeroPoint);
    }

    return cluster;
}

/** Frees memory allocated for the point object. */
static void picoClusterPoint_free(const PicoClusterPoint* point)
{
	PicoClusterPoint* freeablePoint = (PicoClusterPoint*) point;
    if (freeablePoint->id != NULL)
    {
        free(freeablePoint->id);
    }
    if (freeablePoint->magnitude != NULL)
    {
        free(freeablePoint->magnitude);
    }
    // don't free x and y fields as they are the references to corresponding
    // sensor field.
    free(freeablePoint);
}

BOOL picoClusterPoint_isZero(const PicoClusterPoint* point)
{
    if (point == &_zeroPoint)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

PicoCluster* picoCluster_getZero()
{
    return &_zeroCluster;
}

PicoCluster* picoCluster_getEmpty()
{
    return &_emptyCluster;
}

void picoCluster_free(PicoCluster* cluster)
{
    if ((cluster == &_zeroCluster) || (cluster == &_emptyCluster))
    {	// do not try to delete empty cluster
        return;
    }

    if (cluster->x != NULL)
    {
        free(cluster->x);
    }
    if (cluster->y != NULL)
    {
        free(cluster->y);
    }
    if (cluster->vx != NULL)
    {
        free(cluster->vx);
    }
    if (cluster->vy != NULL)
    {
        free(cluster->vy);
    }
    if (cluster->id != NULL)
    {
        free(cluster->id);
    }
    if (cluster->magnitude != NULL)
    {
        free(cluster->magnitude);
    }
    if (cluster->points != NULL)
    {
        int i;
        for (i = 0; i < cluster->pointsCount; i++)
        {
            if (cluster->points[i] != &_zeroPoint)
            {
                picoClusterPoint_free(cluster->points[i]);
            }
        }
        free(cluster->points);
    }
    free(cluster);
}

/**
 * Checks whether the provided cluster contains NULL fields or empty strings.
 */
static int picoCluster_checkNullorEmpty(PicoCluster* cluster)
{
    return (cluster->id == NULL) || (cluster->magnitude == NULL) ||
           (cluster->x == NULL) || (cluster->y == NULL) ||
           (cluster->vx == NULL) || (cluster->vy == NULL) ||
           (*(cluster->id) == '\0') || (*(cluster->magnitude) == '\0') ||
           (*(cluster->x) == '\0') || (*(cluster->y) == '\0') ||
           (*(cluster->vx) == '\0') || (*(cluster->vy) == '\0');
}

/**
 * Parses attributes of the opening <cluster> tag.
 * This attributes contain main cluster data: id, coordinates, etc.
 * @return New parsed cluster object, or @a NULL if parsing failed.
 */
static PicoCluster* parseClusterStartElement(const xmlChar **attributes)
{
    PicoCluster* cluster = picoCluster_init();

    //parse cluster attributes
    int i = 0;
    while (attributes[i] != NULL)
    {
        if (strcmp((const char*) attributes[i], "id") == 0)
        {
            cluster->id = strdup((const char*) attributes[i+1]);
        }
        else if (strcmp((const char*) attributes[i], "x") == 0)
        {
            cluster->x = strdup((const char*) attributes[i+1]);
        }
        else if (strcmp((const char*) attributes[i], "y") == 0)
        {
            cluster->y = strdup((const char*) attributes[i+1]);
        }
        else if (strcmp((const char*) attributes[i], "vx") == 0)
        {
            cluster->vx = strdup((const char*) attributes[i+1]);
        }
        else if (strcmp((const char*) attributes[i], "vy") == 0)
        {
            cluster->vy = strdup((const char*) attributes[i+1]);
        }
        else if (strcmp((const char*) attributes[i], "magnitude") == 0)
        {
            cluster->magnitude = strdup((const char*) attributes[i+1]);
        }

        i += 2;
    }

    if (picoCluster_checkNullorEmpty(cluster))
    {
        log_error("Cluster is not parsed completely: id = %s; x = %s; y = %s; "
                  "vx = %s; vy = %s; magnitude = %s.",
                  cluster->id, cluster->x, cluster->y,
                  cluster->vx, cluster->vy, cluster->magnitude);
        picoCluster_free(cluster);
        return NULL;
    }

    log_debug("New cluster parsed: id = %s; x = %s; y = %s; "
              "vx = %s; vy = %s; magnitude = %s.",
              cluster->id, cluster->x, cluster->y,
              cluster->vx, cluster->vy, cluster->magnitude);

    return cluster;
}

/**
 * Parses attributes of a point inside cluster (<m> tag).
 * If point is parsed successfully, it is stored to the _currentCluster object.
 */
static void parsePointElement(const xmlChar **attributes)
{
    // we parse only first N points per cluster
    if (_currentCluster->pointsCount >= _pointsPerCluster)
    {
        return;
    }

    PicoClusterPoint* point =
        (PicoClusterPoint*) calloc(1, sizeof(PicoClusterPoint));

    int i = 0;
    while (attributes[i] != NULL)
    {
        if (strcmp((const char*) attributes[i], "id") == 0)
        {
            point->id = strdup((const char*) attributes[i+1]);
        }
        else if (strcmp((const char*) attributes[i], "value") == 0)
        {
            point->magnitude = strdup((const char*) attributes[i+1]);
        }

        i += 2;
    }

    // check that point is parsed completely
    if ((point->id == NULL) || (point->magnitude == NULL))
    {
        log_warning("Cluster point (<m /> tag) is not parsed completely: "
                    "Point ID = \"%s\", point value = \"%s\". "
                    "This point is ignored", point->id, point->magnitude);
        picoClusterPoint_free(point);
        return;
    }

    // find corresponding sensor data
    int id = atoi(point->id);
    if ((id <= 0) || (id > _sensorCount) ||
            (_sensors[id] == NULL))
    {
        log_warning("Cluster point (<m /> tag) has wrong id: \"%s\"."
                    "No sensor with such id found. The point is ignored.",
                    point->id);
        picoClusterPoint_free(point);
        return;
    }

    point->x = _sensors[id]->x;
    point->y = _sensors[id]->y;

    log_debug("New point is parsed: "
              "id=\"%s\"; x=\"%s\"; y=\"%s\"; magnitude=\"%s\".",
              point->id, point->x, point->y, point->magnitude);

    // one more point is successfully parsed, save it
    _currentCluster->points[_currentCluster->pointsCount++] = point;
}

/**
 * Function for parsing XML element start tag, when no points should be
 * parsed inside cluster object.
 * This function is invoked by SAX parser.
 */
static void xmlStartElementNoPoints(void *ctx,
                                    const xmlChar *elementName,
                                    const xmlChar **attributes)
{
    if (strcmp((const char*) elementName, "cluster") == 0)
    {
        PicoCluster* cluster = parseClusterStartElement(attributes);

        if (cluster != NULL)
        {
            // send parsed cluster to the listener
            _clusterListener(cluster);
        }
    }
}

/**
 * Function for parsing XML element start tag, when points should be
 * parsed inside each cluster object.
 * This function is invoked by SAX parser.
 */
static void xmlStartElementWithPoints(void *ctx,
                                      const xmlChar *elementName,
                                      const xmlChar **attributes)
{
    if (strcmp((const char*) elementName, "cluster") == 0)
    {
        _currentCluster = parseClusterStartElement(attributes);
    }
    else if (strcmp((const char*) elementName, "m") == 0)
    {
        if (_currentCluster != NULL)
        {
            parsePointElement(attributes);
        }
    }
}

/**
 * Function for parsing XML element end tag, when points should be
 * parsed inside each cluster object.
 * This function is invoked by SAX parser.
 */
static void xmlEndElementWithPoints(void *ctx,
                                    const xmlChar *name)
{
    if (strcmp((const char*) name, "cluster") == 0)
    {
        if (_currentCluster != NULL)
        {
            // send parsed cluster to the listener
            _clusterListener(_currentCluster);
            _currentCluster = NULL;
        }
    }
}

/**
 * Initializes XML parser.
 * @return @a 0 on success, @a -1 on error.
 */
int static initXmlParser()
{
	static xmlSAXHandler saxHandler;
    // init SAX handler (set callback functions)
    memset( &saxHandler, 0, sizeof(saxHandler) );

    if (_pointsPerCluster == 0)
    {	// simple parsing
        saxHandler.startElement = &xmlStartElementNoPoints;
    }
    else
    {	// the advance one. Parses also points inside a cluster
        saxHandler.startElement = &xmlStartElementWithPoints;
        saxHandler.endElement = &xmlEndElementWithPoints;
    }


    // init parser context
    _xmlParser = xmlCreatePushParserCtxt(&saxHandler, NULL, NULL, 0, NULL);
    if (_xmlParser == NULL)
    {
        log_error("Unable to create XML parser context.");
        return -1;
    }
    return 0;
}

/**
 * Configures Curl handle for reading sensor floor HTTP feed.
 * @return @a 0 on success, @a -1 on error.
 */
int static configureCurlHandle()
{
    // initialize error buffer
    _httpErrorBuffer = (char*) malloc(CURL_ERROR_SIZE);
    if (_httpErrorBuffer == NULL)
    {
        return -1;
    }
    *_httpErrorBuffer = '\0';

    CURLcode code = curl_easy_setopt(_curl, CURLOPT_ERRORBUFFER, _httpErrorBuffer);
    if (code != CURLE_OK)
    {
        log_error("Unable to initialize CURL handle: "
                  "Failed to set error buffer (%d).", code);
        return -1;
    }

    // Set incoming data handler (is called by curl to write down
    // received data)
    code = curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, &inputWriter);
    if (code != CURLE_OK)
    {
        log_error("Unable to initialize CURL handle: "
                  "Failed to set input writer (%d).", code);
        return -1;
    }

    // perform HTTP GET operation
    code = curl_easy_setopt(_curl, CURLOPT_HTTPGET, 1L);
    if (code != CURLE_OK)
    {
        log_error("Unable to initialize HTTP GET request: "
                  "Failed to switch to GET (%d).", code);
        return -1;
    }

    return 0;
}

/**
 * Initializes Curl library.
 * @return @a 0 on success, @a -1 on error.
 */
static int initCurl()
{
    // init Curl environment
    CURLcode code = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (code != CURLE_OK)
    {
        log_error("Unable to initialize CURL: "
                  "curl_global_init() returned %d.", code);
        return -1;
    }

    // Initialize Curl handle
    _curl = curl_easy_init();
    if (_curl == NULL)
    {
        log_error("Unable to initialize CURL handle.");
        curl_easy_cleanup(_curl);
        return -1;
    }

    if (configureCurlHandle(_curl) != 0)
    {
        curl_easy_cleanup(_curl);
        return -1;
    }

    return 0;
}

int pico_initFeedReader(pico_cluster_listener listener, int pointsPerCluster)
{
    _clusterListener = listener;
    _pointsPerCluster = pointsPerCluster;

    if (initXmlParser() != 0)
    {
        return -1;
    }

    picoCluster_initConstants();

    return initCurl();
}

void pico_disposeReader()
{
    free(_httpErrorBuffer);
    curl_easy_cleanup(_curl);
    curl_global_cleanup();
    xmlFreeParserCtxt(_xmlParser);
    if (_currentCluster != NULL)
    {
        picoCluster_free(_currentCluster);
        _currentCluster = NULL;
    }
    if (_sensors != NULL)
    {
        picoSensors_free(_sensors, _sensorCount);
        _sensors = NULL;
        _sensorCount = 0;
    }

    picoCluster_freeConstants();
}

/**
 * Performs HTTP GET request with provided url.
 * @return @a 0 on success, @a -1 on error.
 */
static int performHTTPReadRequest(char* url)
{
    CURLcode code = curl_easy_setopt(_curl, CURLOPT_URL, url);
    if (code != CURLE_OK)
    {
        log_error("Unable to initialize HTTP request: "
                  "Failed to set URL (%d).", code);
        return -1;
    }

    // Retrieve content of the URL
    code = curl_easy_perform(_curl);
    // when the status is "Canceling" we force curl to stop the request
    // and it returns error - ignore it
    if ((code != CURLE_OK) && (_feedReaderCanceled == FALSE))
    {
        log_error("HTTP request to \"%s\" failed (%d): %s.",
                  url, code, _httpErrorBuffer);
        return -1;
    }

    return 0;
}

/**
 * Check that we have sensors array.
 * If not - gets it. If we have it, but do not need - deletes the array.
 */
static int checkPicoSensorData(const char* serverAddress, const char* roomName)
{
    if (_pointsPerCluster == 0)
    {
        if (_sensors != 0)
        {
            picoSensors_free(_sensors, _sensorCount);
            _sensors = NULL;
            _sensorCount = 0;
        }
    }
    else
    {
        if (_sensors == NULL)
        {
            if ((serverAddress == NULL) || (roomName == NULL))
            {
                log_error("Parsing of points inside cluster requires sensor "
                          "information available. "
                          "Please read sensor info first.");
                return -1;
            }

            int error = pico_readSensorsInfoFromUrl(
                            serverAddress, roomName, NULL, NULL);
            if (error != 0)
            {
                log_error("Parsing of points inside cluster requires sensor "
                          "information available. Unable to get this data from "
                          "pico server.");
                return -1;
            }
        }
    }

    return 0;
}

/**
 * Resets the XML parser and prepares it for parsing the feed.
 */
static int resetFeedReader(const char* serverAddress, const char* roomName)
{
	// reset cancellation flag
	_feedReaderCanceled = FALSE;
    // reset XML parser
    if (xmlCtxtResetPush(_xmlParser, NULL, 0, NULL, NULL) != 0)
    {
        log_error("Unable to reset XML parser.");
        return -1;
    }
    if (_currentCluster != NULL)
    {
        picoCluster_free(_currentCluster);
        _currentCluster = NULL;
    }

    checkPicoSensorData(serverAddress, roomName);

    return 0;
}

int pico_readFeedFromFile(const char* fileName)
{
    if (resetFeedReader(NULL, NULL) != 0)
    {
        return -1;
    }

    FILE* feedFile = fopen(fileName, "rb");
    if (feedFile == NULL)
    {
        log_error("Unable to open feed file \"%s\".", fileName);
        return -1;
    }

    char buffer[128];
    int readCount;
    while (!feof(feedFile) && (_feedReaderCanceled == FALSE))
    {
        readCount = fread(buffer, 1, 127, feedFile);
        inputWriter(buffer, 1, readCount, NULL);

        buffer[readCount] = '\0';
        printf("Next chunk of data is read (size %d):\n%s\n\n"
               "Press Enter to continue, or type anything to stop...\n",
               readCount, buffer);

        // wait for user input
        if (getchar() != 10)
        {
            _feedReaderCanceled = TRUE;
        }
    }

    fclose(feedFile);
    log_debug("Reading feed from file is stopped.");

    return 0;
}

int pico_readFeed(const char* serverAddress, const char* roomName)
{
    if (resetFeedReader(serverAddress, roomName) != 0)
    {
        return -1;
    }

    // create the url
    char url[strlen(serverAddress) + strlen(roomName) + 7];
    strcpy(url, serverAddress);
    strcat(url, "/feed/");
    strcat(url, roomName);

    log_debug("Requesting data from %s.", url);
    int error = 0;

    if (_feedReaderCanceled == FALSE)
    {
        error = performHTTPReadRequest(url);
    }

    return error;
}

void pico_stopFeedReader()
{
    // stop the feed reader if it works
    _feedReaderCanceled = TRUE;
}

/**
 * Parses senor id from <sensor> XML tag.
 * @return Positive integer ID, or @a -1 on error.
 */
static int parseSensorId(IXML_Node* sensorXML)
{
    const char* idString =
        ixmlElement_getAttribute(ixmlNode_convertToElement(sensorXML), "id");
    if (idString == NULL)
    {
        char* buffer = ixmlPrintNode(sensorXML);
        log_error("Error during parsing sensor info. <sensor> tags are "
                  "expected to have \'id\' attribute.\n Wrong tag is: %s",
                  buffer);
        free(buffer);
        return -1;
    }
    int id = atoi(idString);
    if (id <= 0)
    {
        char* buffer = ixmlPrintNode(sensorXML);
        log_error("Error during parsing sensor info. <sensor> tags are "
                  "expected to have \'id\' attribute with positive integer "
                  "value.\nWrong tag is: %s", buffer);
        free(buffer);
        return -1;
    }

    return id;
}

/**
 * Finds the maximum sensor id in all <sensor> tags.
 * @return Maximum sensor ID, or @a -1 on error.
 */
static int findMaxSensorId(IXML_NodeList* sensorNodes)
{
    int maxId = -1;
    IXML_NodeList* sensorNode = sensorNodes;
    while(sensorNode != NULL)
    {
        int id = parseSensorId(sensorNode->nodeItem);
        if (id <= 0)
        {
            return -1;
        }
        if (id > maxId)
        {
            maxId = id;
        }

        sensorNode = sensorNode->next;
    }

    return maxId;
}

/**
 * Returns copy of the attribute with provided name, or @a NULL on error.
 */
static char* copySensorAttribute(IXML_Node* sensorNode, const char* attrName)
{
    const char* val = ixmlElement_getAttribute(
                          ixmlNode_convertToElement(sensorNode), attrName);
    if (val == NULL)
    {
        char* buffer = ixmlPrintNode(sensorNode);
        log_error("Unable to parse sensor info. Sensor tag doesn't have \'%s\' attribute: %s", attrName, buffer);
        free(buffer);
        return NULL;
    }

    return strdup(val);
}

/**
 * Parses XML document with sensor data into array of sensor objects.
 */
static PicoSensor** parseSensors(IXML_Document* sensorInfo, int* sensorCount)
{
    IXML_NodeList* sensorNodes =
        ixmlDocument_getElementsByTagName(sensorInfo, "sensor");
    if (sensorNodes == NULL)
    {
        return NULL;
    }

    int maxId = findMaxSensorId(sensorNodes);
    if (maxId < 0)
    {
        free(sensorNodes);
        return NULL;
    }

    *sensorCount = maxId + 1;
    PicoSensor** sensors =
        (PicoSensor**) calloc(*sensorCount, sizeof(PicoSensor*));
    IXML_NodeList* sensorNode = sensorNodes;
    while(sensorNode != NULL)
    {
        int id = parseSensorId(sensorNode->nodeItem);
        PicoSensor* sensor = (PicoSensor*) malloc(sizeof(PicoSensor));
        sensor->id = copySensorAttribute(sensorNode->nodeItem, "id");
        sensor->x = copySensorAttribute(sensorNode->nodeItem, "x");
        sensor->y = copySensorAttribute(sensorNode->nodeItem, "y");
        if ((sensor->x == NULL) || (sensor->y == NULL))
        {
            free(sensorNodes);
            picoSensors_free(sensors, *sensorCount);
            return NULL;
        }

        sensors[id] = sensor;
        sensorNode = sensorNode->next;
    }

    free(sensorNodes);

    // save this data for future reference during cluster parsing
    _sensors = sensors;
    _sensorCount = *sensorCount;

    return sensors;
}

int pico_readSensorInfoFromFile(const char* fileName,
                                const PicoSensor*** sensors,
                                int* sensorCount)
{
    IXML_Document* sensorInfo = ixmlLoadDocument(fileName);
    if (sensorInfo == NULL)
    {
        log_error("Unable to parse XML from file \"%s\".", fileName);
        return -1;
    }

    int count;
    PicoSensor** sensorArray = parseSensors(sensorInfo, &count);
    if (sensorArray == NULL)
    {
        log_error("Unable to parse sensors info.");
        ixmlDocument_free(sensorInfo);
        return -1;
    }

    ixmlDocument_free(sensorInfo);

    *sensors = (const PicoSensor**) sensorArray;
    *sensorCount = count;

    return 0;
}

int pico_readSensorsInfoFromUrl(const char* serverAddress,
                                const char* roomName,
                                const PicoSensor*** sensors,
                                int* sensorCount)
{
    // create the url
    char url[strlen(serverAddress) + strlen(roomName) + 7];
    strcpy(url, serverAddress);
    strcat(url, "/info/");
    strcat(url, roomName);

    // for a normal HTTP request, we use extended curl handle
    CURL_EXT* curlExt;
    IXML_Document* sensorInfo;
    if (curl_ext_create(&curlExt) != 0 )
    {
        return -1;
    }

    if (curl_ext_getDOM(curlExt, url, &sensorInfo) != 0)
    {
        curl_ext_free(curlExt);
        return -1;
    }

    int count;
    PicoSensor** sensorArray = parseSensors(sensorInfo, &count);
    if (sensorArray == NULL)
    {
        log_error("Unable to parse sensors info.");
        ixmlDocument_free(sensorInfo);
        curl_ext_free(curlExt);
        return -1;
    }

    ixmlDocument_free(sensorInfo);
    curl_ext_free(curlExt);

    // return the array
    if ((sensors != NULL) && (sensorCount != NULL))
    {
        *sensors = (const PicoSensor**) sensorArray;
        *sensorCount = count;
    }
    return 0;
}

// This is used only for testing purposes
//int main(int argc, char** argv)
//{
//    int errorCount = 0;
//    int clusterCount = 0;
//
//    void testClusterListener(PicoCluster* cluster)
//    {
//        if (picoCluster_checkNullorEmpty(cluster) ||
//                (cluster->pointsCount != 2))
//        {
//            log_error("Received a bad cluster!");
//            errorCount++;
//        }
//        else
//        {
//            log_debug("Received a good cluster!");
//            clusterCount++;
//        }
//
//        picoCluster_free(cluster);
//    }
//
//    if (pico_initFeedReader(&testClusterListener, 2) != 0)
//    {
//        log_error("!!!Test failed!!!");
//        return -1;
//    }
//
//    _sensorCount = 4;
//    _sensors = (PicoSensor**) calloc(_sensorCount, sizeof(PicoSensor*));
//    _sensors[1] = (PicoSensor*) malloc(sizeof(PicoSensor));
//    _sensors[1]->id = strdup("1");
//    _sensors[1]->x = strdup("1.2");
//    _sensors[1]->y = strdup("1.3");
//    _sensors[3] = (PicoSensor*) malloc(sizeof(PicoSensor));
//    _sensors[3]->id = strdup("3");
//    _sensors[3]->x = strdup("3.2");
//    _sensors[3]->y = strdup("3.3");
//
//    char* testInput =
//        "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?> "
//        "<stream version=\"1.2\"> "
//        "<room id=\"I210\" time=\"117128848694\"> "
//        "<clust";
//    inputWriter(testInput, strlen(testInput), 1, NULL);
//
//    testInput = "er id=\"4\" name=\"\" x=\"1.72\" y=\"0.64\" vx=\"0.14\" vy=\"-0.17\" size=\"0.25\" magnitude=\"211.00\" zones=\"\">\n"
//                "<m mcu=\"1\" sid=\"35\" id=\"1\" value=\"91.00\"/>\n"
//                "<m mcu=\"1\" sid=\"34\" id=\"3\" value=\"120.00\"/>\n"
//                "<m mcu=\"1\" sid=\"34\" id=\"3\" value=\"121.00\"/>\n" //extra point
//                "</cluster>\n";
//    inputWriter(testInput, strlen(testInput), 1, NULL);
//
//    testInput = "<cluster id=\"5\" name=\"\" x=\"1.79\" ";
//    inputWriter(testInput, strlen(testInput), 1, NULL);
//
//    testInput = "y=\"2.21\" vx=\"0.20\" vy=\"0.01\" size=\"0.25\" magnitude=\"83.00\" zones=\"\">\n"
//                "<m mcu=\"1\" sid=\"41\" id=\"2\" value=\"73.00\"/>\n"	//wrong point id
//                "<m mcu=\"1\" sid=\"41\" id=\"25\" value=\"74.00\"/>\n" //wrong point id
//                "<m mcu=\"1\" sid=\"41\" id=\"0\" value=\"75.00\"/>\n"  //wrong point id
//                "<m mcu=\"1\" sid=\"41\" id=\"\" value=\"76.00\"/>\n"   //wrong point id
//                "<m mcu=\"1\" sid=\"41\" value=\"74.00\"/>\n"   		//wrong point id
//                "<m mcu=\"1\" sid=\"40\" id=\"1\" value=\"10.00\"/>\n"
//                "<m mcu=\"1\" sid=\"34\" id=\"3\" value=\"120.00\"/>\n"
//                "</cluster>\n"
//                "</room>\n";
//    inputWriter(testInput, strlen(testInput), 1, NULL);
//
//    // wrong input (no y value)
//    testInput = "<cluster id=\"4\" name=\"\" x=\"1.72\" y=\"\" vx=\"0.14\" vy=\"-0.17\" size=\"0.25\" magnitude=\"211.00\" zones=\"\">\n"
//                "</cluster>\n";
//    inputWriter(testInput, strlen(testInput), 1, NULL);
//
//    // wrong input (no y attribute)
//    testInput = "<cluster id=\"4\" name=\"\" x=\"1.72\" vx=\"0.14\" vy=\"-0.17\" size=\"0.25\" magnitude=\"211.00\" zones=\"\">\n"
//                "</cluster>\n";
//    inputWriter(testInput, strlen(testInput), 1, NULL);
//
//    if (errorCount != 0)
//    {
//        log_error("Listener has received bad clusters!");
//        log_error("!!!Test failed!!!");
//        return -1;
//    }
//
//    if (clusterCount != 2)
//    {
//        log_error("Test data contains two correct clusters data, "
//                  "but parsed %d", clusterCount);
//        log_error("!!!Test failed!!!");
//        return -1;
//    }
//
//    pico_disposeReader();
//
//    log_debug("Test is successful!!!");
//    return 0;
//}
