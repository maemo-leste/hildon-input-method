/**
   @file: hildon-im-ui.h

 */
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
 */
#ifndef __HILDON_IM_UI_H__
#define __HILDON_IM_UI_H__

#include <gtk/gtkwindow.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkenums.h>
#include <gconf/gconf-client.h>
#include <libintl.h>
#include <signal.h>
#include <libosso.h>

#include "hildon-im-protocol.h"

#include <sys/types.h>
#include <sys/stat.h>


/*******************************/

G_BEGIN_DECLS

#ifndef MAEMO_CHANGES
#define HildonGtkInputMode gint
#endif

#define HILDON_IM_TYPE_UI (hildon_im_ui_get_type())
#define HILDON_IM_UI(obj) \
        (GTK_CHECK_CAST (obj, HILDON_IM_TYPE_UI,\
                HildonIMUI))
#define HILDON_IM_UI_CLASS(klass) \
        (GTK_CHECK_CLASS_CAST ((klass), HILDON_IM_TYPE_UI, \
                HildonIMUIClass))
#define HILDON_IM_IS_UI(obj) \
        (GTK_CHECK_TYPE (obj, HILDON_IM_TYPE_UI))
#define HILDON_IM_IS_UI_CLASS(klass) \
        (GTK_CHECK_CLASS_TYPE ((klass), HILDON_IM_TYPE_UI))

typedef struct _HildonIMUI HildonIMUI;
typedef struct _HildonIMUIClass HildonIMUIClass;
typedef struct _HildonIMUIPrivate HildonIMUIPrivate;

#define HILDON_IM_GCONF_DIR "/apps/osso/inputmethod"

#define HILDON_IM_GCONF_ENABLE_STYLUS_IM           HILDON_IM_GCONF_DIR       "/enable-stylus-im"
#define HILDON_IM_GCONF_LAUNCH_FINGER_KB_ON_SELECT HILDON_IM_GCONF_DIR       "/launch_finger_kb_on_select"
#define HILDON_IM_GCONF_THUMB_DETECTION            HILDON_IM_GCONF_DIR       "/thumb_detection"
#define HILDON_IM_GCONF_LANG_DIR                   HILDON_IM_GCONF_DIR       "/hildon-im-languages"
#define _HILDON_IM_GCONF_LANGUAGE                  HILDON_IM_GCONF_LANG_DIR  "/language-"
#define HILDON_IM_GCONF_PRIMARY_LANGUAGE           _HILDON_IM_GCONF_LANGUAGE "0"
#define HILDON_IM_GCONF_SECONDARY_LANGUAGE         _HILDON_IM_GCONF_LANGUAGE "1"
#define HILDON_IM_GCONF_CURRENT_LANGUAGE           HILDON_IM_GCONF_LANG_DIR "/current"


#define HILDON_IM_DEFAULT_LANGUAGE IM_en_GB
#define OSSO_DIR ".osso"
#define IM_HOME_DIR "hildon-input-method"
#define HILDON_IM_PROP_UI 1
#define HILDON_IM_PROP_UI_DESCRIPTION "UI"

#define HILDON_IM_DEFAULT_HEIGHT -1

/**
 * The common IM buttons
 */
typedef enum
{
  HILDON_IM_BUTTON_TAB = 0,
  HILDON_IM_BUTTON_MODE_A,
  HILDON_IM_BUTTON_MODE_B,
  HILDON_IM_BUTTON_INPUT_MENU,
  HILDON_IM_BUTTON_BACKSPACE,
  HILDON_IM_BUTTON_ENTER,
  HILDON_IM_BUTTON_SPECIAL_CHAR,
  HILDON_IM_BUTTON_CLOSE,

  HILDON_IM_NUM_BUTTONS
} HildonIMButton;

typedef enum
{
  HILDON_IM_ILLEGAL_INPUT_SOUND,
  HILDON_IM_NUMBER_INPUT_SOUND,
  HILDON_IM_FINGER_TRIGGER_SOUND
} HildonIMUISound;

