/**
   @file: hildon-im-plugin.h

 */
/*
 * This file is part of hildon-input-method
 *
 * Copyright (C) 2006-2007 Nokia Corporation.
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


#ifndef __HILDON_IM_PLUGIN_H__
#define __HILDON_IM_PLUGIN_H__

#include <gtk/gtkwidget.h>
#include "hildon-im-ui.h"

G_BEGIN_DECLS

typedef struct _HildonIMPlugin HildonIMPlugin;
typedef struct _HildonIMPluginIface HildonIMPluginIface;

#define HILDON_IM_TYPE_PLUGIN (hildon_im_plugin_get_type())
#define HILDON_IM_PLUGIN(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj), HILDON_IM_TYPE_PLUGIN, \
                                    HildonIMPlugin))
#define HILDON_IM_IS_PLUGIN(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj), HILDON_IM_TYPE_PLUGIN))
#define HILDON_IM_PLUGIN_GET_IFACE(inst) \
        (G_TYPE_INSTANCE_GET_INTERFACE((inst), HILDON_IM_TYPE_PLUGIN, \
                                       HildonIMPluginIface))

typedef struct
{
  gchar      *description;
  gchar      *name;
  gchar      *menu_title;
  gchar      *gettext_domain;
  gboolean    visible_in_menu;  
  gboolean    cached;

  gint        type;
  gint        group;
  gint        priority;

  gchar      *special_plugin;
  gchar      *ossohelp_id;

  gboolean        disable_common_buttons;
  gint            height;
  HildonIMTrigger  trigger;
} HildonIMPluginInfo;

#define HILDON_IM_DEFAULT_PLUGIN_PRIORITY 0

enum
{
  HILDON_IM_GROUP_LATIN,
  HILDON_IM_GROUP_CJK,
  HILDON_IM_GROUP_CUSTOM = 0x1000
};

typedef enum
{
  /* The plugin uses default UI */
  HILDON_IM_TYPE_DEFAULT,
  /* The plugin is called by another plugin */
  HILDON_IM_TYPE_SPECIAL,
  /* The plugin is fullscreen */
  HILDON_IM_TYPE_FULLSCREEN,
  /* The plugin is loaded at startup and never removed */
  HILDON_IM_TYPE_PERSISTENT,
  /* The plugin doesn't fall into any of categories above */
  HILDON_IM_TYPE_OTHERS
} HildonIMPluginType;

struct _HildonIMPluginIface
{
  GTypeInterface base_iface;

  void (*enable) (HildonIMPlugin *plugin, gboolean init);
  void (*disable) (HildonIMPlugin *plugin);
  void (*settings_changed) (HildonIMPlugin *plugin, const gchar *key,
                            const GConfValue *value);
  void (*language_settings_changed) (HildonIMPlugin *plugin, gint index);
  void (*input_mode_changed) (HildonIMPlugin *plugin);
  void (*keyboard_state_changed) (HildonIMPlugin *plugin);
  void (*client_widget_changed) (HildonIMPlugin *plugin);
  void (*character_autocase) (HildonIMPlugin *plugin);
  void (*clear) (HildonIMPlugin *plugin);
  void (*save_data) (HildonIMPlugin *plugin);

  void (*mode_a) (HildonIMPlugin *plugin);
  void (*mode_b) (HildonIMPlugin *plugin);
  void (*language) (HildonIMPlugin *plugin);
  void (*backspace) (HildonIMPlugin *plugin);
  void (*enter) (HildonIMPlugin *plugin);
  void (*tab) (HildonIMPlugin *plugin);
  void (*fullscreen) (HildonIMPlugin *plugin, gboolean fullscreen);
  void (*select_region) (HildonIMPlugin *plugin, gint start, gint end);
  void (*key_event) (HildonIMPlugin *plugin, GdkEventType type,
                     guint state, guint keyval, guint hardware_keycode);
  void (*transition) (HildonIMPlugin *plugin, gboolean from);
  void (*surrounding_received) (HildonIMPlugin *plugin,
                                const gchar *surrounding,
                                gint offset);
  void (*button_activated) (HildonIMPlugin *plugin,
                            HildonIMButton button,
                            gboolean long_press);
};

/**
 * hildon_im_plugin_enable:
 * @plugin: #HildonIMPlugin
 * @init: whether it is called during initialized
 *
 * Called when the plugin is enabled
 */
void hildon_im_plugin_enable(HildonIMPlugin *plugin, gboolean init);
/**
 * hildon_im_plugin_disable:
 * @plugin: #HildonIMPlugin
 *
 * Called when the plugin is disabled
 */
void hildon_im_plugin_disable(HildonIMPlugin *plugin);
/**
 * hildon_im_plugin_settings_changed:
 * @plugin: #HildonIMPlugin
 * @key: the key id
 * @value: the value
 *
 * A gconf value has been changed
 */
void hildon_im_plugin_settings_changed(HildonIMPlugin *plugin,
                                       const gchar *key,
                                       const GConfValue *value);
/**
 * hildon_im_plugin_language_settings_changed:
 * @plugin: #HildonIMPlugin
 * @index: 0 = primary, 1 = secondary
 * @value: the language id
 *
 * The language settings has been changed
 */
void hildon_im_plugin_language_settings_changed (HildonIMPlugin *plugin,
                                                gint index);
/**
 * hildon_im_plugin_input_mode_changed:
 * @plugin: #HildonIMPlugin
 *
 * The input mode has been changed
 */
