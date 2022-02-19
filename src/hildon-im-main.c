/*
 * This file is part of hildon-input-method
 * 
 * Copyright (C) 2005-2007 Nokia Corporation. 
 * Copyright (C) 2020-2022 Carl Klemm.
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
#include <atspi/atspi.h>
#include <dbus/dbus.h>
#include <hildon/hildon.h>
#include <stdbool.h>
#include <mce/dbus-names.h>

#define LOCALE_IFACE "com.nokia.LocaleChangeNotification"
#define LOCALE_SIGNAL "locale_changed"
#define LOCALE_RULE "type='signal',interface='" LOCALE_IFACE "',member='" LOCALE_SIGNAL "'"

#define MCE_KBD_RULE "type='signal',interface='" MCE_SIGNAL_IF "',member='" MCE_KEYBOARD_SLIDE_GET "'"

#define DBUS_IFACE_HIM "org.maemo.him"
#define DBUS_SIGNAL_SET_VISIBLE "set_visible"
#define DBUS_MATCH_RULE "type='signal',interface='" DBUS_IFACE_HIM "',member='" DBUS_SIGNAL_SET_VISIBLE "'"

GtkWidget *keyboard = NULL;

static bool slide_open = false;

static void
handle_sigterm(gint t)
{
  gtk_main_quit();
}

static DBusHandlerResult
dbus_system_msg_handler (DBusConnection *connection,
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
    else if (strcmp(interface, MCE_SIGNAL_IF) == 0 &&
        strcmp(method, MCE_KEYBOARD_SLIDE_GET) == 0)
    {
      dbus_bool_t slide;

      dbus_message_get_args(msg, NULL, DBUS_TYPE_BOOLEAN, &slide, DBUS_TYPE_INVALID);
      slide_open = slide;
      if (slide_open && keyboard)
        hildon_im_ui_set_visible((HildonIMUI *)keyboard, false);
    }
  }
  
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
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
    if (strcmp(interface, DBUS_IFACE_HIM) == 0 &&
        strcmp(method, DBUS_SIGNAL_SET_VISIBLE) == 0)
    {
      gboolean visible = TRUE;
      dbus_message_get_args(msg, NULL,
                DBUS_TYPE_BOOLEAN, &visible,
                DBUS_TYPE_INVALID);
      hildon_im_ui_set_visible((HildonIMUI *)keyboard, visible);
      return DBUS_HANDLER_RESULT_HANDLED;
    }
  }
  
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusConnection *
register_on_system_dbus()
{
  DBusConnection *dbus_connection_system = NULL;
  DBusError dbus_error_code;
  
  dbus_error_init(&dbus_error_code);

  /* Establish DBus connection */
  if (!(dbus_connection_system = dbus_bus_get(DBUS_BUS_SYSTEM, &dbus_error_code)))
  {
    g_warning("DBUS connection failed: %s\n", dbus_error_code.message);
    dbus_error_free (&dbus_error_code);
  }

  if (dbus_connection_system)
  {
    dbus_bus_add_match(dbus_connection_system, LOCALE_RULE, &dbus_error_code);
    if (dbus_error_is_set(&dbus_error_code))
    {
      g_warning("Unable to add match for locale change: %s\n",
                dbus_error_code.message);
      dbus_error_free(&dbus_error_code);
    }
    
    dbus_bus_add_match(dbus_connection_system, MCE_KBD_RULE, &dbus_error_code);
    if (dbus_error_is_set(&dbus_error_code))
    {
      g_warning("Unable to add match for mce keyboard slide state: %s\n",
                dbus_error_code.message);
      dbus_error_free(&dbus_error_code);
    }
  }
  
  if (!dbus_connection_add_filter(dbus_connection_system,
                                  dbus_system_msg_handler,
                                  NULL, NULL))
  {
    g_warning("Failed to add filter: %s\n", dbus_error_code.message);
    dbus_error_free(&dbus_error_code);
  }
  return dbus_connection_system;
}

static DBusConnection *
register_on_session_dbus()
{
  DBusConnection *dbus_connection = NULL;
  DBusError dbus_error_code;

  dbus_error_init(&dbus_error_code);

  /* Establish DBus connection */
  if (!(dbus_connection = dbus_bus_get(DBUS_BUS_SESSION, &dbus_error_code)))
  {
    g_warning("DBUS connection failed: %s\n", dbus_error_code.message);
    dbus_error_free (&dbus_error_code);
  }

  if (dbus_connection)
  {
    int ret;
    ret = dbus_bus_request_name(dbus_connection, DBUS_IFACE_HIM, 0, &dbus_error_code);
    if (dbus_error_is_set(&dbus_error_code))
    {
      g_warning("Cant own dbus name: %s because: %s\n", DBUS_IFACE_HIM,
                dbus_error_code.message);
      dbus_error_free(&dbus_error_code);
    }
    else if (ret == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    {
      g_info("Registered dbus interface: %s\n", DBUS_IFACE_HIM);
    }
    else
    {
      g_info("Unable to own primary dbus interface name: %s\n", DBUS_IFACE_HIM);
    }

    dbus_bus_add_match(dbus_connection, DBUS_MATCH_RULE, &dbus_error_code);
    if (dbus_error_is_set(&dbus_error_code))
    {
      g_warning("Unable to add match for set_visible: %s\n",
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
  return dbus_connection;
}

static void
at_spi_event_cb(AtspiEvent *event, void *data)
{
  (void)data;
  static bool skip = false;
  
  if (slide_open)
  {
    skip = false;
    return;
  }

  if (event->source == NULL)
    return;

  if (!event->detail1)
    return;

  AtspiStateSet *stateSet = atspi_accessible_get_state_set(event->source);
  GArray *states = atspi_state_set_get_states(stateSet);
  
  bool focused = false;
  bool editable = false;
  bool enabled = false;

  for (size_t i = 0; i < states->len; i++)
  {
    AtspiStateType state = g_array_index(states, AtspiStateType, i);
    if(state == ATSPI_STATE_EDITABLE)
      editable = true;
    else if(state == ATSPI_STATE_FOCUSED)
      focused = true;
    else if(state == ATSPI_STATE_ENABLED)
      enabled = true;
  }

  g_array_free (states, TRUE);
  g_object_unref (stateSet);
  
  if(focused && editable && enabled && keyboard)
  {
    if (skip)
    {
      skip = false;
      return;
    }
    skip = true;
    hildon_im_ui_set_visible((HildonIMUI *)keyboard, true);
  }
}

static void
register_on_at_spi()
{
  atspi_init();
  AtspiEventListener *aptSpiBus = atspi_event_listener_new (at_spi_event_cb, NULL, NULL);

  atspi_event_listener_register(aptSpiBus, "object:state-changed:focused", NULL);
}

int
main(int argc, char *argv[])
{
  struct sigaction sv;
  DBusConnection *dbus_connection;
  DBusConnection *dbus_connection_system;
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

  dbus_connection_system = register_on_system_dbus();
  dbus_connection = register_on_session_dbus();
  (void)dbus_connection;
  (void)dbus_connection_system;
  
  register_on_at_spi();

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