typedef enum
{
  HILDON_IM_THUMB_DETECT_FINGER,
  HILDON_IM_THUMB_DETECT_FINGER_AND_STYLUS,
  HILDON_IM_THUMB_DETECT_NEVER,
} HildonIMThumbDetection;

#define HILDON_IM_DEFAULT_THUMB_DETECTION HILDON_IM_THUMB_DETECT_FINGER

struct _HildonIMUI
{
  GtkWindow parent;
  GConfClient *client;
  osso_context_t *osso;

  HildonIMUIPrivate *priv;
};

struct _HildonIMUIClass
{
  GtkWindowClass parent_class;
};

GType hildon_im_ui_get_type(void);

GtkWidget *hildon_im_ui_new(void);
GtkWidget* hildon_im_dynamic_widget_new(const gchar *library_name,
                                        const gchar *widget_name);

/*******************************************************************
 * Settings and states
 */
/**
 * hildon_im_ui_get_visibility:
 * @self: #HildonIMUI
 *
 * Gets the visibility of the main UI
 *
 * Return value: a #gboolean
 */
gboolean hildon_im_ui_get_visibility(HildonIMUI *self);

/**
 * hildon_im_ui_get_current_input_mode:
 * @self: #HildonIMUI
 * 
 * Gets the current input mode
 *
 * Return value: a #HildonGtkInputMode
 */
HildonGtkInputMode hildon_im_ui_get_current_input_mode(HildonIMUI *self);

/**
 * hildon_im_ui_get_autocase_mode:
 * @self: #HildonIMUI
 *
 * Gets the autocase mode
 *
 * Return value: a #HildonIMCommand
 */
HildonIMCommand hildon_im_ui_get_autocase_mode(HildonIMUI *self);

/**
 * hildon_im_ui_get_commit_mode:
 * @self: #HildonIMUI
 *
 * Gets the current commit mode of the input method
 *
 * Return value: a #HildonIMCommitMode
 */
HildonIMCommitMode hildon_im_ui_get_commit_mode(HildonIMUI *self);

/**
 * hildon_im_ui_is_first_boot:
 * @self: #HildonIMUI
 *
 * Determines whether this is a first boot
 */
gboolean hildon_im_ui_is_first_boot(HildonIMUI *self);

/**
 * hildon_im_ui_set_keyboard_state:
 * @self: #HildonIMUI
 * @available: is a keyboard available?
 *
 * Inform the UI of the external keyboard state
 */
void hildon_im_ui_set_keyboard_state(HildonIMUI *self,
                                     gboolean available);

/**
 * hildon_im_ui_set_visible:
 * @ui: #HildonIMUI
 *
 * Set the visibility of the IM UI
 */
void hildon_im_ui_set_visible(HildonIMUI *ui, gboolean visible);


/*******************************************************************
 * Plugin interaction
 */
/**
 * hildon_im_ui_restore_previous_mode:
 * @self: #HildonIMUI
 *
 * Restores the input method to the previous input method selected
 */
void hildon_im_ui_restore_previous_mode(HildonIMUI *self);

/**
 * hildon_im_ui_activate_plugin:
 * @self: #HildonIMUI
 * @name: the plugin's name
 * @init: requires initialization?
 *
 * Activates the specified input method plugin
 */
void hildon_im_ui_activate_plugin (HildonIMUI *self, gchar *,
        gboolean init);

/**
 * hildon_im_ui_toggle_special_plugin:
 * @self: #HildonIMUI
 *
 * Toggle between the special and active plugin
 */
void hildon_im_ui_toggle_special_plugin(HildonIMUI *self);

/*******************************************************************
 * im context interaction
 */
/**
 * hildon_im_ui_send_utf8:
 * @self: #HildonIMUI
 * @utf8: a utf-8 string
 *
 * Sends a utf-8 string to the client widget
 */
void hildon_im_ui_send_utf8(HildonIMUI *self, const gchar *utf8);

