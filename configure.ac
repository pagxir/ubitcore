#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.62)
AC_INIT(kadmin, 0.0.1)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/main.cc])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_RANLIB
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h])
AC_CHECK_HEADERS([stdlib.h string.h sys/socket.h])
AC_CHECK_HEADERS([endian.h machine/endian.h])
AC_CHECK_HEADERS([windows.h])
AC_CHECK_HEADERS([winsock.h], WS2_LDADD="-lws2_32";
                 WS2_INCLUDE="/include/win32")
AC_SUBST(WS2_INCLUDE)
AC_SUBST(WS2_LDADD)

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([gethostbyname inet_ntoa memmove memset mkdir pow select socket])

AC_OUTPUT([Makefile test/Makefile src/Makefile src/btdht/Makefile])
