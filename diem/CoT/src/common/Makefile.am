## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES				= libcot-utils.la

pkginclude_HEADERS			= obix_utils.h xml_config.h bool.h \
							  ptask.h ixml_ext.h log_utils.h

libcot_utils_la_SOURCES		= xml_config.h xml_config.c \
							  ixml_ext.h ixml_ext.c \
							  obix_utils.h obix_utils.c \
							  ptask.h ptask.c \
							  log_utils.h log_utils.c \
							  table.h sorted_table.c \
							  bool.h
							  
EXTRA_DIST 					= table.c							  

libcot_utils_la_CFLAGS		= $(WARN_FLAGS) $(UPNP_CFLAGS)

libcot_utils_la_LIBADD		= $(UPNP_LIBS) $(RT_LIB) 

libcot_utils_la_LDFLAGS	= -version-info ${LIBCOT_VERSION}
