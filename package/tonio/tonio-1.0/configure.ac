AC_INIT([tonio], [1.0], [mcdev@playlinux.net])
AM_INIT_AUTOMAKE([-Werror -Wall foreign])
AC_PROG_CC
PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([DEPENDENCIES], [libmicrohttpd libvlc alsa libconfuse libgpiod])
PKG_CHECK_MODULES([CHECK], [check])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
    Makefile
    src/Makefile
    www/Makefile
    tests/Makefile
])
AC_OUTPUT
