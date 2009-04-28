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

#define SOUND_REPEAT_ILLEGAL_CHARACTER 800
#define SOUND_REPEAT_NUMBER_INPUT 0
#define SOUND_REPEAT_FINGER_TRIGGER 1500

#define HILDON_IM_UI_ICON_SIZE 26
#define CACHED_BUTTON_PIXMAP_COUNT 3

#define PACKAGE_OSSO    "hildon_input_method"
#define HILDON_COMMON_STRING  "hildon-common-strings"

/* Hardcoded values for key repeat. */
#define KEYREPEAT_THRESHOLD     800
#define KEYREPEAT_INTERVAL      167

#define IM_DEFAULT_HEIGHT 140
#define IM_FRAME_HEIGHT 10

#define IM_WINDOW_HEIGHT (IM_DEFAULT_HEIGHT + IM_FRAME_HEIGHT)

#define COMMON_B_WIDTH 78
#define COMMON_B_HEIGHT 35

#define BUTTON_ID_LEN 8

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

enum
{
  MENU_TYPE_APPLICATION,
  MENU_TYPE_CPA
};

static const gchar *language_gconf [NUM_LANGUAGES] = 
{
  HILDON_IM_GCONF_PRIMARY_LANGUAGE,
  HILDON_IM_GCONF_SECONDARY_LANGUAGE
};

typedef struct {
  HildonIMPluginInfo  *info;
  GSList              *languages;
  GtkWidget           *menu;      /* plugin menu item */
  GtkWidget           *widget;    /* actual IM plugin */

  gchar               *filename;
} PluginData;

typedef struct {
  HildonIMTrigger trigger;
  gint            type;
} TriggerType;

typedef GtkWidget *(*im_init_func)(HildonIMUI *);
typedef const HildonIMPluginInfo *(*im_info_func)(void);

struct button_info {
  GtkWidget *button;
  GtkWidget *menu;

  gchar id[BUTTON_ID_LEN];

  gboolean sensitive;
  gboolean toggle;
  gboolean repeat;
  gboolean long_press;
};

enum {
  EDIT_MENU_ITEM_CUT = 0,
  EDIT_MENU_ITEM_COPY,
  EDIT_MENU_ITEM_PASTE,

  NUM_EDIT_MENU_ITEMS
};

typedef struct
{
  gchar *title;
  gint  type;
  gchar *gettext_domain;
  gchar *command;
  gchar *dbus_service;
  gint  priority;
} HildonIMAdditionalMenuEntry;

struct _HildonIMUIPrivate
{
  GtkWidget *controlmenu;
  GtkWidget *menu_language_list;
  GtkWidget *menu_lang[NUM_LANGUAGES];
  GtkWidget *editmenuitem[NUM_EDIT_MENU_ITEMS];

  gchar selected_languages[NUM_LANGUAGES][BUFFER_SIZE];
  gint current_language_index;  /* to previous table */ 

  Window input_window;
  Window app_window;
  Window shown_app_window;

  GtkWidget *vbox;

  HildonGtkInputMode input_mode;
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
  gint menu_button;
  gboolean repeat_done;
  guint repeat_timeout_id;
  guint long_press_timeout_id;

  gboolean first_boot;

  Window transiency;

  gboolean select_launches_fullscreen_plugin;
  gboolean return_key_pressed;
  gboolean use_finger_kb;

  GtkWidget *menu;
  GSList *all_methods;
  GtkBox *im_box;
  gboolean has_special;

  gchar *cached_button_pixbuf_name[CACHED_BUTTON_PIXMAP_COUNT];
  GdkPixbuf *cached_button_pixbuf[CACHED_BUTTON_PIXMAP_COUNT];

  struct button_info buttons[HILDON_IM_NUM_BUTTONS];
  DBusConnection *dbus_connection;

  gboolean keyboard_available;
  GSList   *additional_menu;

  gboolean plugins_available;
  GSList   *last_plugins;
  PluginData *current_plugin;

  GtkWidget *menu_plugin_list;

  GString *plugin_buffer;
  gboolean enable_stylus_ui;
  gboolean ui_is_visible;

	gint width;

  osso_context_t *osso;

};

typedef struct
{
  const gchar *id;
  GType type;
} button_data_type;

static void hildon_im_ui_init(HildonIMUI *self);
static void hildon_im_ui_class_init(HildonIMUIClass *klass);
static void hildon_im_ui_button_pressed(GtkButton *button, gpointer data);
static void hildon_im_ui_button_released(GtkButton *button,
                                               gpointer data);
static void hildon_im_ui_button_enter(GtkButton *button, gpointer data);
static void hildon_im_ui_button_leave(GtkButton *button, gpointer data);
static void hildon_im_ui_get_work_area (HildonIMUI *self);

static GtkWidget *hildon_im_ui_create_control_menu(
        HildonIMUI *ui);
static void hildon_im_ui_input_method_selected(GtkWidget *widget,
       gpointer data);
static void hildon_im_ui_language_selected(GtkWidget *widget,
       gpointer data);

static void hildon_im_ui_process_button_click(HildonIMUI *self,
                                              HildonIMButton button,
                                              gboolean long_press);

static gboolean hildon_im_ui_long_press(gpointer data);
static gboolean hildon_im_ui_repeat_start(gpointer data);
static gboolean hildon_im_ui_repeat(gpointer data);

static void hildon_im_ui_send_event(HildonIMUI *self,
                                          Window window, XEvent *event);

static void hildon_im_ui_size_allocate (GtkWidget *widget,
                                              GtkAllocation *allocation);

static gboolean hildon_im_ui_expose(GtkWidget *self,
                                          GdkEventExpose *event);

static void hildon_im_ui_resize_window(HildonIMUI *self);

static void hildon_im_ui_button_set_label_real(
        HildonIMUI *self,
        HildonIMButton button,
        const gchar *label,
        gboolean clear_id);
static void hildon_im_ui_button_set_id_real(
        HildonIMUI *self,
        HildonIMButton button,
        const gchar *id,
        gboolean clear_label);
static void hildon_im_ui_button_set_menu_real(
        HildonIMUI *self,
        HildonIMButton button,
        GtkWidget *menu,
        gboolean automatic);
static void hildon_im_ui_button_set_toggle_real(
        HildonIMUI *self,
        HildonIMButton button,
        gboolean toggle,
        gboolean automatic);

static gboolean hildon_im_ui_restore_previous_mode_real(HildonIMUI *self);
static void flush_plugins(HildonIMUI *, PluginData *, gboolean);
static void hildon_im_hw_cb(osso_hw_state_t*, gpointer);

static void set_im_menu_sensitivity_for_language (HildonIMUI *);
static void detect_first_boot (HildonIMUI *self);

static void populate_additional_menu (HildonIMUI *self);
static void launch_additional_menu (GtkWidget *widget,
    gpointer data);

static void activate_plugin (HildonIMUI *s, PluginData *, gboolean);

static void hildon_im_ui_foreach_plugin(HildonIMUI *self,
                                        void (*function) (HildonIMPlugin *));

static void hide_controlmenu (HildonIMUI *self);

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
find_plugin_by_trigger_type (HildonIMUI *self, HildonIMTrigger trigger,
    gint type)
{
  GSList *found;
  TriggerType tt;

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
last_plugin_by_trigger_type (HildonIMUI *self, HildonIMTrigger trigger,
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

static void
set_current_plugin_by_name (HildonIMUI *self, const gchar *name)
{
  PluginData *plugin;

  if (name == NULL)
  {
    g_warning ("No input method plugin specified.");
    g_warning ("Will try to get the available plugin.");
    plugin = find_plugin_by_trigger_type (self,
                            HILDON_IM_TRIGGER_KEYBOARD, HILDON_IM_TYPE_DEFAULT);
  }
  else
  {
    plugin = find_plugin_by_name (self, name); 

    if (plugin == NULL)
    {
      g_warning ("Input method specified in the configuration: %s.",
                 name);
      g_warning ("However this IM is not available.");
      g_warning ("Will try to get the available plugin.");
      plugin = find_plugin_by_trigger_type (self,
                            HILDON_IM_TRIGGER_KEYBOARD, HILDON_IM_TYPE_DEFAULT);
    }
  }

  if (plugin == NULL)
  {
    g_warning ("Unable to set any input method.");
  } else {
    activate_plugin (self, plugin, TRUE);
  }
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
    }
  }
}

static void
init_plugin_menu (HildonIMUI *self)
{
  PluginData *current_plugin;
  GtkWidget *menu;
  GSList *group_menu;
  GSList *iter;
  gint group_id = -1;
  gboolean im_set = FALSE;

  menu = self->priv->menu_plugin_list;
  if (menu == NULL)
  {
    g_warning ("Plugin list menu is missing");
    return;
  }

  current_plugin = CURRENT_PLUGIN (self);
  if (current_plugin != NULL)
  {
    group_id = current_plugin->info->group;   
  }
 
  group_menu = NULL;
  
  for (iter = self->priv->all_methods; iter != NULL; iter = iter->next)
  {
    PluginData *plugin = (PluginData *) iter->data;
    if (plugin->info->visible_in_menu &&
        plugin->info->menu_title != NULL) 
    {
      if (group_id != -1)
      {
        if (plugin->info->group != group_id)
          continue;
      }
      plugin->menu = gtk_radio_menu_item_new_with_label(group_menu,
          dgettext (plugin->info->gettext_domain, plugin->info->menu_title));
    
      gtk_menu_shell_append (GTK_MENU_SHELL(menu), plugin->menu);
      group_menu = gtk_radio_menu_item_get_group (
          GTK_RADIO_MENU_ITEM (plugin->menu));

      if (plugin == current_plugin)
      {
        gtk_check_menu_item_set_active (
            GTK_CHECK_MENU_ITEM (plugin->menu), TRUE);
        im_set = TRUE;
      }

      g_signal_connect (G_OBJECT (plugin->menu), "activate",
          G_CALLBACK (hildon_im_ui_input_method_selected),
          self);
    }
  }
}

