/* *****************************************************************************
 * Copyright (c) 2009, 2010 Andrey Litvinov
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
 * Tests for oBIX server.
 *
 * @author Andrey Litvinov
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <obix_utils.h>
#include <xml_storage.h>
#include <log_utils.h>
#include <xml_config.h>
#include <ixml_ext.h>
#include <server.h>
#include <watch.h>
#include <obix_fcgi.h>
#include "test_main.h"

/** @name Global server test variables.
 * Used in tests, where response is handled in a separate thread.
 * @{ */
BOOL _responseIsSent = FALSE;
Response* _lastResponse;
pthread_mutex_t _responseMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t _responseReceived = PTHREAD_COND_INITIALIZER;
/** @} */

/**
 * Listener, which finally receives generated response from server.
 */
void dummyResponseListener(Response* response)
{
    pthread_mutex_lock(&_responseMutex);
    _responseIsSent = TRUE;
    _lastResponse = response;
    pthread_cond_signal(&_responseReceived);
    pthread_mutex_unlock(&_responseMutex);
}

/** Checks whether any response has been received since last call to this
 * function. */
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

/** Waits for response to be generated in a separate thread. */
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

/** Creates request object for tests. */
static Request* createDummyRequest(BOOL canWait)
{
    Request* request = (Request*) malloc(sizeof(Request));
    if (request == NULL)
    {
        printf("ERROR: Unable to allocate memory for request object.\n");
        return NULL;
    }

    request->serverAddress = "http://localhost";
    request->serverAddressLength = 16;
    request->canWait = canWait;
    request->next = NULL;
    return request;
}

/** Creates response object for tests. */
static Response* createTestResponse(BOOL withRequest, BOOL canWait)
{
    Request* request = withRequest ? createDummyRequest(canWait) : NULL;
    Response* response = obixResponse_create(request);
    return response;
}

/**
 * Deletes test response object.
 */
static void freeTestResponse(Response* response)
{
    if (response->request != NULL)
    {
        free(response->request);
    }
    obixResponse_free(response);
}

/**
 * Checks how #xmldb_get searches for required oBIX object in the storage.
 * @param href URI to search for.
 * @param checkStr String, which should be present in found object.
 * @param exists if @a FALSE, than search is supposed to fail.
 */
static int testSearch(const char* testName,
                      const char* href,
                      const char* checkStr,
                      BOOL exists)
{
    char* node = xmldb_get(href, NULL);
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
        printf("Search test failed.\n");
        //        printf("Search test failed. Current db contents:\n");
        //        xmldb_printDump();
        printTestResult(testName, FALSE);
        return 1;
    }
}

/**
 * Checks #xmldb_put or #xmldb_updateDOM function.
 *
 * @param writeNew If @a TRUE, than #xmldb_put is used to put new data to the
 * 				storage. If @a FALSE, than #xmldb_updateDOM is used to update
 * 				value of existing object in the storage.
 * @param newData Data, which should be written.
 * @param href URI to where data should be written (in case of
 * 				#xmldb_updateDOM).
 * @param checkString String, which should appear in storage somewhere under
 * 				provided @a href after new data has been written.
 * @param shouldPass If @a FALSE, than the test is supposed to fail.
 */
