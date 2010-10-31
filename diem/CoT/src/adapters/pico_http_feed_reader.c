/*
 * pico_http_reader.c

 *  Created on: Oct 30, 2010
 *      Author: andrey
 */

#include <string.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <curl/curl.h>

#include <log_utils.h>
#include "pico_http_feed_reader.h"

static CURL* _curl = NULL;
static xmlSAXHandler _saxHandler;
static xmlParserCtxtPtr _xmlParser = NULL;
static char* _errorBuffer = NULL;
static pico_cluster_listener _clusterListener = NULL;

/**
 * libcurl write callback function. Called each time when
 * something is received to write down the received data.
 */
static size_t inputWriter(char* inputData,
                          size_t size,
                          size_t nmemb,
                          void* arg)
{
    size_t newDataSize = size * nmemb;

    log_debug("Pico HTTP reader: New data block size %d.", newDataSize);

    int error = xmlParseChunk(_xmlParser, inputData, newDataSize, 0);
    if (error != 0)
    {
        char receivedChunk[newDataSize + 1];
        memcpy(receivedChunk, inputData, newDataSize);
        receivedChunk[newDataSize] = '\0';
        log_error("Pico HTTP reader: "
                  "Unable to parse received buffer (error #%d): %s",
                  error, receivedChunk);
        return 0;
    }

    return newDataSize;
}

static picoCluster* picoCluster_init()
{
    picoCluster* cluster = (picoCluster*) calloc(1, sizeof(picoCluster));
    return cluster;
}

static int picoCluster_checkNullorEmpty(picoCluster* cluster)
{
    return (cluster->id == NULL) || (cluster->magnitude == NULL) ||
           (cluster->x == NULL) || (cluster->y == NULL) ||
           (cluster->vx == NULL) || (cluster->vy == NULL) ||
           (*(cluster->id) == '\0') || (*(cluster->magnitude) == '\0') ||
           (*(cluster->x) == '\0') || (*(cluster->y) == '\0') ||
           (*(cluster->vx) == '\0') || (*(cluster->vy) == '\0');
}

static void xmlStartElement (void *ctx,
                             const xmlChar *elementName,
                             const xmlChar **attributes)
{
    if (strcmp((const char*) elementName, "cluster") == 0)
    {
        //parse attributes
        int i = 0;

        picoCluster* cluster = picoCluster_init();

        while (attributes[i] != NULL)
        {
            if (strcmp((const char*) attributes[i], "id") == 0)
            {
                cluster->id = (const char*) attributes[i+1];
            }
            else if (strcmp((const char*) attributes[i], "x") == 0)
            {
                cluster->x = (const char*) attributes[i+1];
            }
            else if (strcmp((const char*) attributes[i], "y") == 0)
            {
                cluster->y = (const char*) attributes[i+1];
            }
            else if (strcmp((const char*) attributes[i], "vx") == 0)
            {
                cluster->vx = (const char*) attributes[i+1];
            }
            else if (strcmp((const char*) attributes[i], "vy") == 0)
            {
                cluster->vy = (const char*) attributes[i+1];
            }
            else if (strcmp((const char*) attributes[i], "magnitude") == 0)
            {
                cluster->magnitude = (const char*) attributes[i+1];
            }

            i += 2;
        }

        if (picoCluster_checkNullorEmpty(cluster))
        {
            log_error("Cluster is not parsed completely: id = %s; x = %s; y = %s; "
                      "vx = %s; vy = %s; magnitude = %s.",
                      cluster->id, cluster->x, cluster->y,
                      cluster->vx, cluster->vy, cluster->magnitude);
            free(cluster);
            return;
        }

        log_debug("New cluster parsed: id = %s; x = %s; y = %s; "
                  "vx = %s; vy = %s; magnitude = %s.",
                  cluster->id, cluster->x, cluster->y,
                  cluster->vx, cluster->vy, cluster->magnitude);

        // send parsed cluster to the listener
        _clusterListener(cluster);
    }
}

int static initXmlParser()
{
    // init SAX handler (set callback functions)
    memset( &_saxHandler, 0, sizeof(_saxHandler) );
    _saxHandler.startElement = &xmlStartElement;

    // init parser context
    _xmlParser = xmlCreatePushParserCtxt(&_saxHandler, NULL, NULL, 0, NULL);
    if (_xmlParser == NULL)
    {
        log_error("Unable to create XML parser context.");
        return -1;
    }
    return 0;
}