static void
init_language_menu (HildonIMUI *self)
{
  GSList *group;
  GtkWidget *menu;
  GtkWidget *submenu;
  GtkWidget *menuitem;
  gchar *language, *converted_label;
  gint i;

  menu = self->priv->menu_language_list;
  if (menu == NULL)
  {
    g_warning ("Language list menu is missing");
    return;
  }

  for (i = 0; i < NUM_LANGUAGES; i++)
  {
    menuitem = self->priv->menu_lang [i];
    if (menuitem != NULL)
      gtk_widget_destroy (menuitem);
  }

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (menu));
  if (submenu == NULL)
    submenu = gtk_menu_new();
  else 
  {
    gtk_widget_destroy (submenu);
    submenu = gtk_menu_new();
  }
  group = NULL;
  gtk_menu_item_set_submenu (GTK_MENU_ITEM(menu), submenu);

  language = gconf_client_get_string (self->client,
      HILDON_IM_GCONF_PRIMARY_LANGUAGE, NULL);

  if (language == NULL)
    language = g_strdup ("en");

  converted_label = hildon_im_get_language_description (language);
  if (converted_label == NULL)
    converted_label = g_strdup (language);

  g_free(language);

  menuitem = self->priv->menu_lang [PRIMARY_LANGUAGE] =
    gtk_radio_menu_item_new_with_label(group, converted_label);
  g_free(converted_label);

  gtk_menu_shell_append (GTK_MENU_SHELL(submenu), menuitem);
  group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitem));

  language = gconf_client_get_string (self->client,
      HILDON_IM_GCONF_SECONDARY_LANGUAGE, NULL);
  if (language == NULL || *language == '\0')
  {
    /* Add a placeholder for secondary language */
    menuitem = self->priv->menu_lang [SECONDARY_LANGUAGE] =
        gtk_radio_menu_item_new_with_label (group, "");

    gtk_menu_shell_append (GTK_MENU_SHELL(submenu), menuitem);

    /* no secondary language set. dim the whole language selection menu. */
    gtk_widget_set_sensitive (menu, FALSE);
  }
  else
  {
    converted_label = hildon_im_get_language_description(language);    
    if (converted_label == NULL)
      converted_label = g_strdup (language);
    g_free(language);

    menuitem = self->priv->menu_lang [SECONDARY_LANGUAGE] =
      gtk_radio_menu_item_new_with_label (group, converted_label);
    g_free(converted_label);
    gtk_widget_set_sensitive (menu, TRUE);

    gtk_menu_shell_append (GTK_MENU_SHELL(submenu), menuitem);
  }

  /* Set selected language. */
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(
          self->priv->menu_lang [self->priv->current_language_index]),
                                 TRUE);
  for (i = 0; i < NUM_LANGUAGES; i++)
  {
    if (self->priv->menu_lang[i])
    {
      g_signal_connect(G_OBJECT(self->priv->menu_lang[i]), "activate",
                       G_CALLBACK(hildon_im_ui_language_selected),
                       self);
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
      if (plugin->menu)
        gtk_widget_destroy (plugin->menu);

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
reload_plugins (HildonIMUI *self)
{
  self->priv->plugins_available = init_plugins (self);
  if (self->priv->plugins_available == FALSE)
  {
    g_warning ("Failed loading the plugins.");
    g_warning ("No IM will show.");
    return;
  }

  init_plugin_menu (self);
  init_language_menu (self);
}

/**
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

  hide_controlmenu (self);

  if (self->priv->current_plugin != NULL &&
      CURRENT_IM_WIDGET (self) != NULL)
  {
    hildon_im_plugin_disable (CURRENT_IM_PLUGIN (self));
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
set_plugin_to_stylus_im (HildonIMUI *self)
{
  gchar *plugin_name;

  plugin_name = gconf_client_get_string(self->client,
                                        GCONF_INPUT_METHOD,
                                        NULL);
  if (plugin_name)
  {
    set_current_plugin_by_name (self, plugin_name);
    g_free (plugin_name);
  }
}


static void
hildon_im_ui_show(HildonIMUI *self)
{
  g_return_if_fail(HILDON_IM_IS_UI(self));

  if (self->priv->enable_stylus_ui == FALSE &&
      self->priv->trigger == HILDON_IM_TRIGGER_STYLUS &&
      self->priv->keyboard_available == FALSE &&
      self->priv->ui_is_visible == TRUE)
  {
    return;
  }
  
  if ((self->priv->keyboard_available || !self->priv->use_finger_kb) &&
      (self->priv->trigger == HILDON_IM_TRIGGER_FINGER))
  {
    return;
  }

  /* Remember the window which caused the IM to be shown, so we can send
     hide event to the same window when we're hiding */
  self->priv->shown_app_window = self->priv->app_window;

  if (self->priv->trigger == HILDON_IM_TRIGGER_FINGER)
  {
    PluginData *plugin;

    plugin = find_plugin_by_trigger_type (self, HILDON_IM_TRIGGER_FINGER, -1);
    if (plugin == NULL) 
    {
      self->priv->trigger = HILDON_IM_TRIGGER_STYLUS;
      plugin = find_plugin_by_trigger_type (self, HILDON_IM_TRIGGER_STYLUS, -1);
      if  (plugin == NULL)
      {
        g_warning ("No finger plugin installed");
        return;
      }
    }

    /* If the plugin is loaded, but now shown, ask it to reshow itself */
    if (plugin->widget != NULL)
    {
      set_current_plugin (self, plugin);
      hildon_im_plugin_enable (HILDON_IM_PLUGIN(plugin->widget), FALSE);
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
       Restore the previous plugin for a consistent state. */
    if (self->priv->current_plugin != NULL  &&
        CURRENT_PLUGIN_IS_FULLSCREEN(self))
    {
      hildon_im_ui_restore_previous_mode_real (self);
    }
  }

  /* IM may not be loaded yet!
   * TODO is hildon_im_plugin_enable called too many times? */
  activate_plugin (self, self->priv->current_plugin, TRUE);
  hildon_im_plugin_enable (HILDON_IM_PLUGIN(self->priv->current_plugin->widget), FALSE);
  /* The plugin draws itself, or it doesn't need a UI */
  if (self->priv->current_plugin != NULL &&
      (self->priv->current_plugin->info->type == HILDON_IM_TYPE_HIDDEN 
          || self->priv->current_plugin->info->type == HILDON_IM_TYPE_SPECIAL_STANDALONE))
      return;
  if (self->priv->ui_is_visible && GTK_WIDGET_VISIBLE (self->priv->current_plugin->widget))
    gtk_widget_show(GTK_WIDGET(self));
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

static gint
get_button_enum(HildonIMUIPrivate *priv, GtkWidget *button)
{
  gint i;

  for (i = 0; i < HILDON_IM_NUM_BUTTONS; i++)
  {
    if (priv->buttons[i].button == button)
    {
      return i;
    }
  }
  return -1;
}

static gboolean
hildon_im_ui_restore_previous_mode_real(HildonIMUI *self)
{
  PluginData *plugin;
  g_return_val_if_fail(HILDON_IM_IS_UI(self), FALSE);

  if (self->priv->keyboard_available)
  {
    plugin = last_plugin_by_trigger_type (self, HILDON_IM_TRIGGER_KEYBOARD,
                                          HILDON_IM_TYPE_DEFAULT);
  }
  else
  {
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

/* Signal handler for all of the main buttons */
static void
hildon_im_ui_process_button_click(HildonIMUI *self,
                                  HildonIMButton button,
                                  gboolean long_press)
{
  PluginData *info;
  g_return_if_fail(HILDON_IM_IS_UI(self));

  info = CURRENT_PLUGIN (self);

  if (self->priv->current_plugin == NULL)
    return;

  hildon_im_plugin_button_activated(CURRENT_IM_PLUGIN (self),
                                    button, long_press);

  if (self->priv->buttons[button].menu != NULL)
  {
    if (hildon_im_ui_button_get_active(self, button) == TRUE)
    {
      gtk_menu_popup(GTK_MENU(self->priv->buttons[button].menu),
                     NULL, NULL, NULL, NULL, 0,
                     gtk_get_current_event_time());
      self->priv->menu_button = button;
    }
    return;
  }

  switch (button)
  {
    case HILDON_IM_BUTTON_TAB:
      hildon_im_plugin_tab (CURRENT_IM_PLUGIN (self));
      break;

    case HILDON_IM_BUTTON_MODE_A:
      hildon_im_plugin_mode_a (CURRENT_IM_PLUGIN (self));
      break;

    case HILDON_IM_BUTTON_MODE_B:
      hildon_im_plugin_mode_b (CURRENT_IM_PLUGIN (self));
      break;

    case HILDON_IM_BUTTON_BACKSPACE:
      hildon_im_plugin_backspace (CURRENT_IM_PLUGIN (self));
      break;

    case HILDON_IM_BUTTON_ENTER:
      hildon_im_plugin_enter (CURRENT_IM_PLUGIN (self));
      break;

    case HILDON_IM_BUTTON_SPECIAL_CHAR:
      if (info->info->special_plugin != NULL)
      {
        hildon_im_ui_activate_plugin (self, info->info->special_plugin, TRUE);
      } else {
        hildon_im_ui_restore_previous_mode_real(self);
      }
      break;

    case HILDON_IM_BUTTON_CLOSE:
      hildon_im_ui_hide(GTK_WIDGET(self));
      break;

    default: break;
  }
}

static void
hildon_im_ui_button_pressed(GtkButton *button, gpointer data)
{
  HildonIMUI *self;
  GtkWidget *widget;

  g_return_if_fail(HILDON_IM_IS_UI(data));
  self = HILDON_IM_UI(data);
  widget = GTK_WIDGET(button);

  self->priv->repeat_done = FALSE;
  self->priv->long_press = FALSE;
  self->priv->pressed_button = get_button_enum(self->priv, GTK_WIDGET(button));
  if (self->priv->pressed_button == -1)
  {
    g_warning("Pressed unmapped button!?");
    return;
  }

  if (self->priv->buttons[self->priv->pressed_button].repeat == TRUE)
  {
    self->priv->repeat_timeout_id =
            g_timeout_add(KEYREPEAT_THRESHOLD, hildon_im_ui_repeat_start,
                          (gpointer) self);
  }

  if (self->priv->buttons[self->priv->pressed_button].long_press == TRUE)
  {
    self->priv->long_press_timeout_id =
      g_timeout_add(HILDON_WINDOW_LONG_PRESS_TIME, hildon_im_ui_long_press,
                    (gpointer) self);
  }

  self->priv->pressed_ontop = TRUE;
}

static void
hildon_im_ui_button_released(GtkButton *button, gpointer data)
{
  HildonIMUI *self;
  gint i;

  g_return_if_fail(HILDON_IM_IS_UI(data));
  self = HILDON_IM_UI(data);

  if (self->priv->repeat_timeout_id)
  {
    g_source_remove(self->priv->repeat_timeout_id);
    self->priv->repeat_timeout_id = 0;
  }

  if (self->priv->long_press_timeout_id)
  {
    g_source_remove(self->priv->long_press_timeout_id);
    self->priv->long_press_timeout_id = 0;
  }

  if (self->priv->long_press)
  {
    self->priv->long_press = FALSE;
    return;
  }

  i = get_button_enum(self->priv, GTK_WIDGET(button));
  if (i < 0                           ||
      self->priv->pressed_button != i ||
      self->priv->pressed_ontop == FALSE)
  {
    return;
  }

  self->priv->pressed_button = -1;
  if (self->priv->repeat_done == FALSE ||
      self->priv->buttons[i].repeat == FALSE)
  {
    hildon_im_ui_process_button_click(self, i, FALSE);
  }

  if (self->priv->buttons[i].toggle == FALSE)
  {
    hildon_im_ui_button_set_active(self, i, FALSE);
  }
  self->priv->repeat_done = FALSE;
}

static void
hildon_im_ui_button_enter(GtkButton *button, gpointer data)
{
  HildonIMUI *self;
  gint i;

  g_return_if_fail(HILDON_IM_IS_UI(data));
  self = HILDON_IM_UI(data);

  i = get_button_enum(self->priv, GTK_WIDGET(button));
  if (self->priv->pressed_button == i)
  {
    self->priv->pressed_ontop = TRUE;
  }
}

static void
hildon_im_ui_button_leave(GtkButton *button, gpointer data)
{
  HildonIMUI *self;
  gint i;

  g_return_if_fail(HILDON_IM_IS_UI(data));
  self = HILDON_IM_UI(data);

  i = get_button_enum(self->priv, GTK_WIDGET(button));
  if (self->priv->pressed_button == i)
  {
    self->priv->pressed_ontop = FALSE;

    self->priv->repeat_done = TRUE;
    if (self->priv->repeat_timeout_id)
    {
      g_source_remove(self->priv->repeat_timeout_id);
      self->priv->repeat_timeout_id = 0;
    }

    if (self->priv->long_press_timeout_id)
    {
      g_source_remove(self->priv->long_press_timeout_id);
      self->priv->long_press_timeout_id = 0;
    }
  }
}

static void
hildon_im_ui_menu_deactivated(GtkMenuShell *menu, gpointer data)
{
  HildonIMUI *self;

  g_return_if_fail(HILDON_IM_IS_UI(data));
  self = HILDON_IM_UI(data);
  if (self->priv->menu_button == -1)
  {
    return;
  }

  hildon_im_ui_button_set_active(self,
                                       self->priv->menu_button,
                                       FALSE);
  self->priv->menu_button = -1;
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
    plugin = find_plugin_by_trigger_type (self,
          HILDON_IM_TRIGGER_KEYBOARD, -1);
    if (plugin)
    {
      self->priv->trigger = HILDON_IM_TRIGGER_KEYBOARD;
      activate_plugin(self, plugin, TRUE);
    }
  }
  else
  {
    if (self->priv->enable_stylus_ui)
    {
      hildon_im_ui_hide(self);
      set_plugin_to_stylus_im (self);
    }
    else 
    {
      if (GTK_WIDGET_VISIBLE(self))
      {
        hildon_im_ui_hide(self);
      }
      flush_plugins(self, NULL, FALSE);
    }
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
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                self->priv->menu_lang[new_value]), TRUE);
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
      GtkWidget *label;
      const gchar *language = gconf_value_get_string(value);
      gchar *converted_label;
      GSList *iter;

      strncpy (self->priv->selected_languages [PRIMARY_LANGUAGE], language,
          strlen (language) > BUFFER_SIZE ? BUFFER_SIZE -1: strlen (language));

      label = gtk_bin_get_child(GTK_BIN(
              self->priv->menu_lang[PRIMARY_LANGUAGE]));
      if (GTK_IS_LABEL(label))
      {
        converted_label = hildon_im_get_language_description (language);
        if (converted_label == NULL)
          converted_label = g_strdup (language);

        gtk_label_set_text(GTK_LABEL(label), converted_label);
        g_free(converted_label);
      }

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
    set_im_menu_sensitivity_for_language (self);
  }
  else if (strcmp(key, HILDON_IM_GCONF_SECONDARY_LANGUAGE) == 0)
  {
    if (value->type == GCONF_VALUE_STRING)
    {
      gboolean lang_valid;
      GtkWidget *label;
      const gchar *language = gconf_value_get_string(value);
      gchar *converted_label;

      lang_valid = !(language == NULL || language [0] == 0);
      if (lang_valid)
      {
        strncpy (self->priv->selected_languages [SECONDARY_LANGUAGE], language,
          strlen (language) > BUFFER_SIZE ? BUFFER_SIZE -1: strlen (language));

        label = gtk_bin_get_child(GTK_BIN(
                self->priv->menu_lang[SECONDARY_LANGUAGE]));
        if (GTK_IS_LABEL(label))
        {
          converted_label = hildon_im_get_language_description(language);
          if (converted_label == NULL)
            converted_label = g_strdup (language);

          gtk_label_set_text(GTK_LABEL(label), converted_label);
          g_free(converted_label);
        }        
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
      gtk_widget_set_sensitive(self->priv->menu_language_list, lang_valid);
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
    set_im_menu_sensitivity_for_language (self);
  }
  else if (strcmp(key, HILDON_IM_GCONF_LAUNCH_FINGER_KB_ON_SELECT) == 0)
  {
    if (value->type == GCONF_VALUE_BOOL)
    {
      self->priv->select_launches_fullscreen_plugin = gconf_value_get_bool(value);
    }
  }
  else if (strcmp(key, HILDON_IM_GCONF_ENABLE_STYLUS_IM) == 0)
  {
    if (value->type == GCONF_VALUE_BOOL)
    {
      self->priv->enable_stylus_ui = gconf_value_get_bool(value);
      if (self->priv->keyboard_available == FALSE &&
          self->priv->enable_stylus_ui == TRUE)
      {
        set_plugin_to_stylus_im(self);
      }
      if (self->priv->enable_stylus_ui == FALSE)
        hildon_im_ui_hide(self);
    }
  }
  else if (strcmp(key, HILDON_IM_GCONF_USE_FINGER_KB) == 0)
  {
    if (value->type == GCONF_VALUE_BOOL)
    {
      self->priv->use_finger_kb = gconf_value_get_bool(value);
    }
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
}

static void
hildon_im_ui_load_gconf(HildonIMUI *self)
{
  gchar *language;
  gint size;
  GConfValue *gvalue;
  
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
    gconf_client_set_int (self->client,
                          GCONF_CURRENT_LANGUAGE, PRIMARY_LANGUAGE, NULL);
  }
  g_free(language);  

  self->priv->select_launches_fullscreen_plugin =
    gconf_client_get_bool(self->client, HILDON_IM_GCONF_LAUNCH_FINGER_KB_ON_SELECT, NULL);

  self->priv->use_finger_kb =
    gconf_client_get_bool(self->client, HILDON_IM_GCONF_USE_FINGER_KB, NULL);

  gvalue = gconf_client_get (self->client, HILDON_IM_GCONF_ENABLE_STYLUS_IM, NULL);
  if (gvalue == NULL)
  {
    self->priv->enable_stylus_ui = TRUE;
  } else
  {
    self->priv->enable_stylus_ui = gconf_value_get_bool (gvalue);
    gconf_value_free (gvalue);
  }
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
  if (self->priv->input_window != msg->input_window)
  {
    PluginData *info = NULL;
    self->priv->input_window = msg->input_window;

    info = CURRENT_PLUGIN (self);

    hildon_im_ui_send_communication_message(self,
          HILDON_IM_CONTEXT_WIDGET_CHANGED);

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

  /* update the mode with every message we receive, except hide */
  if (self->priv->input_mode != msg->input_mode &&
      msg->cmd != HILDON_IM_HIDE)
  {
    self->priv->input_mode = msg->input_mode;
#ifdef MAEMO_CHANGES
    if ((msg->input_mode & HILDON_GTK_INPUT_MODE_SPECIAL) ||
         (msg->input_mode & HILDON_GTK_INPUT_MODE_ALPHA) ||
         (msg->input_mode & HILDON_GTK_INPUT_MODE_TELE)
         )
    {
        hildon_im_ui_button_set_sensitive(self,
            HILDON_IM_BUTTON_SPECIAL_CHAR,
            TRUE);
    }
    else
    {
        hildon_im_ui_button_set_sensitive(self,
            HILDON_IM_BUTTON_SPECIAL_CHAR,
            FALSE);
    }
#endif

    if (plugin != NULL)
    {     
      hildon_im_plugin_input_mode_changed(plugin);      
    }
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
  if (CURRENT_PLUGIN (self) && CURRENT_IM_WIDGET (self) == NULL)
    return;

  self->priv->input_window = msg->input_window;

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
    else if (self->priv->select_launches_fullscreen_plugin)
    {
      PluginData *plugin;

      plugin = find_plugin_by_trigger_type (self, -1,
                                            HILDON_IM_TYPE_FULLSCREEN);
      if (plugin)
      {
        set_current_plugin (self, plugin);
        activate_plugin (self, plugin, TRUE);
      }
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
        hildon_im_protocol_get_atom( HILDON_IM_CLIPBOARD_SELECTION_REPLY)
        && cme->format == HILDON_IM_CLIPBOARD_SELECTION_REPLY_FORMAT)
    {
      gtk_widget_set_sensitive(self->priv->editmenuitem[EDIT_MENU_ITEM_CUT],
                               cme->data.l[0]);
      gtk_widget_set_sensitive(self->priv->editmenuitem[EDIT_MENU_ITEM_COPY],
                               cme->data.l[0]);
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

      hildon_im_plugin_surrounding_received(CURRENT_IM_PLUGIN (self),
                                            self->priv->surrounding,
                                            self->priv->surrounding_offset);
      
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
      hildon_banner_show_information (GTK_WIDGET(self), NULL,
        dgettext(HILDON_COMMON_STRING, "ecoc_ib_edwin_copied"));

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
    Atom active_window_atom =
            XInternAtom (GDK_DISPLAY(), "_NET_ACTIVE_WINDOW", False);
    Atom workarea_change_atom = 
	    XInternAtom (GDK_DISPLAY(), "_NET_WORKAREA", False);
    XPropertyEvent *prop = (XPropertyEvent *) xevent;

    if (prop->atom == active_window_atom && prop->window == GDK_ROOT_WINDOW())
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
                         0, 32, False, XA_WINDOW, &actual_type,
                         &actual_format, &nitems, &bytes_after,
                         &window_value.char_value);
      if (gdk_error_trap_pop() == 0)
      {
        if (nitems > 0 && window_value.window[0] != self->priv->transiency)
        {
          if (hildon_im_ui_x_window_want_im_hidden(
                  window_value.window[0]))
          {
            /* dialog / normal window,  */
            if (GTK_WIDGET_DRAWABLE(GTK_WIDGET(self)) && 
                self->priv->current_plugin != NULL)
            {
              /* Do not hide the UI when the window is another IM plugin */
              if (CURRENT_PLUGIN_IS_FULLSCREEN(self) == FALSE)
              {
                hildon_im_ui_hide(self);
              }
            }
          }
        }

        XFree(window_value.char_value);
      }
    }
    else if (CURRENT_PLUGIN (self) && CURRENT_PLUGIN_IS_FULLSCREEN(self))
    {
      Atom current_app_window_atom =
        XInternAtom (GDK_DISPLAY(), "_MB_CURRENT_APP_WINDOW", False);

      /* A new application was started, but it did not become the
         topmost window. Close the fullscreen plugin, showing the
         new window */
      if (prop->atom == current_app_window_atom &&
          prop->window == GDK_ROOT_WINDOW())
      {
        if (CURRENT_IM_WIDGET(self) != NULL)
        {
          hildon_im_ui_hide(self);
          flush_plugins(self, NULL, FALSE);
        }
      }
    }
    else if (prop->atom == workarea_change_atom && prop->window == GDK_ROOT_WINDOW())
    {
	    hildon_im_ui_get_work_area (self);
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
  gint i;

  g_return_if_fail(HILDON_IM_IS_UI(obj));
  self = HILDON_IM_UI(obj);

  cleanup_plugins (self);
  for (i = 0; i < CACHED_BUTTON_PIXMAP_COUNT; i++)
  {
    if (self->priv->cached_button_pixbuf_name[i] != NULL)
    {
      g_free(self->priv->cached_button_pixbuf_name[i]);
      g_object_unref(self->priv->cached_button_pixbuf[i]);
    }
  }

  for (i = 0; i < HILDON_IM_NUM_BUTTONS; i++)
  {
    if (self->priv->buttons[i].menu != NULL)
    {
      gtk_widget_destroy(self->priv->buttons[i].menu);
    }
  }

  if (self->osso)
  {
    osso_deinitialize(self->osso);
  }

  if (self->priv->menu)
  {
    gtk_widget_destroy(self->priv->menu);
  }

  gconf_client_remove_dir(self->client, HILDON_IM_GCONF_DIR, NULL);
  g_object_unref(self->client);
  g_string_free(self->priv->plugin_buffer, TRUE);

  G_OBJECT_CLASS(hildon_im_ui_parent_class)->finalize(obj);
}

static void
hildon_im_ui_targets_received_callback(GtkClipboard *clipboard,
                                       GdkAtom *targets,
                                       gint n_targets,
                                       gpointer data)
{
  HildonIMUI *self;
  gboolean have_text_target = FALSE;
  gint i;

  g_return_if_fail(HILDON_IM_IS_UI(data));
  self = HILDON_IM_UI(data);

  for (i = 0; i < n_targets; i++)
  {
    if (targets[i] == gdk_atom_intern("UTF8_STRING", FALSE) ||
        targets[i] == gdk_atom_intern("COMPOUND_TEXT", FALSE) ||
        targets[i] == GDK_TARGET_STRING)
    {
      have_text_target = TRUE;
      break;
    }
  }

  gtk_widget_set_sensitive(self->priv->editmenuitem[EDIT_MENU_ITEM_PASTE],
                           have_text_target);
}

static void
hildon_im_ui_show_controlmenu(GtkWidget *widget, gpointer data)
{
  HildonIMUI *self;

  g_return_if_fail(HILDON_IM_IS_UI(data));
  self = HILDON_IM_UI(data);

#ifdef MAEMO_CHANGES
  if (self->priv->input_mode & HILDON_GTK_INPUT_MODE_INVISIBLE)
  {
    /* copy/cut not allowed in invisible entries */
    gtk_widget_set_sensitive(self->priv->editmenuitem[EDIT_MENU_ITEM_CUT],
           FALSE);
    gtk_widget_set_sensitive(self->priv->editmenuitem[EDIT_MENU_ITEM_COPY],
           FALSE);
  }
  else
  {
    /* ask from main app if there's any text selected so we can disable
     cut/copy */
    hildon_im_ui_send_communication_message(self,
          HILDON_IM_CONTEXT_CLIPBOARD_SELECTION_QUERY);

  }
#endif

  /* disable paste if there's no text in clipboard */
  gtk_clipboard_request_targets(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),
                                hildon_im_ui_targets_received_callback,
                                self);
}

static void
set_basic_buttons(HildonIMUI *self)
{
  hildon_im_ui_button_set_sensitive(self,
      HILDON_IM_BUTTON_TAB, TRUE);
  hildon_im_ui_button_set_toggle(self, 
      HILDON_IM_BUTTON_TAB, FALSE);
  hildon_im_ui_button_set_id(self, 
      HILDON_IM_BUTTON_TAB,
      "qgn_inpu_common_tab");
  hildon_im_ui_button_set_repeat(self, 
      HILDON_IM_BUTTON_TAB, FALSE);

  hildon_im_ui_button_set_sensitive(self,
      HILDON_IM_BUTTON_BACKSPACE, TRUE);
  hildon_im_ui_button_set_toggle(self, 
      HILDON_IM_BUTTON_BACKSPACE, FALSE);
  hildon_im_ui_button_set_id(self, 
      HILDON_IM_BUTTON_BACKSPACE,
      "qgn_inpu_common_backspace");
  hildon_im_ui_button_set_repeat(self, 
      HILDON_IM_BUTTON_BACKSPACE, TRUE);

  hildon_im_ui_button_set_sensitive(self, 
      HILDON_IM_BUTTON_ENTER, TRUE);
  hildon_im_ui_button_set_toggle(self, 
      HILDON_IM_BUTTON_ENTER, FALSE);
  hildon_im_ui_button_set_id(self, 
      HILDON_IM_BUTTON_ENTER,
      "qgn_inpu_common_enter");
  hildon_im_ui_button_set_repeat(self, 
      HILDON_IM_BUTTON_ENTER, TRUE);

  hildon_im_ui_button_set_sensitive(self, 
      HILDON_IM_BUTTON_SPECIAL_CHAR,
      FALSE);
  hildon_im_ui_button_set_toggle(self,
      HILDON_IM_BUTTON_SPECIAL_CHAR, TRUE);
  hildon_im_ui_button_set_id(self,
      HILDON_IM_BUTTON_SPECIAL_CHAR,
      "qgn_inpu_common_special");
  hildon_im_ui_button_set_repeat(self,
      HILDON_IM_BUTTON_SPECIAL_CHAR, FALSE);
}

static void
hildon_im_ui_unlatch_special(GtkWidget *widget, gpointer data)
{
  HildonIMUI *self;
  self = HILDON_IM_UI(data);

  hildon_im_ui_button_set_active(self,
      HILDON_IM_BUTTON_SPECIAL_CHAR, FALSE);
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
  GtkWidget *vbox_left;
  GtkWidget *vbox_right;
  GtkWidget *hbox;
  gint i;
  GtkIconTheme *icon_theme;
  osso_return_t status;

  g_return_if_fail(HILDON_IM_IS_UI(self));

  self->priv = priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
                                                  HILDON_IM_TYPE_UI,
                                                  HildonIMUIPrivate);

  priv->current_plugin = NULL;
  priv->surrounding = g_strdup("");
  priv->committed_preedit = g_strdup("");
  priv->plugin_buffer = g_string_new(NULL);

  /* initialize buttons */
  for (i = 0; i < HILDON_IM_NUM_BUTTONS; i++)
  {
    priv->buttons[i].button = gtk_toggle_button_new();
    gtk_container_add(GTK_CONTAINER(priv->buttons[i].button), gtk_image_new());

    g_signal_connect(priv->buttons[i].button, "pressed",
                     G_CALLBACK(hildon_im_ui_button_pressed),
                     (gpointer) self);
    g_signal_connect(priv->buttons[i].button, "released",
                     G_CALLBACK(hildon_im_ui_button_released),
                     (gpointer) self);
    g_signal_connect(priv->buttons[i].button, "enter",
                     G_CALLBACK(hildon_im_ui_button_enter),
                     (gpointer) self);
    g_signal_connect(priv->buttons[i].button, "leave",
                     G_CALLBACK(hildon_im_ui_button_leave),
                     (gpointer) self);
     
    priv->buttons[i].sensitive = TRUE;
    priv->buttons[i].toggle = FALSE;
    priv->buttons[i].id[0] = priv->buttons[i].id[BUTTON_ID_LEN - 1] = '\0';

    priv->buttons[i].menu = NULL;

    GTK_WIDGET_UNSET_FLAGS(priv->buttons[i].button, GTK_CAN_FOCUS);
    gtk_widget_set_size_request(priv->buttons[i].button,
                                COMMON_B_WIDTH, COMMON_B_HEIGHT);
  }

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
 
  priv->vbox = gtk_vbox_new(FALSE, 0);

  hbox = gtk_hbox_new(FALSE, 0);
  vbox_left = gtk_vbox_new(TRUE, 0);
  vbox_right = gtk_vbox_new(TRUE, 0);

  icon_theme = gtk_icon_theme_get_default();

  /* The rest. */
  for (i = HILDON_IM_NUM_BUTTONS - 1; i >= 4; i--)
  {
    gtk_box_pack_end(GTK_BOX(vbox_right), priv->buttons[i].button,
                     FALSE, FALSE, 0);
  }

  /* BUTTON_TAB ... BUTTON_INPUT_MENU */
  for (; i >= 0; i--)
  {
    gtk_box_pack_end(GTK_BOX(vbox_left), priv->buttons[i].button,
                     FALSE, FALSE, 0);
  }

  gtk_box_pack_start(GTK_BOX(hbox), vbox_left, FALSE, FALSE, 0);


  priv->im_box = GTK_BOX(hbox);
  gtk_widget_set_name (GTK_WIDGET (self), 
      "hildon-input-method-ui");

  gtk_box_pack_end(GTK_BOX(hbox), vbox_right, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(priv->vbox), hbox, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(self), priv->vbox);
  gtk_widget_show_all(vbox_left);
  gtk_widget_show_all(vbox_right);
  gtk_widget_show(hbox);
  gtk_widget_show(priv->vbox);
 
  set_basic_buttons(self);

  populate_additional_menu (self);
  priv->menu = hildon_im_ui_create_control_menu(self);  

  self->priv->plugins_available = init_plugins (self);
  hildon_im_ui_load_gconf(self);
  if (self->priv->plugins_available == FALSE)
  {
    g_warning ("Failed loading the plugins.");
    g_warning ("No IM will show.");
  }
  init_persistent_plugins(self);
  init_plugin_menu (self);
  init_language_menu (self);

  g_signal_connect(priv->menu, "show",
                   G_CALLBACK(hildon_im_ui_show_controlmenu), self);

  /* initialize static buttons */
  hildon_im_ui_button_set_id(self, 
      HILDON_IM_BUTTON_INPUT_MENU,
      "qgn_inpu_common_menu");
  /* setting menu will set toggle and repeat */
  hildon_im_ui_button_set_menu(self, 
      HILDON_IM_BUTTON_INPUT_MENU, priv->menu);
  hildon_im_ui_button_set_sensitive(self,
      HILDON_IM_BUTTON_INPUT_MENU, TRUE);

  hildon_im_ui_button_set_id(self, 
      HILDON_IM_BUTTON_CLOSE,
      "qgn_inpu_common_minimize");
  hildon_im_ui_button_set_sensitive(self, 
      HILDON_IM_BUTTON_CLOSE, TRUE);
  hildon_im_ui_button_set_toggle(self, 
      HILDON_IM_BUTTON_CLOSE, FALSE);
  hildon_im_ui_button_set_repeat(self, 
      HILDON_IM_BUTTON_CLOSE, FALSE);

  gtk_widget_set_size_request(GTK_WIDGET(self), 500, -1);

  set_im_menu_sensitivity_for_language (self);
  
#ifdef MAEMO_CHANGES
  priv->first_boot = TRUE;
#else
  priv->first_boot = FALSE;
#endif
  
  hildon_im_ui_button_set_sensitive(self, 
      HILDON_IM_BUTTON_INPUT_MENU, FALSE);

  if (CURRENT_PLUGIN(self) == NULL && self->priv->enable_stylus_ui)
    set_plugin_to_stylus_im (self);

  g_message("ui up and running");
}

static void
hildon_im_ui_class_init(HildonIMUIClass *klass)
{
  g_type_class_add_private(klass, sizeof(HildonIMUIPrivate));

  GTK_WIDGET_CLASS(klass)->size_allocate = hildon_im_ui_size_allocate;
  GTK_WIDGET_CLASS(klass)->expose_event = hildon_im_ui_expose;
  G_OBJECT_CLASS(klass)->finalize = hildon_im_ui_finalize;
}

#ifdef MAEMO_CHANGES
static void
hildon_im_ui_menu_selected(GtkWidget *widget, gpointer data)
{
  HildonIMUI *self;

  g_return_if_fail(HILDON_IM_IS_UI(data));
  self = HILDON_IM_UI(data);

  if (widget == self->priv->editmenuitem[EDIT_MENU_ITEM_CUT])
  {
    hildon_im_ui_send_communication_message(
            self, HILDON_IM_CONTEXT_CLIPBOARD_CUT);
  }
  else if (widget == self->priv->editmenuitem[EDIT_MENU_ITEM_COPY])
  {
    hildon_im_ui_send_communication_message(
            self, HILDON_IM_CONTEXT_CLIPBOARD_COPY);
    hildon_banner_show_information (GTK_WIDGET (data), NULL,
      dgettext(HILDON_COMMON_STRING, "ecoc_ib_edwin_copied"));

  }
  else if (widget == self->priv->editmenuitem[EDIT_MENU_ITEM_PASTE])
  {
    hildon_im_ui_send_communication_message(
            self, HILDON_IM_CONTEXT_CLIPBOARD_PASTE);
  }
}
#endif

static void
set_im_menu_sensitivity_for_language (HildonIMUI *self)
{
  GSList *iter = NULL;
  PluginData *plugin, *latest_supported = NULL;
  gboolean supported, supported_by_current = FALSE;
  const gchar *language = hildon_im_ui_get_active_language (self);

  for (iter = self->priv->all_methods; iter != NULL; iter = iter->next)
  {
    plugin = (PluginData*) iter->data;
    if (plugin->languages != NULL)
    {
      supported = (g_slist_find_custom (plugin->languages, language,
          (GCompareFunc) g_ascii_strcasecmp) != NULL);

      if (plugin->menu != NULL)
      {
        gtk_widget_set_sensitive (plugin->menu, supported);
      }

      if (supported == TRUE && 
          plugin->info->visible_in_menu)
      {
        latest_supported = plugin;
      }
      if (plugin == self->priv->current_plugin && supported == TRUE)
      {
        supported_by_current = TRUE;
      }
    } else if (plugin == self->priv->current_plugin)
    {
      supported_by_current = TRUE;
    }
  }

/*   if (supported_by_current == FALSE && */
/*       latest_supported != NULL) */
/*   { */
/*      activate_plugin (self, latest_supported, TRUE); */
/*   } */
}

static GtkWidget *
hildon_im_ui_create_control_menu(HildonIMUI *self)
{
  GtkWidget *menu;
  GtkWidget *submenu;
  GtkWidget *menuitem;
  GSList *iter;

  g_return_val_if_fail(HILDON_IM_IS_UI(self), NULL);

  menu = gtk_menu_new();

#ifdef MAEMO_CHANGES
  self->priv->editmenuitem[EDIT_MENU_ITEM_CUT] = menuitem =
          gtk_menu_item_new_with_label(_("inpu_nc_common_menu_edit_cut"));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
  g_signal_connect(G_OBJECT(menuitem), "activate",
                   G_CALLBACK(hildon_im_ui_menu_selected), self);
  self->priv->editmenuitem[EDIT_MENU_ITEM_COPY] = menuitem =
          gtk_menu_item_new_with_label(_("inpu_nc_common_menu_edit_copy"));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
  g_signal_connect(G_OBJECT(menuitem), "activate",
                   G_CALLBACK(hildon_im_ui_menu_selected), self);
  self->priv->editmenuitem[EDIT_MENU_ITEM_PASTE] = menuitem =
          gtk_menu_item_new_with_label(_("inpu_nc_common_menu_edit_paste"));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
  g_signal_connect(G_OBJECT(menuitem), "activate",
                   G_CALLBACK(hildon_im_ui_menu_selected), self);
  hildon_helper_set_insensitive_message (menuitem, 
      dgettext(HILDON_COMMON_STRING,"ecoc_ib_edwin_nothing_to_paste"));
#endif

  menuitem = gtk_menu_item_new_with_label(_("inpu_nc_common_menu_method"));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
  self->priv->menu_plugin_list = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), 
      self->priv->menu_plugin_list);

  self->priv->menu_language_list =
          gtk_menu_item_new_with_label(_("inpu_nc_common_menu_language"));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu),
                        self->priv->menu_language_list);

  menuitem = gtk_menu_item_new_with_label(_("inpu_nc_common_menu_tools"));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);


  submenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

  for (iter = self->priv->additional_menu; 
      iter != NULL; iter = iter->next)
  {
    HildonIMAdditionalMenuEntry *entry = iter->data;

    if (entry)
    {
      menuitem = gtk_menu_item_new_with_label (entry->title);
      gtk_menu_shell_append (GTK_MENU_SHELL(submenu), menuitem);
      g_signal_connect (G_OBJECT(menuitem), "activate",
          G_CALLBACK (launch_additional_menu), self);
    }
  }
  
  return menu;
}