/**
 * hildon_im_ui_send_communication_message:
 * @self: #HildonIMUI
 * @message: the message
 *
 * Sends communication message to the im context
 */
void hildon_im_ui_send_communication_message(HildonIMUI *self,
                                                   gint message);

/**
 * hildon_im_ui_send_surrounding_content:
 * @self: #HildonIMUI
 * @surrounding: the surrounding string
 *
 * Sends surrounding string to the im context
 */
void hildon_im_ui_send_surrounding_content(HildonIMUI *self,
                                           const gchar *surrounding);
/**
 * hildon_im_ui_send_surrounding_offset:
 * @self: #HildonIMUI
 * @is_relative: whether it is a relative offset
 * @offset: the offset
 *
 * Sends surrounding offset and state
 */
void hildon_im_ui_send_surrounding_offset(HildonIMUI *self,
                                          gboolean is_relative,
                                          gint offset);
/**
 * hildon_im_ui_get_surrounding:
 * @self: #HildonIMUI
 *
 * Gets surrounding string
 */
const gchar *hildon_im_ui_get_surrounding(HildonIMUI *keyboard);

/**
 * hildon_im_ui_get_surrounding_offset:
 * @self: #HildonIMUI
 *
 * Gets the surrounding offset
 *
 * Return value: a #gint
 */
gint hildon_im_ui_get_surrounding_offset(HildonIMUI *keyboard);

/**
 * hildon_im_ui_set_context_options:
 * @self: #HildonIMUI
 * @option: #HildonIMOptionMask
 * @enable: the option is enabled?
 */
void
hildon_im_ui_set_context_options(HildonIMUI *ui,
                                 HildonIMOptionMask options,
                                 gboolean enable);


/*******************************************************************
 * UI interaction
 */ 
/**
 * hildon_im_ui_play_sound:
 * @self: #HildonIMUI
 * @sound: the #HildonIMUISound
 *
 * Play a sound
 */
void hildon_im_ui_play_sound(HildonIMUI *, HildonIMUISound);

/**
 * hildon_im_ui_button_set_menu:
 * @self: #HildonIMUI
 * @button: the button's id
 * @menu: the menu widget
 *
 * Attaches a menu widget to a common button 
 */
void hildon_im_ui_button_set_menu(HildonIMUI *keyboard,
                                  HildonIMButton button,
                                  GtkWidget *menu);

/**
 * hildon_im_ui_set_sensitive:
 * @self: #HildonIMUI
 * @button: the button's id
 * @sensitive: the sensitivity state
 *
 * Sets the sensitivity state of a common button
 */
void hildon_im_ui_button_set_sensitive(HildonIMUI *keyboard,
                                       HildonIMButton button,
                                       gboolean sensitive);

/**
 * hildon_im_ui_set_toggle:
 * @self: #HildonIMUI
 * @button: the button's id
 * @toggle: the toggle state
 *
 * Sets the toggle state of a common button
 */
void hildon_im_ui_button_set_toggle(HildonIMUI *keyboard,
                                    HildonIMButton button,
                                    gboolean toggle);

/**
 * hildon_im_ui_set_repeat:
 * @self: #HildonIMUI
 * @button: the button's id
 * @repeat: whether it should be repeated
 *
 * Sets whether a common button should be repeated when pressed long enough
 */
void hildon_im_ui_button_set_repeat(HildonIMUI *keyboard,
                                    HildonIMButton button,
                                    gboolean repeat);

/**
 * hildon_im_ui_set_long_press:
 * @self: #HildonIMUI
 * @button: the button's id
 * @long_press: whether the button should react to long press
 *
 * Sets whether a common button should activate without release when pressed long enough
 */
void hildon_im_ui_button_set_long_press(HildonIMUI *keyboard,
                                        HildonIMButton button,
                                        gboolean long_press);

