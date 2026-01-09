# This file is part of hildon-input-method
#
# Copyright (C) 2005-2008 Nokia Corporation.
#
# Contact: Mohammad Anwari <Mohammad.Anwari@nokia.com>
#
# Released under the terms of LGPL version 2.1
#

AC_INIT([hildon-input-method], [2.0.4])
AC_CONFIG_SRCDIR([Makefile.am])
AM_INIT_AUTOMAKE

LIBVERSION=3
AC_SUBST(LIBVERSION)

AM_CONFIG_HEADER(config.h)

CFLAGS="$CFLAGS -Wall -Wmissing-prototypes -Wmissing-declarations -fno-strict-aliasing"

AC_CANONICAL_HOST

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LIBTOOL

PKG_PROG_PKG_CONFIG

AC_HEADER_STDC

# check for gtk-doc
GTK_DOC_CHECK(1.9)

AC_ARG_ENABLE([maemo-launcher],
	[AS_HELP_STRING([--enable-maemo-launcher],
		[build with maemo-launcher support])],
		[case "${enableval}" in
			yes) maemo_launcher=true ;;
			no)  maemo_launcher=false ;;
			*) AC_MSG_ERROR([bad value ${enableval} for --enable-maemo-launcher]) ;;
		esac], [maemo_launcher=false])

if test x$maemo_launcher = xtrue
then
	PKG_CHECK_MODULES(MAEMO_LAUNCHER, [maemo-launcher-app])
	AC_SUBST(MAEMO_LAUNCHER_CFLAGS)
	MAEMO_LAUNCHER_LIBS="-Xcompiler \"$MAEMO_LAUNCHER_LIBS\""
	AC_SUBST(MAEMO_LAUNCHER_LIBS)
	echo "Enabling maemo launcher"
fi

AM_CONDITIONAL(USE_MAEMO_LAUNCHER, test x$maemo_launcher = xtrue)

PKG_CHECK_MODULES(GTK, gtk+-2.0 >= 2.14.7)
AC_SUBST(GTK_LIBS)
AC_SUBST(GTK_CFLAGS)

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.4.0 gmodule-2.0)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(HILDON, hildon-1 >= 1.0.0)
AC_SUBST(HILDON_LIBS)
AC_SUBST(HILDON_CFLAGS)

PKG_CHECK_MODULES(HILDON_IMF, hildon-input-method-framework-3.0 >= 2.1.44)
AC_SUBST(HILDON_IMF_LIBS)
AC_SUBST(HILDON_IMF_CFLAGS)

PKG_CHECK_MODULES(GCONF, gconf-2.0 >= 2.6.2)
AC_SUBST(GCONF_LIBS)
AC_SUBST(GCONF_CFLAGS)

AM_GCONF_SOURCE_2

PKG_CHECK_MODULES(LIBOSSO, libosso >= 0.7.6)
AC_SUBST(LIBOSSO_LIBS)
AC_SUBST(LIBOSSO_CFLAGS)

PKG_CHECK_MODULES(DBUS, dbus-1 >= 0.60)
AC_SUBST(DBUS_LIBS)
AC_SUBST(DBUS_CFLAGS)

PKG_CHECK_MODULES(ATSPI2, atspi-2)
AC_SUBST(ATSPI2_LIBS)
AC_SUBST(ATSPI2_CFLAGS)

PKG_CHECK_MODULES(X11, x11)
AC_SUBST(X11_LIBS)
AC_SUBST(X11_CFLAGS)

PKG_CHECK_MODULES(MCE, mce)
AC_SUBST(MCE_LIBS)
AC_SUBST(MCE_CFLAGS)

PKG_CHECK_MODULES(XTST, xtst)
AC_SUBST(XTST_LIBS)

localedir=${datadir}/locale

etcdir=$sysconfdir
echo "Etcdir=$etcdir"
echo 'comp=${prefix}/etc'
if test "$etcdir" = '${prefix}/etc'; then
	etcdir=$prefix"/etc"
	echo "** Modified **"
	echo $etcdir
else
	echo "** Unmodified **"
	echo $etcdir
fi

im_widget_dir="hildon-input-method-widgets"
im_plugin_dir="hildon-input-method"
AH_TEMPLATE([PREFIX], [Installation path])
AC_DEFINE_UNQUOTED([PREFIX], ["$prefix"])
AH_TEMPLATE([IM_WIDGET_DIR], [Widget installation path])
AC_DEFINE_UNQUOTED([IM_WIDGET_DIR], ["$im_widget_dir"])
AH_TEMPLATE([IM_PLUGIN_DIR], [Plugins installation path])
AC_DEFINE_UNQUOTED([IM_PLUGIN_DIR], ["$im_plugin_dir"])



AC_SUBST(etcdir)
AC_SUBST(initdir)
AC_SUBST(localedir)
AC_SUBST(im_widget_dir)
AC_SUBST(im_plugin_dir)

ALL_LINGUAS="en_GB"
AC_SUBST(ALL_LINGUAS)
AM_GLIB_GNU_GETTEXT

AC_OUTPUT([Makefile \
	hildon-input-method-ui-3.0.pc \
	src/Makefile \
	docs/Makefile])
