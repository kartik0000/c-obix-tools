## Process this file with automake to produce Makefile.in

pkginclude_HEADERS			= obix_client.h

lib_LTLIBRARIES				= libcot_client.la

libcot_client_la_SOURCES	= obix_client.h obix_client.c \
							  obix_comm.h \
							  obix_http.h obix_http.c \
							  table.h table.c \
							  curl_ext.h curl_ext.c

libcot_client_la_CFLAGS		= $(DEPS_CFLAGS) -I$(top_srcdir)/src/common

libcot_client_la_LIBADD		= $(top_builddir)/src/common/libcot_utils.la

libcot_client_la_LDFLAGS	= -version-info ${COT_CLIENT_VERSION}