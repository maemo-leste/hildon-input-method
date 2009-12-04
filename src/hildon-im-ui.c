/**
   @file: hildon-im-ui.c

 */
/*
 * This file is part of hildon-input-method
 *
 * Copyright (C) 2005-2008 Nokia Corporation.
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
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkicontheme.h>
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <libosso.h>
#include <hildon/hildon-window.h>
#include <hildon/hildon-banner.h>
#include <hildon/hildon-sound.h>
#include <hildon/hildon-helper.h>
#include <log-functions.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdio.h>
#include <langinfo.h>
#include <config.h>
#include <string.h>
#include <dbus/dbus.h>

#include "hildon-im-ui.h"
#include "hildon-im-plugin.h"
#include "hildon-im-protocol.h"
#include "hildon-im-languages.h"
#include "internal.h"
#include "cache.h"

#define MAD_SERVICE "com.nokia.AS_DIMMED_infoprint"

#define _(String) gettext(String)

#define IM_MENU_DIR   PREFIX "/share/hildon-input-method"

#define SOUND_PREFIX PREFIX "/share/sounds/"
#define ILLEGAL_CHARACTER_SOUND_PATH SOUND_PREFIX "ui-default_beep.wav"
#define NUMBER_INPUT_SOUND_PATH SOUND_PREFIX "ui-gesture_number_recognized.wav"
#define FINGER_TRIGGER_SOUND_PATH SOUND_PREFIX "ui-thumb_keyboard_launch.wav"

#define GCONF_CURRENT_LANGUAGE       HILDON_IM_GCONF_LANG_DIR "/current"
#define GCONF_INPUT_METHOD           HILDON_IM_GCONF_DIR "/input_method_plugin"

#define GCONF_IM_DEFAULT_PLUGINS     HILDON_IM_GCONF_DIR "/default-plugins"
#define GCONF_IM_HKB_PLUGIN          GCONF_IM_DEFAULT_PLUGINS "/hw-keyboard"
#define GCONF_IM_STYLUS_PLUGIN       GCONF_IM_DEFAULT_PLUGINS "/stylus"
#define GCONF_IM_FINGER_PLUGIN       GCONF_IM_DEFAULT_PLUGINS "/finger"

#define SOUND_REPEAT_ILLEGAL_CHARACTER 800
#define SOUND_REPEAT_NUMBER_INPUT 0
#define SOUND_REPEAT_FINGER_TRIGGER 1500

#define PACKAGE_OSSO    "hildon_input_method"
#define HILDON_COMMON_STRING  "hildon-common-strings"
#define HILDON_IM_STRING  "hildon-input-method"

#define BUFFER_SIZE 128

#define THUMB_LAUNCHES_FULLSCREEN_PLUGIN TRUE

/* CURRENT_PLUGIN is the current PluginData */
#define CURRENT_PLUGIN(self) self->priv->current_plugin
/* CURRENT_IM_WIDGET is the current widget of PluginData */
#define CURRENT_IM_WIDGET(self) self->priv->current_plugin->widget
/* CURRENT_IM_PLUGIN is the current HildonIMPlugin of PluginData */
#define CURRENT_IM_PLUGIN(self) HILDON_IM_PLUGIN (CURRENT_IM_WIDGET (self))

#define CURRENT_PLUGIN_IS_FULLSCREEN(self) (CURRENT_PLUGIN (self)->info->type == HILDON_IM_TYPE_FULLSCREEN)

enum
{
  PRIMARY_LANGUAGE = 0,
  SECONDARY_LANGUAGE,
  
  NUM_LANGUAGES
};

static const gchar *language_gconf [NUM_LANGUAGES] = 
{
  HILDON_IM_GCONF_PRIMARY_LANGUAGE,
  HILDON_IM_GCONF_SECONDARY_LANGUAGE
};

typedef struct {
  HildonIMPluginInfo  *info;
  GSList              *languages;
  GtkWidget           *widget;    /* actual IM plugin */
  gboolean            enabled;

  gchar               *filename;
} PluginData;

typedef struct {
  HildonIMTrigger trigger;
  gint            type;
} TriggerType;

typedef GtkWidget *(*im_init_func)(HildonIMUI *);
typedef const HildonIMPluginInfo *(*im_info_func)(void);

struct _HildonIMUIPrivate
{
  GtkWidget *current_banner;

  gchar selected_languages[NUM_LANGUAGES][BUFFER_SIZE];
  gint current_language_index;  /* to previous table */ 

  Window input_window;
  Window app_window;
  Window transiency;

  HildonGtkInputMode input_mode;
  HildonGtkInputMode default_input_mode;
  HildonIMOptionMask options;
  HildonIMCommand autocase;
  HildonIMTrigger trigger;

  gchar *surrounding;
  gint surrounding_offset;
  HildonIMCommitMode commit_mode;
  
  gchar *committed_preedit;

  guint sound_timeout_id;

  gboolean pressed_ontop;
  gboolean long_press;
  gint pressed_button;
  gboolean repeat_done;
  guint repeat_timeout_id;
  guint long_press_timeout_id;

  gboolean first_boot;

  gboolean return_key_pressed;
  gboolean use_finger_kb;

  GSList *all_methods;
  GtkBox *im_box;
  gboolean has_special;

  DBusConnection *dbus_connection;

  gboolean keyboard_available;
  GSList   *additional_menu;

  gboolean plugins_available;
  GSList   *last_plugins;
  PluginData *current_plugin;

  GtkWidget *menu_plugin_list;

  GString *plugin_buffer;

  osso_context_t *osso;

  gchar* cached_hkb_plugin_name;
  gchar* cached_finger_plugin_name;
  gchar* cached_stylus_plugin_name;

  PluginData *default_hkb_plugin;
  PluginData *default_finger_plugin;
  PluginData *default_stylus_plugin;
  
  GList *parsed_rc_files;
};

typedef struct
{
  const gchar *id;
  GType type;
} button_data_type;

static void hildon_im_ui_init(HildonIMUI *self);
static void hildon_im_ui_class_init(HildonIMUIClass *klass);

static void hildon_im_ui_send_event(HildonIMUI *self,
                                          Window window, XEvent *event);

static gboolean hildon_im_ui_restore_previous_mode_real(HildonIMUI *self);
static void flush_plugins(HildonIMUI *, PluginData *, gboolean);
static void hildon_im_hw_cb(osso_hw_state_t*, gpointer);

static void detect_first_boot (HildonIMUI *self);

static void activate_plugin (HildonIMUI *s, PluginData *, gboolean);

static void hildon_im_ui_foreach_plugin(HildonIMUI *self,
                                        void (*function) (HildonIMPlugin *));

G_DEFINE_TYPE(HildonIMUI, hildon_im_ui, GTK_TYPE_WINDOW)

static gint
_plugin_by_trigger_type (gconstpointer a, gconstpointer b)
{
  TriggerType *tt = (TriggerType*) b;
  gint trigger_a, type_a;
  
  if (a == NULL || 
      ((PluginData*) a)->info == NULL ||
      tt == NULL)
    return -1;

  trigger_a = ((PluginData *) a)->info->trigger;
  type_a = ((PluginData *) a)->info->type;

  /* Only compare the trigger */
  if (tt->type == -1 &&
      trigger_a == tt->trigger)
    return 0;

  /* Only compare the type */
  if (tt->trigger == -1 &&
      type_a == tt->type)
    return 0;

  if (trigger_a == tt->trigger &&
      type_a == tt->type)
    return 0;

  return -1;
}

static gint
_plugin_by_name (gconstpointer a, gconstpointer b)
{
  if (a == NULL || 
      ((PluginData*) a)->info == NULL ||
      b == NULL)
    return -1;

  return g_ascii_strcasecmp (((PluginData*) a)->info->name,
      (gchar *) b);
}

static PluginData *
find_plugin_by_trigger_type (HildonIMUI *self,
                             HildonIMTrigger trigger,
                             gint type)
{
  GSList *found;
  TriggerType tt;
  
  /* TODO use the default plugins */

  tt.trigger = trigger;
  tt.type = type;
  found = g_slist_find_custom (self->priv->all_methods, 
                               &tt,
                               _plugin_by_trigger_type);
  if (found)
  {
    return found->data;
  }

  return NULL;
}

static PluginData *
last_plugin_by_trigger_type (HildonIMUI *self,
                             HildonIMTrigger trigger,
                             gint type)
{
  GSList *found;
  TriggerType tt;

  tt.trigger = trigger;
  tt.type = type;
  found = g_slist_find_custom (self->priv->last_plugins, 
                               &tt,
                               _plugin_by_trigger_type);
  if (found)
  {
    return found->data;
  }

  return NULL;
}


static PluginData *
find_plugin_by_name (HildonIMUI *self, const gchar *name)
{
  GSList *found;
  found = g_slist_find_custom (self->priv->all_methods, name, 
      _plugin_by_name);

  if (found)
    return (PluginData*) found->data;

  return NULL;
}