static void
hildon_im_ui_input_method_selected(GtkWidget *widget,
                                   gpointer data)
{
  HildonIMUI *self;
  GSList *iter;

  g_return_if_fail(HILDON_IM_IS_UI(data));
  self = HILDON_IM_UI(data);

  for (iter = self->priv->all_methods; iter != NULL; iter = iter->next)
  {
    PluginData *info;
    info = (PluginData *) iter->data;
    if (info->menu == widget)
    {
      if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)) == TRUE)
      {
        activate_plugin(self, info, TRUE);
      }
      return;
    }
  }

  g_critical("Invalid input method selected from control menu");
}

static void
hildon_im_ui_language_selected(GtkWidget *widget, 
                               gpointer data)
{
  HildonIMUI *self;
  int i;

  g_return_if_fail(HILDON_IM_IS_UI(data));
  self = HILDON_IM_UI(data);

  for (i = 0; i < NUM_LANGUAGES; i++)
  {
    if (widget == self->priv->menu_lang[i])
    { 
      if (i != self->priv->current_language_index &&
          gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
      {
        self->priv->current_language_index = i;
        hildon_im_ui_activate_current_language(self);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                self->priv->menu_lang[i]), TRUE);

        gconf_client_set_int (self->client, GCONF_CURRENT_LANGUAGE,
                self->priv->current_language_index,
                NULL);
      }

      set_im_menu_sensitivity_for_language (self);
      
      return;
    }
  }

  g_critical("Invalid language selected from control menu");
}

