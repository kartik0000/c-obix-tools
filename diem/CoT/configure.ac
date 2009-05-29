#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(cot, 1.0.0, Litvinov.Andrey@gmail.com)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

# LWL version
LWL_LT_VERSION="1:1:0"
AC_SUBST(LWL_LT_VERSION)
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

# Checks for libraries.
PKG_CHECK_MODULES(DEPS, libupnp >= 1.4.3 libcurl)
AC_SEARCH_LIBS(
	[FCGX_Init], 
	[fcgi],, 
	AC_MSG_ERROR([Unable to link to libfcgi. Please install Fast CGI library first.]), 
	[ -lm])
AC_SEARCH_LIBS(
	[lwl_alloc], 
	[lwl],, 
	AC_MSG_ERROR([Unable to link to liblwl. Please install LWL library first.]))
			
AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)

# Checks for header files.
AC_FUNC_ALLOCA
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h limits.h netdb.h netinet/in.h stdlib.h string.h sys/socket.h syslog.h unistd.h])

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
AC_CHECK_FUNCS([clock_gettime gethostbyname gethostname inet_ntoa localtime_r strchr strdup strstr strtol])

AC_CONFIG_FILES([Makefile                 
                 src/common/Makefile
                 src/client/Makefile
                 src/adapters/Makefile
                 src/server/Makefile
                 src/test/Makefile])
AC_OUTPUT
