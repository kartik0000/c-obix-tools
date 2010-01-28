## Process this file with automake to produce Makefile.in

pkginclude_HEADERS			= obix_client.h

lib_LTLIBRARIES				= libcot-client.la

libcot_client_la_SOURCES	= obix_client.h obix_client.c \
							  obix_batch.h obix_batch.c \
							  obix_comm.h \
							  obix_http.h obix_http.c \
							  curl_ext.h curl_ext.c

libcot_client_la_CFLAGS		= $(WARN_FLAGS) \
							  $(CURL_CFLAGS) -I$(top_srcdir)/src/common

libcot_client_la_LIBADD		= $(CURL_LIBS) \
							  $(top_builddir)/src/common/libcot-utils.la

libcot_client_la_LDFLAGS	= -version-info ${LIBCOT_VERSION}
