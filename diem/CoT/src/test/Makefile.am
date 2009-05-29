## Process this file with automake to produce Makefile.in

bin_PROGRAMS 		= obix_test

obix_test_SOURCES 	= test_main.h test_main.c \
					  test_common.h test_common.c \
					  test_server.h test_server.c \
					  test_client.h test_client.c \
					  test_ptask.h test_ptask.c \
					  test_table.h test_table.c \
					  $(top_srcdir)/src/server/xml_storage.h \
					  $(top_srcdir)/src/server/xml_storage.c \
					  $(top_srcdir)/src/server/server.h \
					  $(top_srcdir)/src/server/server.c \
					  $(top_srcdir)/src/server/watch.h \
					  $(top_srcdir)/src/server/watch.c \
					  $(top_srcdir)/src/server/response.h \
					  $(top_srcdir)/src/server/response.c \
					  $(top_srcdir)/src/server/post_handler.h \
					  $(top_srcdir)/src/server/post_handler.c			   
               
obix_test_CFLAGS 	= $(DEPS_CFLAGS) -I$(top_srcdir)/src/client \
					  -I$(top_srcdir)/src/server \
					  -I$(top_srcdir)/src/common					  
					  
obix_test_LDADD 	= $(DEPS_LIBS) \
					  $(top_builddir)/src/client/libcot_client.la
				  