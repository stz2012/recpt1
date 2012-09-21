#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([recpt1], [1.0.0], yaz@honeyplanet.jp)
AC_CONFIG_SRCDIR([recpt1.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for b25 support.
AC_ARG_ENABLE(b25,
  [AS_HELP_STRING([--enable-b25],[enable b25 support])],
  [AC_CHECK_LIB([arib25], [create_arib_std_b25], , [AC_MSG_WARN(libarib25 is not available.)], [-lpcsclite])]
)

# Checks for libraries.
AC_CHECK_LIB([m], [log10])
AC_CHECK_LIB([pthread], [pthread_kill])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
