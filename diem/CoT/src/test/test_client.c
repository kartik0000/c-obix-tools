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
 * Tests for C oBIX Client library.
 *
 * @author Andrey Litvinov
 */
#include <stdlib.h>
#include <string.h>
#include <obix_client.h>
#include <curl_ext.h>
#include <xml_config.h>
#include "test_main.h"
#include "test_client.h"

/** Different request types used in tests. */
typedef enum
{
    REQUEST_HTTP_GET,
    REQUEST_HTTP_PUT,
    REQUEST_HTTP_POST
} REQUEST_TYPE;

/**
 * Checks how curl_ext library performs HTTP requests.
 *
 * @param requestType Type of request to test.
 * @param handle CURL_EXT handle, which should be used in test.
 * @param uri URI to request.
 * @param exists if @a TRUE, than URI should exist on server, @a FALSE - if the
 * 			provided URI is wrong.
 */
static int testCurlExtRequest(const char* testName,
                              REQUEST_TYPE requestType,
                              CURL_EXT* handle,
                              const char* uri,
                              BOOL exists)
{
    char* requestName;
    int error;

    switch(requestType)
    {
    default:
    case REQUEST_HTTP_GET:
        {
            requestName = "GET";
            error = curl_ext_get(handle, uri);
            break;
        }
    case REQUEST_HTTP_PUT:
        {
            requestName = "PUT";
            error = curl_ext_put(handle, uri);
            break;
        }
    case REQUEST_HTTP_POST:
        {
            requestName = "POST";
            error = curl_ext_post(handle, uri);
            break;
        }
    }

    if (error != 0)
    {
        if (exists)
        {
            printf("HTTP %s request failed.\n", requestName);
            printTestResult(testName, FALSE);
            return 1;
        }
        else
        {
            printf("HTTP %s did not found uri \"%s\". "
                   "Returned error: %s.\n",
                   requestName, uri, handle->errorBuffer);
            printTestResult(testName, TRUE);
            return 0;
        }
    }

    printf("HTTP %s %s response:\n%s\n",
           requestName, uri, handle->inputBuffer);
    if (strstr(handle->inputBuffer, "html") != NULL)
    {
        printf("Server replied with HTTP error message!\n");
        printTestResult(testName, FALSE);
        return 1;
    }

    if (exists)
    {
        printTestResult(testName, TRUE);
        return 0;
    }
    else
    {
        printf("HTTP %s should fail but it doesn't.\n", requestName);
        printTestResult(testName, FALSE);
        return 1;
    }
}

/**
 * Performs testing of curl_ext module.
 * Creates new CURL_EXT handler and tried to execute several requests using it.
 */
static int testCurlExt()
{
    const char* testName = "curl_ext* test";
    // initialize communication
    int error = curl_ext_init(0);
    if (error != 0)
    {
        printf("Unable to initialize curl: "
               "curl_ext_init() returned %d.\n", error);
        printTestResult(testName, FALSE);
        return 1;
    }

    // create new handle
    CURL_EXT* curl;
    error = curl_ext_create(&curl);
    if (error != 0)
    {
        printf("Unable to initialize curl: "
               "curl_ext_create() returned %d.\n", error);
        printTestResult(testName, FALSE);
        return 1;
    }

    // make curl library write debug info
    //    curl_easy_setopt(curl->curl, CURLOPT_VERBOSE, 1L);
    //    curl_easy_setopt(curl->curl, CURLOPT_STDERR, stdout);

    // try to get a working link
    error += testCurlExtRequest("GET correct page",
                                REQUEST_HTTP_GET,
                                curl,
                                "http://localhost/obix",
                                TRUE);
    error += testCurlExtRequest("GET wrong page",
                                REQUEST_HTTP_GET,
                                curl,
                                "http://server12345.com",
                                FALSE);
    // try to put some data to the server
    curl->outputBuffer = "<bool "
                         "href=\"http://localhost/obix/devices/kitchen/main-lamp\" "
                         "val=\"true\"/>";
    error += testCurlExtRequest("PUT to oBIX server",
                                REQUEST_HTTP_PUT,
                                curl,
                                "http://localhost/obix/devices/kitchen/main-lamp",
                                TRUE);
    //    error += testCurlExtRequest("PUT to wrong address",
    //                                REQUEST_HTTP_PUT,
    //                                curl,
    //                                "http://server123.com/123",
    //                                FALSE);
    // try now to call signUp operation on the server
    curl->outputBuffer = "<obj name=\"TestLamp\" href=\"/obix/bedroom/lamp/\">"
                         "<bool "
                         "href=\"main-lamp\" "
                         "val=\"true\"/>"
                         "</obj>";
    error += testCurlExtRequest("POST to oBIX server",
                                REQUEST_HTTP_POST,
                                curl,
                                "http://localhost/obix/signUp/",
                                TRUE);
    // check that the signedUp object exists
    error += testCurlExtRequest("Check previous POST result",
                                REQUEST_HTTP_GET,
                                curl,
                                "http://localhost/obix/bedroom/lamp/",
                                TRUE);
    if (strstr(curl->inputBuffer, "<err") != NULL)
    {
        printf("Object was not actually signed up!\n");
        error++;
    }

    if (error != 0)
    {
        printTestResult(testName, FALSE);
        return error;
    }



    printTestResult(testName, TRUE);
    return 0;
}

