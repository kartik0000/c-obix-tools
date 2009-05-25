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
