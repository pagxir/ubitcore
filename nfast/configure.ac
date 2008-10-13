#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.62)
AC_INIT(btload, 0.4.1)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/bchunk.cc])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stdlib.h string.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([gethostbyname inet_ntoa memmove memset mkdir pow select socket])

AC_OUTPUT([Makefile src/Makefile])
