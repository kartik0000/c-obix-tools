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
 * Defines simple API for HTTP client.
 *
 * This API is created specially for oBIX communication over HTTP.
 * It allows to perform three HTTP request types:
 * @li GET - oBIX read request;
 * @li PUT - oBIX write request;
 * @li POST - oBIX execute operation request.
 *
 * API is based on @a libcurl.
 *
 * It is internal library which is supposed to be used only by C oBIX Client API
 * implementation.
 *
 * @author Andrey Litvinov
 */

#ifndef CURL_EXT_H_
#define CURL_EXT_H_

#include <ixml_ext.h>
#include <curl/curl.h>

/**
 * Defines a handle for HTTP client, which wraps CURL handle.
 */
typedef struct _CURL_EXT
{
    /** CURL handle */
    CURL* curl;

    /** Buffer for storing incoming data.*/
    char* inputBuffer;
    // counters for that buffer:
    int inputBufferSize;
    int inputBufferFree;
    /** Buffer for storing sending data.*/
    const char* outputBuffer;
    // counters for outgoing data
    int outputSize; // size of output data
    int outputPos; // number of sent bytes
    /** buffer for storing CURL error messages.*/
    char* errorBuffer;
}
CURL_EXT;

/**
 * Initialized HTTP client library. Must be called once, when the application is
 * launched.
 *
 * @param defaultInputBufferSize Sets the default size for the input buffer. Can
 * 				be useful to adjust this size if the amount of input data is
 * 				expected to be always small (then small buffer will save memory)
 * 				or big (then big buffer will be quicker).
 * @return @a 0 if initialization was successful; @a -1 on error.
 */
int curl_ext_init(int defaultInputBufferSize);

/**
 * Cleans up everything allocated during initialization (but not created
 * CURL_EXT handles). Should be called once during program shutdown.
 */
void curl_ext_dispose();

/**
 * Creates handle for HTTP client.
 *
 * @param handle A pointer to created handle is returned here.
 * @return  @li @a 0 - On success.
 * 			@li @a -2 - Not enough memory.
 * 			@li @a -1 - Other error.
 */
int curl_ext_create(CURL_EXT** handle);

/**
 * Sets SSL settings to the provide handle.
 *
 * @param verifyPeer @a 0 will switch all SSL certificates off (all remaining
 *                   arguments will make no effect). @a 1 will switch
 * 					 on checking remote peer, i.e. checking that he has trusted
 * 					 certificate.
 * @param verifyHost @li @a 1 Will switch on verifying remote host name. It
 * 					 should correspond to the Common Name field of provided
 * 					 certificate.
 * 					 @li @a 0 Will switch off checking remote host name.
 * 					 @li @a <0 Will leave default curl settings.
 * @param caFile     Name of the file with trusted certificates. if @a NULL,
 * 					 default curl setting will be used.
 * @return @a 0 on success, @a -1 on error.
 */
int curl_ext_setSSL(CURL_EXT* handle,
                    int verifyPeer,
                    int verifyHost,
                    const char* caFile);

/**
 * Cleans memory allocated for HTTP client handle.
 *
 * @param handle A handle to free.
 */
void curl_ext_free(CURL_EXT* handle);

/**
 * Performs HTTP GET request using provided handle.
 * Response will be stored at handle's input buffer.
 *
 * @param handle A handle which will be used to perform the request.
 * @param uri    Requesting URI.
 * @return @a 0 on success; @a -1 on error.
 */
int curl_ext_get(CURL_EXT* handle, const char* uri);

/**
 * Performs HTTP PUT request using provided handle.
 * A reference to the data to be sent should be stored at handle's output buffer
 * field. Don't forget to free memory allocated by sending data after the
 * request.
 *
 * Response will be stored at handle's input buffer.
 *
 * @param handle A handle which will be used to perform the request.
 * @param uri    Requesting URI.
 * @return @a 0 on success; @a -1 on error.
 */
int curl_ext_put(CURL_EXT* handle, const char* uri);

/**
 * Performs HTTP POST request using provided handle.
 * A reference to the data to be sent should be stored at handle's output buffer
 * field. Don't forget to free memory allocated by sending data after the
 * request.
 *
 * Response will be stored at handle's input buffer.
 *
 * @param handle A handle which will be used to perform the request.
 * @param uri    Requesting URI.
 * @return @a 0 on success; @a -1 on error.
 */
int curl_ext_post(CURL_EXT* handle, const char* uri);

/**
 * Works as #curl_ext_get. In addition, tries to parse received XML response.
 *
 * @param response A pointer to the parsed XML DOM structure is returned here.
 */
int curl_ext_getDOM(CURL_EXT* handle,
                    const char* uri,
                    IXML_Document** response);

/**
 * Works as #curl_ext_put. In addition, tries to parse received XML response.
 *
 * @param response A pointer to the parsed XML DOM structure is returned here.
 */
int curl_ext_putDOM(CURL_EXT* handle,
                    const char* uri,
                    IXML_Document** response);

/**
 * Works as #curl_ext_post. In addition, tries to parse received XML response.
 *
 * @param response A pointer to the parsed XML DOM structure is returned here.
 */
int curl_ext_postDOM(CURL_EXT* handle,
                     const char* uri,
                     IXML_Document** response);

#endif /* CURL_EXT_H_ */
