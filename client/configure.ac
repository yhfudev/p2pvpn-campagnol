AC_PREREQ([2.61])
AC_INIT([campagnol],[0.3.1],[florent.bondoux@resel.fr])
AM_INIT_AUTOMAKE([-Wall foreign])
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR([src/campagnol.c])
AC_CONFIG_HEADERS([config.h])

AC_GNU_SOURCE

# Checks host type
AC_CANONICAL_HOST
case $host_os in
	*linux*)
		AC_DEFINE([HAVE_LINUX], [1], [Linux])
		host_os_name=linux
		;;
	*freebsd*)
		AC_DEFINE([HAVE_FREEBSD], [1], [FreeBSD])
		host_os_name=freebsd
		;;
	*openbsd*)
		AC_DEFINE([HAVE_OPENBSD], [1], [OpenBSD])
		host_os_name=openbsd
		;;
	*)
		AC_MSG_ERROR([OS $host_os not supported.])
		;;
esac
AM_CONDITIONAL([HAVE_LINUX], [test "$host_os_name" = "linux"])
AM_CONDITIONAL([HAVE_FREEBSD], [test "$host_os_name" = "freebsd"])
AM_CONDITIONAL([HAVE_OPENBSD], [test "$host_os_name" = "openbsd"])
AC_CACHE_SAVE

# Checks for programs.
AC_PROG_CC
AC_CACHE_SAVE

# Checks for libraries.
AC_SEARCH_LIBS([pthread_create], [pthread], [], [AC_MSG_ERROR([Requires pthreads])])
OPENSSL_CHECK
# later: search into libposix4 for Solaris support
AC_SEARCH_LIBS([clock_gettime], [rt], [], [AC_MSG_ERROR([Requires clock_gettime])])
AC_CACHE_SAVE

# Checks for header files.
AC_CHECK_HEADERS([ifaddrs.h])
AC_HEADER_ASSERT

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
# getline and tdestroy are missing on FreeBSD and OpenBSD
# sem_timedwait is missing on OpenBSD
AC_REPLACE_FUNCS([getline tdestroy])
AC_CHECK_FUNCS([sem_timedwait])

AC_CONFIG_FILES([Makefile src/Makefile man/Makefile samples/Makefile])
AC_OUTPUT