static PluginData *
get_default_plugin_by_trigger (HildonIMUI *self,
                               HildonIMTrigger trigger)
{
  PluginData *plugin = NULL;
  gchar *plugin_name;
  
  switch (trigger)
  {
  case HILDON_IM_TRIGGER_KEYBOARD :
    plugin_name = self->priv->cached_hkb_plugin_name;
    break;
  case HILDON_IM_TRIGGER_FINGER :
    plugin_name = self->priv->cached_finger_plugin_name;
    break;
  case HILDON_IM_TRIGGER_STYLUS :
    plugin_name = self->priv->cached_stylus_plugin_name;
    break;
  case HILDON_IM_TRIGGER_NONE :
  case HILDON_IM_TRIGGER_UNKNOWN :
  default :
    plugin_name = NULL;
    break;
  }

  if (plugin_name != NULL)
    plugin = find_plugin_by_name (self, plugin_name);
  if (plugin == NULL)
    plugin = find_plugin_by_trigger_type (self,
                            trigger, HILDON_IM_TYPE_DEFAULT);
  return plugin;
}

static void
update_last_plugins (HildonIMUI *self, PluginData *plugin)
{
  GSList *found;
  TriggerType tt;

  g_return_if_fail (self != NULL);
  g_return_if_fail (plugin != NULL && plugin->info != NULL);

  tt.trigger = plugin->info->trigger;
  tt.type = plugin->info->type;

  found = g_slist_find_custom (self->priv->last_plugins, 
      &tt,
      _plugin_by_trigger_type);
  if (found)
  {
    if (found->data != plugin) {
      g_warning ("Replacing for %d %d", tt.trigger, tt.type);
      self->priv->last_plugins = g_slist_delete_link (
          self->priv->last_plugins, 
          found);   
    }
  }
  self->priv->last_plugins = g_slist_prepend (self->priv->last_plugins,
      plugin);

  return;
}

/* This two functions don't activate the current plugin, just set
 * the current_plugin */
static void
set_current_plugin (HildonIMUI *self, PluginData *plugin)
{
  self->priv->current_plugin = plugin;
  update_last_plugins (self, plugin);
}

static GSList *
merge_languages (GSList *all, GSList *partial)
{
  GSList *iter, *found;

  iter = partial;
  while (iter)
  {
    gchar *data = (gchar *) iter->data;
    found = g_slist_find_custom (all, data, 
        (GCompareFunc) g_ascii_strcasecmp);
    if (found == NULL)
      all = g_slist_prepend (all, g_strdup (data));

    iter = g_slist_next (iter);
  }

  return all;
}

static void
init_persistent_plugins(HildonIMUI *self)
{
  GSList *iter;

  for (iter = self->priv->all_methods; iter != NULL; iter = iter->next)
  {
    PluginData *plugin = (PluginData *) iter->data;

    if (plugin->info->type == HILDON_IM_TYPE_PERSISTENT)
    {
      plugin->widget =
        GTK_WIDGET(hildon_im_plugin_create(self, plugin->filename));
      hildon_im_plugin_enable(HILDON_IM_PLUGIN(plugin->widget), TRUE);
      plugin->enabled = TRUE;
    }
  }
}

static void
cleanup_plugins (HildonIMUI *self)
{
  GSList *iter;
  PluginData *plugin;
  if (self->priv->all_methods == NULL)
    return;

  iter = self->priv->all_methods;
  flush_plugins(self, NULL, TRUE);
  while (iter)
  {
    plugin = (PluginData *) iter->data;
    if (plugin != NULL)
    {
      FREE_IF_SET (plugin->filename);
      free_language_list (plugin->languages);
      free_iminfo (plugin->info);
    }
    iter = g_slist_next (iter);
  }
  return;
}

static gboolean
init_plugins (HildonIMUI *self)
{
  FILE *f;

  if (self->priv->all_methods != NULL)
    cleanup_plugins (self);

  f = init_cache ();
  if (f)
  {
    gint number_of_plugins;
    gint i;
    GSList *merged_languages = NULL;
    HildonIMPluginInfo *info;
    PluginData *plugin;

    number_of_plugins = cache_get_number_of_plugins (f);
    for (i = 0; i < number_of_plugins; i ++)
    {
      plugin = (PluginData *) g_malloc0 (sizeof (PluginData));
      plugin->filename = cache_get_soname (f);      
      plugin->languages = cache_get_languages (f);
      plugin->enabled = FALSE;
      merged_languages = merge_languages (merged_languages,
          plugin->languages);
      info     = cache_get_iminfo (f);

      plugin->info = info;
      self->priv->all_methods =
        g_slist_prepend (self->priv->all_methods, plugin);      
    }

    hildon_im_populate_available_languages (merged_languages);
    free_language_list (merged_languages);
    fclose (f);
  } else
    return FALSE;

  return TRUE;
}

void
hildon_im_reload_plugins (HildonIMUI *self)
{
  self->priv->plugins_available = init_plugins (self);
  if (self->priv->plugins_available == FALSE)
  {
    g_warning ("Failed loading the plugins.");
    g_warning ("No IM will show.");
    return;
  }
}

/*
 * hildon_im_hw_cb:
 * @state: device HW state structure
 * @data: user defined data (self)
 *
 * Handles libosso callbacks for changing device HW state
 */
static void
hildon_im_hw_cb(osso_hw_state_t* state, gpointer data)
{
  HildonIMUI *self = HILDON_IM_UI(data);
  g_return_if_fail(HILDON_IM_IS_UI(self));

  if (state->shutdown_ind)
  {
    GSList *iter;
    for (iter = self->priv->all_methods; iter != NULL; iter = iter->next)
    {
      PluginData *info = (PluginData *) iter->data;
      if (info->widget != NULL)
      {
        hildon_im_plugin_save_data(HILDON_IM_PLUGIN(info->widget));
      }
    }
    gtk_main_quit();
  }
  else if (state->save_unsaved_data_ind)
  { /* The shutdown_ind does the saving as well */
    GSList *iter;
    for (iter = self->priv->all_methods; iter != NULL; iter = iter->next)
    {
      PluginData *info = (PluginData *) iter->data;
      if (info->widget != NULL)
      {
        hildon_im_plugin_save_data(HILDON_IM_PLUGIN(info->widget));
      }
    }
  }

  if (state->memory_low_ind)
  {
    /* TODO:
     * - prevent new allocations (WC, language swithing)?
     */
  }
  if (state->system_inactivity_ind)
  {
    /* TODO:
     * - ask WC to save dictionaries? Seems the shutdown saving is good enough
     */
  }
}

static gboolean
hildon_im_ui_hide(gpointer data)
{
  HildonIMUI *self = HILDON_IM_UI(data);
  g_return_val_if_fail(HILDON_IM_IS_UI(self), FALSE);

  /* Window IDs may be reused. Forget our old input_window when hiding. */
  if (CURRENT_PLUGIN (self) && CURRENT_PLUGIN_IS_FULLSCREEN(self) == FALSE)
  {
    self->priv->input_window = 0;
  }

  if (self->priv->repeat_timeout_id != 0)
  {
    g_source_remove(self->priv->repeat_timeout_id);
    self->priv->repeat_timeout_id = 0;
  }

  if (self->priv->long_press_timeout_id != 0)
  {
    g_source_remove(self->priv->long_press_timeout_id);
    self->priv->long_press_timeout_id = 0;
  }

  gtk_widget_hide(GTK_WIDGET(self));

  if (self->priv->current_plugin != NULL &&
      CURRENT_IM_WIDGET (self) != NULL)
  {
    hildon_im_plugin_disable (CURRENT_IM_PLUGIN (self));
    self->priv->current_plugin->enabled = FALSE;
  }
  return FALSE;
}

inline static PluginData *
hildon_im_ui_get_plugin_info(HildonIMUI *self, gchar *name)
{
  GSList *iter;

  g_return_val_if_fail(HILDON_IM_IS_UI(self), NULL);

  for (iter = self->priv->all_methods; iter != NULL; iter = iter->next)
  {
    PluginData *info;
    info = (PluginData *) iter->data;
    if (info->info != NULL && g_ascii_strcasecmp (info->info->name, name) == 0)
    {
      return info;
    }
  }

  return NULL;
}

static void
hildon_im_ui_show(HildonIMUI *self)
{
  PluginData *plugin = NULL;

  g_return_if_fail(HILDON_IM_IS_UI(self));

  if (self->priv->trigger == HILDON_IM_TRIGGER_UNKNOWN)
  {
    if (self->priv->keyboard_available && self->priv->use_finger_kb)
      self->priv->trigger = HILDON_IM_TRIGGER_KEYBOARD;
    else
      self->priv->trigger = HILDON_IM_TRIGGER_FINGER;
  }

  if (self->priv->trigger == HILDON_IM_TRIGGER_STYLUS ||
      self->priv->trigger == HILDON_IM_TRIGGER_FINGER)
  {
    if (!self->priv->use_finger_kb)
      self->priv->current_plugin = NULL;
    else
    {
      HildonIMTrigger fallback = self->priv->trigger == HILDON_IM_TRIGGER_FINGER ?
                                  HILDON_IM_TRIGGER_STYLUS : HILDON_IM_TRIGGER_FINGER;

      plugin = get_default_plugin_by_trigger (self, self->priv->trigger);

      if (plugin == NULL)
        plugin = get_default_plugin_by_trigger (self, fallback);
    }
  }
  else if (self->priv->keyboard_available)
  {
    plugin = get_default_plugin_by_trigger (self, HILDON_IM_TRIGGER_KEYBOARD);
  }

  /* Check if the widget does not want fullscreen plugins */
  if (plugin != NULL && plugin->info->type == HILDON_IM_TYPE_FULLSCREEN &&
      (self->priv->input_mode & HILDON_GTK_INPUT_MODE_NO_SCREEN_PLUGINS) != 0)
  {
    self->priv->current_plugin = plugin = NULL;
  }

  if (plugin != NULL)
  {
    /* If the plugin is loaded, but now shown, ask it to reshow itself */
    if (plugin->widget != NULL)
    {
      set_current_plugin (self, plugin);
      if (!plugin->enabled)
      {
        hildon_im_plugin_enable (HILDON_IM_PLUGIN(plugin->widget), FALSE);
        plugin->enabled = TRUE;
      }
      return;
    } 

    activate_plugin(self, plugin, TRUE);
    return;
  }
  else
  {
    /* Something went wrong, the plugin name, the widget and the kb mode
       are not in sync. The one known case is MCE intercepting the home
       key and the desktop forcing the fullscreen plugin to the background.
       Restore the previous plugin for a consistent state.
       TODO is this still needed? */
    if (self->priv->current_plugin != NULL  &&
        CURRENT_PLUGIN_IS_FULLSCREEN(self))
    {
      hildon_im_ui_restore_previous_mode_real (self);
    }
  }

  /* IM may not be loaded yet? */
  if (self->priv->current_plugin != NULL)
  {
    activate_plugin (self, self->priv->current_plugin, TRUE);
    hildon_im_plugin_enable (HILDON_IM_PLUGIN(self->priv->current_plugin->widget), FALSE);
    self->priv->current_plugin->enabled = TRUE;
  }
}