void hildon_im_plugin_input_mode_changed(HildonIMPlugin *plugin);
/**
 * hildon_im_plugin_keyboard_state_changed:
 * @plugin: #HildonIMPlugin
 *
 * The keyboard availability has changed
 */
void hildon_im_plugin_keyboard_state_changed(HildonIMPlugin *plugin);
/**
 * hildon_im_plugin_character_autocase:
 * @plugin: #HildonIMPlugin
 *
 * Called when it is necessary to check the autocase
 */
void hildon_im_plugin_character_autocase(HildonIMPlugin *plugin);
/**
 * hildon_im_plugin_client_widget_changed:
 * @plugin: #HildonIMPlugin
 *
 * Called when the client widget has changed
 */
void hildon_im_plugin_client_widget_changed(HildonIMPlugin *plugin);
/**
 * hildon_im_plugin_save_data:
 * @plugin: #HildonIMPlugin
 *
 * Called when the main UI instructs us to save our own data
 */
void hildon_im_plugin_save_data(HildonIMPlugin *plugin);
/**
 * hildon_im_plugin_clear:
 * @plugin: #HildonIMPlugin
 *
 * Clear the plugin
 */
void hildon_im_plugin_clear(HildonIMPlugin *plugin);
/**
 * hildon_im_plugin_button_activated:
 * @plugin: #HildonIMPlugin
 * @button: #HildonIMButton
 * @long_press: whether the press was a long one
 *
 * Called when one of the common IM UI buttons is activated
 */
void hildon_im_plugin_button_activated(HildonIMPlugin *plugin,
                                       HildonIMButton button,
                                       gboolean long_press);
/**
 * hildon_im_plugin_mode_a:
 * @plugin: #HildonIMPlugin
 *
 * Deprecated: Use hildon_im_plugin_button_activated
 * Called when the mode a button is pressed
 */
void hildon_im_plugin_mode_a(HildonIMPlugin *plugin);
/**
 * hildon_im_plugin_mode_b:
 * @plugin: #HildonIMPlugin
 *
 * Deprecated: Use hildon_im_plugin_button_activated
 * Called when the mode b button is pressed
 */
void hildon_im_plugin_mode_b(HildonIMPlugin *plugin);
/**
 * hildon_im_plugin_language:
 * @plugin: #HildonIMPlugin
 *
 * Deprecated
 */
void hildon_im_plugin_language(HildonIMPlugin *plugin);
/**
 * hildon_im_plugin_backspace:
 * @plugin: #HildonIMPlugin
 *
 * Called when the backspace button is pressed
 */
void hildon_im_plugin_backspace(HildonIMPlugin *plugin);
/**
 * hildon_im_plugin_enter:
 * @plugin: #HildonIMPlugin
 *
 * Called when the enter button is pressed
 */
void hildon_im_plugin_enter(HildonIMPlugin *plugin);
/**
 * hildon_im_plugin_tab:
 * @plugin: #HildonIMPlugin
 *
 * Called when the tab button is pressed
 */
void hildon_im_plugin_tab(HildonIMPlugin *plugin);
/**
 * hildon_im_plugin_fullscreen:
 * @plugin: #HildonIMPlugin
 * @fullscreen: whether it is a fullscreen or not
 *
 * Called when it is needed to change the screen size
 *
 * Return value:
 */
void hildon_im_plugin_fullscreen(HildonIMPlugin *plugin, gboolean fullscreen);
/**
 * hildon_im_plugin_select_region:
 * @plugin: #HildonIMPlugin
 *
 * When it is instructed to select a region
 */
void hildon_im_plugin_select_region(HildonIMPlugin *plugin, gint start, gint end);
/**
 * hildon_im_plugin_key_event:
 * @plugin: #HildonIMPlugin
 *
 * Called when for any key event
 */
void hildon_im_plugin_key_event(HildonIMPlugin *plugin, GdkEventType type,
                                guint state, guint keyval, guint hardware_keycode);

/**
 * hildon_im_plugin_transition:
 * @plugin: #HildonIMPlugin
 * @from: whether the transition is from or to the plugin
 *
 * Called when a plugin receives or loses control as the active plugin
 */
void hildon_im_plugin_transition(HildonIMPlugin *plugin, gboolean from);

/**
 * hildon_im_plugin_surrounding_received:
 * @plugin: #HildonIMPlugin
 * @surrounding: the new surrounding
 * @offset: the offset of the cursor within the surrounding
 *
 * Provides the plugin with the surrounding of the client application
 */
void hildon_im_plugin_surrounding_received(HildonIMPlugin *plugin,
                                           const gchar *surrounding,
                                           gint offset);

/**
 * hildon_im_plugin_duplicate_info:
 * @plugin: #HildonIMPlugin
 * @src: source
 *
 * Make a copy of a HildonIMPluginInfo
 *
 * Return value: a #HildonIMPluginInfo
 */
HildonIMPluginInfo *hildon_im_plugin_duplicate_info(
        const HildonIMPluginInfo *src);
/**
 * hildon_im_plugin_create:
 * @plugin: #HildonIMPlugin
 * @plugin_name: the name
 *
 * Creates a plugin
 */
HildonIMPlugin* hildon_im_plugin_create(HildonIMUI *keyboard,
                                        const gchar* plugin_name);

GType hildon_im_plugin_get_type(void);

G_END_DECLS

#endif
