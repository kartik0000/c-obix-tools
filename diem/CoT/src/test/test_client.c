/** @file
 * @todo add description here
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#include <obix_client.h>
#include <curl_ext.h>
#include <xml_config.h>
#include "test_main.h"
#include "test_client.h"

#define REQUEST_HTTP_GET 0
#define REQUEST_HTTP_PUT 1
#define REQUEST_HTTP_POST 2

static int testCurlExtRequest(const char* testName,
                              int requestType,
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

//    error = obix_dispose();
//    if (error != OBIX_SUCCESS)
//    {
//        printf("obix_dispose() returned %d\n", error);
//        printTestResult(testName, FALSE);
//        return 1;
//    }


    printTestResult(testName, TRUE);
    return 0;
}

int testConnectionAndDevices()
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
    int id1 = obix_registerDevice(0, "<obj href=\"/test1/\" />");
    if (id1 < 0)
    {
        printf("obix_registerDevice(0) returned %d\n", id1);
        printTestResult(testName, FALSE);
        return 1;
    }
    int id2 = obix_registerDevice(0, "<obj href=\"/test2/\" />");
    if (id2 < 0)
    {
        printf("obix_registerDevice(0) returned %d\n", id2);
        printTestResult(testName, FALSE);
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
