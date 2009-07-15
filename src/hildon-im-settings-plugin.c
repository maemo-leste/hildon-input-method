/**
   @file: hildon-im-settings-plugin.c

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


#include <glib.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>

#include "config.h"
#include "hildon-im-settings-plugin.h"

#define PLUGIN_INIT "settings_plugin_init" 
#define PLUGIN_INFO_NAME "settings_plugin_info"


static GType module_get_type (void);

typedef struct 
{
  GType     type;
  gpointer  value;
} InternalData;

struct _HildonIMSettingsPluginManager
{
  GHashTable *values;
  GSList *plugin_list;
  gboolean exit_registered;
  osso_context_t *osso;
};

GSList *module_list = NULL;

#define HILDON_IM_TYPE_SETTINGS_MODULE       (module_get_type())
#define HILDON_IM_SETTINGS_MODULE(module) \
        (G_TYPE_CHECK_INSTANCE_CAST((module), \
                HILDON_IM_TYPE_SETTINGS_MODULE, HildonIMSettingsModule))

typedef struct _HildonIMSettingsModule      HildonIMSettingsModule;
typedef struct _HildonIMSettingsModuleClass HildonIMSettingsModuleClass;

struct _HildonIMSettingsModule
{
  GTypeModule parent;
  gchar   *name;
  gchar   *path;
  GModule *lib;

  void (*init) (GTypeModule *);
  void (*exit) (void);  
  HildonIMSettingsPlugin *(*create) (void);
};

struct _HildonIMSettingsModuleClass
{
  GTypeModuleClass parent;
};

G_DEFINE_TYPE(HildonIMSettingsModule, module, G_TYPE_TYPE_MODULE);

HildonIMSettingsPluginManager *manager;


static void
module_init (HildonIMSettingsModule *module)
{
  /* empty */
}

static gboolean
module_load (GTypeModule *_module)
{
  HildonIMSettingsModule *module = HILDON_IM_SETTINGS_MODULE (_module);

  if (module->path == NULL)
  {
    return FALSE;
  }

  module->lib = g_module_open (module->path, 0);

  if (module->lib == NULL)
  {
    g_warning (g_module_error());
    return FALSE;
  }

  if (!g_module_symbol (module->lib, "settings_plugin_init",
        (gpointer *)&module->init) ||
      !g_module_symbol (module->lib, "settings_plugin_exit",
        (gpointer *)&module->exit) ||
      !g_module_symbol (module->lib, "settings_plugin_new",
        (gpointer *)&module->create))
  {
    g_debug ("Not a HildonIMSettignsPlugin: %s. Skip it.", g_module_error ());
    g_module_close (module->lib);

    module->lib = NULL;
    return FALSE;
  }

  module->init (_module);
 
  return TRUE;
}

static void
module_unload (GTypeModule *_module)
{
  HildonIMSettingsModule *module = HILDON_IM_SETTINGS_MODULE (_module);

  if (module->exit)
    module->exit();
 
  if (module->lib)
    g_module_close (module->lib);
 
  module->lib = NULL;

  module->init = NULL;
  module->exit = NULL;
  module->create = NULL;  
}

static void
module_finalize (GObject *object)
{
  HildonIMSettingsModule *module = HILDON_IM_SETTINGS_MODULE (object);
  
  g_free (module->path);
  module->path = NULL;
  G_OBJECT_CLASS (module_parent_class)->finalize(object);
}

static void
module_class_init (HildonIMSettingsModuleClass *class)
{
  GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS(class);
  GObjectClass *gobject_class = G_OBJECT_CLASS(class);

  module_class->load = module_load;
  module_class->unload = module_unload;

  gobject_class->finalize = module_finalize;
}

GType
hildon_im_settings_plugin_get_type(void)
{
  static GType type = 0;

  if (!type)
  {
    static const GTypeInfo plugin_info =
    {
      sizeof (HildonIMSettingsPluginIface),  /* class_size */
      NULL,    /* base_init */
      NULL,    /* base_finalize */
    };

    type = g_type_register_static (G_TYPE_INTERFACE,
                                   "HildonIMSettingsPlugin", &plugin_info, 0);
 
  }

  return type;
}

GtkWidget *
hildon_im_settings_plugin_create_widget (HildonIMSettingsPlugin *plugin,
    HildonIMSettingsCategory where, GtkSizeGroup *size_group,
    gint *weight)
{
  HildonIMSettingsPluginIface *iface=NULL;

  g_return_val_if_fail(HILDON_IM_IS_SETTINGS_PLUGIN(plugin), NULL);

  iface = HILDON_IM_SETTINGS_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return NULL;
  }

  if (iface->create_widget)
  {
    return iface->create_widget (plugin, where, size_group, weight);
  }

  return NULL;
}

