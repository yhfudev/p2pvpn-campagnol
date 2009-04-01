# serial 2

# OPENSSL_CHECK(TEXT_VER, NUM_VER)
# --------------------------------
# Check for a minimum version for OpenSSL
# The first argument is the textual version number (e.g. 0.9.8g)
# The second argument is the numerical number (eg 0x0090807f)
#
# The macro defines the --with-openssl, --with-openssl-include and
# --with-openssl-lib parameters to define the base directories of OpenSSL. If
# theses arguments are not given, the macro uses pkg-config.
#
# The variables OPENSSL_CFLAGS and OPENSSL_LIBS are set by the macro
# 
# When pkg-config is used, the required version number is TEXT_VER.
# The version number in openssl/opensslv.h is checked agains NUM_VER.
# Check whether SSL_library_init is available, abort if not
# Check whether CRYPTO_THREADID_current is available (OpenSSL >= 0.9.9).
AC_DEFUN([OPENSSL_CHECK],
[
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
AS_IF([test "x$openssl_set" = "x1"],[
        AC_CHECK_FUNC([dlopen],[],[AC_SEARCH_LIBS([dlopen], [dl], [OPENSSL_LIBS+=" -ldl"])])
        AC_SUBST(OPENSSL_CFLAGS)
        AC_SUBST(OPENSSL_LIBS)
],[
        PKG_CHECK_MODULES([OPENSSL],[openssl >= $1])
])

AC_MSG_NOTICE([OpenSSL CFLAGS: $OPENSSL_CFLAGS])
AC_MSG_NOTICE([OpenSSL LIBS: $OPENSSL_LIBS])

OLD_LIBS=$LIBS
OLD_CFLAGS=$CFLAGS
LIBS="$LIBS $OPENSSL_LIBS"
CFLAGS="$CFLAGS $OPENSSL_CFLAGS"

# version checking
AC_MSG_CHECKING([whether OpenSSL version number is >= $1])
AC_COMPILE_IFELSE(
[AC_LANG_SOURCE([#include <openssl/opensslv.h>
#if OPENSSL_VERSION_NUMBER < $2L
#error
#endif
])],
[AC_MSG_RESULT([yes])],
[AC_MSG_RESULT([no])
 AC_MSG_ERROR([OpenSSL version must be at least $1])
]
)

# usability checking
AC_CHECK_FUNCS([SSL_library_init], [], [AC_MSG_ERROR([OpenSSL is not usable])])

# check for the new THREADID API
AC_CHECK_FUNCS([CRYPTO_THREADID_current])

LIBS=$OLD_LIBS
CFLAGS=$OLD_CFLAGS
])
