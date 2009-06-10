#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(cot, 0.0.0, Litvinov.Andrey@gmail.com)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

# oBIX Client API version
COT_CLIENT_VERSION="0:0:0"
AC_SUBST(COT_CLIENT_VERSION)

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_CC_C_O
AC_PROG_LIBTOOL

AC_PROG_TRY_DOXYGEN

# Check for doxygen program.
if test -z "$DOXYGEN"; then
  echo "-----------------------------------------"
  echo " No Doxygen program found - continuing"
  echo " without Doxygen documentation support."
  echo "-----------------------------------------"
fi
AM_CONDITIONAL([HAVE_DOXYGEN], [test "x$DOXYGEN" != x])

# Checks for libraries.
PKG_CHECK_MODULES(UPNP, libupnp >= 1.4.3)
PKG_CHECK_MODULES(CURL, libcurl)

# AC_SEARCH_LIBS stores found linking flags to the LIBS variable which is added 
# by default to all Makefiles. But we want to store that flags in separate
# variables and also keep the original contents of LIBS. That's why we are doing
# such a trick.
LIBS_BACKUP=$LIBS
LIBS=""
AC_SEARCH_LIBS(
	[clock_gettime], 
	[rt posix4],,
	AC_MSG_ERROR([Unable to find library with clock_gettime definition\
	(normally it is rt). Sorry but the project will not compile.]),
	$LIBS_BACKUP)
RT_LIB=$LIBS
LIBS=""	
AC_SEARCH_LIBS(
	[FCGX_Init], 
	[fcgi],, 
	AC_MSG_ERROR([Unable to link to libfcgi. Please install Fast CGI library\
	first (fastcgi.com).]), 
	[ -lm $LIBS_BACKUP])
FCGI_LIB=$LIBS
LIBS=""
AC_SEARCH_LIBS(
	[lwl_alloc], 
	[lwl],, 
	AC_MSG_ERROR([Unable to link to liblwl. Please install LWL library first\
	(http://home.gna.org/lwl/).]),
	$LIBS_BACKUP)
LWL_LIB=$LIBS
LIBS=$LIBS_BACKUP 	
			
AC_SUBST(UPNP_CFLAGS)
AC_SUBST(UPNP_LIBS)
AC_SUBST(CURL_CFLAGS)
AC_SUBST(CURL_LIBS)
AC_SUBST(RT_LIB)
AC_SUBST(FCGI_LIB)
AC_SUBST(LWL_LIB)

# Checks for header files.
AC_FUNC_ALLOCA
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h limits.h netdb.h netinet/in.h stdlib.h string.h \
				  sys/socket.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_HEADER_STDBOOL
AC_C_RESTRICT
AC_TYPE_SIZE_T
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRFTIME
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([clock_gettime gethostbyname gethostname inet_ntoa localtime_r \
				strchr strdup strstr strtol])

AC_CONFIG_FILES([Makefile                 
                 src/common/Makefile
                 src/client/Makefile
                 src/adapters/Makefile
                 src/server/Makefile
                 src/test/Makefile
                 res/Makefile
                 doc/Makefile
                 doc/doxygen/Makefile
                 doc/doxygen/libcot.doxyfile])
AC_OUTPUT

echo "
  C oBIX Tools ($PACKAGE_NAME) version $PACKAGE_VERSION
  Prefix.........: $prefix    
  Doxygen........: ${DOXYGEN:-NONE}
"
