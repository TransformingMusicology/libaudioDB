AC_INIT
AM_INIT_AUTOMAKE(mod_audiodb, 0.1)
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CC
AM_PROG_LIBTOOL

AC_PATH_PROG([APXS], [apxs2], [false], [/sbin:/usr/sbin:/usr/local/sbin:/bin:/usr/bin])
if test "$APXS" = false; then
	AC_MSG_ERROR([Cannot find apxs2. Make sure you have apache-devel installed])
fi

AC_MSG_NOTICE([checking for apreq2])
AC_PATH_PROG([APREQ2], [apreq2-config], [false], [/sbin:/usr/bin:/usr/local/sbin:/bin:/usr/bin])
if test "$APREQ2" = false; then
	AC_MSG_ERROR([Cannot find apreq2-config.])
fi

AC_SUBST(APXS)
AC_SUBST(APREQ2)
AC_OUTPUT(Makefile)
