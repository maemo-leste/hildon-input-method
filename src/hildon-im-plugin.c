/**
   @file: hildon-im-plugin.c

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

#include <string.h>

#include "hildon-im-plugin.h"

static GSList *loaded_modules = NULL;

typedef struct _HildonIMPluginModule            HildonIMPluginModule;
typedef struct _HildonIMPluginModuleClass       HildonIMPluginModuleClass;

struct _HildonIMPluginModule
{
  GTypeModule parent_instance;

  GModule *library;

  void               (*init)     (GTypeModule    *module);
  void               (*exit)     (void);
  HildonIMPlugin*    (*create)   (HildonIMUI *keyboard);

  /*if any member needs mem alloc, finalized is used to clean it*/
  gchar *path; /*where the share object is*/
};

struct _HildonIMPluginModuleClass
{
  GTypeModuleClass parent_class;
};

static GType hildon_im_plugin_module_get_type(void);

G_DEFINE_TYPE(HildonIMPluginModule, hildon_im_plugin_module, G_TYPE_TYPE_MODULE)
#define HILDON_IM_TYPE_PLUGIN_MODULE       (hildon_im_plugin_module_get_type())
#define HILDON_IM_PLUGIN_MODULE(module) \
        (G_TYPE_CHECK_INSTANCE_CAST((module), \
                HILDON_IM_TYPE_PLUGIN_MODULE, HildonIMPluginModule))

static gboolean hildon_im_plugin_module_load            (GTypeModule *module);

static void     hildon_im_plugin_module_unload          (GTypeModule *module);

static void     hildon_im_plugin_module_finalize        (GObject *object);

static void
hildon_im_plugin_module_class_init(HildonIMPluginModuleClass *class)
{
  GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS(class);
  GObjectClass *gobject_class = G_OBJECT_CLASS(class);

  module_class->load = hildon_im_plugin_module_load;
  module_class->unload = hildon_im_plugin_module_unload;

  gobject_class->finalize = hildon_im_plugin_module_finalize;
}

static void
hildon_im_plugin_module_init(HildonIMPluginModule *module)
{
  /* empty */
}

/*we come to this function from by g_type_module_use
 *purpose of this function is to find the strings in the
 *shared object to link the functions*/

static gboolean
hildon_im_plugin_module_load(GTypeModule *module)
{
  HildonIMPluginModule *himp_module = HILDON_IM_PLUGIN_MODULE(module);

  if (himp_module->path == NULL)
  {
    return FALSE;
  }

  himp_module->library = g_module_open(himp_module->path, 0);

  if (!himp_module->library)
  {
    g_warning (g_module_error());
    return FALSE;
  }

  /*module_init -> register the type
   *module_exit -> clean up the mess(done in init)
   *module_create -> create instance (g_type_new)*/
  if (!g_module_symbol(himp_module->library, "module_init",
                       (gpointer *)&himp_module->init) ||
      !g_module_symbol(himp_module->library, "module_exit",
                       (gpointer *)&himp_module->exit) ||
      !g_module_symbol(himp_module->library, "module_create",
                       (gpointer *)&himp_module->create))
  {
    g_warning (g_module_error());
    g_module_close (himp_module->library);

    return FALSE;
  }

  himp_module->init (module);

  return TRUE;
}

/*clear all pointers*/
static void
hildon_im_plugin_module_unload(GTypeModule *module)
{
  HildonIMPluginModule *himp_module;

  himp_module = HILDON_IM_PLUGIN_MODULE(module);
  himp_module->exit();

  g_module_close (himp_module->library);
  himp_module->library = NULL;

  himp_module->init = NULL;
  himp_module->exit = NULL;
  himp_module->create = NULL;
}

static void
hildon_im_plugin_module_finalize(GObject *object)
{
  HildonIMPluginModule *himp_module;

  himp_module = HILDON_IM_PLUGIN_MODULE(object);
  g_free(himp_module->path);

  himp_module->path = NULL;

  G_OBJECT_CLASS(hildon_im_plugin_module_parent_class)->finalize(object);
}

static HildonIMPlugin *
hildon_im_plugin_module_create(HildonIMUI *keyboard,
                               HildonIMPluginModule *himp_module)
{
  HildonIMPlugin *plugin;

  g_return_val_if_fail(HILDON_IM_IS_UI(keyboard), NULL);

  if (g_type_module_use(G_TYPE_MODULE(himp_module)))
  {
    plugin = himp_module->create(keyboard);
    g_type_module_unuse(G_TYPE_MODULE(himp_module));
    return plugin;
  }
  return NULL;
}

GType
hildon_im_plugin_get_type(void)
{
  static GType type = 0;

  if (!type)
  {
    static const GTypeInfo plugin_info =
    {
      sizeof (HildonIMPluginIface),  /* class_size */
      NULL,    /* base_init */
      NULL,    /* base_finalize */
    };

    type = g_type_register_static (G_TYPE_INTERFACE,
                                   "HildonIMPlugin", &plugin_info, 0);

    g_type_interface_add_prerequisite (type, GTK_TYPE_WIDGET);
  }

  return type;
}

/*public function to create plugin
 * string passed for opening the shared object*/
HildonIMPlugin *
hildon_im_plugin_create(HildonIMUI *keyboard,
                        const gchar *plugin_name)
{
  GSList *i = NULL;
  HildonIMPluginModule *module = NULL;
  HildonIMPlugin *plugin = NULL;

  for (i = loaded_modules; i != NULL; i = i->next)
  {
    module = i->data;
    if (strcmp(G_TYPE_MODULE(module)->name, plugin_name) == 0)
    {
      return hildon_im_plugin_module_create(keyboard, module);
    }
  }

  plugin = NULL;
  if (g_module_supported())
  {
    module = g_object_new(HILDON_IM_TYPE_PLUGIN_MODULE, NULL);
    g_type_module_set_name(G_TYPE_MODULE(module), plugin_name);
    module->path = g_strdup(plugin_name);
    loaded_modules = g_slist_prepend(loaded_modules, (void *) module);
    plugin = hildon_im_plugin_module_create(keyboard, module);
  }

  return plugin;
}