/**
 * hildon_im_ui_set_label:
 * @self: #HildonIMUI
 * @button: the button's id
 * @label: the label
 *
 * Puts a label on a common button
 */
void hildon_im_ui_button_set_label(HildonIMUI *keyboard,
                                   HildonIMButton button,
                                   const gchar *label);
/**
 * hildon_im_ui_set_id:
 * @self: #HildonIMUI
 * @button: the button's id
 * @id: an id
 *
 * Sets the id of a common button
 */
void hildon_im_ui_button_set_id(HildonIMUI *self,
                                HildonIMButton button,
                                const gchar *id);

/**
 * hildon_im_ui_set_active:
 * @self: #HildonIMUI
 * @button: the button's id
 * @active: the state
 *
 * Sets the active state of a certain common button
 */
void hildon_im_ui_button_set_active(HildonIMUI *keyboard,
                                    HildonIMButton button,
                                    gboolean active);
/**
 * hildon_im_ui_button_get_active:
 * @self: #HildonIMUI
 * @button: the button's id
 *
 * Gets the active state of a certain common button
 */
gboolean hildon_im_ui_button_get_active(HildonIMUI *keyboard,
                                        HildonIMButton button);

/**
 * hildon_im_ui_set_common_buttons_visibility:
 * @self: #HildonIMUI
 * @visible: TRUE to set them to be visible, FALSE otherwise
 *
 * Sets the visibility of the common buttons.
 */
void hildon_im_ui_set_common_buttons_visibility(HildonIMUI *ui,
                                                gboolean visible);

/**
 * hildon_im_ui_get_language_setting:
 * @self: #HildonIMUI
 * @index: language index
 *
 * Gets the language setting for the specified language index
 */
const gchar * hildon_im_ui_get_language_setting(HildonIMUI *self,
                                                gint index);

/**
 * hildon_im_ui_set_language_setting:
 * @self: #HildonIMUI
 * @index: language index
 *
 * Sets the language setting to the specified language index
 *
 * Return value: TRUE if successful FALSE otherwise
 */
gboolean hildon_im_ui_set_language_setting(HildonIMUI *self,
                                           gint index,
                                           const gchar *new_language);

/**
 * hildon_im_ui_get_active_language:
 * @self: #HildonIMUI
 *
 * Gets the active language.
 */
const gchar * hildon_im_ui_get_active_language(HildonIMUI *self);

/**
 * hildon_im_ui_get_active_language_index:
 * @self: #HildonIMUI
 *
 * Gets the index of the active language.
 */
gint hildon_im_ui_get_active_language_index(HildonIMUI *self);

/**
 * hildon_im_ui_set_active_language_index:
 * @self: #HildonIMUI
 *
 * Sets the index of the active language.
 *
 * Return value: TRUE if successful FALSE otherwise
 */
gboolean hildon_im_ui_set_active_language_index(HildonIMUI *ui, gint index);

  /**
 * hildon_im_ui_get_plugin_buffer:
 * @ui: #HildonIMUI
 *
 * Retrieve the contents of the shared plugin buffer
 */
const gchar * hildon_im_ui_get_plugin_buffer(HildonIMUI *ui);

/**
 * hildon_im_ui_append_plugin_buffer:
 * @ui: #HildonIMUI
 * @val: The value to append
 *
 * Append a value to the end of the shared plugin buffer
 */
void hildon_im_ui_append_plugin_buffer(HildonIMUI *ui, const gchar *val);

/**
 * hildon_im_ui_erase_plugin_buffer:
 * @ui: #HildonIMUI
 * @len: number of characters to erase
 *
 * Erase a number of characters from the end of the shared plugin buffer
 */
void hildon_im_ui_erase_plugin_buffer(HildonIMUI *ui, gint len);

/**
 * hildon_im_ui_clear_plugin_buffer:
 * @ui: #HildonIMUI
 *
 * Clear the shared plugin buffer
 */
void hildon_im_ui_clear_plugin_buffer(HildonIMUI *ui);

G_END_DECLS
#endif