static void
hildon_im_ui_resize_window(HildonIMUI *self)
{
  PluginData *info;

  info = CURRENT_PLUGIN (self);
  if (info != NULL)
  {
    gint height;

    if (info->info->height == HILDON_IM_DEFAULT_HEIGHT)
      height = IM_WINDOW_HEIGHT;
    else
      height = info->info->height;

    if (height != GTK_WIDGET(self)->allocation.height)
    {
      gtk_window_resize(GTK_WINDOW(self), GTK_WIDGET(self)->allocation.width,
                        height);
    }
  }
}

/* Public methods **********************************************/


/**
 * hildon_im_ui_new:
 * @dir: the name od the directory where ui layouts are located
 *
 * @Returns: a newly allocated #HildonIMUI
 *
 * Creates and returns a #HildonIMUI
 */
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

  hildon_im_ui_get_work_area(self);

  gtk_widget_realize(GTK_WIDGET(self));
  gtk_window_set_default_size(GTK_WINDOW(self), 
      self->priv->width, -1);

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

/**
 * hildon_im_ui_get_surrounding:
 * @self: a #HildonIMUI
 *
 * @Returns: the surrounding text
 *
 * Returns the context around the cursor
 */
const gchar *
hildon_im_ui_get_surrounding(HildonIMUI *self)
{
  return self->priv->surrounding;
}