static void
hildon_im_ui_activate_current_language(HildonIMUI *self)
{
  g_return_if_fail(HILDON_IM_IS_UI(self));
  g_return_if_fail (self->priv->current_plugin != NULL);

  if (GTK_WIDGET_VISIBLE(self) == TRUE)
  {
    activate_plugin (self, self->priv->current_plugin,
                                     TRUE);
  }
 
  if (CURRENT_IM_WIDGET (self) != NULL)
  {
    hildon_im_ui_foreach_plugin(self, hildon_im_plugin_language);
  }
}

static gboolean
hildon_im_ui_restore_previous_mode_real(HildonIMUI *self)
{
  PluginData *plugin;
  g_return_val_if_fail(HILDON_IM_IS_UI(self), FALSE);

  if (self->priv->keyboard_available)
  {
    plugin = get_default_plugin_by_trigger (self, HILDON_IM_TRIGGER_KEYBOARD);
    if (plugin != NULL)
      plugin = last_plugin_by_trigger_type (self, HILDON_IM_TRIGGER_KEYBOARD,
                                            HILDON_IM_TYPE_DEFAULT);
  }
  else
  {
    plugin = get_default_plugin_by_trigger (self, HILDON_IM_TRIGGER_FINGER);
    if (plugin != NULL)
      plugin = last_plugin_by_trigger_type (self, HILDON_IM_TRIGGER_FINGER,
                                            HILDON_IM_TYPE_DEFAULT);
  }
  if (plugin)
  {
    activate_plugin (self, plugin, FALSE);
  }
  else
  {
    flush_plugins(self, NULL, FALSE); 
  }

  return FALSE;
}

void
hildon_im_ui_set_keyboard_state(HildonIMUI *self,
                                gboolean available)
{
  PluginData *plugin;

  g_return_if_fail(HILDON_IM_IS_UI(self));

  self->priv->keyboard_available = available;

  if (self->priv->keyboard_available)
  { 
    plugin = get_default_plugin_by_trigger (self, HILDON_IM_TRIGGER_KEYBOARD);

    if (plugin)
    {
      self->priv->trigger = HILDON_IM_TRIGGER_KEYBOARD;
      activate_plugin(self, plugin, TRUE);
    }
  }
  else
  {
    /* TODO if the state changes, we remove the current plugins */
    if (GTK_WIDGET_VISIBLE(self))
    {
      hildon_im_ui_hide(self);
    }
    flush_plugins(self, NULL, FALSE);
  }

  hildon_im_ui_foreach_plugin(self, hildon_im_plugin_keyboard_state_changed);
}

static void
hildon_im_ui_gconf_change_callback(GConfClient* client,
                                   guint cnxn_id,
                                   GConfEntry *entry,
                                   gpointer user_data)
{
  HildonIMUI *self;
  GConfValue *value;
  const gchar *key;
  GSList *iter;

  if ((value = gconf_entry_get_value(entry)) == NULL)
  {
    return;
  }

  self = HILDON_IM_UI(user_data);
  key = gconf_entry_get_key(entry);

  g_return_if_fail(HILDON_IM_IS_UI(self));

  if (strcmp(key, GCONF_CURRENT_LANGUAGE) == 0)
  {
    if (value->type == GCONF_VALUE_INT)
    {
      gint new_value;
      new_value = gconf_value_get_int(value);
      if (new_value != self->priv->current_language_index)
      {
        self->priv->current_language_index = new_value;
        hildon_im_ui_activate_current_language(self);
      }      
    }
  }
  else if (strcmp(key, GCONF_INPUT_METHOD) == 0)
  {
    if (value->type == GCONF_VALUE_STRING)
    {
      gchar *new_value;

      new_value = (gchar *)gconf_value_get_string (value);
      hildon_im_ui_activate_plugin (self, new_value, TRUE);
    }
  }
  else if (strcmp(key, HILDON_IM_GCONF_PRIMARY_LANGUAGE) == 0)
  {
    if(value->type == GCONF_VALUE_STRING)
    {
      const gchar *language = gconf_value_get_string(value);
      GSList *iter;

      strncpy (self->priv->selected_languages [PRIMARY_LANGUAGE], language,
          strlen (language) > BUFFER_SIZE ? BUFFER_SIZE -1: strlen (language));

      for (iter = self->priv->all_methods; iter != NULL; iter = iter->next)
      {
        PluginData *info;
        info = (PluginData *) iter->data;
        if (info->widget != NULL)
        {
          hildon_im_plugin_language_settings_changed(
                  HILDON_IM_PLUGIN(info->widget),
                  PRIMARY_LANGUAGE);
        }
      }
    }
  }
  else if (strcmp(key, HILDON_IM_GCONF_SECONDARY_LANGUAGE) == 0)
  {
    if (value->type == GCONF_VALUE_STRING)
    {
      gboolean lang_valid;
      const gchar *language = gconf_value_get_string(value);

      lang_valid = !(language == NULL || language [0] == 0);
      if (lang_valid)
      {
        strncpy (self->priv->selected_languages [SECONDARY_LANGUAGE], language,
          strlen (language) > BUFFER_SIZE ? BUFFER_SIZE -1: strlen (language));
      }
      else
      {
        /* Secondary was set to empty */
        if (self->priv->current_language_index != PRIMARY_LANGUAGE)
        {
          /* We have the secondary set as current - set to primary.
           * Just set the gconf here, the others will propagate. */
          gconf_client_set_int (self->client,
                                GCONF_CURRENT_LANGUAGE, PRIMARY_LANGUAGE, NULL);
        }
        self->priv->selected_languages[SECONDARY_LANGUAGE][0] = 0;
      }
    }
    for (iter = self->priv->all_methods; iter != NULL; iter = iter->next)
    {
      PluginData *info = (PluginData *) iter->data;
      if (info->widget != NULL)
      {
        hildon_im_plugin_language_settings_changed(
                HILDON_IM_PLUGIN(info->widget),
                SECONDARY_LANGUAGE);
      }
    }
  }
  else if (strcmp(key, HILDON_IM_GCONF_USE_FINGER_KB) == 0)
  {
    if (value->type == GCONF_VALUE_BOOL)
    {
      self->priv->use_finger_kb = gconf_value_get_bool(value);
    }
  }
  else if (strcmp(key, GCONF_IM_HKB_PLUGIN) == 0)
  {
    g_free(self->priv->cached_hkb_plugin_name);
    self->priv->cached_hkb_plugin_name =
      gconf_client_get_string (self->client, GCONF_IM_HKB_PLUGIN, NULL);
  }
  else if (strcmp(key, GCONF_IM_FINGER_PLUGIN) == 0)
  {
    g_free(self->priv->cached_finger_plugin_name);
    self->priv->cached_finger_plugin_name =
      gconf_client_get_string (self->client, GCONF_IM_FINGER_PLUGIN, NULL);
  }
  else if (strcmp(key, GCONF_IM_STYLUS_PLUGIN) == 0)
  {
    g_free(self->priv->cached_stylus_plugin_name);
    self->priv->cached_stylus_plugin_name =
      gconf_client_get_string (self->client, GCONF_IM_STYLUS_PLUGIN, NULL);
  }

  for (iter = self->priv->all_methods; iter != NULL; iter = iter->next)
  {
    PluginData *info = (PluginData *) iter->data;
    if (info->widget != NULL)
    {
      hildon_im_plugin_settings_changed(HILDON_IM_PLUGIN(info->widget),
                                        key, value);
    }
  }
  
  /* TODO default plugins */
}

