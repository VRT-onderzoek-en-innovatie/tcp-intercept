AC_PREREQ(2.61)
AC_INIT([tcp-intercept], [0.0.1], [])
AM_INIT_AUTOMAKE([dist-bzip2 -Wall])
LT_INIT

AC_CONFIG_SRCDIR([src/tcp-intercept.cxx])
AC_CONFIG_HEADERS([config.h])
AC_LANG([C++])

# Check for options.
####################
AC_ARG_ENABLE([ipv6],
	AC_HELP_STRING([--disable-ipv6],[Disable IPv6 support]),
	[enable_ipv6=$enableval],[enable_ipv6=yes])
AS_IF([test x$enable_ipv6 == xyes], [AC_DEFINE([ENABLE_IPV6],[1],[Define to 1 to enable IPv6 support])] )

# Checks for programs.
######################
AC_PROG_CXX
AM_PROG_CC_C_O # per product options in Makefile.am

# Checks for libraries.
#######################

# Checks for header files.
##########################
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h netdb.h netinet/in.h string.h strings.h sys/socket.h unistd.h])
AC_CHECK_HEADER([boost/ptr_container/ptr_vector.hpp], [], [AC_MSG_ERROR([Couldn't find boost library])], []) dnl '


# Checks for typedefs, structures, and compiler characteristics.
################################################################
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Check for the sin_len field in sockaddr_in
AC_CHECK_MEMBER([struct sockaddr_in.sin_len],
	[AC_DEFINE([SOCKADDR_IN_HAS_LEN_FIELD], [], [struct sockaddr_in has sin_len field])],
	[],
	[#include <netinet/in.h>])
AS_IF([test x$enable_ipv6 == xyes], [
	AC_CHECK_MEMBER([struct sockaddr_in6.sin6_len],
		[AC_DEFINE([SOCKADDR_IN6_HAS_LEN_FIELD], [], [struct sockaddr_in6 has sin6_len field])],
		[],
		[#include <netinet/in.h>])
	])

AC_CHECK_DECL([IPPROTO_IP], [], [AC_MSG_ERROR([Couldn't find IPPROTO_IP option])], [
#include <netinet/in.h>]) dnl '
AC_CHECK_DECL([IP_TRANSPARENT], [], [AC_MSG_ERROR([Couldn't find IP_TRANSPARENT option])], [
#include <netinet/in.h>]) dnl '

# Checks for library functions.
###############################
AC_CHECK_FUNCS([bzero socket strerror])


# Output
########
AC_CONFIG_FILES([
	Makefile
	Socket/Makefile
	src/Makefile
	])
AC_OUTPUT

echo "
--------------------------------------------------------------------------------
${PACKAGE_NAME} Version ${PACKAGE_VERSION}

 Prefix: '${prefix}'
 Compiler: '${CC} ${CFLAGS} ${CPPFLAGS}'
 Libraries: '${LIBS}'

 Configured with:
  IPv6: $enable_ipv6
--------------------------------------------------------------------------------
"