/**
 * hildon_im_ui_get_surrounding_offest:
 * @self: a #HildonIMUI
 *
 * @Returns: the offset
 *
 * Returns the character offset of the cursor in the surrounding content
 */
gint
hildon_im_ui_get_surrounding_offset(HildonIMUI *self)
{
  return self->priv->surrounding_offset;
}

/**
 * hildon_im_ui_get_commit_mode:
 * @self: a #HildonIMUI
 *
 * @Returns: the commit mode
 *
 * Returns the mode by which insertions can be made into the surrounding
 */
HildonIMCommitMode
hildon_im_ui_get_commit_mode(HildonIMUI *self)
{
  return self->priv->commit_mode;
}

/**
 *hildon_im_ui_restore_previous_mode:
 * @self: a #HildonIMUI
 *
 * Restores the previous mode.
 */
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

  hide_controlmenu (self);
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

    if (activate_special == TRUE)
    {
      g_signal_connect(plugin->widget, "hide",
                       G_CALLBACK (hildon_im_ui_unlatch_special),
                       self);
    }
  }

  set_basic_buttons (self);

#ifdef MAEMO_CHANGES
  if (((self->priv->input_mode & HILDON_GTK_INPUT_MODE_SPECIAL) ||
      (self->priv->input_mode & HILDON_GTK_INPUT_MODE_ALPHA) ||
      (self->priv->input_mode & HILDON_GTK_INPUT_MODE_TELE)) && 
      (activate_special == TRUE || plugin->info->special_plugin != NULL))
  {
    hildon_im_ui_button_set_sensitive(self, 
        HILDON_IM_BUTTON_SPECIAL_CHAR,
        TRUE);
  }
