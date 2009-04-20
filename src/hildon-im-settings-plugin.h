/**
   @file: hildon-im-settings-plugin.h

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


#ifndef __HILDON_IM_SETTINGS_PLUGIN_H__
#define __HILDON_IM_SETTINGS_PLUGIN_H__

#include <gtk/gtksizegroup.h>
#include <gtk/gtkwidget.h>
#include <libosso.h>
#include "hildon-im-ui.h"

G_BEGIN_DECLS

typedef struct _HildonIMSettingsPluginManager HildonIMSettingsPluginManager;
typedef struct _HildonIMSettingsPlugin HildonIMSettingsPlugin;
typedef struct _HildonIMSettingsPluginIface HildonIMSettingsPluginIface;

#define HILDON_IM_TYPE_SETTINGS_PLUGIN (hildon_im_settings_plugin_get_type())
#define HILDON_IM_SETTINGS_PLUGIN(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj), HILDON_IM_TYPE_SETTINGS_PLUGIN, \
                                    HildonIMSettingsPlugin))
#define HILDON_IM_IS_SETTINGS_PLUGIN(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj), HILDON_IM_TYPE_SETTINGS_PLUGIN))
#define HILDON_IM_SETTINGS_PLUGIN_GET_IFACE(inst) \
        (G_TYPE_INSTANCE_GET_INTERFACE((inst), HILDON_IM_TYPE_SETTINGS_PLUGIN, \
                                       HildonIMSettingsPluginIface))

#define HILDON_IM_SETTINGS_DEVICE_ID          "DeviceID"
#define HILDON_IM_SETTINGS_PRIMARY_LANGUAGE   "PrimaryLanguage"
#define HILDON_IM_SETTINGS_SECONDARY_LANGUAGE "SecondaryLanguage"
#define HILDON_IM_SETTINGS_SELECTED_LANGUAGE  "SelectedLanguage"

typedef enum
{
  HILDON_IM_SETTINGS_HARDWARE,
  HILDON_IM_SETTINGS_ONSCREEN,
  HILDON_IM_SETTINGS_LANGUAGE_GENERAL,
  HILDON_IM_SETTINGS_PRIMARY_LANGUAGE_SETTINGS_WIDGET,
  HILDON_IM_SETTINGS_SECONDARY_LANGUAGE_SETTINGS_WIDGET,
  HILDON_IM_SETTINGS_LANGUAGE_ADDITIONAL,
  HILDON_IM_SETTINGS_OTHER,
} HildonIMSettingsCategory;

typedef struct
{
  HildonIMSettingsPlugin *plugin;
  gchar *name;
} HildonIMSettingsPluginInfo;

struct _HildonIMSettingsPluginIface
{
  GTypeInterface base_iface;

  GtkWidget *(*create_widget) (HildonIMSettingsPlugin *, HildonIMSettingsCategory, GtkSizeGroup *, gint *);
  void (*value_changed) (HildonIMSettingsPlugin *, const gchar*, GType type, gpointer value);
  void (*save_data) (HildonIMSettingsPlugin *, HildonIMSettingsCategory where);
  void (*reload) (HildonIMSettingsPlugin *);
	void (*set_manager) (HildonIMSettingsPlugin *, HildonIMSettingsPluginManager *);
};

GType hildon_im_settings_plugin_get_type(void);

GtkWidget *hildon_im_settings_plugin_create_widget (HildonIMSettingsPlugin *,
    HildonIMSettingsCategory, GtkSizeGroup *, gint *);
void hildon_im_settings_plugin_value_changed (HildonIMSettingsPlugin *plugin, const gchar *, GType, gpointer);
void hildon_im_settings_plugin_save_data (HildonIMSettingsPlugin *, HildonIMSettingsCategory);

/* TODO split the APIs of HildonIMSettingsPlugin and 
 * HildonIMSettingsPluginManager in two sets of source files 
 */

/* Settings Manager */
HildonIMSettingsPluginManager *hildon_im_settings_plugin_manager_new (void);
void hildon_im_settings_plugin_manager_destroy (HildonIMSettingsPluginManager *);
gboolean hildon_im_settings_plugin_manager_load_plugins (HildonIMSettingsPluginManager *);
GSList *hildon_im_settings_plugin_manager_get_plugins (HildonIMSettingsPluginManager *);
void hildon_im_settings_plugin_manager_set_internal_value (HildonIMSettingsPluginManager *, GType, const gchar *, gpointer);
void hildon_im_settings_plugin_manager_unset_internal_value (HildonIMSettingsPluginManager *, const gchar *);
gpointer hildon_im_settings_plugin_manager_get_internal_value (HildonIMSettingsPluginManager *, const gchar *, GType *);
osso_context_t* hildon_im_settings_plugin_manager_get_context(HildonIMSettingsPluginManager *m);
void hildon_im_settings_plugin_manager_set_context(HildonIMSettingsPluginManager *m, osso_context_t *osso);

G_END_DECLS

#endif
