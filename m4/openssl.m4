# serial 1

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
        PKG_CHECK_MODULES([OPENSSL],[openssl >= 0.9.8g])
])
])