#endif

  set_current_plugin (self, plugin);
  
  hildon_im_plugin_enable (CURRENT_IM_PLUGIN (self), init);
  hildon_im_plugin_transition(CURRENT_IM_PLUGIN(self), FALSE);

  if (plugin->info->type == HILDON_IM_TYPE_DEFAULT &&
      plugin->info->trigger == HILDON_IM_TRIGGER_STYLUS)
  {
    gconf_client_set_string (self->client, GCONF_INPUT_METHOD,
        plugin->info->name, NULL);
  }
  hildon_im_ui_set_common_buttons_visibility (self, 
        (plugin->info->disable_common_buttons == FALSE));

  if (plugin->menu != NULL)  
  {
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (plugin->menu), TRUE);
  }

  self->priv->ui_is_visible = GTK_WIDGET_VISIBLE (plugin->widget) &&
                              plugin->info->type != HILDON_IM_TYPE_HIDDEN;
  /* needs packing */
  if (self->priv->ui_is_visible && plugin->info->type != HILDON_IM_TYPE_SPECIAL_STANDALONE)
    gtk_box_pack_start(self->priv->im_box, plugin->widget, TRUE, TRUE, 0);

  if (plugin->info->type != HILDON_IM_TYPE_HIDDEN)
    gtk_widget_show_all (plugin->widget);

  if (self->priv->ui_is_visible)
    hildon_im_ui_resize_window(self);
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

static void
hildon_im_ui_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
  HildonIMUI *self;
  HildonIMUIPrivate *priv;
  gboolean fullscreen;
  GtkAllocation alloc = *allocation;
  GtkWidget *child;
  GSList *iter;
  PluginData *info;

  g_return_if_fail(HILDON_IM_IS_UI(widget));
  self = HILDON_IM_UI(widget);
  priv = self->priv;
  info = CURRENT_PLUGIN(self);

  fullscreen = alloc.width > priv->width ||
               priv->width == gdk_screen_get_width(gdk_screen_get_default());

  /*allocation for IM area*/
  if (info && info->info->disable_common_buttons == TRUE)
  {
    alloc.x = 0;
    if(fullscreen == FALSE)
      alloc.width = priv->width;
  }
  else if(fullscreen)
  {
    alloc.x = 0;
  }
  else
  {
    alloc.x = 0;
    alloc.width = allocation->width;
  }
  alloc.height -= IM_FRAME_HEIGHT;
  if (alloc.height < 0)
    alloc.height = 0;
  alloc.y = IM_FRAME_HEIGHT ;
  
  /*inform plugin about size changes first, then allocate sizes*/
  for (iter = self->priv->all_methods; iter != NULL; iter = iter->next)
  {
    PluginData *info;
    info = (PluginData *) iter->data;

    if (info->widget != NULL)
    {
      hildon_im_plugin_fullscreen(HILDON_IM_PLUGIN(info->widget),
                                  fullscreen);
    }
  }

  child = gtk_bin_get_child(GTK_BIN(widget));
  gtk_widget_size_allocate(child, &alloc);
}

static gboolean
hildon_im_ui_expose(GtkWidget *self, GdkEventExpose *event)
{
  HildonIMUI *kbd;
  GtkWidget *child;

  g_return_val_if_fail(HILDON_IM_IS_UI(self), FALSE);
  kbd = HILDON_IM_UI(self);

#ifdef MAEMO_CHANGES
  /* enable menu if not first boot */
  if (!hildon_im_ui_is_first_boot(kbd))
  {
    hildon_im_ui_button_set_sensitive(kbd, 
        HILDON_IM_BUTTON_INPUT_MENU, TRUE);
  }

  /* FIXME: Plugins without common buttons share a 2px strip at the top
     with the IM UI */
  if(self->allocation.width == kbd->priv->width)
  { 
    gtk_paint_box(self->style, self->window,
                  GTK_WIDGET_STATE(self), GTK_SHADOW_NONE,
                  &event->area, self, "osso-im-frame-top",
                  0, 0,
                  self->allocation.width, IM_FRAME_HEIGHT);
  }
  else
  {
    gtk_paint_box(self->style, self->window, GTK_WIDGET_STATE(self),
                  GTK_SHADOW_NONE, &event->area, self, "osso-im-frame-top",
                  0, 0, self->allocation.width, IM_FRAME_HEIGHT);
  }
#endif

  child = gtk_bin_get_child(GTK_BIN(self));

  if(child != NULL)
  {
    gtk_container_propagate_expose(GTK_CONTAINER(self),
                                   GTK_WIDGET(child), event);
  }

  return FALSE;
}

/**
 * hildon_im_ui_send_utf8:
 * @self: a #HildonIMUI
 * @utf8: string to send
 *
 * Sends the utf8 string as an XEvent via XServer
 *
 */
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

/**
 * hildon_im_ui_send_communication_message:
 * @self: a #HildonIMUI
 * @message: the type of #HildonIMComMessage to send
 *
 * Sends a #HildonIMComMessage of type @message with a XEvent
 */
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

/**
 * hildon_im_ui_send_surrounding_content:
 * @self: a #HildonIMUI
 * @surrounding: the surrounding context
 *
 * Sends the surrounding context around the insertion point
 */
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

/**
 * hildon_im_ui_send_surrounding_offset:
 * @self: a #HildonIMUI
 * @offset: the character offset of the insertion point
 *
 * Sends the character offset of the cursor location in the surrounding content
 */
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

/**
 * hildon_im_ui_get_current_input_mode:
 *
 * @self: a #HildonIMUI
 * @Returns: the current #HildonIMMode of the ui
 *
 * Returns the current input mode of the ui or #HILDON_IM_HIDE if
 * @self is not a valid #HildonIMUI
 */
HildonGtkInputMode
hildon_im_ui_get_current_input_mode(HildonIMUI *self)
{
  g_return_val_if_fail(HILDON_IM_IS_UI(self), 0);
  return self->priv->input_mode;
}

/**
 * hildon_im_ui_get_autocase_mode:
 * @self: a #HildonIMUI
 * @Returns: the autocase mode of the ui
 *
 * Returns the autocase mode of the ui or #HILDON_IM_LOW if
 * @self is not a valid #HildonIMUI
 */
HildonIMCommand
hildon_im_ui_get_autocase_mode(HildonIMUI *self)
{
  g_return_val_if_fail(HILDON_IM_IS_UI(self), HILDON_IM_LOW);
  return self->priv->autocase;
}

static gboolean
hildon_im_ui_long_press(gpointer data)
{
  HildonIMUI *self;
  g_return_val_if_fail(HILDON_IM_IS_UI(data), FALSE);
  self = HILDON_IM_UI(data);

  self->priv->long_press_timeout_id = 0;
  self->priv->long_press = TRUE;

  hildon_im_ui_process_button_click(self, self->priv->pressed_button, TRUE);

  return FALSE;
}

static gboolean
hildon_im_ui_repeat_start(gpointer data)
{
  HildonIMUI *self;
  g_return_val_if_fail(HILDON_IM_IS_UI(data), FALSE);
  self = HILDON_IM_UI(data);
  hildon_im_ui_process_button_click(self, self->priv->pressed_button, self->priv->long_press);

  self->priv->repeat_done = TRUE;
  self->priv->repeat_timeout_id = g_timeout_add(KEYREPEAT_INTERVAL,
                                                hildon_im_ui_repeat, (gpointer) self);
  return FALSE;
}

static gboolean
hildon_im_ui_repeat(gpointer data)
{
  HildonIMUI *self;
  g_return_val_if_fail(HILDON_IM_IS_UI(data), FALSE);
  self = HILDON_IM_UI(data);
  hildon_im_ui_process_button_click(self, self->priv->pressed_button, self->priv->long_press);
  return TRUE;
}

static GdkPixbuf *
cached_pixbuf_lookup(HildonIMUI *self, const gchar *id)
{
  GdkPixbuf *pixbuf=NULL;
  gint i, j;

  g_return_val_if_fail(HILDON_IM_IS_UI(self), NULL);

  for (i = 0; i < CACHED_BUTTON_PIXMAP_COUNT; i++)
  {
    gchar *name = self->priv->cached_button_pixbuf_name[i];

    if (name != NULL && strcmp(name, id) == 0)
    {
      pixbuf = self->priv->cached_button_pixbuf[i];

      if (i != 0)
      {
        /* move to front */
        for (j = i; j > 0; j--)
        {
          self->priv->cached_button_pixbuf_name[j] =
                  self->priv->cached_button_pixbuf_name[j-1];
          self->priv->cached_button_pixbuf[j] =
                  self->priv->cached_button_pixbuf[j-1];
        }
        self->priv->cached_button_pixbuf_name[0] = name;
        self->priv->cached_button_pixbuf[0] = pixbuf;
      }
      return pixbuf;
    }
  }

  return NULL;
}