static void
hildon_im_ui_load_gconf(HildonIMUI *self)
{
  gchar *language;
  gint size;
  
  g_return_if_fail(HILDON_IM_IS_UI(self));

  self->priv->current_language_index =
    gconf_client_get_int (self->client, GCONF_CURRENT_LANGUAGE, NULL);

  language = gconf_client_get_string(self->client,
                                     HILDON_IM_GCONF_PRIMARY_LANGUAGE, NULL);
  if (language == NULL)
    size = 0;
  else
    size = strlen (language);

  strncpy (self->priv->selected_languages [PRIMARY_LANGUAGE], language,
          size > BUFFER_SIZE ? BUFFER_SIZE -1: size);

  g_free(language);

  language = gconf_client_get_string(self->client,
                                     HILDON_IM_GCONF_SECONDARY_LANGUAGE, NULL);
  if (language != NULL && *language != '\0')
  {
    strncpy (self->priv->selected_languages [SECONDARY_LANGUAGE], language,
          strlen (language) > BUFFER_SIZE ? BUFFER_SIZE -1: strlen (language));
  }
  else
  {    
    self->priv->selected_languages[SECONDARY_LANGUAGE][0] = 0;
    self->priv->current_language_index = 0;
    gint current_value = -1;
    current_value = gconf_client_get_int (self->client,
                                          GCONF_CURRENT_LANGUAGE,
                                          NULL);
    if (current_value != PRIMARY_LANGUAGE)
        gconf_client_set_int (self->client,
                              GCONF_CURRENT_LANGUAGE, PRIMARY_LANGUAGE, NULL);
  }
  g_free(language);

  self->priv->use_finger_kb = gconf_client_get_bool(self->client, 
                                                    HILDON_IM_GCONF_USE_FINGER_KB,
                                                    NULL);

  g_free(self->priv->cached_hkb_plugin_name);
  self->priv->cached_hkb_plugin_name = gconf_client_get_string (self->client,
                                                                GCONF_IM_HKB_PLUGIN,
                                                                NULL);

  g_free(self->priv->cached_finger_plugin_name);
  self->priv->cached_finger_plugin_name = gconf_client_get_string (self->client,
                                                                   GCONF_IM_FINGER_PLUGIN,
                                                                   NULL);

  g_free(self->priv->cached_stylus_plugin_name);
  self->priv->cached_stylus_plugin_name = gconf_client_get_string (self->client,
                                                                   GCONF_IM_STYLUS_PLUGIN,
                                                                   NULL);
}

/* Call a plugin function for each loaded plugin.
   Supports any function taking only the plugin as an arugment */
static void
hildon_im_ui_foreach_plugin(HildonIMUI *self,
                            void (*function) (HildonIMPlugin *))
{
  PluginData *plugin;
  GSList *iter;

  for (iter = self->priv->all_methods; iter != NULL; iter = iter->next)
  {
    plugin = (PluginData*) iter->data;

    if (plugin->widget == NULL)
      continue;

    (*function)(HILDON_IM_PLUGIN(plugin->widget));
  }
}

/* Call a plugin function for each loaded plugin.
   Supported plugin API functions:
   - hildon_im_plugin_key_event
*/
static void
hildon_im_ui_foreach_plugin_va(HildonIMUI *self,
                               void *function,
                               ...)
{
  PluginData *plugin;
  GSList *iter;
  va_list ap;

  GdkEventType event_type = GDK_NOTHING;
  guint state = 0;
  guint keyval = 0;
  guint hardware_keycode = 0;

  va_start(ap, function);
  if (function == hildon_im_plugin_key_event)
  {
    event_type = va_arg(ap, GdkEventType);
    state = va_arg(ap, guint);
    keyval = va_arg(ap, guint);
    hardware_keycode = va_arg(ap, guint);
  }
  va_end(ap);

  for (iter = self->priv->all_methods; iter != NULL; iter = iter->next)
  {
    plugin = (PluginData*) iter->data;

    if (plugin->widget == NULL)
      continue;

    if (function == hildon_im_plugin_key_event)
    {
      hildon_im_plugin_key_event(HILDON_IM_PLUGIN(plugin->widget),
                                 event_type, state, keyval, hardware_keycode);
    }
  }
}

static void
hildon_im_ui_set_client(HildonIMUI *self,
                        HildonIMActivateMessage *msg,
                        gboolean show)
{
  gboolean input_window_changed;
  g_return_if_fail(HILDON_IM_IS_UI(self));

  /* Workaround for matchbox feature. */
  if (self->priv->transiency != msg->app_window)
  {
    hildon_im_ui_hide(self);
    self->priv->transiency = msg->app_window;
  }
  XSetTransientForHint(GDK_DISPLAY(),
                       GDK_WINDOW_XID(GTK_WIDGET(self)->window),
                       msg->app_window);

  /* Try to avoid unneededly calling client_widget_changed(), because we
     can get here if popup menu closes and we get another focus_in event.
     We don't want word completion list to clear because of it. */
  input_window_changed = self->priv->input_window != msg->input_window;
  if (input_window_changed || msg->cmd == HILDON_IM_SETCLIENT)
  {
    PluginData *info = NULL;
    self->priv->input_window = msg->input_window;

    info = CURRENT_PLUGIN (self);

    if (input_window_changed)
    {
      hildon_im_ui_send_communication_message(self,
                                              HILDON_IM_CONTEXT_WIDGET_CHANGED);
    }

    if (info != NULL && info->widget != NULL)
    {
      HildonIMPlugin *plugin = HILDON_IM_PLUGIN(info->widget);
      hildon_im_ui_foreach_plugin(self, hildon_im_plugin_client_widget_changed);
      hildon_im_plugin_clear(plugin);
    }
  }

  if (show)
  {
    /* When the keyboard is available, non-keyboard plugins are disabled */
    if (self->priv->keyboard_available == TRUE)
    {
      self->priv->trigger = HILDON_IM_TRIGGER_KEYBOARD;
    }

    hildon_im_ui_show(self);
  }
}

gboolean
hildon_im_ui_is_first_boot(HildonIMUI *self)
{
  if (self->priv->first_boot != 0)
  {
    detect_first_boot(self);
  }

  return self->priv->first_boot;
}

static void
hildon_im_ui_process_input_mode_message (HildonIMUI *self,
                                         HildonIMInputModeMessage *msg)
{
  HildonIMPlugin *plugin = NULL;

  g_return_if_fail (HILDON_IM_IS_UI(self));

  if (CURRENT_PLUGIN (self) != NULL)
  {
    plugin = CURRENT_IM_PLUGIN (self);
  }

  if (self->priv->input_mode != msg->input_mode ||
      self->priv->default_input_mode != msg->default_input_mode)
  {
    self->priv->input_mode = msg->input_mode;
    self->priv->default_input_mode = msg->default_input_mode;
    if (plugin != NULL)
    {     
      hildon_im_plugin_input_mode_changed(plugin);      
    }
  }
}

/*called when we need to process an activate message.
 *activate messages are sent as ClientMessages by other applications
 *when they need the ui to show/hide itself
 */
