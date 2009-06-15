/** @file
 * That's a temporary file to test various pieces of functionality.
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <obix_utils.h>
#include <xml_storage.h>
#include <lwl_ext.h>
#include <xml_config.h>
#include <ixml_ext.h>
#include <server.h>
#include <watch.h>
#include <obix_fcgi.h>
#include "test_main.h"

BOOL _responseIsSent = FALSE;
Response* _lastResponse;
pthread_mutex_t _responseMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t _responseReceived = PTHREAD_COND_INITIALIZER;

void dummyResponseListener(Response* response)
{
	pthread_mutex_lock(&_responseMutex);
    _responseIsSent = TRUE;
    _lastResponse = response;
    pthread_cond_signal(&_responseReceived);
    pthread_mutex_unlock(&_responseMutex);
}

static BOOL isResponseSent()
{
    // reset flag
    if (_responseIsSent)
    {
        _responseIsSent = FALSE;
        return TRUE;
    }

    printf("No response is sent...\n");
    return FALSE;
}

static Response* waitForResponse()
{
	pthread_mutex_lock(&_responseMutex);
	struct timespec time;
	clock_gettime(CLOCK_REALTIME, &time);
	time.tv_sec++;
	pthread_cond_timedwait(&_responseReceived, &_responseMutex, &time);
	Response* response = _lastResponse;
	pthread_mutex_unlock(&_responseMutex);
	return response;
}

int testSearch(const char* testName, const char* href, const char* checkStr, BOOL exists)
{
    char* node = xmldb_get(href);
    printf("\nnode uri=%s: %s\n", href, node);
    BOOL success = FALSE;

    if (node != NULL)
    {
        if (checkStr != NULL)
        {
            if (strstr(node, checkStr) != NULL)
            {
                success = TRUE;
            }
        }
        else
        {
            success = TRUE;
        }

        free(node);
    }

    if (success == exists)
    {
        printTestResult(testName, TRUE);
        return 0;
    }
    else
    {
        printf("Search test failed. Current db contents:\n");
        //        xmldb_printDump();
        printTestResult(testName, FALSE);
        return 1;
    }
}

int testWriteToDatabase(const char* testName,
                        BOOL writeNew,
                        const char* newData,
                        const char* href,
                        const char* checkString,
                        BOOL shouldPass)
{
    // add data to the database
    int error;
    if (writeNew)
    {
        error = xmldb_put(newData);
    }
    else
    {
        error = xmldb_update(newData, href, NULL);
    }
    if (error == 1)
    {
        // special case: value which was written is the same with the value
        // already in the database
        printf("New value is the same as the old one.\n");
        printTestResult(testName, shouldPass);
        return shouldPass ? 0 : 1;
    }

    if (error && shouldPass)
    {
        printf("Error occurred during data saving (%d)\n", error);
        printTestResult(testName, FALSE);
        return 1;
    }

    // check that the data is really added
    char* node = xmldb_get(href);
    if (node == NULL)
    {
        printf("no node returned\n");
        if (shouldPass)
        {
            printTestResult(testName, FALSE);
            return 1;
        }
        else
        {
            printTestResult(testName, TRUE);
            return 0;
        }
    }

    // check that found node has the string we wrote to it
    if (strstr(node, checkString) == NULL)
    {
        printf("no check string found\n");
        free(node);
        if (shouldPass)
        {
            printTestResult(testName, FALSE);
            return 1;
        }
        else
        {
            printTestResult(testName, TRUE);
            return 0;
        }
    }

    free(node);
    if (shouldPass)
    {
        printTestResult(testName, TRUE);
        return 0;
    }
    else
    {
        printf("Test should fail but is passed.\n");
        printTestResult(testName, FALSE);
        return 1;
    }

}

int testDelete(const char* testName, const char* href, BOOL exists)
{
    int error = xmldb_delete(href);

    if (error)
    {
        if (exists)
        {
            printTestResult(testName, FALSE);
            return 1;
        }
        else
        {
            printTestResult(testName, TRUE);
            return 0;
        }
    }
    else
    {
        if (!exists)
        {
            //we should have error!
            printf("No error returned when deleting non-existent node.\n");
            printTestResult(testName, FALSE);
            return 1;
        }
    }


    // check that we deleted the node
    char* node = xmldb_get(href);
    if (node != NULL)
    {
        free(node);
        printf("Node still exists in the storage.");
        printTestResult(testName, FALSE);
        return 1;
    }
    printTestResult(testName, TRUE);
    return 0;
}

int testGenerateResponse(const char* testName, const char* uri, const char* newUrl)
{
    IXML_Element* oBIXdoc = xmldb_getDOM(uri);
    if (oBIXdoc == NULL)
    {
        printf("Uri \"%s\" is not found in storage.\n", uri);
        xmldb_printDump();
        printTestResult(testName, FALSE);
        return 1;
    }

    Response* response = obixResponse_create((Request*)1);
    obix_server_generateResponse(response, oBIXdoc, newUrl, TRUE, FALSE, 0, TRUE, FALSE);

    if ((response == NULL) || (response->body == NULL))
    {
        printf("oBIX normalization without saving is failed.\n");
        printTestResult(testName, FALSE);
        return 1;
    }
    printf("normalized object = %s", response->body);
    obixResponse_free(response);

    if (testSearch("check object after normalization", newUrl, NULL, FALSE))
    {
        printf("Changes in object are saved after normalization (but they shouldn't).\n");
        printTestResult(testName, FALSE);
        return 1;
    }

    response = obixResponse_create((Request*)1);
    obix_server_generateResponse(response, oBIXdoc, newUrl, TRUE, FALSE, 0, FALSE, TRUE);
    if ((response == NULL) || (response->body == NULL))
    {
        printf("oBIX normalization with saving is failed.\n");
        printTestResult(testName, FALSE);
        return 1;
    }

    printf("normalized object = %s", response->body);
    obixResponse_free(response);

    if (testSearch("check object after normalization", newUrl, NULL, TRUE) ||
            testSearch("check object for \'meta\' tags after normalization",
                       newUrl, OBIX_META, FALSE))
    {
        printf("Changes in object are not saved after normalization (but they should).\n");
        printTestResult(testName, FALSE);
        return 1;
    }

    printTestResult(testName, TRUE);
    return 0;
}

//int testCompareUri(const char* testName, const char* uri1, const char* uri2, BOOL result, int slashFlag)
//{
//    if (xmldb_compareUri(uri1, uri2) != result)
//    {
//        printf("Compare result of \"%s\" and \"%s\" is not %s.\n", uri1, uri2,
//               result ? XML_TRUE : XML_FALSE);
//        printTestResult(testName, FALSE);
//        return 1;
//    }
//
//    int flag = xmldb_getLastUriCompSlashFlag();
//    if (flag != slashFlag)
//    {
//        printf("Slash flag after comparison of \"%s\" and \"%s\" is %d but should be %d.\n",
//               uri1, uri2, flag, slashFlag);
//        printTestResult(testName, FALSE);
//        return 1;
//    }
//
//    printTestResult(testName, TRUE);
//    return 0;
//
//}

Response* testPostHandler(const char* uri, IXML_Document* input)
{
    printf("Test handler for \"%s\"\n", uri);
    return NULL;
}

//int testGetHandler(const char* testName, const char* uri, BOOL exists)
//{
//    obix_server_postHandler handler = obix_server_getPostHandlerByUri(uri);
//    if ((handler != NULL) && !exists)
//    {
//        printf("No handlers should be returned for this request (\"%s\").\n", uri);
//        printTestResult(testName, FALSE);
//        return 1;
//    }
//
//    if ((handler == NULL) && exists)
//    {
//        printf("No handlers returned for this request (\"%s\"), but there should be something.\n", uri);
//        printTestResult(testName, FALSE);
//        return 1;
//    }
//
//    if (handler != NULL)
//    {
//        printf("Handler received, trying to execute...\n");
//        (*handler)(NULL, uri);
//    }
//
//    return 0;
//}

//int testServerPostHandlers()
//{
//    char* testName = "obix_server_*PostHandler";
//
//    //try to get and remove from empty list
//    if (testGetHandler(testName, "/obix/operation1", FALSE) != 0)
//    {
//        return 1;
//    }
//    int error = obix_server_deletePostHandler("/obix/operation1");
//    if (error = 0)
//    {
//        printf("Deleting non-existing node some why was successful.\n");
//        printTestResult(testName, FALSE);
//        return 1;
//    }
//
//    // put some data
//    error  = obix_server_addPostHandler("/obix/op1", &testPostHandler);
//    error += obix_server_addPostHandler("/obix/op2", &testPostHandler);
//    error += obix_server_addPostHandler("/obix/op3", &testPostHandler);
//    error += obix_server_addPostHandler("/obix/op4", &testPostHandler);
//    error += obix_server_addPostHandler("/obix/op5", &testPostHandler);
//
//    if (error != 0)
//    {
//        printf("Unable to add POST handlers.\n");
//        printTestResult(testName, FALSE);
//        return 1;
//    }
//
//    // search for handlers
//    if (testGetHandler(testName, "/obix/op1", TRUE) != 0)
//    {
//        return 1;
//    }
//    if (testGetHandler(testName, "/obix/op5", TRUE) != 0)
//    {
//        return 1;
//    }
//
//    // delete and then search for it
//    error = obix_server_deletePostHandler("/obix/op3");
//    error = obix_server_deletePostHandler("/obix/op1");
//    error = obix_server_deletePostHandler("/obix/op5");
//    if (error != 0)
//    {
//        printf("Unable to delete POST handlers.\n");
//        printTestResult(testName, FALSE);
//        return 1;
//    }
//
//    error = testGetHandler(testName, "/obix/op1", FALSE);
//    error += testGetHandler(testName, "/obix/op2", TRUE);
//    error += testGetHandler(testName, "/obix/op3", FALSE);
//    error += testGetHandler(testName, "/obix/op4", TRUE);
//    error += testGetHandler(testName, "/obix/op5", FALSE);
//
//    if (error != 0)
//    {
//        return error;
//    }
//
//    printTestResult(testName, TRUE);
//    return 0;
//}

// TODO in order to test dump_environment, obix_fcgi should be stated as on of
// the sources for test application, but it already contains main function
//int testDumpEnvironment()
//{
//    char* testName = "obix_fcgi_dumpEnvironment";
//
//    obix_server_setResponseListener(&dummyResponseListener);
//    Response* response = obixResponse_create(NULL);
//
//    obix_fcgi_dumpEnvironment(response);
//
//    if ((response == NULL) || (response->body == NULL) || !isResponseSent())
//    {
//        printf("Dump environment returned NULL.\n");
//        printTestResult(testName, FALSE);
//        return 1;
//    }
//
//    printf("Dump environment returned the following answer:\n");
//    Response* part = response;
//    int i;
//    for (i = 1; part != NULL; i++)
//    {
//        printf("Part #%d:\n%s\n", i, part->body);
//        part = part->next;
//    }
//    obixResponse_free(response);
//
//    printTestResult(testName, TRUE);
//    return 0;
//}

int obix_client_testListener(int connectionId,
                             int deviceId,
                             const char* paramUri,
                             const char* newValue)
{
    printf("oBIX listener received new event: "
           "connectionId %d; deviceId %d; paramUri %s; newValue %s\n",
           connectionId, deviceId, paramUri, newValue);
    return 0;
}

//int testObixClientLib()
//{
//    // establish connection
//    int connection1 = obix_initConnection("server 1");
//    if (connection1 < 0)
//    {
//        printTestResult("obix client: obix_initConnection", FALSE);
//        return 1;
//    }
//    printTestResult("obix client: obix_initConnection", TRUE);
//
//    // register some devices
//    int device1 = obix_registerDevice(connection1, "test device 1");
//    int device2 = obix_registerDevice(connection1, "test device 2");
//    if ((device1 < 0) || (device2 < 0))
//    {
//        printTestResult("obix client: obix_registerDevice", FALSE);
//        return 1;
//    }
//    printTestResult("obix client: obix_registerDevice", TRUE);
//
//    //register listeners
//    int error = obix_registerListener(connection1, device1, "param1", &obix_client_testListener);
//    error += obix_registerListener(connection1, device1, "param2", &obix_client_testListener);
//    error += obix_registerListener(connection1, device1, "param3", &obix_client_testListener);
//    error += obix_registerListener(connection1, device2, "param3", &obix_client_testListener);
//    if (error != 0)
//    {
//        printTestResult("obix client: obix_registerListener", FALSE);
//        return 1;
//    }
//    printTestResult("obix client: obix_registerListener", TRUE);
//
//    // create second connection
//    int connection2 = obix_initConnection("server 2");
//    if (connection2 < 0)
//    {
//        printTestResult("obix client: obix_initConnection 2", FALSE);
//        return 1;
//    }
//    printTestResult("obix client: obix_initConnection 2", TRUE);
//
//    // write device updates
//    error += obix_writeValue(connection1, device1, "param1\\", "some test value");
//    error += obix_writeValue(connection1, device2, "param2\\", "some test value");
//    if (error != 0)
//    {
//        printTestResult("obix client: obix_writeValue", FALSE);
//        return 1;
//    }
//    printTestResult("obix client: obix_writeValue", TRUE);
//
//    // remove some devices and listeners and write update again
//    error += obix_unregisterListener(connection1, device1, "param1");
//    error += obix_unregisterListener(connection1, device1, "param3");
//    error += obix_unregisterDevice(connection1, device1);
//    error += obix_writeValue(connection1, device2, "param2\\", "some test value");
//    if (error != 0)
//    {
//        printTestResult("obix client: obix_unregister*", FALSE);
//        return 1;
//    }
//    printTestResult("obix client: obix_unregister*", TRUE);
//
//    // try to write to deleted device
//    if (obix_writeValue(connection1, device1, "param1\\", "some test value") == 0)
//    {
//        printTestResult("obix client: obix_writeValue to removed device", FALSE);
//        return 1;
//    }
//    printTestResult("obix client: obix_writeValue to removed device", TRUE);
//
//    // close connection and try to write to it
//    if (obix_closeConnection(connection1) != 0)
//    {
//        printTestResult("obix client: obix_closeConnection", FALSE);
//        return 1;
//    }
//    printTestResult("obix client: obix_closeConnection", TRUE);
//    if (obix_writeValue(connection1, device2, "param2\\", "some test value") == 0)
//    {
//        printTestResult("obix client: obix_writeValue to closed connection", FALSE);
//        return 1;
//    }
//    printTestResult("obix client: obix_writeValue to closed connection", TRUE);
//
//    // try to write and register listener to external server data
//    error += obix_registerListener(connection2, 0, "/obix/device/lampSwitch", &obix_client_testListener);
//    error += obix_writeValue(connection2, 0, "/obix/device/lampSwitch", "off");
//    if (error != 0)
//    {
//        printTestResult("obix client: write to external server data", FALSE);
//        return 1;
//    }
//    printTestResult("obix client: write to external server data", TRUE);
//
//    // close remaining connection
//    if (obix_closeConnection(connection2) != 0)
//    {
//        printTestResult("obix client: obix_closeConnection 2", FALSE);
//        return 1;
//    }
//    printTestResult("obix client: obix_closeConnection 2", TRUE);
//
//    printTestResult("obix client full test", TRUE);
//    return 0;
//}

void printResponse(Response* response)
{
    printf("Received response:\n");
    while (response != NULL)
    {
        printf("%s", response->body);
        response = response->next;
    }

    printf("\n");
}

int checkResponse(Response* response, BOOL containsError)
{
    if ((response == NULL) || (response->body == NULL))
    {
        printf("Empty response is generated.\n");
        return 1;
    }

    if (!isResponseSent())
    {
        printf("No response is sent.\n");
        return 1;
    }

    if (response->request == NULL)
    {
        printf("No request object found in the response.\n");
        return 1;
    }

    printResponse(response);
    // try to find error object in all response parts
    char* error = NULL;
    while ((response != NULL) &&
            (response->body != NULL) &&
            ((error = strstr(response->body, "<err")) == NULL))
    {
        response = response->next;
    }

    if ((error == NULL) && containsError)
    {
        printf("Response contains no error but it should.\n");
        return 1;
    }
    else if ((error != NULL) && !containsError)
    {
        printf("Response contains error but it shouldn't.\n");
        return 1;
    }

    return 0;
}

int findInResponse(Response* response, const char* checkString, BOOL exists)
{
    while(response != NULL)
    {
        if ((response->body != NULL) &&
                (strstr(response->body, checkString) != NULL))
        {
            // we found required substring
            if (exists)
            {   // string supposed to be found
                printf("Test string \"%s\" is found in response, and it is good.\n", checkString);
                return 0;
            }
            else
            {	// string shouldn't exist
                printf("Test string \"%s\" is found in response, but it shouldn\'t.\n",
                       checkString);
                return 1;
            }
        }
        response = response->next;
    }

    // nothing was found
    if (exists)
    {	// string supposed to be found
        printf("Test string \"%s\" is not found in response, but it should.\n", checkString);
        return 1;
    }
    else
    {	// string is not found and that is great :)
        printf("Test string \"%s\" is not found in response, and it is good.\n", checkString);
        return 0;
    }
}

int testWatchPollChanges(const char* testName,
                         const char* uri,
                         char* checkStrings[],
                         int checkSize,
                         BOOL exists,
                         BOOL waitResponse)
{
    obix_server_setResponseListener(&dummyResponseListener);
    Response* response = obixResponse_create((Request*)1);
    obix_server_handlePOST(response, uri, NULL);
    if (waitResponse)
    {
    	response = waitForResponse();
    }
    if (checkResponse(response, FALSE) != 0)
    {
        printf("Poll Changes command failed.\n");
        printTestResult(testName, FALSE);
        return 1;
    }
    int error = 0;
    int i;
    for (i = 0; i < checkSize; i++)
    {
        error += findInResponse(response, checkStrings[i], exists);
    }
    if (error != 0)
    {
        printTestResult(testName, FALSE);
        return 1;
    }
    obixResponse_free(response);

    printTestResult(testName, TRUE);
    return 0;
}

int testWatchRemove()
{
    const char* testName = "Watch.remove test";
    obix_server_setResponseListener(&dummyResponseListener);
    Response* response = obixResponse_create((Request*)1);
    obix_server_handlePOST(
        response,
        "/obix/watchService/watch1/remove",
        "<obj is=\"obix:WatchIn\">\r\n"
        " <list name=\"hrefs\" of=\"obix:WatchInItem\">\r\n"
        "  <uri is=\"obix:WatchInItem\" val=\"/obix/wrongURI\"/>\r\n"
        "  <uri is=\"obix:WatchInItem\" val=\"/obix/kitchen/parent/\"/>\r\n"
        " </list>\r\n"
        "</obj>");
    // we should receive empty object
    if (checkResponse(response, FALSE) != 0)
    {
        printTestResult(testName, FALSE);
        return 1;
    }
    obixResponse_free(response);

    // now try to poll refresh and check that we do not receive object which
    // we've just removed from the watch list
    response = obixResponse_create((Request*)1);
    obix_server_handlePOST(response,
                           "/obix/watchService/watch1/pollRefresh",
                           NULL);
    if (checkResponse(response, FALSE) != 0)
    {
        printTestResult(testName, FALSE);
        return 1;
    }
    if (findInResponse(response, "testWatch3", FALSE) != 0)
    {
        printf("pollRefresh still returnes removed object.\n");
        printTestResult(testName, FALSE);
        return 1;
    }
    obixResponse_free(response);
    // check that meta info was also removed
    int error = testSearch("oBIX Watch: check removed meta",
                           "/obix/kitchen/parent/",
                           "<wi-1",
                           FALSE);
    if (error != 0)
    {
        printf("Meta info was not removed.\n");
        printTestResult(testName, FALSE);
        return 1;
    }

    printTestResult(testName, TRUE);
    return 0;
}

int testPutHandler(const char* testName, const char* uri, const char* data)
{
    Response* response = obixResponse_create((Request*)1);
    obix_server_handlePUT(response, uri, data);
    if (checkResponse(response, FALSE) != 0)
    {
        printTestResult(testName, FALSE);
        return 1;
    }
    obixResponse_free(response);
    printTestResult(testName, TRUE);
    return 0;
}

int testWatch()
{
    const char* testName = "oBIX Watch test";
    // create new Watch object
    obix_server_setResponseListener(&dummyResponseListener);
    Response* response = obixResponse_create((Request*)1);
    obix_server_handlePOST(response, "/obix/watchService/make", NULL);
    if (checkResponse(response, FALSE) != 0)
    {
        printTestResult(testName, FALSE);
        return 1;
    }
    obixResponse_free(response);

    // consider that we received a watch with name watch1
    // modify lease time
    int error = testPutHandler(
                    "Changing Watch lease time",
                    "/obix/watchService/watch1/lease",
                    "<reltime href=\"/obix/watchService/watch1/lease\" val=\"PT5M\"/>");
    if (error != 0)
    {
        printTestResult(testName, FALSE);
        return error;
    }

    // add object to the watch
    // + one with wrong trailing slash, one <op/> object,
    // one comment and one wrong object.
    // TODO check duplicate request for same object
    response = obixResponse_create((Request*)1);
    obix_server_handlePOST(
        response,
        "/obix/watchService/watch1/add",
        "<obj is=\"obix:WatchIn\">\r\n"
        " <list name=\"hrefs\" of=\"obix:WatchInItem\">\r\n"
        "  <!-- Comment goes here -->\r\n"
        "  and not only the comment\r\n"
        "  <obj name=\"Wrong Object\"/>\r\n"
        "  <uri is=\"obix:WatchInItem\" val=\"/obix/watchService/make\"/>\r\n"
        "  <uri is=\"obix:WatchInItem\" val=\"/obix/kitchen/temperature/\"/>\r\n"
        "  <uri is=\"obix:WatchInItem\" val=\"/obix/kitchen/temperature2\"/>\r\n"
        "  <uri is=\"obix:WatchInItem\" val=\"/obix/kitchen/parent/\"/>\r\n"
        " </list>\r\n"
        "</obj>");
    if (checkResponse(response, TRUE) != 0)
    {
        printTestResult(testName, FALSE);
        return 1;
    }
    // TODO check that response has subscribed object
    // and error messages for wrong objects
    // we should 2 <err/> objects with links watchService/make and temperature2
    // and object testWatch1. We shouldn't have "testWatch2" and "Make new watch"
    obixResponse_free(response);

    //check that corresponding meta tags are added to the storage
    error = testSearch("oBIX Watch: check created meta",
                       "/obix/kitchen/temperature/",
                       "<wi-1 val=\"n\"",
                       TRUE);
    error += testSearch("oBIX Watch: check created parent\'s meta",
                        "/obix/kitchen/parent/",
                        "<wi-1 val=\"n\"",
                        TRUE);
    if (error != 0)
    {
        printf("Meta information was not created.\n");
        printTestResult(testName, FALSE);
        return error;
    }

    // let's change value of monitored objects and check for update
    error = testPutHandler(
                testName,
                "/obix/kitchen/temperature/",
                "<int href=\"/obix/kitchen/temperature/\" val=\"newValue\"/>");
    error += testPutHandler(
                 testName,
                 "/obix/kitchen/parent/child/",
                 "<int href=\"/obix/kitchen/parent/child/\" val=\"newValue\"/>");
    if (error != 0)
    {
        return 1;
    }

    // check that updated objects do have meta tags
    error = testSearch("oBIX Watch: check updated meta",
                       "/obix/kitchen/temperature/",
                       "<wi-1 val=\"y\"",
                       TRUE);
    error += testSearch("oBIX Watch: check updated parent\'s meta",
                        "/obix/kitchen/parent/",
                        "<wi-1 val=\"y\"",
                        TRUE);

    if (error != 0)
    {
        printf("Meta information was not updated.\n");
        printTestResult(testName, FALSE);
        return error;
    }

    // now check that poll changes will return updated objects
    char* checkStrings[] = {"testWatch1", "testWatch3"};
    error = testWatchPollChanges("test Watch.pollChanges with some changes happened",
                                 "/obix/watchService/watch1/pollChanges",
                                 checkStrings, 2,
                                 TRUE,
                                 FALSE);
    error += testWatchPollChanges("test Watch.pollChanges with no changes",
                                  "/obix/watchService/watch1/pollChanges",
                                  checkStrings, 2,
                                  FALSE,
                                  FALSE);

    if (error != 0)
    {
        printf("Poll changes test(s) failed.\n");
        printTestResult(testName, FALSE);
        return 1;
    }
    // check long poll handling
    error = testPutHandler(
                "Changing Watch poll interval",
                "/obix/watchService/watch1/pollWaitInterval/max",
                "<reltime href=\"/obix/watchService/watch1/pollWaitInterval/max\" val=\"PT0.010S\"/>");
    if (error != 0)
    {
        printTestResult(testName, FALSE);
        return error;
    }
    error = testWatchPollChanges("test Watch.pollChanges with delay",
                                 "/obix/watchService/watch1/pollChanges",
                                 checkStrings, 2,
                                 FALSE,
                                 TRUE);

    // let's try to write once again to the same object, but write the same
    // value as it already has
    response = obixResponse_create((Request*)1);
    obix_server_handlePUT(
        response,
        "/obix/kitchen/temperature/",
        "<int href=\"/obix/kitchen/temperature/\" val=\"newValue\"/>");
    if (checkResponse(response, FALSE) != 0)
    {
        printTestResult(testName, FALSE);
        return 1;
    }
    obixResponse_free(response);
    // updated object should not have updated meta tags (because actual value
    // did not change)
    error = testSearch("oBIX Watch: check that meta is not updated",
                       "/obix/kitchen/temperature/",
                       "<wi-1 val=\"n\"",
                       TRUE);
    if (error != 0)
    {
        printf("Meta information is updated but it should not.\n");
        printTestResult(testName, FALSE);
        return error;
    }

    // test removing watch item
    error = testWatchRemove();
    if (error != 0)
    {
        printTestResult(testName, FALSE);
        return 1;
    }



    printTestResult(testName, TRUE);
    return 0;
}

int testSignUpHelper(const char* testName,
                     const char* inputData,
                     const char* checkUri,
                     const char* checkString,
                     BOOL shouldPass)
{
    // invoke signup operation
    obix_server_setResponseListener(&dummyResponseListener);
    Response* response = obixResponse_create((Request*)1);
    obix_server_handlePOST(response, "/obix/signUp/", inputData);
    if (checkResponse(response, !shouldPass) != 0)
    {
        printTestResult(testName, FALSE);
        return 1;
    }
    obixResponse_free(response);

    // check that storage contains everything stored properly
    int result = testSearch("search for signed up object",
                            checkUri, checkString, shouldPass);
    if (result != 0)
    {
        printTestResult(testName, FALSE);
        return 1;
    }

    printTestResult(testName, TRUE);
    return 0;
}

int testSignUp()
{
    int result = 0;
    result += testSignUpHelper("SignUp test: storing data",
                               "<obj name=\"signedDevice1\" href=\"/signedDevice1/\" />",
                               "/obix/signedDevice1/",
                               "signedDevice1",
                               TRUE);
    result += testSignUpHelper("SignUp test: storing ref",
                               "<obj name=\"signedDevice2\" displayName=\"Signed Device 2\" href=\"/signedDevice2/\" />",
                               "/obix/devices/",
                               "Signed Device 2",
                               TRUE);

    result += testSignUpHelper("SignUp test: storing with no name",
                               "<obj href=\"/signedDevice3/\" />",
                               "/obix/signedDevice3/",
                               "signedDevice3",
                               FALSE);

    result += testSignUpHelper("SignUp test: storing with wrong URI",
                               "<obj href=\"signedDevice3/\" />",
                               "/obixsignedDevice3/",
                               "signedDevice3",
                               FALSE);
    return result;
}

int test_server(char* resFolder)
{
    config_setResourceDir(resFolder);

    int result = 0;

    if (xmldb_init("http://localhost"))
    {
        printf("Unable to start tests. database init failed.\n");
        return 1;
    }

    if (xmldb_loadFile("test_devices.xml") != 0)
    {
        printf("Unable to start tests. Check the test data set.\n");
        return 1;
    }

    if (obixWatch_init() != 0)
    {
        printf("Unable to start tests. Watches initialization failed.\n");
        return 1;
    }

    xmldb_printDump();
    // run tests
    result += testDelete("xmldb_delete: remove existing",
                         "/obix/about/serverName/", TRUE);

    result += testDelete("xmldb_delete: remove already deleted",
                         "/obix/about/serverName/", FALSE);

    result += testDelete("xmldb_delete: remove non-existent",
                         "/obix/about/noSuchNode/", FALSE);

    result += testSearch("xmldb_get: simple batch",
                         "/obix/batch/", NULL, TRUE);

    result += testSearch("xmldb_get: obix version",
                         "/obix/about/obixVersion/", NULL, TRUE);

    result += testSearch("xmldb_get: ignore ref tag",
                         "/obix/alarms/", NULL, FALSE);

    result += testSearch("xmldb_get: separate object about",
                         "/obix/about/", NULL, TRUE);

    result += testSearch("xmldb_get: no end slash in request",
                         "/obix/devices", NULL, TRUE);

    result += testSearch("xmldb_get: devices with no slash 1",
                         "/obix/kitchen/device1", NULL, TRUE);

    result += testSearch("xmldb_get: devices with no slash 2",
                         "/obix/kitchen/device1/", NULL, TRUE);

    result += testSearch("xmldb_get: long address",
                         "/obix/kitchen/1/2/3/long", NULL, TRUE);

    result += testSearch("xmldb_get: long address no slash 1",
                         "/obix/kitchen/1/2/3/4/6/long", NULL, TRUE);

    result += testSearch("xmldb_get: long address no slash 2",
                         "/obix/kitchen/1/2/3/4/5/6/long", NULL, FALSE);

    result += testSearch("xmldb_get: very long address no slash",
                         "/obix/kitchen/1/2/3/4/6/7/veryLong", NULL, TRUE);

    result += testWriteToDatabase("xmldb_put: new node",
                                  TRUE,
                                  "<obj href=\"/obix/device/\" name=\"LampSwitch\">\n"
                                  "  <obj name=\"NestedNode\" href=\"nested/\"/>\n"
                                  "</obj>",
                                  "/obix/device/",
                                  "NestedNode",
                                  TRUE);

    result += testDelete("xmldb_delete: remove just added nested",
                         "/obix/device/nested/", TRUE);

    result += testWriteToDatabase("xmldb_put: overwrite existing node",
                                  TRUE,
                                  "<obj href=\"/obix/batch/\" name=\"newBatch!\"/>",
                                  "/obix/batch/",
                                  "newBatch!",
                                  FALSE);

    result += testWriteToDatabase("xmldb_put: wrong server address",
                                  TRUE,
                                  "<obj href=\"http://serve1.com/obix/deviceWrong/\" name=\"LampSwitchWrong\"/>",
                                  "/obix/deviceWrong/",
                                  "LampSwitchWrong",
                                  FALSE);

    result += testWriteToDatabase("xmldb_put: two nodes simultaneously",
                                  TRUE,
                                  "<obj href=\"/obix/device1/\" name=\"LampSwitch1\"/>\n"
                                  "<obj href=\"/obix/device2/\" name=\"LampSwitch2\"/>",
                                  "/obix/device1/",
                                  "LampSwitch1",
                                  FALSE
                                 );
    result += testWriteToDatabase("xmldb_update: correct update",
                                  FALSE,
                                  "<obj href=\"/obix/kitchen/1/2/3/long\" val=\"test string 1\"/>",
                                  "/obix/kitchen/1/2/3/long",
                                  "test string 1",
                                  TRUE);

    result += testWriteToDatabase("xmldb_update: updating with the same value",
                                  FALSE,
                                  "<obj href=\"/obix/kitchen/1/2/3/long\" val=\"test string 1\"/>",
                                  "/obix/kitchen/1/2/3/long",
                                  "test string 1",
                                  TRUE);

    result += testWriteToDatabase("xmldb_update: too much of data 1",
                                  FALSE,
                                  "<obj href=\"/obix/kitchen/1/2/3/long\" val=\"test string 2\" name=\"test string 3\"/>",
                                  "/obix/kitchen/1/2/3/long",
                                  "test string 2",
                                  TRUE);

    result += testWriteToDatabase("xmldb_update: too much of data 2",
                                  FALSE,
                                  "<obj href=\"/obix/kitchen/1/2/3/long\" val=\"test string 4\" name=\"test string 5\"/>",
                                  "/obix/kitchen/1/2/3/long",
                                  "test string 5",
                                  FALSE);

    result += testWriteToDatabase("xmldb_update: explicit not writable",
                                  FALSE,
                                  "<obj href=\"/obix/kitchen/1/2/3/4/6/7/veryLong\" val=\"test string 6\"/>",
                                  "/obix/kitchen/1/2/3/4/6/7/veryLong",
                                  "test string 6",
                                  FALSE);

    result += testWriteToDatabase("xmldb_update: implicit not writable",
                                  FALSE,
                                  "<obj href=\"/obix/kitchen/1/2/3/4/6/long\" val=\"test string 7\"/>",
                                  "/obix/kitchen/1/2/3/4/6/long",
                                  "test string 7",
                                  FALSE);

    result += testWriteToDatabase("xmldb_update: not exist",
                                  FALSE,
                                  "<obj href=\"/obix/kitchen/notExist\" val=\"test string 8\"/>",
                                  "/obix/kitchen/notExist",
                                  "test string 8",
                                  FALSE);

    result += testSearch("xmldb_get: \"..corner/lamp\" exists but we ask for \"corner\"",
                         "/obix/kitchen/corner", NULL, FALSE);

    //    result += testCompareUri("xmldb_compareUri: full match",
    //                             "/obix/shmobix/device1", "/obix/shmobix/device1", TRUE, 0);
    //
    //    result += testCompareUri("xmldb_compareUri: full match with slashes",
    //                             "/obix/shmobix/device1/", "/obix/shmobix/device1/", TRUE, 0);
    //
    //    result += testCompareUri("xmldb_compareUri: match ignoring slashes",
    //                             "/obix/shmobix/device1", "/obix/shmobix/device1/", TRUE, -1);
    //
    //    result += testCompareUri("xmldb_compareUri: match ignoring slashes 2",
    //                             "/obix/shmobix/device1/", "/obix/shmobix/device1", TRUE, 1);
    //
    //    result += testCompareUri("xmldb_compareUri: ends don't match",
    //                             "/obix/shmobix/device2/", "/obix/shmobix/device1", FALSE, 1);
    //
    //    result += testCompareUri("xmldb_compareUri: starts don't match",
    //                             "oobix/shmobix/device1", "/obix/shmobix/device1", FALSE, 0);
    //
    //    result += testServerPostHandlers();

    result += testGenerateResponse("Normalize object", "/obix/kitchen/normalize", "/obix/kitchen/newName/");

    result += testGenerateResponse("Normalize <op/> object", "/obix/kitchen/normalizeOp", "/obix/kitchen/UNnormalizeOp");

    // testing dummy implementation of the client lib
    //    result += testObixClientLib();

    // TODO read todo of testDumpEnvironment
    //    result += testDumpEnvironment();

    result += testSignUp();

    result += testWatch();

    // restart database to clean everything which is broken by previous tests
    //    xmldb_dispose();
    //    if (xmldb_init("http://localhost"))
    //    {
    //        printf("Unable to restart database.\n");
    //        printTestResult("xmldb restart", FALSE);
    //        result++;
    //    }
    //
    //    FCGX_Request* request = (FCGX_Request*) malloc(sizeof(FCGX_Request));
    //    int error = FCGX_InitRequest(request, 0, 0);
    //    if (error != 0)
    //    {
    //    	printf("Unable to initialize fcgi request. running server tests failed.\n");
    //    	return -1;
    //    }
    //
    //    result+= testServerHandlePost(NULL);
    //
    //    free(request);

    return result;
}