/**
 * hildon_im_ui_button_set_id_real:
 * @self: a #HildonIMUI
 * @button: the index of the button
 * @id: name of the of the image
 *
 * Sets the image with the name @id for the specified button.
 */
static void
hildon_im_ui_button_set_id_real(HildonIMUI *self,
                                HildonIMButton button, const gchar *id,
                                gboolean clear_label)
{
  HildonIMUIPrivate *priv;
  GtkWidget *widget;
  GtkImage *image;
  GtkIconTheme *icon_theme;
  GdkPixbuf *pixbuf;
  gint seek, i;

  g_return_if_fail(HILDON_IM_IS_UI(self));
  g_return_if_fail(button >= 0 && button < HILDON_IM_NUM_BUTTONS);
  priv = self->priv;

  if (clear_label == TRUE)
  {
    hildon_im_ui_button_set_label_real(self, button,
                                       NULL, FALSE);
  }

  if (id == NULL)
  {
    if (priv->buttons[button].id[0] == '\0')
    {
      return;
    }
    priv->buttons[button].id[0] = '\0';
  }
  else
  {
    seek = MAX(strlen(id) - BUTTON_ID_LEN + 1, 0);
    if (strncmp(priv->buttons[button].id, id + seek, BUTTON_ID_LEN) == 0)
    {
      return;
    }
    g_strlcpy(priv->buttons[button].id, id + seek, BUTTON_ID_LEN);
  }

  icon_theme = gtk_icon_theme_get_default();

  widget = gtk_bin_get_child(GTK_BIN(priv->buttons[button].button));
  if (widget == NULL || GTK_IS_IMAGE(widget) == FALSE)
  {
    if (widget != NULL && GTK_IS_WIDGET(widget) == TRUE)
    {
      gtk_widget_destroy(widget);
    }
    widget = gtk_image_new();
    gtk_widget_show (widget);
    gtk_container_add(GTK_CONTAINER(priv->buttons[button].button), widget);
  }
  image = GTK_IMAGE(widget);

  if (id == NULL)
  {
    gtk_image_set_from_pixbuf(image, NULL);
  }
  else
  {
    /* try to use cached pixbufs. */
    pixbuf = cached_pixbuf_lookup(self, id);
    if (pixbuf == NULL)
    {
      pixbuf = gtk_icon_theme_load_icon(icon_theme,
                                        id,
                                        HILDON_IM_UI_ICON_SIZE,
                                        GTK_ICON_LOOKUP_NO_SVG,
                                        NULL);

      /* delete last cached item */
      i = CACHED_BUTTON_PIXMAP_COUNT - 1;
      if (priv->cached_button_pixbuf_name[i] != NULL)
      {
        g_free(priv->cached_button_pixbuf_name[i]);
        g_object_unref(priv->cached_button_pixbuf[i]);
      }

      /* move cached items forward */
      memmove(priv->cached_button_pixbuf_name + 1,
              priv->cached_button_pixbuf_name,
              (CACHED_BUTTON_PIXMAP_COUNT - 1) *
              sizeof(priv->cached_button_pixbuf_name[0]));
      memmove(priv->cached_button_pixbuf + 1,
              priv->cached_button_pixbuf,
              (CACHED_BUTTON_PIXMAP_COUNT - 1) *
              sizeof(priv->cached_button_pixbuf[0]));

      /* add to beginning of cache */
      priv->cached_button_pixbuf_name[0] = g_strdup(id);
      priv->cached_button_pixbuf[0] = pixbuf;
    }
    gtk_image_set_from_pixbuf(image, pixbuf);
  }
}

/**
 * hildon_im_ui_button_set_id:
 * @ui: a #HildonIMUI
 * @button: the index of the button
 * @id: name of the of the image
 *
 * Sets the image with the name @id for the specified button.
 */
void
hildon_im_ui_button_set_id(HildonIMUI *self,
                           HildonIMButton button, const gchar *id)
{
  hildon_im_ui_button_set_id_real(self, button, id, TRUE);
}

void
hildon_im_ui_button_set_label_real(HildonIMUI *self,
                                   HildonIMButton button,
                                   const gchar *label,
                                   gboolean clear_id)
{
  HildonIMUIPrivate *priv;
  GtkWidget *child;
  const gchar *stuff;

  g_return_if_fail(HILDON_IM_IS_UI(self));
  g_return_if_fail(button >= 0 && button < HILDON_IM_NUM_BUTTONS);
  priv = self->priv;

  /* clear icon (id) */
  if (clear_id == TRUE)
  {
    hildon_im_ui_button_set_id_real(self, button, NULL, FALSE);
  }

  child = gtk_bin_get_child(GTK_BIN(priv->buttons[button].button));
  if (GTK_IS_LABEL(child) == TRUE)
  {
    stuff = label != NULL ? label : "";
  }
  else
  {
    stuff = label;
  }
  gtk_button_set_label(GTK_BUTTON(priv->buttons[button].button), stuff);
}

void
hildon_im_ui_button_set_label(HildonIMUI *self,
                              HildonIMButton button,
                              const gchar *label)
{
  hildon_im_ui_button_set_label_real(self, button, label, TRUE);
}

static void
hildon_im_ui_button_set_menu_real(HildonIMUI *self,
                                  HildonIMButton button,
                                  GtkWidget *menu,
                                  gboolean automatic)
{
  HildonIMUIPrivate *priv;
  g_return_if_fail(HILDON_IM_IS_UI(self));
  g_return_if_fail(button >= 0 && button < HILDON_IM_NUM_BUTTONS);
  g_return_if_fail(menu == NULL || GTK_IS_MENU(menu));
  priv = self->priv;

  if (priv->buttons[button].menu != NULL)
  {
    g_object_ref(priv->buttons[button].menu);
    g_object_ref_sink(GTK_OBJECT(priv->buttons[button].menu));
    gtk_widget_destroy(priv->buttons[button].menu);
    g_object_unref(priv->buttons[button].menu);
  }

  priv->buttons[button].menu = menu;

  if (menu != NULL)
  {
    gtk_widget_show_all(menu);
  }

  if (menu != NULL)
  {
    g_signal_connect(menu, "deactivate",
                     G_CALLBACK(hildon_im_ui_menu_deactivated),
                     (gpointer) self);
  }

  if (automatic == TRUE)
  {
    hildon_im_ui_button_set_toggle_real(self, button,
                                              menu != NULL, FALSE);
    hildon_im_ui_button_set_repeat(self, button,
                                         menu == NULL);
  }
}

void
hildon_im_ui_button_set_menu(HildonIMUI *self,
                             HildonIMButton button,
                             GtkWidget *menu)
{
  hildon_im_ui_button_set_menu_real(self, button, menu, TRUE);
}

void
hildon_im_ui_button_set_long_press(HildonIMUI *self,
                                   HildonIMButton button,
                                   gboolean long_press)
{
  HildonIMUIPrivate *priv;
  g_return_if_fail(HILDON_IM_IS_UI(self));
  g_return_if_fail(button >= 0 && 
      button < HILDON_IM_NUM_BUTTONS);
  priv = self->priv;

  priv->buttons[button].long_press = long_press;
}

void
hildon_im_ui_button_set_sensitive(HildonIMUI *self,
                                  HildonIMButton button,
                                  gboolean sensitive)
{
  HildonIMUIPrivate *priv;
  g_return_if_fail(HILDON_IM_IS_UI(self));
  g_return_if_fail(button >= 0 && 
      button < HILDON_IM_NUM_BUTTONS);
  priv = self->priv;

  gtk_widget_set_sensitive(priv->buttons[button].button, sensitive);
  priv->buttons[button].sensitive = sensitive;
}

static void
hildon_im_ui_button_set_toggle_real(HildonIMUI *self,
                                    HildonIMButton button,
                                    gboolean toggle,
                                    gboolean automatic)
{
  HildonIMUIPrivate *priv;
  g_return_if_fail(HILDON_IM_IS_UI(self));
  g_return_if_fail(button >= 0 && 
      button < HILDON_IM_NUM_BUTTONS);
  priv = self->priv;

  priv->buttons[button].toggle = toggle;

  if (toggle == TRUE)
  {
    gtk_widget_set_name(priv->buttons[button].button,
                        "OssoIMSideToggleButton");
  }
  else
  {
    if (button == HILDON_IM_BUTTON_CLOSE)
    {
      gtk_widget_set_name(priv->buttons[button].button,
                          "OssoIMSideCloseButton");
    } else {
      gtk_widget_set_name(priv->buttons[button].button,
                          "OssoIMSideButton");
    }
  }

  if (automatic == TRUE)
  {
    hildon_im_ui_button_set_menu_real(self, button,
                                            NULL, FALSE);
    hildon_im_ui_button_set_repeat(self, button,
                                         toggle == FALSE);
    hildon_im_ui_button_set_active(self, button, FALSE);
  }
}

void
hildon_im_ui_button_set_toggle(HildonIMUI *self,
                               HildonIMButton button,
                               gboolean toggle)
{
  hildon_im_ui_button_set_toggle_real(self, button,
                                            toggle, TRUE);
}

void
hildon_im_ui_button_set_repeat(HildonIMUI *self,
                               HildonIMButton button,
                               gboolean repeat)
{
  HildonIMUIPrivate *priv;
  g_return_if_fail(HILDON_IM_IS_UI(self));
  g_return_if_fail(button >= 0 && 
      button < HILDON_IM_NUM_BUTTONS);
  priv = self->priv;

  priv->buttons[button].repeat = repeat;
}

void
hildon_im_ui_button_set_active(HildonIMUI *self,
                               HildonIMButton button,
                               gboolean active)
{
  HildonIMUIPrivate *priv;
  g_return_if_fail(HILDON_IM_IS_UI(self));
  g_return_if_fail(button >= 0 && 
      button < HILDON_IM_NUM_BUTTONS);
  priv = self->priv;

  gtk_toggle_button_set_active(
                               GTK_TOGGLE_BUTTON(priv->buttons[button].button),
                               active);
}

