AC_PREREQ([2.61])
AC_INIT([campagnol],[0.3.4],[florent.bondoux@resel.fr])
AM_INIT_AUTOMAKE([-Wall foreign dist-bzip2])
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR([client/campagnol.c])
AC_CONFIG_HEADERS([config.h])

AC_ARG_ENABLE([warnings],[AS_HELP_STRING([--enable-warnings],[enable additional warnings during the compilation])],
[add_warnings=$enableval],[add_warnings=no]
)
AC_ARG_ENABLE([client],[AS_HELP_STRING([--disable-client],[do not build the client])],
[build_client=$enableval],[build_client=yes]
)
AC_ARG_ENABLE([server],[AS_HELP_STRING([--disable-server],[do not build the RDV server])],
[build_server=$enableval],[build_server=yes]
)

AM_CONDITIONAL([BUILD_CLIENT], [test "x$build_client" != "xno"])
AM_CONDITIONAL([BUILD_SERVER], [test "x$build_server" != "xno"])

AS_IF([test "x$build_client" = "xno" -a "x$build_server" = "xno"],[
  AC_MSG_ERROR([Both the client and the server are disabled. Please enable at least one of them.])
])


AC_USE_SYSTEM_EXTENSIONS

case "$CC" in
	*icc*)
		COMMON_CFLAGS="-Wcheck -wd810"
		EXTRA_CFLAGS="-Wall -wd981"
		;;
	*)
		COMMON_CFLAGS="-Wall -Wextra -Wstrict-prototypes"
		EXTRA_CFLAGS="-Wshadow -Wformat=2 -Wmissing-prototypes -Wmissing-declarations -Wmissing-format-attribute -Wwrite-strings -Wcast-qual -Wpointer-arith -fdiagnostics-show-option"
		;;
esac

AS_IF(
  [test "x$add_warnings" = "xyes"],
  [
    COMMON_CFLAGS="$COMMON_CFLAGS $EXTRA_CFLAGS"
  ]
)

#Filter CFLAGS
SAFE_CFLAGS=""
for flag in $COMMON_CFLAGS; do
  CHECK_CC_FLAG([$flag],[SAFE_CFLAGS="$SAFE_CFLAGS $flag"],[])
done
COMMON_CFLAGS="$SAFE_CFLAGS"
AC_SUBST(COMMON_CFLAGS)

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
	*cygwin*)
		AC_DEFINE([HAVE_CYGWIN], [1], [Cygwin])
		host_os_name=cygwin
		;;
	*netbsd*)
		AC_DEFINE([HAVE_NETBSD], [1], [NetBSD])
		host_os_name=netbsd
		;;
	*)
		AC_MSG_ERROR([OS $host_os not supported.])
		;;
esac
AM_CONDITIONAL([HAVE_LINUX], [test "$host_os_name" = "linux"])
AM_CONDITIONAL([HAVE_FREEBSD], [test "$host_os_name" = "freebsd"])
AM_CONDITIONAL([HAVE_OPENBSD], [test "$host_os_name" = "openbsd"])
AM_CONDITIONAL([HAVE_CYGWIN], [test "$host_os_name" = "cygwin"])
AM_CONDITIONAL([HAVE_NETBSD], [test "$host_os_name" = "netbsd"])
AC_CACHE_SAVE

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_PROG_RANLIB
AC_CACHE_SAVE

# Backup LIBS
OLD_LIBS=$LIBS

# If we build the client:
AS_IF([test "x$build_client" != "xno"],[
  # Checks for libraries.
  AC_SEARCH_LIBS([pthread_create], [pthread],
    [test "$ac_res" = "none required" || CLIENT_LIBS="$ac_res $CLIENT_LIBS"],
    [AC_MSG_ERROR([Requires pthreads])]
  )
  OPENSSL_CHECK([0.9.8m], [0x009080df])
  # later: search into libposix4 for Solaris support
  AC_SEARCH_LIBS([clock_gettime], [rt], 
    [test "$ac_res" = "none required" || CLIENT_LIBS="$ac_res $CLIENT_LIBS"],
    [AC_MSG_ERROR([Requires clock_gettime])]
  )
  AC_CACHE_SAVE

  # Checks for header files.
  AC_CHECK_HEADERS([ifaddrs.h])

  AC_SUBST(CLIENT_LIBS)
  LIBS=$OLD_LIBS
])

RDV_LIBS=""
AC_SUBST(RDV_LIBS)


AC_HEADER_ASSERT

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for particular functions
AC_FUNC_STRERROR_R
AC_CHECK_FUNCS([mlockall])

# Checks for library functions.
# tdestroy is missing on FreeBSD and OpenBSD
AC_CONFIG_LIBOBJ_DIR([lib])
AC_REPLACE_FUNCS([tdestroy])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
