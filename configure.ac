# This file is part of hildon-input-method
#
# Copyright (C) 2005-2008 Nokia Corporation.
#
# Contact: Mohammad Anwari <Mohammad.Anwari@nokia.com>
#
# Released under the terms of LGPL version 2.1
#

AC_INIT(Makefile.am)
VERSION=2.0.4
LIBVERSION=3
MAJORVERSION=`echo $VERSION|cut -d . -f1`
AC_SUBST(VERSION)
AC_SUBST(LIBVERSION)
AC_SUBST(MAJORVERSION)
AM_INIT_AUTOMAKE(hildon-input-method, $VERSION)
AM_CONFIG_HEADER(config.h)

CFLAGS="$CFLAGS -Werror -Wall -Wmissing-prototypes -Wmissing-declarations -fno-strict-aliasing"

GCCVERSION=`gcc -dumpversion`;
if test x$GCCVERSION = "x3.4.4"
then
	LDFLAGS = "$LDFLAGS -Wl,--as-needed"
fi

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
	AC_SUBST(MAEMO_LAUNCHER_LIBS)
	echo "Enabling maemo launcher"
fi

AM_CONDITIONAL(USE_MAEMO_LAUNCHER, test x$maemo_launcher = xtrue)

PKG_CHECK_MODULES(GTK, gtk+-2.0 >= 2.6.10)
AC_SUBST(GTK_LIBS)
AC_SUBST(GTK_CFLAGS)

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.4.0)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(HILDON, hildon-1 >= 1.0.0)
AC_SUBST(HILDON_LIBS)
AC_SUBST(HILDON_CFLAGS)

PKG_CHECK_MODULES(HILDON_IMF, hildon-input-method-framework-3.0 >= 1.99.16)
AC_SUBST(HILDON_IMF_LIBS)
AC_SUBST(HILDON_IMF_CFLAGS)

PKG_CHECK_MODULES(GCONF, gconf-2.0 >= 2.6.2)
AC_SUBST(GCONF_LIBS)
AC_SUBST(GCONF_CFLAGS)

AC_PATH_PROG(GCONFTOOL, gconftool-2, no)
AC_SUBST(GCONFTOOL)
if test "x$GCONFTOOL" = "xno"; then
	AC_MSG_ERROR([gconftool-2 executable not found in your path - should be installed with GConf])
fi
AC_SUBST(GCONF_SCHEMA_SUBDIRS)
AM_GCONF_SOURCE_2

PKG_CHECK_MODULES(LIBOSSO, libosso >= 0.7.6)
AC_SUBST(LIBOSSO_LIBS)
AC_SUBST(LIBOSSO_CFLAGS)

PKG_CHECK_MODULES(DBUS, dbus-1 >= 0.60)
AC_SUBST(DBUS_LIBS)
AC_SUBST(DBUS_CFLAGS)

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
	debian/libhildon-im-ui3.postinst \
	src/Makefile \
	docs/Makefile])
