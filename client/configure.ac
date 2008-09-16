AC_PREREQ([2.61])
AC_INIT([campagnol],[0.2],[florent.bondoux@resel.fr])
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
	*)
		AC_MSG_ERROR([OS $host_os not supported.])
		;;
esac
AM_CONDITIONAL([HAVE_LINUX], [test "$host_os_name" = "linux"])
AM_CONDITIONAL([HAVE_FREEBSD], [test "$host_os_name" = "freebsd"])
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
AC_CHECK_HEADERS([arpa/inet.h fcntl.h ifaddrs.h netdb.h netinet/in.h string.h sys/ioctl.h sys/socket.h sys/time.h syslog.h unistd.h])
AC_HEADER_ASSERT

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_CHECK_MEMBERS([struct stat.st_rdev])
AC_HEADER_TIME
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([bzero gethostbyname getline inet_ntoa memset select socket strchr strerror strstr strtol])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
