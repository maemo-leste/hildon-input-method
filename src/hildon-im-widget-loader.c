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

#include "config.h"
#include <string.h>
#include "hildon-im-widget-loader.h"

static gboolean   hildon_im_widget_loader_load    (GTypeModule *module);
static void       hildon_im_widget_loader_unload  (GTypeModule *module);
static void       hildon_im_widget_loader_finalize(GObject *object);

static GSList *loaded_modules = NULL;

typedef struct _HildonIMWidgetLoader            HildonIMWidgetLoader;
typedef struct _HildonIMWidgetLoaderClass       HildonIMWidgetLoaderClass;

struct _HildonIMWidgetLoader
{
  GTypeModule parent_instance;

  GModule *library;

  void               (*init)     (GTypeModule *module);
  void               (*exit)     (void);
  GtkWidget*         (*create)   ();

  gchar *library_name;
  gchar *widget_name;
};

struct _HildonIMWidgetLoaderClass
{
  GTypeModuleClass parent_class;
};

static GType hildon_im_widget_loader_get_type(void);

G_DEFINE_TYPE(HildonIMWidgetLoader, hildon_im_widget_loader, G_TYPE_TYPE_MODULE)
#define HILDON_IM_TYPE_WIDGET_LOADER       (hildon_im_widget_loader_get_type())
#define HILDON_IM_WIDGET_LOADER(module) \
        (G_TYPE_CHECK_INSTANCE_CAST((module), \
                HILDON_IM_TYPE_WIDGET_LOADER, HildonIMWidgetLoader))

static void
hildon_im_widget_loader_class_init(HildonIMWidgetLoaderClass *class)
{
  GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS(class);
  GObjectClass *gobject_class = G_OBJECT_CLASS(class);

  module_class->load = hildon_im_widget_loader_load;
  module_class->unload = hildon_im_widget_loader_unload;

  gobject_class->finalize = hildon_im_widget_loader_finalize;
}

static void
hildon_im_widget_loader_init(HildonIMWidgetLoader *loader)
{
}

static inline gboolean
resolve_symbol(GModule *module, const gchar *widget, const gchar *func,
               gpointer *target)
{
  gboolean re;
  gchar *sym;
  
  sym = g_strconcat("dyn_" , widget, "_", func, NULL);
  re = g_module_symbol(module, sym, target);
  g_free(sym);
  
  return re;
}

static gboolean
hildon_im_widget_loader_load(GTypeModule *module)
{
  HildonIMWidgetLoader *widget_loader = HILDON_IM_WIDGET_LOADER(module);
  gboolean re;

  g_return_val_if_fail(widget_loader->library_name != NULL &&
                       widget_loader->widget_name != NULL,
                       FALSE);

  widget_loader->library = g_module_open(widget_loader->library_name, 0);

  if(widget_loader->library == NULL)
  {
    g_warning (g_module_error());
    return FALSE;
  }

  /*module_init -> register the type
   *module_exit -> clean up the mess(done in init)
   *module_create -> create instance (g_type_new)*/
  re = resolve_symbol(widget_loader->library,
                      widget_loader->widget_name, "init",
                      (gpointer *) &widget_loader->init);
  re &= resolve_symbol(widget_loader->library,
                       widget_loader->widget_name, "exit",
                       (gpointer *) &widget_loader->exit);
  re &= resolve_symbol(widget_loader->library,
                       widget_loader->widget_name, "create",
                       (gpointer *) &widget_loader->create);
  if (!re)
  {
    g_warning(g_module_error());
    g_module_close(widget_loader->library);

    return FALSE;
  }

  widget_loader->init(module);

  return TRUE;
}

/*clear all pointers*/
static void
hildon_im_widget_loader_unload(GTypeModule *module)
{
  HildonIMWidgetLoader *widget_loader = HILDON_IM_WIDGET_LOADER(module);

  widget_loader->exit();

  g_module_close(widget_loader->library);
  widget_loader->library = NULL;

  widget_loader->init = NULL;
  widget_loader->exit = NULL;
  widget_loader->create = NULL;
}

static void
hildon_im_widget_loader_finalize(GObject *object)
{
  HildonIMWidgetLoader *widget_loader = HILDON_IM_WIDGET_LOADER(object);

  g_free(widget_loader->widget_name);
  g_free(widget_loader->library_name);

  widget_loader->widget_name = NULL;
  widget_loader->library_name = NULL;

  G_OBJECT_CLASS(hildon_im_widget_loader_parent_class)->finalize(object);
}

static GtkWidget*
hildon_im_widget_loader_create(HildonIMWidgetLoader *widget_loader,
                               const gchar *first_property_name,
                               va_list valist)
{
  GtkWidget *widget = NULL;

  if (g_type_module_use(G_TYPE_MODULE(widget_loader)))
  {
    widget = widget_loader->create(first_property_name, valist);
    g_type_module_unuse(G_TYPE_MODULE(widget_loader));
    return widget;
  }

  return NULL;
}

/*public interface*/
GtkWidget *
hildon_im_widget_load(const gchar *library_name, const gchar *widget_name, 
                      const gchar *first_property_name, ...)
{
  GSList *i;
  HildonIMWidgetLoader *module;
  GtkWidget *widget = NULL;
  va_list var_args;

  va_start(var_args, first_property_name);

  for (i = loaded_modules; i != NULL; i = i->next)
  {
    module = i->data;
    if (strcmp(G_TYPE_MODULE(module)->name, widget_name) == 0)
    {
      widget = hildon_im_widget_loader_create
              (module, first_property_name, var_args);
    }
  }

  if (widget == NULL && g_module_supported())
  {
    module = g_object_new(HILDON_IM_TYPE_WIDGET_LOADER, NULL);
    g_type_module_set_name(G_TYPE_MODULE(module), widget_name);
    module->library_name = g_strconcat(LIBDIR, G_DIR_SEPARATOR_S,
				IM_WIDGET_DIR, G_DIR_SEPARATOR_S,
				library_name, ".", G_MODULE_SUFFIX,
				NULL);
    module->widget_name = g_strdup(widget_name);
    loaded_modules = g_slist_prepend(loaded_modules, (void *) module);
    widget = hildon_im_widget_loader_create
            (module, first_property_name, var_args);
  }
  
  va_end(var_args);

  return widget;
}


