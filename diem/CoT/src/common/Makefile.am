## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES				= libcot-utils.la

pkginclude_HEADERS			= obix_utils.h xml_config.h bool.h \
							  ptask.h ixml_ext.h lwl_ext.h

libcot_utils_la_SOURCES		= xml_config.h xml_config.c \
							  ixml_ext.h ixml_ext.c \
							  obix_utils.h obix_utils.c \
							  ptask.h ptask.c \
							  lwl_ext.h lwl_ext.c \
							  bool.h

libcot_utils_la_CFLAGS		= $(WARN_FLAGS) $(UPNP_CFLAGS)

libcot_utils_la_LIBADD		= $(UPNP_LIBS) $(LWL_LIB) $(RT_LIB) 

