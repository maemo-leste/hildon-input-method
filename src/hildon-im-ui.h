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

#define HILDON_IM_GCONF_LANG_DIR                   HILDON_IM_GCONF_DIR       "/hildon-im-languages"
#define _HILDON_IM_GCONF_LANGUAGE                  HILDON_IM_GCONF_LANG_DIR  "/language-"
#define HILDON_IM_GCONF_PRIMARY_LANGUAGE           _HILDON_IM_GCONF_LANGUAGE "0"
#define HILDON_IM_GCONF_SECONDARY_LANGUAGE         _HILDON_IM_GCONF_LANGUAGE "1"
#define HILDON_IM_GCONF_CURRENT_LANGUAGE           HILDON_IM_GCONF_LANG_DIR  "/current"
#define HILDON_IM_GCONF_USE_FINGER_KB              HILDON_IM_GCONF_DIR       "/use_finger_kb"


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

/**
 * hildon_im_ui_new:
 * @dir: the name od the directory where ui layouts are located
 *
 * @Returns: a newly allocated #HildonIMUI
 *
 * Creates and returns a #HildonIMUI
 */
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
 * Returns: a #gboolean
 */
gboolean hildon_im_ui_get_visibility(HildonIMUI *self);

/**
 * hildon_im_ui_get_current_input_mode:
 * @self: #HildonIMUI
 * 
 * Returns the current input mode of the ui or #HILDON_IM_HIDE if
 * @self is not a valid #HildonIMUI.
 *
 * Returns: the current #HildonIMMode of the UI.
 */
HildonGtkInputMode hildon_im_ui_get_current_input_mode(HildonIMUI *self);

/**
 * hildon_im_ui_get_current_default_input_mode:
 * @self: #HildonIMUI
 * 
 * Gets the current default input mode
 *
 * Returns: the current default #HildonGtkInputMode
 */
HildonGtkInputMode hildon_im_ui_get_current_default_input_mode(HildonIMUI *self);

/**
 * hildon_im_ui_get_autocase_mode:
 * @self: #HildonIMUI
 *
 * Returns the autocase mode of the ui or #HILDON_IM_LOW if
 * @self is not a valid #HildonIMUI
 *
 * Returns: a #HildonIMCommand
 */
HildonIMCommand hildon_im_ui_get_autocase_mode(HildonIMUI *self);

/**
 * hildon_im_ui_get_commit_mode:
 * @self: #HildonIMUI
 *
 * Gets the current commit mode of the input method
 *
 * Returns: a #HildonIMCommitMode
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
 * Sends a utf-8 string to the client widget.
 */
void hildon_im_ui_send_utf8(HildonIMUI *self, const gchar *utf8);

/**
 * hildon_im_ui_send_communication_message:
 * @self: #HildonIMUI
 * @message: the message
 *
 * Sends a #HildonIMComMessage of type @message with a XEvent
 * to the IM context
 */
void hildon_im_ui_send_communication_message(HildonIMUI *self,
                                                   gint message);

/**
 * hildon_im_ui_send_surrounding_content:
 * @self: #HildonIMUI
 * @surrounding: the surrounding string
 *
 * Sends the surrounding context around the insertion point to the IM context.
 */
void hildon_im_ui_send_surrounding_content(HildonIMUI *self,
                                           const gchar *surrounding);
/**
 * hildon_im_ui_send_surrounding_offset:
 * @self: #HildonIMUI
 * @is_relative: whether it is a relative offset
 * @offset: the offset
 *
 * Sends the character offset of the cursor location in the surrounding content.
 */
void hildon_im_ui_send_surrounding_offset(HildonIMUI *self,
                                          gboolean is_relative,
                                          gint offset);
/**
 * hildon_im_ui_get_surrounding:
 * @self: a #HildonIMUI
 *
 * @Returns: the surrounding text
 *
 * Returns the context around the cursor
 */
const gchar *hildon_im_ui_get_surrounding(HildonIMUI *keyboard);

/**
 * hildon_im_ui_get_surrounding_offest:
 * @self: a #HildonIMUI
 *
 * Returns the character offset of the cursor in the surrounding content.
 * 
 * @Returns: the offset
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
 * Returns: TRUE if successful FALSE otherwise
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
 * Returns: TRUE if successful, FALSE otherwise
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

/**
 * hildon_im_ui_parse_rc_file:
 * @self: #HildonIMUI
 * @rc_file: the gtkrc file to parse
 *
 * Parses the given rc_file only if it has not been parsed before.
 */
void hildon_im_ui_parse_rc_file (HildonIMUI *ui, gchar *rc_file);

/**
 * hildon_im_ui_get_input_window:
 * @ui: #HildonIMUI
 *
 * Returns: The client application's X window
 */
Window hildon_im_ui_get_input_window (HildonIMUI *ui);

/**
 * hildon_im_ui_get_mask:
 * @self: #HildonIMUI
 *
 * Returns: The mask representing shift and level states as in client context
 */
HildonIMInternalModifierMask hildon_im_ui_get_mask (HildonIMUI *self);

/**
 * hildon_im_ui_set_mask:
 * @self: #HildonIMUI
 * @mask: a #HildonIMInternalModifierMask representing the new mask
 *
 * Sets the new states of shift and level to be sent to the client context
 */
void hildon_im_ui_set_mask (HildonIMUI *self, HildonIMInternalModifierMask mask);

gboolean hildon_im_ui_get_shift_locked (HildonIMUI *self);

void hildon_im_ui_set_shift_locked (HildonIMUI *self, gboolean locked);

gboolean hildon_im_ui_get_level_locked (HildonIMUI *self);

void hildon_im_ui_set_level_locked (HildonIMUI *self, gboolean locked);

gboolean hildon_im_ui_get_shift_sticky (HildonIMUI *self);

void hildon_im_ui_set_shift_sticky (HildonIMUI *self, gboolean sticky);

gboolean hildon_im_ui_get_level_sticky (HildonIMUI *self);

void hildon_im_ui_set_level_sticky (HildonIMUI *self, gboolean sticky);

G_END_DECLS
#endif
