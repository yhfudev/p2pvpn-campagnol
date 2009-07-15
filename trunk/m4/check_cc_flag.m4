#serial 1

# CHECK_CC_FLAG(FLAG,[ACTION-IF-ACCEPTED],[ACTION-IF-REJECTED])
# -------------------------------------------------------------
# Check whether the flag FLAG is accepted by the C compiler. If the
# flag is accepted, execute ACTION-IF-ACCEPTED. Otherwhise, execute
# ACTION-IF-REJECTED.

AC_DEFUN([CHECK_CC_FLAG], [
  AC_REQUIRE([AC_PROG_CC])

  AC_MSG_CHECKING([whether the C compiler accepts $1])

  AC_LANG_PUSH([C])
      
  old_cflags="$CFLAGS"
  CFLAGS="$CFLAGS $1"

  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([],[])
  ],[
    flag_supported="yes"
  ],[
    flag_supported="no"
  ])

  CFLAGS="$old_cflags"

  AC_LANG_POP

  AC_MSG_RESULT([$flag_supported])

  AS_IF([test "x$flag_supported" = "xyes"],[
    :
    $2
  ],[
    :
    $3
  ])
])

