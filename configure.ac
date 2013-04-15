AC_PREREQ(2.61)
AC_INIT([tcp-intercept], [0.0.1], [])
AM_INIT_AUTOMAKE([dist-bzip2 -Wall])
LT_INIT

AC_CONFIG_SRCDIR([src/tcp-intercept.cxx])
AC_CONFIG_HEADERS([config.h])
AC_LANG([C++])

# Check for options.
####################
AC_ARG_ENABLE([useless],
	AC_HELP_STRING([--enable-useless],[Enable useless build. When some features are missing, this build becomes useless. Normally ./configure will bail out with an error. This option turns these errors into warnings.]),
	[enable_useless=$enableval],[enable_useless=no])

AC_ARG_ENABLE([ipv6],
	AC_HELP_STRING([--disable-ipv6],[Disable IPv6 support]),
	[enable_ipv6=$enableval],[enable_ipv6=yes])
AS_IF([test x$enable_ipv6 == xyes], [AC_DEFINE([ENABLE_IPV6],[1],[Define to 1 to enable IPv6 support])] )

# Checks for programs.
######################
AC_PROG_CXX
AC_PROG_CXXCPP
AM_PROG_CC_C_O # per product options in Makefile.am

# Checks for libraries.
#######################
AC_CHECK_LIB(ev, ev_run, , [AC_MSG_ERROR([Couldn't find libev])]) dnl '

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
	[AC_DEFINE([SOCKADDR_IN_HAS_LEN_FIELD], [1], [struct sockaddr_in has sin_len field])],
	[],
	[#include <netinet/in.h>])
AS_IF([test x$enable_ipv6 == xyes], [
	AC_CHECK_MEMBER([struct sockaddr_in6.sin6_len],
		[AC_DEFINE([SOCKADDR_IN6_HAS_LEN_FIELD], [1], [struct sockaddr_in6 has sin6_len field])],
		[],
		[#include <netinet/in.h>])
	])

AC_CHECK_DECL([IPPROTO_IP], [AC_DEFINE([HAVE_DECL_IPPROTO_IP], [1], [IPPROTO_IP is defined])],
		AS_IF([test x$enable_useless == xyes],[AC_MSG_WARN([Couldn't find IPPROTO_IP option]);have_useful_build=no],
			[AC_MSG_ERROR([Couldn't find IPPROTO_IP option])]), [
#include <netinet/in.h>])
AC_CHECK_DECL([IP_TRANSPARENT], [AC_DEFINE([HAVE_DECL_IP_TRANSPARENT], [1], [IP_TRANSPARENT is defined])],
		AS_IF([test x$enable_useless == xyes],[AC_MSG_WARN([Couldn't find IP_TRANSPARENT option]);have_useful_build=no],
			[AC_MSG_ERROR([Couldn't find IP_TRANSPARENT option])]), [
#include <netinet/in.h>])

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
${PACKAGE_NAME} version ${PACKAGE_VERSION}

 Prefix:       '${prefix}'
 C Compiler:   '${CC} ${CFLAGS} ${CPPFLAGS}'
 C++ Compiler: '${CXX} ${CXXFLAGS} ${CPPFLAGS}'
 Libraries:    '${LIBS}'

 Configured with:
  IPv6: $enable_ipv6
--------------------------------------------------------------------------------
"

AS_IF([test x$have_useful_build == xno], [
		echo
		echo "WARNING: THIS WILL RESULT IN A USELESS BUILD!!!"
		echo
	])
