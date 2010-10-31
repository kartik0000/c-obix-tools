/*
 * pico_http_feed_reader.h
 *
 *  Created on: Oct 30, 2010
 *      Author: andrey
 */

#ifndef PICO_HTTP_FEED_READER_H_
#define PICO_HTTP_FEED_READER_H_

typedef struct _picoCluster {
	const char* id;
	const char* x;
	const char* y;
	const char* vx;
	const char* vy;
	const char* magnitude;
} picoCluster;

typedef void (*pico_cluster_listener)(picoCluster* cluster);

int pico_initFeedReader(pico_cluster_listener listener);

void pico_disposeReader();

int pico_readFeed(char* url);

#endif /* PICO_HTTP_FEED_READER_H_ */
