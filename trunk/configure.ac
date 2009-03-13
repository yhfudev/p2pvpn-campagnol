AC_PREREQ([2.61])
AC_INIT([campagnol],[0.3.2],[florent.bondoux@resel.fr])
AM_INIT_AUTOMAKE([-Wall foreign])
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR([client/campagnol.c])
AC_CONFIG_HEADERS([config.h])

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
  OPENSSL_CHECK
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

# Checks for library functions.
# getline and tdestroy are missing on FreeBSD and OpenBSD
AC_CONFIG_LIBOBJ_DIR([lib])
AS_IF([test "x$build_client" != "xno"],[
  LIBS=$CLIENT_LIBS
  AC_REPLACE_FUNCS([getline tdestroy])
  LIBS=$OLD_LIBS
])

AC_CONFIG_FILES([Makefile common/Makefile client/Makefile rdvserver/Makefile man/Makefile samples/Makefile])
AC_OUTPUT
