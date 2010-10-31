## Process this file with automake to produce Makefile.in

bin_PROGRAMS 					= example_timer sensor_floor_driver \
								  memtest_adapter poll_generator

check_PROGRAMS					= test_pico_feed_reader								  
								  
## TODO distribute these scripts to the bin folder								  
EXTRA_DIST						= max_mem_test.sh max_mem_test_stop.sh \
								  poll_generator.sh

example_timer_SOURCES			= example_timer.c

example_timer_CFLAGS			= -I$(top_srcdir)/src/client \
								  -I$(top_srcdir)/src/common

example_timer_LDADD				= $(top_builddir)/src/client/libcot-client.la

sensor_floor_driver_SOURCES		= sensor_floor_driver.c

sensor_floor_driver_CFLAGS		= $(WARN_FLAGS) \
								  -I$(top_srcdir)/src/client \
								  -I$(top_srcdir)/src/common

sensor_floor_driver_LDADD		= $(top_builddir)/src/client/libcot-client.la


memtest_adapter_SOURCES			= memtest_adapter.c

memtest_adapter_CFLAGS			= -I$(top_srcdir)/src/client \
								  -I$(top_srcdir)/src/common 

memtest_adapter_LDADD			= $(top_builddir)/src/client/libcot-client.la

poll_generator_SOURCES			= poll_generator.c

poll_generator_CFLAGS			= -I$(top_srcdir)/src/client \
								  -I$(top_srcdir)/src/common 

poll_generator_LDADD			= $(top_builddir)/src/client/libcot-client.la

test_pico_feed_reader_SOURCES	= pico_http_feed_reader.c pico_http_feed_reader.h

test_pico_feed_reader_CFLAGS	= $(WARN_FLAGS) $(LIBXML_CFLAGS) \
								  -I$(top_srcdir)/src/client \
								  -I$(top_srcdir)/src/common 

test_pico_feed_reader_LDADD		= $(LIBXML_LIBS) \
								  $(top_builddir)/src/client/libcot-client.la 