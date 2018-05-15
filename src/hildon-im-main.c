/*
 * This file is part of hildon-input-method
 * 
 * Copyright (C) 2005-2007 Nokia Corporation. 
 *
 * Contact: Mohammad Anwari <Mohammad.Anwari@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */


#include <gtk/gtk.h>
#include <glib/gthread.h>
#include "hildon-im-ui.h"
#include "internal.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "osso-log.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <locale.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <dbus/dbus.h>
#include <hildon/hildon.h>

#define LOCALE_IFACE "com.nokia.LocaleChangeNotification"
#define LOCALE_SIGNAL "locale_changed"
#define LOCALE_RULE "type='signal',interface='" LOCALE_IFACE "',member='" LOCALE_SIGNAL "'"

GtkWidget *keyboard = NULL;

static void
handle_sigterm(gint t)
{
  gtk_main_quit();
}

static DBusHandlerResult
dbus_msg_handler (DBusConnection *connection,
                  DBusMessage *msg,
                  void *user_data)
{
  const char *interface = dbus_message_get_interface(msg);
  const char *method = dbus_message_get_member(msg);
  const char *sender = dbus_message_get_sender(msg);
  gint message_type = -1;
    
  if (!(message_type = dbus_message_get_type(msg)) ||
      !sender || !interface || !method)
    return FALSE;
  
  if (message_type == DBUS_MESSAGE_TYPE_SIGNAL)
  {
    if (strcmp(interface, LOCALE_IFACE) == 0 &&
        strcmp(method, LOCALE_SIGNAL) == 0)
    {
      char *new_locale = NULL;

      dbus_message_get_args(msg, NULL,
			    DBUS_TYPE_STRING, &new_locale,
			    DBUS_TYPE_INVALID);
      if (new_locale)
      {
        /* Note: This will only affect UI elements created
           from here on. Existing UI will not be localized. */
        setlocale(LC_ALL, new_locale);
      }
    }
  }
  
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

int
main(int argc, char *argv[])
{
  struct sigaction sv;
  DBusConnection *dbus_connection = NULL;
  DBusError dbus_error_code;
#if !GLIB_CHECK_VERSION(2,32,0)
  if (!g_thread_supported ()) g_thread_init (NULL);
#endif
  hildon_gtk_init(&argc, &argv);
  /* TODO call hildon_init() here
   * or replace both calls with hildon_gtk_init() */

  gconf_init(argc, argv, NULL);

  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  keyboard = hildon_im_ui_new();

  gtk_window_set_accept_focus(GTK_WINDOW(keyboard), FALSE);

  g_signal_connect(keyboard, "destroy", gtk_main_quit, NULL);

  dbus_error_init(&dbus_error_code);
  /* Establish DBus connection */
  if (!(dbus_connection = dbus_bus_get(DBUS_BUS_SYSTEM, &dbus_error_code)))
  {
    g_warning("DBUS connection failed: %s\n", dbus_error_code.message);
    dbus_error_free (&dbus_error_code);
  }

  if (dbus_connection)
  {
    dbus_bus_add_match(dbus_connection, LOCALE_RULE, &dbus_error_code);
    if (dbus_error_is_set(&dbus_error_code))
    {
      g_warning("Unable to add match for locale change: %s\n",
                dbus_error_code.message);
      dbus_error_free(&dbus_error_code);
    }
  }
  
  if (!dbus_connection_add_filter(dbus_connection,
                                  dbus_msg_handler,
                                  NULL, NULL))
  {
    g_warning("Failed to add filter: %s\n", dbus_error_code.message);
    dbus_error_free(&dbus_error_code);
  }

  /* gtk_main_quit at SIGTERM */
  sigemptyset(&sv.sa_mask);
  sv.sa_flags = 0;
  sv.sa_handler = handle_sigterm;
  sigaction(SIGTERM, &sv, NULL);

  /* ignore SIGHUP */
  signal(SIGHUP, SIG_IGN);

  gtk_main();

  return 0;
}