gboolean
hildon_im_ui_button_get_active(HildonIMUI *self,
                               HildonIMButton button)
{
  HildonIMUIPrivate *priv;
  g_return_val_if_fail(HILDON_IM_IS_UI(self), FALSE);
  g_return_val_if_fail(button >= 0 && 
      button < HILDON_IM_NUM_BUTTONS, FALSE);
  priv = self->priv;

  return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
          priv->buttons[button].button));
}

gboolean
hildon_im_ui_get_visibility(HildonIMUI *self)
{
#ifdef MAEMO_CHANGES
  HildonGtkInputMode mode = self->priv->input_mode;
  gboolean is_secret_mode, is_dictionary_mode;

  is_secret_mode = (mode & HILDON_GTK_INPUT_MODE_INVISIBLE) != 0;
  is_dictionary_mode = (mode & HILDON_GTK_INPUT_MODE_DICTIONARY) != 0;

  return is_dictionary_mode && !is_secret_mode;
#else
  return TRUE;
#endif
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

static void
free_each_additional_menu (gpointer data, gpointer user_data)
{
  HildonIMAdditionalMenuEntry *entry = 
    (HildonIMAdditionalMenuEntry *) data;

  if (entry == NULL)
    return;

  FREE_IF_SET(entry->title);
  FREE_IF_SET(entry->gettext_domain);
  FREE_IF_SET(entry->command);
  FREE_IF_SET(entry->dbus_service);
  g_free (entry);
  entry = NULL;
}

static void
free_additional_menu (HildonIMUI *self)
{
  HildonIMUIPrivate *priv;
  
  g_return_if_fail(HILDON_IM_IS_UI(self));
  priv = self->priv;

  if (priv->additional_menu != NULL)
  {
    g_slist_foreach (priv->additional_menu, 
        free_each_additional_menu, NULL);
    g_slist_free (priv->additional_menu);
    priv->additional_menu = NULL;
  }
}

static void
add_additional_menu (HildonIMUI *self, const gchar *file)
{
#define GROUP_NAME "Desktop Entry"
  HildonIMAdditionalMenuEntry *entry;
  HildonIMUIPrivate *priv;
  GKeyFile *key_file;

  g_return_if_fail(HILDON_IM_IS_UI(self));
  priv = self->priv;

  if (!g_str_has_suffix (file, ".desktop"))
  {
    g_warning ("Skipping non .desktop file: %s", file);
    return;
  }

  key_file = g_key_file_new ();
  if (!key_file)
    return;

  if (g_key_file_load_from_file (key_file, file,
        G_KEY_FILE_NONE, NULL))
  {
    gchar *value;
    value = g_key_file_get_value (key_file, GROUP_NAME, "Type", NULL);
    if (!value)
    {
      g_warning ("Type is not found in %s", file);
      return;
    }
    entry = (HildonIMAdditionalMenuEntry*) g_malloc0 (sizeof
        (HildonIMAdditionalMenuEntry));

    if (g_ascii_strncasecmp (value, "HildonControlPanelPlugin", 24) == 0)
    {
      entry->type = MENU_TYPE_CPA;
    }
    else if (g_ascii_strncasecmp (value, 
          "Application", 11) == 0)
    {
      entry->type = MENU_TYPE_APPLICATION;
    }
    else 
    {
      g_warning ("Type %s in %s is not supported", value, file);
      g_free (entry);
      g_free (value);
      return;
    }

    g_free (value);

    value = g_key_file_get_value (key_file, GROUP_NAME, "X-gettext-domain", NULL);
    if (value)
    {      
      entry->gettext_domain = value;
    }

    value = g_key_file_get_value (key_file, GROUP_NAME, "Name", NULL);
    if (!value)
    {
      g_warning ("Name is not found in %s", file);
      free_each_additional_menu (entry, NULL);
      return;
    }
    
    entry->title = (entry->gettext_domain != NULL) ?
          dgettext (entry->gettext_domain, value)  :
          _(value);

    if (entry->title == value)
      entry->title = g_strdup(value);

    g_free(value);
    
    if (entry->type == MENU_TYPE_APPLICATION)
    {
      value = g_key_file_get_value (key_file, GROUP_NAME, "Exec", NULL);
      if (value)
      {
        entry->command = value;
      }
      
      value = g_key_file_get_value (key_file, GROUP_NAME, "X-Osso-Service", NULL);
      if (value)
      {        
        if (!strchr (value , '.'))
        {
          gchar *tmp = g_strconcat ("com.nokia.", value, NULL);
          g_free (value);
          value = tmp;
        }

        entry->dbus_service = value;
      } else {
        if (entry->command == NULL)
        {
          free_each_additional_menu (entry, NULL);
          g_warning ("Type is application but doesn't have Exec or Dbus-service in %s", file);
          return;
        }
      }
    } else if (entry->type == MENU_TYPE_CPA)
    {
      value = g_key_file_get_value (key_file, GROUP_NAME, "X-control-panel-plugin", NULL);
      if (value)
      {
        entry->command = value;
      } else {
        free_each_additional_menu (entry, NULL);
        g_warning ("Type is CPA but doesn't have x-control-panel-plugin in %s", file);
        return;
      }
    }

    priv->additional_menu = g_slist_append (priv->additional_menu, entry);
    g_key_file_free (key_file);
  } else
    g_warning ("File %s couldn't be opened", file);

  return;
}

static void 
populate_additional_menu (HildonIMUI *self)
{
  HildonIMUIPrivate *priv;  
  GDir *dir;
  
  g_return_if_fail(HILDON_IM_IS_UI(self));
  priv = self->priv;

  free_additional_menu (self);

  dir = g_dir_open (IM_MENU_DIR, 0, NULL);
  if (dir) 
  {
    const gchar *file_name;
    gchar *file_name_full;
    
    while ((file_name = g_dir_read_name (dir)) != NULL)
    {
      file_name_full = g_build_filename (IM_MENU_DIR, file_name, NULL);
      if (file_name_full)
      {
        add_additional_menu (self, file_name_full);
        g_free (file_name_full);        
      }
    }
    g_dir_close (dir);
  }

  return;
}

static void
run_additional_menu (HildonIMUI *self, 
    HildonIMAdditionalMenuEntry *entry)
{
  if (!self->osso)
    return;

  if (entry->type == MENU_TYPE_APPLICATION)
  {
    if (entry->dbus_service)
    {
      gchar *path = g_strdup_printf ("/%s", entry->dbus_service);
      if (!path)
        return;
  
      path = g_strdelimit (path, ".", '/');
      
      osso_rpc_run(self->osso,
                   entry->dbus_service, path,
                   entry->dbus_service, "top_application", NULL, 
                   DBUS_TYPE_INVALID);
    }
    else 
    {
      g_spawn_command_line_async (entry->command, NULL);
    }
  } else if (entry->type == MENU_TYPE_CPA)
  {
    osso_return_t ret;
    ret = osso_cp_plugin_execute(self->osso, entry->command,
                                 NULL, TRUE);
    if (ret == OSSO_ERROR)
    {
      osso_log(LOG_ERR, "%s: Error with osso_cp_plugin_execute",
               __FUNCTION__);
    }
  } 
}

static void 
launch_additional_menu (GtkWidget *widget, gpointer data)
{
  HildonIMUI *self = (HildonIMUI *) data;
  HildonIMUIPrivate *priv;
  GtkWidget *label;
  const gchar *title;
  GSList *iter;

  g_return_if_fail (HILDON_IM_IS_UI (self));
  priv = self->priv;
  label = gtk_bin_get_child (GTK_BIN (widget));
  if (!label) 
  {
    g_warning ("Unable to get gtk_label");
    return;
  }

  title = gtk_label_get_label (GTK_LABEL (label));
  if (!title)
  {
    g_warning ("Unable to get the title from the gtk_label");
    return;
  }

  for (iter = priv->additional_menu; iter != NULL; iter = iter->next)
  {
    HildonIMAdditionalMenuEntry *entry = iter->data;
    if (g_ascii_strcasecmp (entry->title, title) == 0)
    {
      run_additional_menu (self, entry);
      return;
    }
  }

  g_warning ("Menu %s is not found. Should not reach here", title);

  return;
}

void
hildon_im_ui_set_common_buttons_visibility (
    HildonIMUI *self, gboolean status)
{
  gint i;
  HildonIMUIPrivate *priv;

  g_return_if_fail (HILDON_IM_IS_UI (self));
  priv = self->priv;

  for (i = 0; i < HILDON_IM_NUM_BUTTONS; i++)
  {
    if (priv->buttons[i].button != NULL)
    {
      if (status)
        gtk_widget_show (priv->buttons[i].button);
      else
        gtk_widget_hide (priv->buttons[i].button);
    }
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
  ui->priv->ui_is_visible = visible;

  if(GTK_WIDGET_DRAWABLE(ui) == FALSE)
    return;

  if (visible == FALSE)
  {
    gtk_widget_hide(GTK_WIDGET(ui));
  }
}


static void hildon_im_ui_get_work_area (HildonIMUI *self)
{
  Display *dpy;
  Atom act_type;
  gint status, act_format;
  gulong nitems, bytes;
  unsigned char *data = NULL;
  gulong *data_l;

  dpy = GDK_DISPLAY();
  status = XGetWindowProperty (dpy, 
                               GDK_ROOT_WINDOW(),
                               XInternAtom(dpy, "_NET_WORKAREA", True),
                               0, (~0L), False,
                               AnyPropertyType,
                               &act_type, &act_format, &nitems, &bytes,
                               (unsigned char**)&data);

  if (status == Success && nitems > 3) 
  {
    data_l = (gulong *) data + 2; /* screen width is in the third value */
    if (*data_l > 0)
    {
      self->priv->width = *data_l;
    }
  }

  if (data)
  {
    XFree(data);
  }
}

static void
hide_controlmenu (HildonIMUI *self)
{
  if (GTK_MENU(self->priv->buttons[HILDON_IM_BUTTON_INPUT_MENU].menu) 
      != NULL &&
      hildon_im_ui_button_get_active(self,
        HILDON_IM_BUTTON_INPUT_MENU) == TRUE)
  {
    hildon_im_ui_button_set_active(self,
        HILDON_IM_BUTTON_INPUT_MENU, FALSE);
    gtk_menu_popdown (GTK_MENU (
            self->priv->buttons [HILDON_IM_BUTTON_INPUT_MENU].menu));
  }
}
