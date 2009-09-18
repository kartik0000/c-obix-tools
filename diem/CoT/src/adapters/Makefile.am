## Process this file with automake to produce Makefile.in

bin_PROGRAMS 					= example_timer sensor_floor_driver test_adapter

example_timer_SOURCES			= example_timer.c

example_timer_CFLAGS			= -I$(top_srcdir)/src/client \
								  -I$(top_srcdir)/src/common

example_timer_LDADD				= $(top_builddir)/src/client/libcot-client.la

sensor_floor_driver_SOURCES		= sensor_floor_driver.c

sensor_floor_driver_CFLAGS		= $(WARN_FLAGS) \
								  -I$(top_srcdir)/src/client \
								  -I$(top_srcdir)/src/common

sensor_floor_driver_LDADD		= $(top_builddir)/src/client/libcot-client.la


test_adapter_SOURCES			= test_adapter.c

test_adapter_CFLAGS				= -I$(top_srcdir)/src/client \
								  -I$(top_srcdir)/src/common 

test_adapter_LDADD				= $(top_builddir)/src/client/libcot-client.la 