static void
hildon_im_ui_process_activate_message(HildonIMUI *self,
                                      HildonIMActivateMessage *msg )
{
  HildonIMPlugin *plugin = NULL;

  g_return_if_fail(HILDON_IM_IS_UI(self));

  /* Check if a request comes from a different main window. Don't change it
     for HIDE events since with browser when opening IM popup menu it sends
     HIDE from app_window 0.. */
  if( msg->app_window != self->priv->app_window &&
      msg->cmd != HILDON_IM_HIDE )
  {
    self->priv->app_window = msg->app_window;
  }

  if (CURRENT_PLUGIN (self) != NULL)
  {
    plugin = CURRENT_IM_PLUGIN (self);
  }

  switch(msg->cmd)
  {
    case HILDON_IM_MODE:/*already done what needs to be done*/
      break;
    case HILDON_IM_SETCLIENT:
      hildon_im_ui_set_client(self, msg, FALSE);
      break;
    case HILDON_IM_SHOW:
      self->priv->trigger = msg->trigger;
      hildon_im_ui_show(self);
      break;
    case HILDON_IM_SETNSHOW:
      self->priv->trigger = msg->trigger;
      hildon_im_ui_set_client(self, msg, TRUE);
      break;
    case HILDON_IM_HIDE:
      hildon_im_ui_hide(self);
      break;
    case HILDON_IM_LOW:
    case HILDON_IM_UPP:
      self->priv->autocase = msg->cmd;
      if (plugin != NULL)
      {
        hildon_im_plugin_character_autocase(plugin);
      }
      break;
    case HILDON_IM_CLEAR:
      if (plugin != NULL)
      {
        hildon_im_plugin_clear(plugin);
      }
      break;
    case HILDON_IM_SELECT_ALL:
      hildon_im_plugin_select_region(plugin, 0, -1);
      break;
    case HILDON_IM_SHIFT_LOCKED :
    self->priv->current_banner = 
         hildon_banner_show_information (GTK_WIDGET(self), NULL,
                                         dgettext(HILDON_IM_STRING,
                                                  "inpu_ib_mode_shift_locked"));
    g_signal_connect (self->priv->current_banner,
                      "destroy",
                      G_CALLBACK (gtk_widget_destroyed),
                      &self->priv->current_banner);
      break;
    case HILDON_IM_SHIFT_UNLOCKED :
      break;
    case HILDON_IM_MOD_LOCKED :
      self->priv->current_banner =
         hildon_banner_show_information (GTK_WIDGET(self), NULL,
                                         dgettext(HILDON_IM_STRING,
                                                  "inpu_ib_mode_fn_locked"));
      g_signal_connect (self->priv->current_banner,
                        "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &self->priv->current_banner);
      break;
    case HILDON_IM_MOD_UNLOCKED :
      break;
    default:
      g_warning("Invalid message from im context");
      break;
  }
}

void
hildon_im_ui_toggle_special_plugin(HildonIMUI *self)
{
  PluginData *info;

  info = CURRENT_PLUGIN (self);

  if (info && info->info->special_plugin == NULL)
  {
    hildon_im_ui_restore_previous_mode_real(self);
  }
  else if (info)
  {
    hildon_im_ui_activate_plugin (self, info->info->special_plugin, TRUE);
  }
}

static void
hildon_im_ui_handle_key_message (HildonIMUI *self, HildonIMKeyEventMessage *msg)
{
  self->priv->input_window = msg->input_window;

  if (msg->type == GDK_KEY_PRESS && self->priv->current_banner != NULL)
  {
    gtk_widget_destroy (self->priv->current_banner);
    self->priv->current_banner = NULL;
  }
  
  /* Typing a printable character forces the plugin to show */
  if (self->priv->keyboard_available &&
      !GTK_WIDGET_VISIBLE(self))
  {
    guint c = gdk_keyval_to_unicode(msg->keyval);

    if ((g_unichar_isprint(c) || msg->keyval == GDK_Multi_key) &&
        (msg->state & GDK_CONTROL_MASK) == 0)
    {
      hildon_im_ui_show(self);
    }
  }

  if (self->priv->return_key_pressed &&
      msg->type == GDK_KEY_RELEASE &&
      msg->keyval == GDK_Return)
  {
    /* Allow the client widget to insert a new line/activate */
    if (self->priv->keyboard_available)
    {
      hildon_im_ui_send_communication_message(self,
          HILDON_IM_CONTEXT_HANDLE_ENTER);
    }
    else /* Toggle the visibility of the IM */
    {
      self->priv->trigger = HILDON_IM_TRIGGER_STYLUS;
      if(GTK_WIDGET_DRAWABLE(self))
        hildon_im_ui_hide(self);
      else
        hildon_im_ui_show(self);
    }
  }
  self->priv->return_key_pressed = (msg->keyval == GDK_Return &&
                                    msg->type == GDK_KEY_PRESS);
 
  hildon_im_ui_foreach_plugin_va(self,
                                 hildon_im_plugin_key_event,
                                 msg->type,
                                 msg->state,
                                 msg->keyval,
                                 msg->hardware_keycode);
}

/*filters client messages to see if we need to show/hide the ui*/
static GdkFilterReturn
hildon_im_ui_client_message_filter(GdkXEvent *xevent,
                                   GdkEvent *event,
                                   gpointer data)
{
  HildonIMUI *self;

  g_return_val_if_fail( HILDON_IM_IS_UI(data), GDK_FILTER_CONTINUE );
  self = HILDON_IM_UI(data);

  if (((XEvent *) xevent)->type == ClientMessage)
  {
    XClientMessageEvent *cme = (XClientMessageEvent *) xevent;

    if (cme->message_type ==
        hildon_im_protocol_get_atom( HILDON_IM_ACTIVATE)
        && cme->format == HILDON_IM_ACTIVATE_FORMAT)
    {
      HildonIMActivateMessage *msg = (HildonIMActivateMessage *) &cme->data;
      hildon_im_ui_process_activate_message(self, msg);
      return GDK_FILTER_REMOVE;
    }

    if (cme->message_type ==
      hildon_im_protocol_get_atom ( HILDON_IM_INPUT_MODE)
      && cme->format == HILDON_IM_INPUT_MODE_FORMAT)
    {
      HildonIMInputModeMessage *msg = (HildonIMInputModeMessage *) &cme->data;
      hildon_im_ui_process_input_mode_message (self, msg);
      return GDK_FILTER_REMOVE;
    }

    if (cme->message_type ==
      hildon_im_protocol_get_atom( HILDON_IM_SURROUNDING_CONTENT )
      && cme->format == HILDON_IM_SURROUNDING_CONTENT_FORMAT)
    {
      HildonIMSurroundingContentMessage *msg =
        (HildonIMSurroundingContentMessage *) &cme->data;
      gchar *new_surrounding;

      if (msg->msg_flag == HILDON_IM_MSG_START &&
          self->priv->surrounding)
      {
        g_free(self->priv->surrounding);
        self->priv->surrounding = g_strdup("");
      }

      new_surrounding = g_strconcat(self->priv->surrounding,
                                    msg->surrounding,
                                    NULL);

      if (self->priv->surrounding)
      {
        g_free(self->priv->surrounding);
      }

      self->priv->surrounding = new_surrounding;

      return GDK_FILTER_REMOVE;
    }

    if (cme->message_type ==
        hildon_im_protocol_get_atom( HILDON_IM_SURROUNDING )
        && cme->format == HILDON_IM_SURROUNDING_FORMAT)
    {
      HildonIMSurroundingMessage *msg =
        (HildonIMSurroundingMessage *) &cme->data;

      self->priv->commit_mode = msg->commit_mode;
      self->priv->surrounding_offset = msg->cursor_offset;

      if (CURRENT_PLUGIN(self) != NULL && CURRENT_IM_WIDGET(self) != NULL)
      {
        hildon_im_plugin_surrounding_received(CURRENT_IM_PLUGIN (self),
                                              self->priv->surrounding,
                                              self->priv->surrounding_offset);
      }
      
      return GDK_FILTER_REMOVE;
    }

    if (cme->message_type ==
      hildon_im_protocol_get_atom( HILDON_IM_PREEDIT_COMMITTED_CONTENT )
      && cme->format == HILDON_IM_PREEDIT_COMMITTED_CONTENT_FORMAT)
    {
      HildonIMPreeditCommittedContentMessage *msg =
                          (HildonIMPreeditCommittedContentMessage *) &cme->data;
      gchar *new_committed_preedit;

      if (msg->msg_flag == HILDON_IM_MSG_START && self->priv->committed_preedit)
      {
        g_free(self->priv->committed_preedit);
        self->priv->committed_preedit = g_strdup("");
      }

      new_committed_preedit = g_strconcat(self->priv->committed_preedit,
                                          msg->committed_preedit,
                                          NULL);

      g_free(self->priv->committed_preedit);
      self->priv->committed_preedit = new_committed_preedit;

      return GDK_FILTER_REMOVE;
    }
    
    if (cme->message_type ==
        hildon_im_protocol_get_atom( HILDON_IM_PREEDIT_COMMITTED )
        && cme->format == HILDON_IM_PREEDIT_COMMITTED_FORMAT)
    {
      HildonIMPreeditCommittedMessage *msg =
                                 (HildonIMPreeditCommittedMessage *) &cme->data;

      self->priv->commit_mode = msg->commit_mode;

      hildon_im_plugin_preedit_committed(CURRENT_IM_PLUGIN (self),
                                         self->priv->committed_preedit);

      return GDK_FILTER_REMOVE;
    }
    
    if (cme->message_type ==
        hildon_im_protocol_get_atom( HILDON_IM_KEY_EVENT )
        && cme->format == HILDON_IM_KEY_EVENT_FORMAT)
    {
      HildonIMKeyEventMessage *msg =
        (HildonIMKeyEventMessage *) &cme->data; 

      hildon_im_ui_handle_key_message (self, msg);
      return GDK_FILTER_REMOVE;
    }

    if (cme->message_type ==
        hildon_im_protocol_get_atom( HILDON_IM_CLIPBOARD_COPIED )
        && cme->format == HILDON_IM_CLIPBOARD_FORMAT)
    {
      self->priv->current_banner = hildon_banner_show_information (GTK_WIDGET(self), NULL,
                                      dgettext(HILDON_COMMON_STRING,
                                               "ecoc_ib_edwin_copied"));

      g_signal_connect (self->priv->current_banner,
                        "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &self->priv->current_banner);

      return GDK_FILTER_REMOVE;
    }

  }

  return GDK_FILTER_CONTINUE;
}

static gboolean
hildon_im_ui_x_window_want_im_hidden(Window window)
{
  Atom wm_type = XInternAtom(GDK_DISPLAY(), "_NET_WM_WINDOW_TYPE", False);
  Atom actual_type;
  gint actual_format = 0;
  unsigned long i, nitems, bytes_after;
  union {
    Atom *atom;
    unsigned char *char_value;
  } wm_type_value;
  gboolean ret = FALSE;

  gdk_error_trap_push();
  XGetWindowProperty(GDK_DISPLAY(), window, wm_type, 0, G_MAXLONG, False,
                     XA_ATOM, &actual_type, &actual_format, &nitems,
                     &bytes_after, &wm_type_value.char_value);
  if (gdk_error_trap_pop() != 0)
  {
    return FALSE;
  }

  for (i = 0; i < nitems && !ret; i++)
  {
    char *type_str = XGetAtomName(GDK_DISPLAY(), wm_type_value.atom[i]);

    /* IM needs to be hidden when changing to another window or dialog.
       desktop case happens when all windows are closed, we want to hide IM
       then as well of course.. */
    ret = strcmp(type_str, "_NET_WM_WINDOW_TYPE_NORMAL") == 0 ||
          strcmp(type_str, "_NET_WM_WINDOW_TYPE_DIALOG") == 0 ||
          strcmp(type_str, "_NET_WM_WINDOW_TYPE_DESKTOP") == 0 ||
          strcmp(type_str, "_HILDON_WM_WINDOW_TYPE_HOME_APPLET") == 0 ||
          strcmp(type_str, "_HILDON_WM_WINDOW_TYPE_STACKABLE") == 0 ||
          strcmp(type_str, "_HILDON_WM_WINDOW_TYPE_APP_MENU") == 0;
    /* Not: _NET_WM_WINDOW_TYPE_NOTIFICATION _NET_WM_WINDOW_TYPE_INPUT */
    XFree(type_str);
  }

  XFree(wm_type_value.char_value);
  return ret;
}

static gint
get_window_pid (Window window)
{
  Atom atom, actual_type;
  int actual_format;
  unsigned long nitems;
  unsigned long bytes_after;
  unsigned char *prop = NULL;
  gint pid = -1, status = -1;

  atom = XInternAtom(GDK_DISPLAY(), "_NET_WM_PID", True);
  
  gdk_error_trap_push();
  status = XGetWindowProperty (GDK_DISPLAY(),
                               window,
                               atom,
                               0,
                               1024,
                               False,
                               AnyPropertyType,
                               &actual_type,
                               &actual_format,
                               &nitems,
                               &bytes_after,
                               &prop);
  if (gdk_error_trap_pop())
  {
    /* an error in X happened :-( */
    pid = -1;
  }
  else if (status == 0 && prop != NULL)
  {
    pid = prop[1] * 256;
    pid += prop[0];

    XFree (prop);
  }

  return pid;
}

static GdkFilterReturn
hildon_im_ui_focus_message_filter(GdkXEvent *xevent, GdkEvent *event,
                                  gpointer data)
{
  HildonIMUI *self=NULL;

  g_return_val_if_fail(HILDON_IM_IS_UI(data), GDK_FILTER_CONTINUE);
  g_return_val_if_fail(xevent, GDK_FILTER_CONTINUE);

  self = HILDON_IM_UI(data);

  if ( ((XEvent *) xevent)->type == PropertyNotify )
  {
    /* We check the _NET_CLIENT_LIST because we need to get the last window
     * within the list. Before, we were checking the _MB_CURRNET_APP_WINDOW
     * but it wouldn't always point to the last application called, this way,
     * applications called when a fullscreen and modal plugin was running,
     * would be shown behind that plugin. */
    Atom active_window_atom =
            XInternAtom (GDK_DISPLAY(), "_NET_CLIENT_LIST_STACKING", False);
    XPropertyEvent *prop = (XPropertyEvent *) xevent;

    gboolean is_fullscreen = CURRENT_PLUGIN (self) != NULL && CURRENT_PLUGIN_IS_FULLSCREEN (self);

    if ((prop->atom == active_window_atom || is_fullscreen) && prop->window == GDK_ROOT_WINDOW())
    {
      /* Focused window changed, if it's a dialog or normal window hide IM. */
      Atom actual_type;
      gint actual_format = 0;
      unsigned long nitems, bytes_after;
      union {
        Window *window;
        unsigned char *char_value;
      } window_value;

      gdk_error_trap_push();
      XGetWindowProperty(GDK_DISPLAY(), GDK_ROOT_WINDOW(), active_window_atom,
                         0, G_MAXLONG, False, XA_WINDOW, &actual_type,
                         &actual_format, &nitems, &bytes_after,
                         &window_value.char_value);
      if (gdk_error_trap_pop() == 0)
      {
        /* We need the last window from the list as it represents the last window to be shown */
        gint last_window_index = nitems - 1;
        if (nitems > 0 && window_value.window[last_window_index] != self->priv->transiency)
        {
          if (get_window_pid (window_value.window[last_window_index]) != getpid() &&
              hildon_im_ui_x_window_want_im_hidden (window_value.window[last_window_index]))
          {
            flush_plugins(self, NULL, FALSE);
          }
        }

        XFree(window_value.char_value);
      }
    }
  }
  return GDK_FILTER_CONTINUE;
}

static void
hildon_im_ui_init_root_window_properties(HildonIMUI *self)
{
  GtkWidget *widget;
  Atom atom;
  Window xid;
  int result;

  g_return_if_fail(HILDON_IM_IS_UI(self));

  atom = hildon_im_protocol_get_atom( HILDON_IM_WINDOW );
  widget = GTK_WIDGET(self);

  gtk_widget_realize(widget);
  xid = GDK_WINDOW_XID(widget->window);

  result = XChangeProperty(GDK_DISPLAY(), GDK_ROOT_WINDOW(), atom,
                  XA_WINDOW, HILDON_IM_WINDOW_ID_FORMAT, PropModeReplace,
                  (unsigned char *) &xid, 1);

  gdk_window_add_filter(widget->window,
          (GdkFilterFunc) hildon_im_ui_client_message_filter, self);

  gdk_window_set_events(gdk_get_default_root_window(),
                        gdk_window_get_events(gdk_get_default_root_window()) |
                        GDK_PROPERTY_CHANGE_MASK);
  gdk_window_add_filter(gdk_get_default_root_window(),
                        hildon_im_ui_focus_message_filter, self);
}

static void
hildon_im_ui_finalize(GObject *obj)
{
  /* I think we never get this far */
  HildonIMUI *self;

  g_return_if_fail(HILDON_IM_IS_UI(obj));
  self = HILDON_IM_UI(obj);

  cleanup_plugins (self);
  
  if (self->osso)
  {
    osso_deinitialize(self->osso);
  }

  if (self->priv->current_banner)
  {
    gtk_widget_destroy (self->priv->current_banner);
  }
  
  gconf_client_remove_dir(self->client, HILDON_IM_GCONF_DIR, NULL);
  g_object_unref(self->client);
  g_string_free(self->priv->plugin_buffer, TRUE);
  
  g_free(self->priv->cached_hkb_plugin_name);
  g_free(self->priv->cached_finger_plugin_name);
  g_free(self->priv->cached_stylus_plugin_name);

  GList *parsed_rc_files = self->priv->parsed_rc_files;
  for (; parsed_rc_files != NULL; parsed_rc_files = g_list_next (parsed_rc_files))
  {
    g_free ((gchar *) parsed_rc_files->data);
  }
  g_list_free (self->priv->parsed_rc_files);

  G_OBJECT_CLASS(hildon_im_ui_parent_class)->finalize(obj);
}

static void
detect_first_boot (HildonIMUI *self)
{
  gint i;
  DBusError dbus_error_code;

  /* First boot detection 
   * We check whether maemo-af-desktop (MAD) is running by requesting a
   * MAD service.
   */
  dbus_error_init (&dbus_error_code);
  if (self->priv->dbus_connection == NULL)
  {
    self->priv->dbus_connection = dbus_bus_get (DBUS_BUS_SESSION, &dbus_error_code);
    if (self->priv->dbus_connection == NULL)
    {
      g_warning ("DBUS Connection failed: %s\n", dbus_error_code.message);
      dbus_error_free (&dbus_error_code);
      return;
    }
  }
  
  i = dbus_bus_request_name (self->priv->dbus_connection, 
      MAD_SERVICE,
      DBUS_NAME_FLAG_DO_NOT_QUEUE,
      NULL);
  if (i == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
  {
    /* MAD is not running so it is likely that this is the first boot */
    self->priv->first_boot = 1;
    /* Return the name */
    dbus_bus_release_name (self->priv->dbus_connection, MAD_SERVICE, NULL);
  }
  else if (i == DBUS_REQUEST_NAME_REPLY_EXISTS)
  {
    self->priv->first_boot = 0;
  }
}


static void
hildon_im_ui_init(HildonIMUI *self)
{
  HildonIMUIPrivate *priv;
  osso_return_t status;

  g_return_if_fail(HILDON_IM_IS_UI(self));

  self->priv = priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
                                                  HILDON_IM_TYPE_UI,
                                                  HildonIMUIPrivate);

  priv->current_plugin = NULL;
  priv->surrounding = g_strdup("");
  priv->committed_preedit = g_strdup("");
  priv->plugin_buffer = g_string_new(NULL);
  priv->current_banner = NULL;

  /* default */
  priv->options = 0;
  priv->trigger = HILDON_IM_TRIGGER_FINGER;
  priv->transiency = 0;
  priv->repeat_done = FALSE;
  priv->repeat_timeout_id = 0;
  priv->long_press_timeout_id = 0;
  priv->pressed_button = -1;
  priv->keyboard_available = FALSE;
  self->client = gconf_client_get_default();

  priv->cached_hkb_plugin_name = NULL;
  priv->cached_finger_plugin_name = NULL;
  priv->cached_stylus_plugin_name = NULL;

  priv->parsed_rc_files = NULL;

  self->osso = osso_initialize(PACKAGE_OSSO, VERSION, FALSE, NULL);
  if (!self->osso)
  {
    g_warning("Could not initialize osso from " PACKAGE);
  }

  gconf_client_add_dir(self->client,
                       HILDON_IM_GCONF_DIR, GCONF_CLIENT_PRELOAD_ONELEVEL,
                       NULL);
  gconf_client_notify_add(self->client, HILDON_IM_GCONF_DIR,
                          hildon_im_ui_gconf_change_callback,
                          self, NULL, NULL);

  status = osso_hw_set_event_cb(self->osso, NULL, hildon_im_hw_cb, self);
  if (status != OSSO_OK)
  {
    g_warning("Could not register the osso_hw_set_event_cb");
  }
 
  gtk_widget_set_name (GTK_WIDGET (self), "hildon-input-method-ui");

  self->priv->plugins_available = init_plugins (self);
  hildon_im_ui_load_gconf(self);
  if (self->priv->plugins_available == FALSE)
  {
    g_warning ("Failed loading the plugins.");
    g_warning ("No IM will show.");
  }
  init_persistent_plugins(self);

#ifdef MAEMO_CHANGES
  priv->first_boot = TRUE;
#else
  priv->first_boot = FALSE;
#endif

  g_message("ui up and running");
}

static void
hildon_im_ui_class_init(HildonIMUIClass *klass)
{
  g_type_class_add_private(klass, sizeof(HildonIMUIPrivate));

  G_OBJECT_CLASS(klass)->finalize = hildon_im_ui_finalize;
}

/* Public methods **********************************************/

GtkWidget *
hildon_im_ui_new()
{
  HildonIMUI *self;
  Atom atoms[2];
  Window win;
  Display *dpy;

  /* We actually should buid a constructor to be able to set base dir (as
   * property) and window properties BEFORE realizing window and creating
   * plugins */

    
  dpy = GDK_DISPLAY();
  self = g_object_new(HILDON_IM_TYPE_UI,
                      "border-width", 1, "decorated", FALSE,
                      "accept-focus", FALSE,
                      NULL);

  gtk_widget_realize(GTK_WIDGET(self));
  gtk_window_set_default_size(GTK_WINDOW(self), -1, -1);

  g_object_set_data(G_OBJECT(GTK_WIDGET(self)->window),
                    "_NEW_WM_STATE", (gpointer) PropModeAppend);

  win = GDK_WINDOW_XID(GTK_WIDGET(self)->window);
  atoms[0] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
  atoms[1] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_INPUT", False);
  XChangeProperty(dpy, win, atoms[0], XA_ATOM, 32, PropModeReplace,
                  (unsigned char *) &atoms[1], 1);

  hildon_im_ui_init_root_window_properties(self);
  return GTK_WIDGET(self);
}

const gchar *
hildon_im_ui_get_surrounding(HildonIMUI *self)
{
  return self->priv->surrounding;
}

gint
hildon_im_ui_get_surrounding_offset(HildonIMUI *self)
{
  return self->priv->surrounding_offset;
}

HildonIMCommitMode
hildon_im_ui_get_commit_mode(HildonIMUI *self)
{
  return self->priv->commit_mode;
}

void
hildon_im_ui_restore_previous_mode(HildonIMUI *self)
{
  g_return_if_fail(HILDON_IM_IS_UI(self));
  /*
   * If plugin calls restore_previous_mode_real, there will be trouble. Stack
   * and return address you know. Everything will be ok if we do this instead.
   */
  g_timeout_add(1, (GSourceFunc) hildon_im_ui_restore_previous_mode_real,
                self);
}

static void
flush_plugins(HildonIMUI *self,
    PluginData *current, gboolean force)
{
  GSList *iter;

  for (iter = self->priv->all_methods; iter != NULL; iter = iter->next)
  {
    gboolean flush = TRUE;
    PluginData *i = iter->data;

    if (i != current && i->widget != NULL)
    {
      if (current != NULL)
      {
        /* If it is connected to special plugin then no flush necessary */
        if (i->info->special_plugin != NULL &&
            g_ascii_strcasecmp (i->info->special_plugin, current->info->name) == 0)
        {
          flush = FALSE;
        }
      }
      /* Do not flush if the plugin says not to */
      if (i->info->cached == TRUE && force == FALSE)
      {
        flush = FALSE;
      }

      if (flush == TRUE)
      {
        hildon_im_plugin_disable(HILDON_IM_PLUGIN(i->widget));
        i->enabled = FALSE;

        gtk_widget_destroy(i->widget);
        i->widget = NULL;
      } else
      {
        gtk_widget_hide (i->widget);
      }
    }
  }
}

static void
activate_plugin (HildonIMUI *self, PluginData *plugin,
                 gboolean init)
{
  gboolean activate_special = FALSE;
  
  g_return_if_fail (HILDON_IM_IS_UI(self));
  g_return_if_fail (plugin != NULL);

  if (plugin == self->priv->current_plugin &&
      plugin->widget != NULL)
    return;

  if (CURRENT_PLUGIN(self) && CURRENT_IM_PLUGIN(self))
    hildon_im_plugin_transition(CURRENT_IM_PLUGIN(self), TRUE);

  if (plugin->info->type == HILDON_IM_TYPE_SPECIAL || plugin->info->type == HILDON_IM_TYPE_SPECIAL_STANDALONE)
    activate_special = TRUE;

  flush_plugins(self, plugin, FALSE);

  /* Make sure current plugin is created and packed! */
  if (plugin->widget == NULL)
  {
    plugin->widget = GTK_WIDGET (hildon_im_plugin_create (self, plugin->filename));
    if (plugin->widget == NULL)
    {
      g_warning ("Unable create widget for %s", plugin->info->name);
      return;
    }
  }

  set_current_plugin (self, plugin);
  
  hildon_im_plugin_enable (CURRENT_IM_PLUGIN (self), init);
  self->priv->current_plugin->enabled = TRUE;
  hildon_im_plugin_transition(CURRENT_IM_PLUGIN(self), FALSE);
}

void 
hildon_im_ui_activate_plugin (HildonIMUI *self, 
    gchar *name,
    gboolean init)
{
  PluginData *plugin;

  plugin = find_plugin_by_name (self, name);
  if (plugin)
    return activate_plugin (self, plugin, init);

  g_warning ("Plugin %s is not found in the plugin cache.", name);
}


/* Returns pointer to next packet start */
static const char *
get_next_packet_start(const char *utf8)
{
  const char *candidate, *good;

  candidate = good = utf8;

  while (*candidate)
  {
    candidate = g_utf8_next_char(good);
    if (candidate - utf8 >= HILDON_IM_CLIENT_MESSAGE_BUFFER_SIZE)
    {
      return good;
    }
    good = candidate;
  }

  /* The whole string is small enough */
  return candidate;
}

static void
hildon_im_ui_send_event(HildonIMUI *self, Window window, XEvent *event)
{
  gint xerror;

  g_return_if_fail(HILDON_IM_IS_UI(self));
  g_return_if_fail(event);

  if(window != None )
  {
    event->xclient.type = ClientMessage;
    event->xclient.window = window;

    /* trap X errors. Sometimes we recieve a badwindow error,
     * because the input_window id is wrong.
     */
    gdk_error_trap_push();

    XSendEvent(GDK_DISPLAY(), window, False, 0, event);
    XSync( GDK_DISPLAY(), False );

    xerror = gdk_error_trap_pop();
    if( xerror )
    {
      if( xerror == BadWindow )
      {
        /* Here we prevent the self from crashing */
        /*g_warning( "UI recieved a BadWindow error."
          " This was most probably caused by a false "
          "input window id. Please tap on the text "
          "widget again to update the XID. \n" );   */
      }
      else
      {
        g_warning( "Received the X error %d\n", xerror );
      }
    }
  }
}

void
hildon_im_ui_send_utf8(HildonIMUI *self, const gchar *utf8)
{
  HildonIMInsertUtf8Message *msg=NULL;
  XEvent event;
  gint flag;

  g_return_if_fail(HILDON_IM_IS_UI(self));

  if (utf8 == NULL || self->priv->input_window == None)
  {
    return;
  }

  flag = HILDON_IM_MSG_START;

  /* Split utf8 text into pieces that are small enough */
  do
  {
    const gchar *next_start;
    gsize len;

    next_start = get_next_packet_start(utf8);
    len = next_start - utf8;
    g_return_if_fail (0 <= len && len < HILDON_IM_CLIENT_MESSAGE_BUFFER_SIZE);

    if (*next_start == '\0')
      flag = HILDON_IM_MSG_END;

    /*this call will take care of adding the null terminator*/
    memset( &event, 0, sizeof(XEvent) );
    event.xclient.message_type =
            hildon_im_protocol_get_atom( HILDON_IM_INSERT_UTF8 );
    event.xclient.format = HILDON_IM_INSERT_UTF8_FORMAT;

    msg = (HildonIMInsertUtf8Message *) &event.xclient.data;
    msg->msg_flag = flag;
    memcpy(msg->utf8_str, utf8, len);

    hildon_im_ui_send_event(self, self->priv->input_window, &event);

    utf8 = (gchar *) next_start;
    flag = HILDON_IM_MSG_CONTINUE;
  } while (*utf8);
}

void
hildon_im_ui_send_communication_message(HildonIMUI *self,
                                        gint message)
{
  HildonIMComMessage *msg;
  XEvent event;

  g_return_if_fail(HILDON_IM_IS_UI(self));

  memset( &event, 0, sizeof(XEvent) );
  event.xclient.message_type = hildon_im_protocol_get_atom(HILDON_IM_COM);
  event.xclient.format = HILDON_IM_COM_FORMAT;
  msg = (HildonIMComMessage *) &event.xclient.data;
  msg->input_window = self->priv->input_window;
  msg->type = message;
  msg->options = self->priv->options;

  hildon_im_ui_send_event(self, self->priv->input_window, &event);
}

void
hildon_im_ui_send_surrounding_content(HildonIMUI *self,
                                      const gchar *surrounding)
{
  HildonIMSurroundingContentMessage *msg=NULL;
  XEvent event;
  gint flag;

  g_return_if_fail(HILDON_IM_IS_UI(self));

  if (surrounding == NULL || self->priv->input_window == None)
  {
    return;
  }
  flag = HILDON_IM_MSG_START;

  /* Split surrounding context into pieces that are small enough */
  do
  {
    const gchar *next_start;
    gsize len;

    next_start = get_next_packet_start(surrounding);
    len = next_start - surrounding;
    g_return_if_fail(0 <= len && len < HILDON_IM_CLIENT_MESSAGE_BUFFER_SIZE);

    /*this call will take care of adding the null terminator*/
    memset( &event, 0, sizeof(XEvent) );
    event.xclient.message_type =
            hildon_im_protocol_get_atom( HILDON_IM_SURROUNDING_CONTENT );
    event.xclient.format = HILDON_IM_SURROUNDING_CONTENT_FORMAT;

    msg = (HildonIMSurroundingContentMessage *) &event.xclient.data;
    msg->msg_flag = flag;
    memcpy(msg->surrounding, surrounding, len);

    hildon_im_ui_send_event(self, self->priv->input_window, &event);

    surrounding = next_start;
    flag = HILDON_IM_MSG_CONTINUE;
  } while (*surrounding);

  /* Send end marker to commit changes to the client widget */
  memset( &event, 0, sizeof(XEvent) );
  event.xclient.message_type =
    hildon_im_protocol_get_atom( HILDON_IM_SURROUNDING_CONTENT );
  event.xclient.format = HILDON_IM_SURROUNDING_CONTENT_FORMAT;
  msg = (HildonIMSurroundingContentMessage *) &event.xclient.data;
  msg->msg_flag = HILDON_IM_MSG_END;
  hildon_im_ui_send_event(self, self->priv->input_window, &event);
}

void
hildon_im_ui_send_surrounding_offset(HildonIMUI *self,
                                     gboolean is_relative,
                                     gint offset)
{
  HildonIMSurroundingMessage *msg;
  XEvent event;

  g_return_if_fail(HILDON_IM_IS_UI(self));

  memset( &event, 0, sizeof(XEvent) );
  event.xclient.message_type = hildon_im_protocol_get_atom(HILDON_IM_SURROUNDING);
  event.xclient.format = HILDON_IM_SURROUNDING_FORMAT;
  msg = (HildonIMSurroundingMessage *) &event.xclient.data;
  msg->offset_is_relative = is_relative;
  msg->cursor_offset = offset;

  hildon_im_ui_send_event(self, self->priv->input_window, &event);
}

HildonGtkInputMode
hildon_im_ui_get_current_input_mode(HildonIMUI *self)
{
  g_return_val_if_fail(HILDON_IM_IS_UI(self), 0);
  return self->priv->input_mode;
}

HildonGtkInputMode
hildon_im_ui_get_current_default_input_mode(HildonIMUI *self)
{
  g_return_val_if_fail(HILDON_IM_IS_UI(self), 0);
  return self->priv->default_input_mode;
}

HildonIMCommand
hildon_im_ui_get_autocase_mode(HildonIMUI *self)
{
  g_return_val_if_fail(HILDON_IM_IS_UI(self), HILDON_IM_LOW);
  return self->priv->autocase;
}

gboolean
hildon_im_ui_get_visibility(HildonIMUI *self)
{
  return FALSE;
}

static gboolean
hildon_im_ui_play_sound_done(gpointer data)
{
  HildonIMUI *self = HILDON_IM_UI(data);
  g_return_val_if_fail(HILDON_IM_IS_UI(self), FALSE);

  if (self->priv->sound_timeout_id)
  {
    g_source_remove(self->priv->sound_timeout_id);
    self->priv->sound_timeout_id = 0;
  }

  return FALSE;
}

void
hildon_im_ui_play_sound(HildonIMUI *self, HildonIMUISound sound)
{
  g_return_if_fail(HILDON_IM_IS_UI(self));

  if (self->priv->sound_timeout_id == 0)
  {
    const gchar *sound_path;
    gint repeat_rate;

    switch (sound)
    {
      case HILDON_IM_ILLEGAL_INPUT_SOUND:
        sound_path = ILLEGAL_CHARACTER_SOUND_PATH;
        repeat_rate = SOUND_REPEAT_ILLEGAL_CHARACTER;
        break;
      case HILDON_IM_NUMBER_INPUT_SOUND:
        sound_path = NUMBER_INPUT_SOUND_PATH;
        repeat_rate = SOUND_REPEAT_NUMBER_INPUT;
        break;
      case HILDON_IM_FINGER_TRIGGER_SOUND:
        sound_path = FINGER_TRIGGER_SOUND_PATH;
        repeat_rate = SOUND_REPEAT_FINGER_TRIGGER;
        break;
      default:
        g_warning ("No sound defined for id: %d", (int) sound);
        return;
    }

    self->priv->sound_timeout_id =
      g_timeout_add(repeat_rate,
                    hildon_im_ui_play_sound_done,
                    self);

    hildon_play_system_sound(sound_path);
  }
}

void
hildon_im_ui_set_context_options (HildonIMUI *self,
                                  HildonIMOptionMask options,
                                  gboolean enable)
{
  HildonIMOptionMask old_options;

  g_return_if_fail(HILDON_IM_IS_UI(self));

  old_options = self->priv->options;

  if (enable)
  {
    self->priv->options |= options;
  }
  else
  {
    self->priv->options &= ~options;
  }

  if (old_options != self->priv->options)
  {
    hildon_im_ui_send_communication_message(self, 
                                            HILDON_IM_CONTEXT_OPTION_CHANGED);
  }
}

const gchar *
hildon_im_ui_get_language_setting (HildonIMUI *ui, gint index)
{
  g_return_val_if_fail (ui != NULL, NULL);
  g_return_val_if_fail (index >= 0 && index < NUM_LANGUAGES, NULL);

  return ui->priv->selected_languages [index];
}

gboolean
hildon_im_ui_set_language_setting (HildonIMUI *ui, gint index,
                                   const gchar *new)
{
  g_return_val_if_fail (index >= 0 && index < NUM_LANGUAGES, FALSE);
  if (g_ascii_strcasecmp (new, hildon_im_ui_get_language_setting (ui,
          index)) != 0)
  {
    /* the priv->selected_languages will be populated in the gconf watch */
    return gconf_client_set_string (ui->client, language_gconf [index], new, NULL);
  }
  return FALSE;
}

const gchar *
hildon_im_ui_get_active_language (HildonIMUI *ui)
{

  return ui->priv->selected_languages [ui->priv->current_language_index];
}

gint
hildon_im_ui_get_active_language_index (HildonIMUI *ui)
{
  return ui->priv->current_language_index;
}

gboolean
hildon_im_ui_set_active_language_index (HildonIMUI *ui, gint index)
{
  g_return_val_if_fail (index >= 0 && index < NUM_LANGUAGES, FALSE);
  if (index == ui->priv->current_language_index)
    return FALSE;

  return gconf_client_set_int (ui->client, GCONF_CURRENT_LANGUAGE, index, NULL); 
}

const gchar *
hildon_im_ui_get_plugin_buffer(HildonIMUI *ui)
{
  return ui->priv->plugin_buffer->str;
}

void
hildon_im_ui_append_plugin_buffer(HildonIMUI *ui, const gchar *val)
{
  g_string_append(ui->priv->plugin_buffer, val);
}

void
hildon_im_ui_erase_plugin_buffer(HildonIMUI *ui, gint len)
{
  const gchar *p, *str;
  gsize prefix_len;
  int i;

  if (ui->priv->plugin_buffer->len == 0)
    return;

  str = ui->priv->plugin_buffer->str;
  p = &str[strlen(str)];
  for (i = 0; i < len; i++)
  {
    p = g_utf8_find_prev_char(str, p);
  }

  prefix_len = p - str;
  g_string_truncate(ui->priv->plugin_buffer, prefix_len);
}

void
hildon_im_ui_clear_plugin_buffer(HildonIMUI *ui)
{
  g_string_truncate(ui->priv->plugin_buffer, 0);
}

void
hildon_im_ui_set_visible(HildonIMUI *ui, gboolean visible)
{
  /* always hidden */
}

static gboolean
hildon_im_ui_rc_file_is_parsed (HildonIMUI *ui, gchar *rc_file)
{
  if (!ui->priv->parsed_rc_files)
  {
    return FALSE;
  }

  GList *parsed_rc_files = g_list_first (ui->priv->parsed_rc_files);
  for (; parsed_rc_files != NULL; parsed_rc_files = g_list_next(parsed_rc_files))
  {
    gchar *current_rc_file = (gchar *) parsed_rc_files->data;
    if (g_strcmp0 (current_rc_file, rc_file) == 0)
    {
      return TRUE;
    }
  }

  return FALSE;
}

void
hildon_im_ui_parse_rc_file (HildonIMUI *ui, gchar *rc_file)
{
  if (!hildon_im_ui_rc_file_is_parsed (ui, rc_file))
  {
    gtk_rc_parse (rc_file);
    ui->priv->parsed_rc_files = g_list_prepend (ui->priv->parsed_rc_files, rc_file);
  }
}