/**
 * Tries to load client configuration file.
 */
static int testObixLoadConfigFile()
{
    const char* testName = "obix_loadConfigFile test";
    //    setResourceDir("res/");
    int error = obix_loadConfigFile("test_obix_client_config.xml");
    if (error != OBIX_SUCCESS)
    {
        printf("obix_loadConfigFile() returned %d\n", error);
        printTestResult(testName, FALSE);
        return 1;
    }

    printTestResult(testName, TRUE);
    return 0;
}

/**
 * Helper function which tests Batch utilities.
 */
static int testBatch()
{
    oBIX_Batch* batch = obix_batch_create(0);
    if (batch == NULL)
    {
        printf("obix_batch_create(0) returned NULL.\n");
        return 1;
    }

    int error = obix_batch_read(batch, 0, "/obix");
    if (error < 0)
    {
        printf("obix_batch_read(batch, 0, \"/obix\") returned %d.\n", error);
        return 1;
    }

    error = obix_batch_readValue(batch, 1, NULL);
    if (error < 0)
    {
        printf("obix_batch_readValue(batch, 1, NULL) returned %d.\n", error);
        return 1;
    }

    error = obix_batch_writeValue(batch, 2, "int", "1", OBIX_T_INT);
    if (error < 0)
    {
        printf("obix_batch_writeValue(batch, 2, \"int\", \"1\", OBIX_T_INT) "
               "returned %d.\n", error);
        return 1;
    }

    error = obix_batch_send(batch);
    if (error != OBIX_SUCCESS)
    {
        printf("obix_batch_send(batch) returned %d.\n", error);
        return 1;
    }

    int i;
    for (i = 1; i < 4; i++)
    {
        const oBIX_BatchResult* result = obix_batch_getResult(batch, i);
        if (result == NULL)
        {
            printf("obix_batch_getResult(batch, %d) returned NULL.\n", i);
            return 1;
        }

        char* object = NULL;
        if (result->obj != NULL)
        {
            object = ixmlPrintNode(ixmlElement_getNode(result->obj));
        }
        printf("result #%d:\n\tstatus %d;\n\tvalue %s;\n\tobj:\n%s\n",
               i, result->status, result->value, object);
        if (object != NULL)
        {
            free(object);
        }
    }

    obix_batch_free(batch);
    return 0;
}

/**
 * Tests C oBIX Client API.
 * Opens connection with server, registers test device data there and performs
 * other Client API operations.
 */
static int testConnectionAndDevices()
{
    const char* testName = "test obix_client common utils";

    int error = testObixLoadConfigFile();
    if (error != 0)
    {
        printTestResult(testName, FALSE);
        return 1;
    }

    error = obix_openConnection(0);
    if (error != OBIX_SUCCESS)
    {
        printf("obix_openConnection(0) returned %d\n", error);
        printTestResult(testName, FALSE);
        return 1;
    }
    error = obix_openConnection(1);
    if (error != OBIX_SUCCESS)
    {
        printf("obix_openConnection(1) returned %d\n", error);
        printTestResult(testName, FALSE);
        return 1;
    }
    int id1 = obix_registerDevice(0, "<bool href=\"/test1/\" val=\"true\" />");
    if (id1 < 0)
    {
        printf("obix_registerDevice(0) returned %d\n", id1);
        printTestResult(testName, FALSE);
        return 1;
    }
    int id2 = obix_registerDevice(0,
                                  "<obj href=\"/test2/\" >\r\n"
                                  " <int href=\"int\" writable=\"true\" "
                                  "val=\"0\" />\r\n"
                                  "</obj>");
    if (id2 < 0)
    {
        printf("obix_registerDevice(0) returned %d\n", id2);
        printTestResult(testName, FALSE);
        return 1;
    }

    // test obix batch
    error = testBatch();
    if (error != 0)
    {
        printTestResult("test obix client batch utils", FALSE);
        return 1;
    }

    error = obix_unregisterDevice(0, id1);
    if (error != OBIX_SUCCESS)
    {
        printf("obix_unregisterDevice(0) returned %d\n", error);
        printTestResult(testName, FALSE);
        return 1;
    }
    error = obix_registerDevice(0, "<obj href=\"/test3/\" />");
    if (error != id1)
    {
        printf("obix_registerDevice(0) returned %d\n", error);
        printTestResult(testName, FALSE);
        return 1;
    }

    error = obix_dispose();
    if (error != OBIX_SUCCESS)
    {
        printf("obix_dispose() returned %d\n", error);
        printTestResult(testName, FALSE);
        return 1;
    }

    printTestResult(testName, TRUE);
    return 0;
}

int test_client()
{
    int result = 0;

    // this is already tested in testConnectionAndDevices
    //    result += testObixLoadConfigFile();

    result += testConnectionAndDevices();

    result += testCurlExt();

    return result;
}

void test_client_byHands()
{
    testConnectionAndDevices();
}
