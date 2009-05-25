bin_PROGRAMS = obix.fcgi

obix_fcgi_SOURCES = obix_fcgi.h obix_fcgi.c \
                    server.h server.c \
                    xml_storage.h xml_storage.c \
                    watch.h watch.c \
                    response.h response.c \
                    post_handler.h post_handler.c
                    
obix_fcgi_CFLAGS  = $(DEPS_CFLAGS) -I$(top_srcdir)/src/common 

obix_fcgi_LDADD   = $(DEPS_LIBS) $(top_builddir)/src/common/libcot_utils.la