void
hildon_im_settings_plugin_value_changed (HildonIMSettingsPlugin *plugin, 
		const gchar *key,
		GType type, gpointer value)
{
  HildonIMSettingsPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_SETTINGS_PLUGIN(plugin));

  iface = HILDON_IM_SETTINGS_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->value_changed)
  {
    iface->value_changed (plugin, key, type, value);
  }
}

void
hildon_im_settings_plugin_save_data (HildonIMSettingsPlugin *plugin, HildonIMSettingsCategory where)
{
  HildonIMSettingsPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_SETTINGS_PLUGIN(plugin));

  iface = HILDON_IM_SETTINGS_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->save_data)
  {
    iface->save_data (plugin, where);
  }
}

static void
hildon_im_settings_plugin_set_manager (HildonIMSettingsPlugin *plugin,
                                       HildonIMSettingsPluginManager *m)
{
  HildonIMSettingsPluginIface *iface=NULL;

  g_return_if_fail (HILDON_IM_IS_SETTINGS_PLUGIN(plugin));
	g_return_if_fail (m != NULL);

  iface = HILDON_IM_SETTINGS_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->set_manager)
  {
    iface->set_manager (plugin, m);
  }
}

static void
cleanup_hash (HildonIMSettingsPluginManager *m)
{
  g_return_if_fail (m != NULL);
  if (m->values == NULL)
    return;

  g_hash_table_destroy (m->values);
  m->values = NULL;
}

static void
init_hash (HildonIMSettingsPluginManager *m)
{
  g_return_if_fail (m != NULL);
  if (m->values != NULL)
    cleanup_hash (m);

  m->values = g_hash_table_new_full (g_str_hash, g_str_equal, 
      (GDestroyNotify) g_free,
      (GDestroyNotify) g_free);
}

static gint
find_plugin_by_name (gconstpointer data, gconstpointer userdata)
{
  HildonIMSettingsPluginInfo *info = (HildonIMSettingsPluginInfo *) data;

  g_return_val_if_fail (info != NULL && info->name != NULL && userdata != NULL, -1);
  return g_ascii_strcasecmp (info->name, (gchar *) userdata);
}

static gint
find_module_by_name (gconstpointer data, gconstpointer userdata)
{
  HildonIMSettingsModule *m = (HildonIMSettingsModule *) data;

  g_return_val_if_fail (m != NULL && m->name != NULL, -1);
  if (userdata != NULL)
    return g_ascii_strcasecmp (m->name, (gchar *) userdata);

  return -1;
}

static gboolean
load_module (HildonIMSettingsPluginManager *m, const gchar *filename)
{
  HildonIMSettingsModule *module = NULL;
  HildonIMSettingsPlugin *plugin;
  gboolean add_module = FALSE;
  GSList *module_found = NULL;

  g_return_val_if_fail (m != NULL, FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);

  if (m->plugin_list &&
      g_slist_find_custom (m->plugin_list, filename, find_plugin_by_name))
    return FALSE;

  if (module_list)
    module_found = g_slist_find_custom (module_list, filename, find_module_by_name);
  
  if (module_found == NULL)
  {
    module = g_object_new (HILDON_IM_TYPE_SETTINGS_MODULE, NULL);
    module->path = g_build_filename (PREFIX, "lib", IM_PLUGIN_DIR, 
      filename, NULL);
    add_module = TRUE;
  } else {
    module = (HildonIMSettingsModule *)module_found->data;
  }

  if (module != NULL)
  {
    if (g_type_module_use (G_TYPE_MODULE (module)))
    {
      HildonIMSettingsPluginInfo *info = g_malloc (sizeof (HildonIMSettingsPluginInfo));
      if (module->create == NULL)
        module_load (G_TYPE_MODULE (module));

      plugin = module->create ();
      g_type_module_unuse (G_TYPE_MODULE (module));

      hildon_im_settings_plugin_set_manager (plugin, m);
      info->plugin = plugin;
      info->name = g_strdup (filename);
      m->plugin_list = g_slist_prepend (m->plugin_list, info);
      if (add_module)
      {
        module->name = g_strdup (filename);
        module_list = g_slist_prepend (module_list, module);
      }

      return TRUE;
    } else {
      g_object_unref (module);
    }    
  }
  return FALSE;
}

