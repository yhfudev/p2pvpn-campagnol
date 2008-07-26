AC_INIT([campagnol],[0.2],[florent.bondoux@resel.fr])
AM_INIT_AUTOMAKE([-Wall foreign])
AC_CONFIG_SRCDIR([src/campagnol.c])
AC_CONFIG_HEADERS([config.h])

AC_GNU_SOURCE

# Checks host type
AC_CANONICAL_HOST
case $host_os in
	*linux*)
		TUNFILE="src/tun_device_linux.c"
		AC_DEFINE([HAVE_LINUX], [1], [Linux])
		;;
	*freebsd*)
		TUNFILE="src/tun_device_freebsd.c"
		AC_DEFINE([HAVE_FREEBSD], [1], [FreeBSD])
		;;
	*)
		AC_MSG_ERROR("OS $host_os not supported.")
		;;
esac
AC_CONFIG_LINKS([src/tun_device.c:$TUNFILE])
AC_CACHE_SAVE

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_CACHE_SAVE

# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_create])
openssl_set=0
AC_ARG_WITH([openssl], [AS_HELP_STRING([--with-openssl=DIR],[OpenSSL base directory. This option is overwritten by --with-openssl-include and --with-openssl-lib. If neither of these options is given, the script will configure openssl with pkg-config.])],
            [
	     openssl_set=1
	     OPENSSL_CFLAGS="-I$withval/include"
	     OPENSSL_LIBS="-L$withval/lib -lssl -lcrypto"
	    ]
	   )
AC_ARG_WITH([openssl-include], [AS_HELP_STRING([--with-openssl-include=DIR],[OpenSSL headers directory without the trailing '/openssl'])],
            [
	     openssl_set=1
	     OPENSSL_CFLAGS="-I$withval"
	    ]
	   )
AC_ARG_WITH([openssl-libs], [AS_HELP_STRING([--with-openssl-libs=DIR],[OpenSSL libraries directory])],
            [
	     openssl_set=1
	     OPENSSL_LIBS="-L$withval -lssl -lcrypto"
	    ]
	   )
if test $openssl_set == 1; then
	AC_SEARCH_LIBS([dlopen], [ld])
	AC_SUBST(OPENSSL_CFLAGS)
	AC_SUBST(OPENSSL_LIBS)
else
	PKG_CHECK_MODULES([OPENSSL],[openssl >= 0.9.8g])
fi


AC_SEARCH_LIBS([clock_gettime], [rt])
AC_CACHE_SAVE

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h syslog.h unistd.h])
AC_CHECK_HEADERS([ifaddrs.h])
AC_HEADER_ASSERT

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_SIZE_T
AC_CHECK_MEMBERS([struct stat.st_rdev])
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([bzero gethostbyname inet_ntoa memset select socket strchr strerror strstr strtol])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
