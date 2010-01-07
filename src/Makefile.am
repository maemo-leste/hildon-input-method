# This file is part of hildon-input-method
#
# Copyright (C) 2005-2007 Nokia Corporation.
#
# Contact: Mohammad Anwari <Mohammad.Anwari.com>

INCLUDES = \
	$(GTK_CFLAGS) \
	$(HILDON_CFLAGS) \
	$(HILDON_IMF_CFLAGS)\
	$(GCONF_CFLAGS)\
	$(LIBOSSO_CFLAGS) \
	$(ESD_CFLAGS) \
	$(LIBXML_CFLAGS) \
	$(HILDON_LGPL_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(MCE_CFLAGS) \
	$(MAEMO_LAUNCHER_CFLAGS) \
	-DLOCALEDIR=\"$(localedir)\"

uilibdir = $(libdir)
uilib_LTLIBRARIES = libhildon-im-ui.la

HIM_SRCS = hildon-im-main.c
HIM_LIBS = $(GTK_LIBS) $(GCONF_LIBS) $(ESD_LIBS) \
        $(HILDON_LIBS) $(LIBOSSO_LIBS) \
        $(LIBXML_LIBS)  \
        libhildon-im-ui.la

bin_PROGRAMS = hildon-input-method hildon-im-recache
hildon_input_method_SOURCES = $(HIM_SRCS)
hildon_input_method_LDADD = $(HIM_LIBS) $(MAEMO_LAUNCHER_LIBS)
hildon_input_method_LDFLAGS = $(MAEMO_LAUNCHER_LDFLAGS)

hildon_im_recache_SOURCES = hildon-im-recache.c
hildon_im_recache_LDADD = cache.lo $(GLIB_LIBS) -ldl
hildon_im_recache_LDFLAGS = -rdynamic

libhildon_im_ui_la_SOURCES = \
	hildon-im-ui.h \
	hildon-im-ui.c \
	hildon-im-plugin.h \
	hildon-im-plugin.c \
	hildon-im-widget-loader.c \
	hildon-im-widget-loader.h \
  hildon-im-languages.c \
  hildon-im-languages.h cache.c cache.h \
	hildon-im-settings-plugin.c internal.h
libhildon_im_ui_la_LIBADD = \
	$(GTK_LIBS) $(GCONF_LIBS) $(ESD_LIBS) $(HILDON_LIBS) \
	$(LIBOSSO_LIBS) $(HILDON_IMF_LIBS)\
	$(LIBIMLAYOUTS_LIBS) $(DBUS_LIBS)
libhildon_im_ui_la_LDFLAGS = -version-info $(LIBVERSION)

hildon_input_methodincludeinstdir = $(includedir)/hildon-input-method
hildon_input_methodincludeinst_DATA = \
	hildon-im-ui.h \
	hildon-im-plugin.h \
	hildon-im-widget-loader.h \
	hildon-im-widget-loader.h \
	hildon-im-languages.h \
	hildon-im-settings-plugin.h

