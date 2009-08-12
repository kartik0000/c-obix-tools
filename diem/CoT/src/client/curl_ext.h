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
 * @todo add description here
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#ifndef CURL_EXT_H_
#define CURL_EXT_H_

#include <ixml_ext.h>
#include <curl/curl.h>

typedef struct _CURL_EXT
{
	CURL* curl;

	// buffer for storing incoming data
	char* inputBuffer;
	// counters for that buffer
	int inputBufferSize; // size of buffer
	int inputBufferFree; // free space in buffer
	// buffer for storing sending data
	const char* outputBuffer;
	// counters for outgoing data
	int outputSize; // size of output data
	int outputPos; // number of sent bytes
	// buffer for storing CURL error messages
	char* errorBuffer;
} CURL_EXT;

int curl_ext_init(int defaultInputBufferSize);
void curl_ext_dispose();

int curl_ext_create(CURL_EXT** handle);
void curl_ext_free(CURL_EXT* handle);

int curl_ext_get(CURL_EXT* handle, const char* uri);
int curl_ext_put(CURL_EXT* handle, const char* uri);
int curl_ext_post(CURL_EXT* handle, const char* uri);

int curl_ext_getDOM(CURL_EXT* handle, const char* uri, IXML_Document** response);
int curl_ext_putDOM(CURL_EXT* handle, const char* uri, IXML_Document** response);
int curl_ext_postDOM(CURL_EXT* handle, const char* uri, IXML_Document** response);

#endif /* CURL_EXT_H_ */