static int testWriteToDatabase(const char* testName,
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
        IXML_Element* data = ixmlElement_parseBuffer(newData);
        error = xmldb_updateDOM(data, href, NULL, NULL);
        ixmlElement_freeOwnerDocument(data);
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
    char* node = xmldb_get(href, NULL);
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

/**
 * Tests #xmldb_delete function.
 */
static int testDelete(const char* testName, const char* href, BOOL exists)
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
    char* node = xmldb_get(href, NULL);
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

/**
 * Tests #obix_server_generateResponse function.
 * Takes object from the storage, and generates a server response from it.
 * @param uri URI where source object should be taken from.
 * @param newUrl URI which should be assigned to the object by
 * 			#obix_server_generateResponse
 */
static int testGenerateResponse(const char* testName,
                                const char* uri,
                                const char* newUrl)
{
    IXML_Element* oBIXdoc = xmldb_getDOM(uri, NULL);
    if (oBIXdoc == NULL)
    {
        printf("Uri \"%s\" is not found in storage.\n", uri);
        xmldb_printDump();
        printTestResult(testName, FALSE);
        return 1;
    }

    Response* response = createTestResponse(TRUE, FALSE);
    obix_server_generateResponse(response,
                                 oBIXdoc,
                                 newUrl,
                                 0,
                                 FALSE);

    if ((response == NULL) || (response->body == NULL))
    {
        printf("oBIX normalization without saving is failed.\n");
        printTestResult(testName, FALSE);
        return 1;
    }
    printf("normalized object = %s", response->body);
    freeTestResponse(response);

    if (testSearch("check object after normalization 1", newUrl, NULL, FALSE))
    {
        printf("Changes in object are saved after normalization "
               "(but they shouldn't).\n");
        printTestResult(testName, FALSE);
        return 1;
    }

    response = createTestResponse(FALSE, FALSE);
    obix_server_generateResponse(response,
                                 oBIXdoc,
                                 newUrl,
                                 0,
                                 TRUE);
    if ((response == NULL) || (response->body == NULL))
    {
        printf("oBIX normalization with saving is failed.\n");
        printTestResult(testName, FALSE);
        return 1;
    }

    printf("normalized object = %s", response->body);
    freeTestResponse(response);

    if (testSearch("check object after normalization 2", newUrl, NULL, TRUE) ||
            testSearch("check object for \'meta\' tags after normalization",
                       newUrl, OBIX_META, FALSE))
    {
        printf("Changes in object are not saved after normalization (but "
               "they should).\n");
        printTestResult(testName, FALSE);
        return 1;
    }

    printTestResult(testName, TRUE);
    return 0;
}

/** Prints response contents to log. */
static void printResponse(Response* response)
{
    printf("Received response:\n");
    while (response != NULL)
    {
        printf("%s", response->body);
        response = response->next;
    }

    printf("\n");
}

/**
 * Checks the response object to be generated correctly. Helper function.
 *
 * @param containsError if @a TRUE, than the response object is supposed to
 * 				contain error object.
 */
static int checkResponse(Response* response, BOOL containsError)
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

/**
 * Searches for the string in provided response.
 *
 * @param exists @a TRUE, than the provided string should exist. @a FALSE means
 * 			that no such string should be found.
 * @return @a 0 if condition is met, @a 1 otherwise.
 */
static int findInResponse(Response* response,
                          const char* checkString,
                          BOOL exists)
{
    while(response != NULL)
    {
        if ((response->body != NULL) &&
                (strstr(response->body, checkString) != NULL))
        {
            // we found required substring
            if (exists)
            {   // string supposed to be found
                printf("Test string \"%s\" is found in response, and it is "
                       "good.\n", checkString);
                return 0;
            }
            else
            {	// string shouldn't exist
                printf("Test string \"%s\" is found in response, but it "
                       "shouldn\'t.\n",
                       checkString);
                return 1;
            }
        }
        response = response->next;
    }

    // nothing was found
    if (exists)
    {	// string supposed to be found
        printf("Test string \"%s\" is not found in response, but it should.\n",
               checkString);
        return 1;
    }
    else
    {	// string is not found and that is great :)
        printf("Test string \"%s\" is not found in response, and it is good.\n",
               checkString);
        return 0;
    }
}

/**
 * Helper function to test Watch.pollChanges operation.
 * Sends request to Watch object and checks that response contains required
 * information.
 * @param uri URI to request.
 * @param checkStrings Array of strings which should (or should not) present in
 * 				the response.
 * @param checkSize Size of @a checkStrings array.
 * @param exists If @a TRUE, than strings from @a checkStrings should present
 * 				in the response. @a FALSE means that no of provided strings
 * 				should appear.
 * @param waitResponse If @a TRUE, than function would consider that the request
 * 				will be handled asynchronously and thus it will wait until
 * 				response is handled in a separate thread.
 */
static int testWatchPollChanges(const char* testName,
                                const char* uri,
                                const char* checkStrings[],
                                int checkSize,
                                BOOL exists,
                                BOOL waitResponse)
{
    obixResponse_setListener(&dummyResponseListener);
    Response* response = createTestResponse(TRUE, TRUE);
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

    freeTestResponse(response);

    if (error != 0)
    {
        printTestResult(testName, FALSE);
        return 1;
    }

    printTestResult(testName, TRUE);
    return 0;
}

/**
 * Tests Watch.remove operation.
 */
static int testWatchRemove()
{
    const char* testName = "Watch.remove test";
    obixResponse_setListener(&dummyResponseListener);
    Response* response = createTestResponse(TRUE, FALSE);
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
    freeTestResponse(response);

    // now try to poll refresh and check that we do not receive object which
    // we've just removed from the watch list
    response = createTestResponse(TRUE, FALSE);
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
    freeTestResponse(response);
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

/**
 * Tests #obix_server_handlePUT function.
 */
static int testPutHandler(const char* testName, const char* uri, const char* data)
{
    Response* response = createTestResponse(TRUE, FALSE);
    obix_server_handlePUT(response, uri, data);
    if (checkResponse(response, FALSE) != 0)
    {
        printTestResult(testName, FALSE);
        return 1;
    }
    freeTestResponse(response);
    printTestResult(testName, TRUE);
    return 0;
}

/**
 * Helper function to create Watch object.
 */
static int testWatchMakeHelper(const char* testName)
{
    Response* response = createTestResponse(TRUE, FALSE);
    obix_server_handlePOST(response, "/obix/watchService/make", NULL);
    if (checkResponse(response, FALSE) != 0)
    {
        printTestResult(testName, FALSE);
        return 1;
    }
    freeTestResponse(response);
    return 0;
}

/**
 * Helper function which deletes watch created by #testWatchMakeHelper.
 */
static int testWatchDeleteHelper(const char* testName)
{
    Response* response = createTestResponse(TRUE, FALSE);
    obix_server_handlePOST(response,
                           "/obix/watchService/watch1/delete",
                           "<obj null=\"true\" />");
    if (checkResponse(response, FALSE) != 0)
    {
        printf("Unable to delete Watch object.\n");
        printTestResult(testName, FALSE);
        return 1;
    }
    freeTestResponse(response);
    return 0;
}

/**
 * Tests all Watch operations.
 */
static int testWatch()
{
    const char* testName = "oBIX Watch test";
    obixResponse_setListener(&dummyResponseListener);
    // create new Watch object
    testWatchMakeHelper(testName);

    // consider that we received a watch with name watch1
    // modify lease time
    int error = testPutHandler(
                    "Changing Watch lease time",
                    "/obix/watchService/watch1/lease",
                    "<reltime href=\"/obix/watchService/watch1/lease\" "
                    "val=\"PT5M\"/>");
    if (error != 0)
    {
        printTestResult(testName, FALSE);
        return error;
    }

    // add object to the watch
    // + one with wrong trailing slash, one <op/> object,
    // one comment and one wrong object.
    // TODO check duplicate request for same object
    Response* response = createTestResponse(TRUE, FALSE);
    obix_server_handlePOST(
        response,
        "/obix/watchService/watch1/add",
        "<obj is=\"obix:WatchIn\">\r\n"
        "<list name=\"hrefs\" of=\"obix:WatchInItem\">\r\n"
        " <!-- Comment goes here -->\r\n"
        " and not only the comment\r\n"
        " <obj name=\"Wrong Object\"/>\r\n"
        " <uri is=\"obix:WatchInItem\" val=\"/obix/watchService/make\"/>\r\n"
        " <uri is=\"obix:WatchInItem\" val=\"/obix/kitchen/temperature/\"/>\r\n"
        " <uri is=\"obix:WatchInItem\" val=\"/obix/kitchen/temperature2\"/>\r\n"
        " <uri is=\"obix:WatchInItem\" val=\"/obix/kitchen/parent/\"/>\r\n"
        "</list>\r\n"
        "</obj>");
    if (checkResponse(response, TRUE) != 0)
    {
        printTestResult(testName, FALSE);
        return 1;
    }
    // TODO check that response has subscribed object
    // and error messages for wrong objects
    // we should 2 <err/> objects with links watchService/make and temperature2
    // and object testWatch1. We shouldn't have "testWatch2" and
    // "Make new watch"
    freeTestResponse(response);

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
                 "<int href=\"/obix/kitchen/parent/child/\" "
                 "val=\"newValue\"/>");
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
    const char* checkStrings[] = {"testWatch1", "testWatch3"};
    error = testWatchPollChanges("test Watch.pollChanges with some changes "
                                 "happened",
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
                "<reltime "
                "href=\"/obix/watchService/watch1/pollWaitInterval/max\" "
                "val=\"PT0.010S\"/>");
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
    response = createTestResponse(TRUE, FALSE);
    obix_server_handlePUT(
        response,
        "/obix/kitchen/temperature/",
        "<int href=\"/obix/kitchen/temperature/\" val=\"newValue\"/>");
    if (checkResponse(response, FALSE) != 0)
    {
        printTestResult(testName, FALSE);
        return 1;
    }
    freeTestResponse(response);
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

    error = testWatchDeleteHelper(testName);
    if (error != 0)
    {
        return 1;
    }

    printTestResult(testName, TRUE);
    return 0;
}

/**
 * Helper function for testing signUp operation.
 * Registers provided data at the server and then tries to retrieve it.
 *
 * @param inputData Data to publish at the server.
 * @param checkUri URI to check after publishing.
 * @param checkString String, which should present at @a checkUri at the server.
 * @param shouldPass if @a FALSE, than test is supposed to fail.
 */
static int testSignUpHelper(const char* testName,
                            const char* inputData,
                            const char* checkUri,
                            const char* checkString,
                            BOOL shouldPass)
{
    // invoke signup operation
    Response* response = createTestResponse(TRUE, FALSE);
    obix_server_handlePOST(response, "/obix/signUp/", inputData);
    if (checkResponse(response, !shouldPass) != 0)
    {
        printTestResult(testName, FALSE);
        return 1;
    }
    freeTestResponse(response);

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

/**
 * Tests signUp operation.
 */
static int testSignUp()
{
    obixResponse_setListener(&dummyResponseListener);
    int result = 0;
    result += testSignUpHelper("SignUp test: storing data",
                               "<obj name=\"signedDevice1\" "
                               "href=\"/signedDevice1/\" />",
                               "/obix/signedDevice1/",
                               "signedDevice1",
                               TRUE);
    result += testSignUpHelper("SignUp test: storing ref",
                               "<obj name=\"signedDevice2\" "
                               "displayName=\"Signed Device 2\" "
                               "href=\"/signedDevice2/\" />",
                               "/obix/devices/",
                               "Signed Device 2",
                               TRUE);

    result +=
        testSignUpHelper("SignUp test: storing with no attributes except href",
                         "<obj href=\"/signedDevice3/\" />",
                         "/obix/signedDevice3/",
                         "signedDevice3",
                         TRUE);

    result += testSignUpHelper("SignUp test: no attributes at all",
                               "<obj />",
                               "/obix/signedDevice4/",
                               "signedDevice4",
                               FALSE);

    result += testSignUpHelper("SignUp test: storing with wrong URI",
                               "<obj href=\"signedDevice4/\" />",
                               "/obixsignedDevice4/",
                               "signedDevice4",
                               FALSE);
    return result;
}

static int testWatchRemoteOperationsHelper(const char* testName,
        const char* operationUri,
        const char* operationName,
        const char* operationResponse,
        const char* testResponseString)
{
    //try to execute this operation and see what happens
    Response* remoteOperationResponse = createTestResponse(TRUE, TRUE);
    obix_server_handlePOST(remoteOperationResponse,
                           operationUri,
                           "<obj null=\"true\" />");

    //try to poll changes: we now should receive operation invocation
    const char* checkStrings[] =
        {"<op", operationName, "OperationInvocation", "null", "in"
        };
    int error = testWatchPollChanges("Remote Operations (server side): "
                                     "Check op response",
                                     "/obix/watchService/watch1/pollChanges",
                                     checkStrings,
                                     5,
                                     TRUE,
                                     FALSE);
    if (error != 0)
    {
        printf("Watch.pollChanges operation did not return expected output.\n"
               "Expected: Operation invocation object.\n");
        printTestResult(testName, FALSE);
        return 1;
    }
    // now we assume that we have processed the request and we send results
    char operationResponseMessage[300];
    sprintf(operationResponseMessage,
            "<op is=\"/obix/def/OperationResponse\" href=\"%s\">\r\n"
            "	%s\r\n"
            "</op>",
            operationUri, operationResponse);
    Response* response = createTestResponse(TRUE, FALSE);
    obix_server_handlePOST(response,
                           "/obix/watchService/watch1/operationResponse",
                           operationResponseMessage);
    if (checkResponse(response, FALSE) != 0)
    {
        printTestResult(testName, FALSE);
        return 1;
    }
    // now remote operation should send the response back. Check that it
    // contains what we have sent
    printf("Remote operation execution results:\n");
    printResponse(remoteOperationResponse);
    error = findInResponse(remoteOperationResponse, testResponseString, TRUE);
    error += findInResponse(remoteOperationResponse, "out", FALSE);
    if (error != 0)
    {
        printf("Remote operation response doesn't contain what it should.\n");
        printTestResult(testName, FALSE);
        return 1;
    }

    printTestResult(testName, TRUE);
    return 0;
}

static int testWatchAddOperationHelper(const char* operationUri)
{
    //add operation to the watch
    char addOperationRequest[250];
    sprintf(addOperationRequest,
            "<obj is=\"obix:WatchIn\">\r\n"
            "<list name=\"hrefs\" of=\"obix:WatchInItem\">\r\n"
            " <uri is=\"obix:WatchInItem\" val=\"%s\"/>\r\n"
            "</list>\r\n"
            "</obj>", operationUri);
    Response* response = createTestResponse(TRUE, FALSE);
    obix_server_handlePOST(response,
                           "/obix/watchService/watch1/addOperation",
                           addOperationRequest);
    if (checkResponse(response, FALSE) != 0)
    {
        return 1;
    }
    if (findInResponse(response, operationUri, TRUE) != 0)
    {
        return 1;
    }
    freeTestResponse(response);

    return 0;
}

/**
 * Tests operation forwarding.
 */
static int testWatchRemoteOperations()
{
    const char* testName = "Remote Operations (server side) test";
    obixResponse_setListener(&dummyResponseListener);
    // save some operation at the server
    int error = testSignUpHelper(
                    "Remote Operations test: storing data with operations",
                    "<obj name=\"remoptest\" href=\"/obix/remoptest/\" >\r\n"
                    "  <op href=\"/obix/remoptest/op1\" in=\"obix:Nil\" "
                    "out=\"obix:obj\" />\r\n"
                    "</obj>",
                    "/obix/remoptest/",
                    "remoptest",
                    TRUE);
    if (error != 0)
    {
        return error;
    }

    // subscribe to this operation, first create watch
    testWatchMakeHelper("Remote Operations (server side): Create watch");
    // then add the operation to the watch
    error = testWatchAddOperationHelper("/obix/remoptest/op1");
    if (error != 0)
    {
        printf("Unable to add operation to the watch using "
               "Watch.addOperation.\n");
        return 1;
    }

    error =
        testWatchRemoteOperationsHelper("Normal remote op execution",
                                        "/obix/remoptest/op1",
                                        "op1",
                                        "<str name=\"out\" val=\"test123\" />",
                                        "test123");
    error +=
        testWatchRemoteOperationsHelper("2nd execution of same op",
                                        "/obix/remoptest/op1",
                                        "op1",
                                        "<str name=\"out\" val=\"2ndEx\" />",
                                        "2ndEx");
    if (error != 0)
    {
        return error;
    }

    if (testWatchDeleteHelper(testName) != 0)
    {
        return 1;
    }

    printTestResult(testName, TRUE);
    return 0;
}

/**
* Tests #obixResponse_setRightUri function.
* @param requestUri Parameter to pass to #obixResponse_setRightUri.
* @param slashFlag Parameter to pass to #obixResponse_setRightUri.
* @param rightUri Correct URI, which should appear in response object.
*/
static int testResponse_setRightUri(const char* testName,
                                    const char* requestUri,
                                    int slashFlag,
                                    const char* rightUri)
{
    Response* response = createTestResponse(FALSE, FALSE);
    obixResponse_setRightUri(response, requestUri, slashFlag);

    if ((response->uri == NULL) || (strcmp(response->uri, rightUri) != 0))
    {
        printf("Response URI after "
               "obixResponse_setRightUri(response, \"%s\", %d); is \"%s\", "
               "but should be \"%s\".\n",
               requestUri, slashFlag, response->uri, rightUri);
        freeTestResponse(response);
        printTestResult(testName, FALSE);
        return 1;
    }

    freeTestResponse(response);
    printTestResult(testName, TRUE);
    return 0;
}

int test_server(char* resFolder)
{
    config_setResourceDir(resFolder);

    int result = 0;

    if (xmldb_init())
    {
        printf("FAILED: Unable to start tests. database init failed.\n");
        return 1;
    }

    if (xmldb_loadFile("test_devices.xml") != 0)
    {
        printf("FAILED: Unable to start tests. Check the test data set.\n");
        return 1;
    }

    if (obixWatch_init() != 0)
    {
        printf("FAILED: Unable to start tests. "
               "Watches initialization failed.\n");
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

    result +=
        testWriteToDatabase("xmldb_put: new node",
                            TRUE,
                            "<obj href=\"/obix/device/\" name=\"LampSwitch\">\n"
                            "  <obj name=\"NestedNode\" href=\"nested/\"/>\n"
                            "</obj>",
                            "/obix/device/",
                            "NestedNode",
                            TRUE);

    result += testDelete("xmldb_delete: remove just added nested",
                         "/obix/device/nested/", TRUE);

    result +=
        testWriteToDatabase("xmldb_put: overwrite existing node",
                            TRUE,
                            "<obj href=\"/obix/batch/\" name=\"newBatch!\"/>",
                            "/obix/batch/",
                            "newBatch!",
                            FALSE);

    result +=
        testWriteToDatabase("xmldb_put: wrong server address",
                            TRUE,
                            "<obj href=\"http://serve1.com/obix/deviceWrong/\" "
                            "name=\"LampSwitchWrong\"/>",
                            "/obix/deviceWrong/",
                            "LampSwitchWrong",
                            FALSE);

    result +=
        testWriteToDatabase("xmldb_put: two nodes simultaneously",
                            TRUE,
                            "<obj href=\"/obix/device1/\" "
                            "name=\"LampSwitch1\"/>\n"
                            "<obj href=\"/obix/device2/\" "
                            "name=\"LampSwitch2\"/>",
                            "/obix/device1/",
                            "LampSwitch1",
                            FALSE
                           );
    result += testWriteToDatabase("xmldb_update: correct update",
                                  FALSE,
                                  "<obj href=\"/obix/kitchen/1/2/3/long\" "
                                  "val=\"test string 1\"/>",
                                  "/obix/kitchen/1/2/3/long",
                                  "test string 1",
                                  TRUE);

    result += testWriteToDatabase("xmldb_update: updating with the same value",
                                  FALSE,
                                  "<obj href=\"/obix/kitchen/1/2/3/long\" "
                                  "val=\"test string 1\"/>",
                                  "/obix/kitchen/1/2/3/long",
                                  "test string 1",
                                  TRUE);

    result +=
        testWriteToDatabase("xmldb_update: too much of data 1",
                            FALSE,
                            "<obj href=\"/obix/kitchen/1/2/3/long\" "
                            "val=\"test string 2\" name=\"test string 3\"/>",
                            "/obix/kitchen/1/2/3/long",
                            "test string 2",
                            TRUE);

    result +=
        testWriteToDatabase("xmldb_update: too much of data 2",
                            FALSE,
                            "<obj href=\"/obix/kitchen/1/2/3/long\" "
                            "val=\"test string 4\" name=\"test string 5\"/>",
                            "/obix/kitchen/1/2/3/long",
                            "test string 5",
                            FALSE);

    result +=
        testWriteToDatabase("xmldb_update: explicit not writable",
                            FALSE,
                            "<obj href=\"/obix/kitchen/1/2/3/4/6/7/veryLong\" "
                            "val=\"test string 6\"/>",
                            "/obix/kitchen/1/2/3/4/6/7/veryLong",
                            "test string 6",
                            FALSE);

    result += testWriteToDatabase("xmldb_update: implicit not writable",
                                  FALSE,
                                  "<obj href=\"/obix/kitchen/1/2/3/4/6/long\" "
                                  "val=\"test string 7\"/>",
                                  "/obix/kitchen/1/2/3/4/6/long",
                                  "test string 7",
                                  FALSE);

    result += testWriteToDatabase("xmldb_update: not exist",
                                  FALSE,
                                  "<obj href=\"/obix/kitchen/notExist\" "
                                  "val=\"test string 8\"/>",
                                  "/obix/kitchen/notExist",
                                  "test string 8",
                                  FALSE);

    result += testSearch("xmldb_get: \"..corner/lamp\" exists but we ask for "
                         "\"corner\"",
                         "/obix/kitchen/corner", NULL, FALSE);

    //    result += testServerPostHandlers();

    result += testGenerateResponse("Normalize object",
                                   "/obix/kitchen/normalize",
                                   "/obix/kitchen/newName/");

    result += testGenerateResponse("Normalize <op/> object",
                                   "/obix/kitchen/normalizeOp",
                                   "/obix/kitchen/UNnormalizeOp");

    result += testSignUp();

    result += testWatch();

    result += testResponse_setRightUri("obixResponse_setRightUri 1",
                                       "/obix/test/",
                                       -1,
                                       "/obix/test");

    result += testResponse_setRightUri("obixResponse_setRightUri 2",
                                       "/obix/test",
                                       1,
                                       "/obix/test/");

    result += testWatchRemoteOperations();

    return result;
}
