AC_INIT([tonio], [1.0], [mcdev@playlinux.net])
AM_PROG_AR
AM_INIT_AUTOMAKE([-Werror -Wall foreign])
LT_INIT
AC_PROG_CC
PKG_PROG_PKG_CONFIG
AC_CHECK_HEADER([iwlib.h], [], AC_MSG_ERROR([cannot find required header iwlib.h]))
AC_ARG_ENABLE([media],
    [AS_HELP_STRING([--enable-media=BACKEND],
        [select media backend: vlc or opus (default: opus)])],
    [],
    [enable_media=opus])

AS_IF([test "x$enable_media" = xvlc], [
    PKG_CHECK_MODULES([DEPENDENCIES], [libmicrohttpd >= 0.9.72 libvlc alsa libconfuse libgpiod >= 2])
    AC_DEFINE([MEDIA_BACKEND_VLC], [1], [Use VLC media backend])
], [test "x$enable_media" = xopus], [
    PKG_CHECK_MODULES([DEPENDENCIES], [libmicrohttpd >= 0.9.72 opusfile alsa libconfuse libgpiod >= 2])
    AC_DEFINE([MEDIA_BACKEND_OPUS], [1], [Use Opusfile media backend])
], [
    AC_MSG_ERROR([Unknown media backend: $enable_media. Supported backends: vlc, opus])
])
AM_CONDITIONAL([MEDIA_VLC], [test "x$enable_media" = xvlc])
AM_CONDITIONAL([MEDIA_OPUS], [test "x$enable_media" = xopus])
PKG_CHECK_MODULES([CHECK], [check], [AC_MSG_NOTICE([Check found])], [AC_MSG_NOTICE([Check not found])])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
    Makefile
    src/Makefile
    www/Makefile
    tests/Makefile
])
AC_OUTPUT