int static configureCurlHandle()
{
    // initialize error buffer
    _errorBuffer = (char*) malloc(CURL_ERROR_SIZE);
    if (_errorBuffer == NULL)
    {
        return -1;
    }
    *_errorBuffer = '\0';

    CURLcode code = curl_easy_setopt(_curl, CURLOPT_ERRORBUFFER, _errorBuffer);
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

int pico_initFeedReader(pico_cluster_listener listener)
{
    _clusterListener = listener;

    if (initXmlParser() != 0)
    {
        return -1;
    }

    return initCurl();
}

void pico_disposeReader()
{
    free(_errorBuffer);
    curl_easy_cleanup(_curl);
    curl_global_cleanup();
    xmlFreeParserCtxt(_xmlParser);
}

int pico_readFeed(char* url)
{
    log_debug("Requesting data from %s.", url);

    CURLcode code = curl_easy_setopt(_curl, CURLOPT_URL, url);
    if (code != CURLE_OK)
    {
        log_error("Unable to initialize HTTP request: "
                  "Failed to set URL (%d).", code);
        return -1;
    }

    // Retrieve content of the URL
    code = curl_easy_perform(_curl);
    if (code != CURLE_OK)
    {
        log_error("HTTP request to \"%s\" failed (%d): %s.",
                  url, code, _errorBuffer);
        return -1;
    }

    return 0;
}

// This is used only for testing purposes
int main(int argc, char** argv)
{
    int errorCount = 0;
    int clusterCount = 0;

    void testClusterListener(picoCluster* cluster)
    {
        if (picoCluster_checkNullorEmpty(cluster))
        {
            log_error("Received a bad cluster!");
            errorCount++;
        }
        else
        {
            log_debug("Received a good cluster!");
            clusterCount++;
        }
        free(cluster);
    }

    if (pico_initFeedReader(&testClusterListener) != 0)
    {
        log_error("!!!Test failed!!!");
        return -1;
    }

    char* testInput =
        "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?> "
        "<stream version=\"1.2\"> "
        "<room id=\"I210\" time=\"117128848694\"> "
        "<clust";
    inputWriter(testInput, strlen(testInput), 1, NULL);

    testInput = "er id=\"4\" name=\"\" x=\"1.72\" y=\"0.64\" vx=\"0.14\" vy=\"-0.17\" size=\"0.25\" magnitude=\"211.00\" zones=\"\">\n"
                "<m mcu=\"1\" sid=\"35\" id=\"79\" value=\"91.00\"/>\n"
                "<m mcu=\"1\" sid=\"34\" id=\"78\" value=\"120.00\"/>\n"
                "</cluster>\n";
    inputWriter(testInput, strlen(testInput), 1, NULL);

    testInput = "<cluster id=\"5\" name=\"\" x=\"1.79\" ";
    inputWriter(testInput, strlen(testInput), 1, NULL);

    testInput = "y=\"2.21\" vx=\"0.20\" vy=\"0.01\" size=\"0.25\" magnitude=\"83.00\" zones=\"\">\n"
                "<m mcu=\"1\" sid=\"41\" id=\"65\" value=\"73.00\"/>\n"
                "<m mcu=\"1\" sid=\"40\" id=\"64\" value=\"10.00\"/>\n"
                "</cluster>\n"
                "</room>\n";
    inputWriter(testInput, strlen(testInput), 1, NULL);

    // wrong input (no y value)
    testInput = "<cluster id=\"4\" name=\"\" x=\"1.72\" y=\"\" vx=\"0.14\" vy=\"-0.17\" size=\"0.25\" magnitude=\"211.00\" zones=\"\">\n"
                "</cluster>\n";
    inputWriter(testInput, strlen(testInput), 1, NULL);

    // wrong input (no y attribute)
    testInput = "<cluster id=\"4\" name=\"\" x=\"1.72\" vx=\"0.14\" vy=\"-0.17\" size=\"0.25\" magnitude=\"211.00\" zones=\"\">\n"
                "</cluster>\n";
    inputWriter(testInput, strlen(testInput), 1, NULL);

    if (errorCount != 0)
    {
        log_error("Listener has received bad clusters!");
        log_error("!!!Test failed!!!");
        return -1;
    }

    if (clusterCount != 2)
    {
        log_error("Test data contains two correct clusters data, "
                  "but parsed %d", clusterCount);
        log_error("!!!Test failed!!!");
        return -1;
    }

    pico_disposeReader();

    log_debug("Test is successful!!!");
    return 0;
}