void
hildon_im_settings_plugin_manager_unload_plugins (HildonIMSettingsPluginManager *m)
{
  GSList *iter;

  g_return_if_fail (m != NULL);

  iter = m->plugin_list;  
  while (iter)
  {
    HildonIMSettingsPluginInfo *plugin = (HildonIMSettingsPluginInfo*) iter->data;

    g_free (plugin->name);
    g_object_unref (plugin->plugin);
    g_free (plugin);
    
    iter = g_slist_next (iter);
  }
}

gboolean
hildon_im_settings_plugin_manager_load_plugins (HildonIMSettingsPluginManager *m)
{
  gchar *plugin_dir_name;
  GDir *dir;
  const gchar *entry;

  plugin_dir_name = g_build_filename (PREFIX, "lib", IM_PLUGIN_DIR, NULL);
  dir = g_dir_open (plugin_dir_name, 0, NULL);
  g_free (plugin_dir_name);
  if (dir == FALSE)
  {
    g_warning ("Unable to open directory %s", IM_PLUGIN_DIR);
    return FALSE;
  }

  while ((entry = g_dir_read_name (dir)) != NULL)
  {
    if (g_str_has_suffix (entry, ".so"))
    {
      if (load_module (m, entry) == FALSE)
      {
        
      }
    }
  }

  return TRUE;
}

HildonIMSettingsPluginManager *
hildon_im_settings_plugin_manager_new (void)
{
  if (manager)
  {
    return manager;
  }
  
  manager = (HildonIMSettingsPluginManager *) g_malloc0 (sizeof (HildonIMSettingsPluginManager));

  init_hash (manager);
  return manager;
}

void
hildon_im_settings_plugin_manager_destroy (HildonIMSettingsPluginManager *m)
{
  g_return_if_fail (m != NULL);
  hildon_im_settings_plugin_manager_unload_plugins (m);  
  cleanup_hash (m);
  g_slist_free (m->plugin_list);
  g_free (m);
  m = NULL;
  manager = NULL;
}

GSList *
hildon_im_settings_plugin_manager_get_plugins (HildonIMSettingsPluginManager *m)
{
  g_return_val_if_fail (m != NULL, NULL);
  return m->plugin_list;
}

static void
plugin_data_changed (HildonIMSettingsPluginManager *m, 
		const gchar *key, GType type, gpointer value)
{
  GSList *iter_p;

  g_return_if_fail (m != NULL);
  iter_p = m->plugin_list;

  while (iter_p)
  {
    HildonIMSettingsPluginInfo *info = (HildonIMSettingsPluginInfo*) 
      iter_p->data;
    if (info && info->plugin)
      hildon_im_settings_plugin_value_changed (info->plugin, key, type, value);
    iter_p = g_slist_next (iter_p);
  }
}

void
hildon_im_settings_plugin_manager_set_internal_value (HildonIMSettingsPluginManager *m, 
    GType type, 
    const gchar *key, gpointer value)
{
  InternalData *d;
  g_return_if_fail (m != NULL);

  d = (InternalData *) g_malloc0 (sizeof (InternalData));
  d->type = type;
  d->value = value;

  g_hash_table_insert (m->values, g_strdup(key), d);
  plugin_data_changed (m, key, type, value);
}

void
hildon_im_settings_plugin_manager_unset_internal_value (HildonIMSettingsPluginManager *m, 
    const gchar *key)
{
  g_return_if_fail (m != NULL);
  g_hash_table_remove (m->values, key);
  plugin_data_changed (m, key, G_TYPE_NONE, NULL);
}

gpointer
hildon_im_settings_plugin_manager_get_internal_value (HildonIMSettingsPluginManager *m, 
    const gchar *key, GType *type)
{
  gpointer retval = NULL;
  InternalData *d;
  g_return_val_if_fail (m != NULL, NULL);

  d = (InternalData *) g_hash_table_lookup (m->values, key);
  if (d != NULL)
  {
    *type         = d->type;
    retval        = d->value;
  }

  return retval;
}


void 
hildon_im_settings_plugin_manager_set_context(HildonIMSettingsPluginManager *m, osso_context_t *osso_context)
{
	g_return_if_fail (m != NULL);
	m->osso = osso_context;
}

osso_context_t* hildon_im_settings_plugin_manager_get_context(HildonIMSettingsPluginManager *m)
{
	g_return_val_if_fail (m != NULL, NULL);
	return m->osso;
}