void
hildon_im_plugin_enable(HildonIMPlugin *plugin, gboolean init)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->enable)
  {
    iface->enable(plugin, init);
  }
}

void
hildon_im_plugin_disable(HildonIMPlugin *plugin)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->disable)
  {
    iface->disable(plugin);
  }
}

void
hildon_im_plugin_settings_changed(HildonIMPlugin *plugin,
                                  const gchar *key,
                                  const GConfValue *value)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->settings_changed)
  {
    iface->settings_changed(plugin, key, value);
  }
}

void
hildon_im_plugin_language_settings_changed(HildonIMPlugin *plugin,
                                           gint index)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->language_settings_changed)
  {
    iface->language_settings_changed(plugin, index);
  }
}

void
hildon_im_plugin_input_mode_changed(HildonIMPlugin *plugin)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->input_mode_changed)
  {
    iface->input_mode_changed(plugin);
  }
}

void
hildon_im_plugin_keyboard_state_changed(HildonIMPlugin *plugin)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->keyboard_state_changed)
  {
    iface->keyboard_state_changed(plugin);
  }
}

void
hildon_im_plugin_character_autocase(HildonIMPlugin *plugin)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->character_autocase)
  {
    iface->character_autocase(plugin);
  }
}

void
hildon_im_plugin_client_widget_changed(HildonIMPlugin *plugin)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->client_widget_changed)
  {
    iface->client_widget_changed(plugin);
  }
}


void
hildon_im_plugin_save_data(HildonIMPlugin *plugin)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->save_data)
  {
    iface->save_data(plugin);
  }
}

void
hildon_im_plugin_clear(HildonIMPlugin *plugin)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->clear)
  {
    iface->clear(plugin);
  }
}

void hildon_im_plugin_button_activated(HildonIMPlugin *plugin,
                                       HildonIMButton button,
                                       gboolean long_press)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->button_activated)
  {
    iface->button_activated(plugin, button, long_press);
  }
}

void
hildon_im_plugin_mode_a(HildonIMPlugin *plugin)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->mode_a)
  {
    iface->mode_a(plugin);
  }
}

void
hildon_im_plugin_mode_b(HildonIMPlugin *plugin)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->mode_b)
  {
    iface->mode_b(plugin);
  }
}

void
hildon_im_plugin_language(HildonIMPlugin *plugin)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->language)
  {
    iface->language(plugin);
  }
}

void
hildon_im_plugin_backspace(HildonIMPlugin *plugin)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->backspace)
  {
    iface->backspace(plugin);
  }
}

void
hildon_im_plugin_enter(HildonIMPlugin *plugin)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->enter)
  {
    iface->enter(plugin);
  }
}

void
hildon_im_plugin_tab(HildonIMPlugin *plugin)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->tab)
  {
    iface->tab(plugin);
  }
}

void
hildon_im_plugin_fullscreen(HildonIMPlugin *plugin, gboolean fullscreen)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->fullscreen)
  {
    iface->fullscreen(plugin, fullscreen);
  }
}

void
hildon_im_plugin_select_region(HildonIMPlugin *plugin, gint start, gint end)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->select_region)
  {
    iface->select_region(plugin, start, end);
  }
}

void
hildon_im_plugin_key_event(HildonIMPlugin *plugin,
                           GdkEventType type,
                           guint state,
                           guint keyval,
                           guint hardware_keycode)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->key_event)
  {
    iface->key_event(plugin, type, state, keyval, hardware_keycode);
  }
}

void
hildon_im_plugin_transition(HildonIMPlugin *plugin,
                            gboolean from)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->transition)
  {
    iface->transition(plugin, from);
  }
}

void
hildon_im_plugin_surrounding_received(HildonIMPlugin *plugin,
                                      const gchar *surrounding,
                                      gint offset)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->surrounding_received)
  {
    iface->surrounding_received(plugin, surrounding, offset);
  }
}

void
hildon_im_plugin_preedit_committed (HildonIMPlugin *plugin,
                                    const gchar *committed_preedit)
{
  HildonIMPluginIface *iface=NULL;

  g_return_if_fail(HILDON_IM_IS_PLUGIN(plugin));

  iface = HILDON_IM_PLUGIN_GET_IFACE(plugin);

  if (!iface)
  {
    return;
  }

  if (iface->preedit_committed)
  {
    iface->preedit_committed(plugin, committed_preedit);
  }
}

HildonIMPluginInfo *
hildon_im_plugin_duplicate_info(const HildonIMPluginInfo *src)
{
  HildonIMPluginInfo *info;

  g_return_val_if_fail(src != NULL, NULL);

  info = g_malloc0(sizeof(HildonIMPluginInfo));

  memcpy(info, src, sizeof(HildonIMPluginInfo));
  info->description = g_strdup(src->description);
  info->ossohelp_id = g_strdup(src->ossohelp_id);
  if (src->menu_title != NULL)
  {
    info->menu_title = g_strdup(src->menu_title);
    if (src->gettext_domain != NULL)
    {
      info->gettext_domain = g_strdup(src->gettext_domain);
    }
  }
  info->name = g_strdup(src->name);
  if (src->special_plugin != NULL)
  {
    info->special_plugin = g_strdup(src->special_plugin);
  }

  return info;
}
