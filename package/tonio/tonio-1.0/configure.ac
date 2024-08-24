AC_INIT([tonio], [1.0], [mcdev@playlinux.net])
AM_PROG_AR
AM_INIT_AUTOMAKE([-Werror -Wall foreign])
LT_INIT
AC_PROG_CC
PKG_PROG_PKG_CONFIG
AC_CHECK_HEADER([iwlib.h], [], AC_MSG_ERROR([cannot find required header iwlib.h]))
PKG_CHECK_MODULES([DEPENDENCIES], [libmicrohttpd >= 0.9.72 libvlc alsa libconfuse libgpiod])
PKG_CHECK_MODULES([CHECK], [check], [AC_MSG_NOTICE([Check found])], [AC_MSG_NOTICE([Check not found])])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
    Makefile
    src/Makefile
    www/Makefile
    tests/Makefile
])
AC_OUTPUT
