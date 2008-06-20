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
#include <glib.h>
#include "hildon-im-languages.h"
#include "hildon-im-ui.h"
#include <dlfcn.h>
#include "internal.h"
#include "config.h"

#define GCONF_TRANSLATION_LIBRARY HILDON_IM_GCONF_LANG_DIR "/translation-library"
#define GCONF_TRANSLATION_LIBNAME GCONF_TRANSLATION_LIBRARY "/name"
#define GCONF_TRANSLATION_FUNCTION GCONF_TRANSLATION_LIBRARY "/function"

#define GCONF_TRANSLATION_STATIC_PATH HILDON_IM_GCONF_LANG_DIR "/endonyms/"

static void *handle;
typedef gchar *(*translate_func)(const gchar*);
translate_func translate_function = NULL;

static void
setup_translation_library (const gchar *libname, const gchar *func_name)
{
  if (libname == NULL || func_name == NULL)
    return;

  handle = dlopen (libname, RTLD_LAZY);
  if (handle == NULL)
  {
    g_warning("Opening %s failed! (%s)", libname, dlerror());
  }

  translate_function = G_GNUC_EXTENSION (translate_func) dlsym(handle, func_name);
  if (translate_function == NULL)
  {
    g_warning ("No translate function %s in %s library.", func_name, libname);
    dlclose (handle);    
  }
}

#define MAX_LANG_LENGTH 20
gchar *
hildon_im_get_language_description (const char *lang)
{
  static GConfClient *client = NULL;
  static gboolean configured = FALSE;

  if (configured == FALSE)
  {
    gchar *lib_name, *func_name;
    if (client == NULL)
      client = gconf_client_get_default ();
    
    lib_name = gconf_client_get_string (client, GCONF_TRANSLATION_LIBNAME, NULL);
    func_name = gconf_client_get_string (client, GCONF_TRANSLATION_FUNCTION, NULL);

    setup_translation_library (lib_name, func_name);
    FREE_IF_SET (lib_name);
    FREE_IF_SET (func_name);
    configured = TRUE;
  }
  if (translate_function)
  {
    return (*translate_function) (lang);
  } else
  {
    gchar path [sizeof(GCONF_TRANSLATION_STATIC_PATH) + MAX_LANG_LENGTH];
    if (client == NULL)
      client = gconf_client_get_default ();

    strcpy (path, GCONF_TRANSLATION_STATIC_PATH);
    strncpy (path + sizeof(GCONF_TRANSLATION_STATIC_PATH) - 1, lang, MAX_LANG_LENGTH - 1);
    return gconf_client_get_string (client, path, NULL);
  }
}

void 
hildon_im_free_available_languages (GSList *list)
{
  GSList *l;
  HildonIMLanguage *himl;
  if (list == NULL)
  {
    return;
  }

  l = list;
  while (l != NULL)
  {
    himl = (HildonIMLanguage*) l->data;
    g_free (himl->language_code);
    g_free (himl->description);
    g_free (himl);
    l = g_slist_next (l);
  }
  g_slist_free (list);
}

GSList *
hildon_im_get_available_languages (void)
{
  GSList *retval = NULL;
  GConfClient *client;
  GSList *list, *l;
  HildonIMLanguage *lang;

  client = gconf_client_get_default();
  list = gconf_client_get_list (client, GCONF_AVAILABLE_LANGUAGES, GCONF_VALUE_STRING, NULL);

  if (list != NULL)
  {
    l = list;
    while (l != NULL)
    {
      lang = g_malloc0 (sizeof (HildonIMLanguage));
      if (lang == NULL)
      {
        /* No memory */
        break;
      }

      lang->language_code = l->data;
      lang->description = hildon_im_get_language_description (l->data);
      if (lang->description == NULL)
        lang->description = g_strdup (l->data);

      retval = g_slist_append (retval, (gpointer) lang);
      l = g_slist_next (l);
    }
    g_slist_free (list);
  }
  
  return retval;
}

void
hildon_im_populate_available_languages (GSList *list)
{
  GConfClient *client;

  if (list == NULL)
  {
    return;
  }

  client = gconf_client_get_default();
  gconf_client_unset (client, GCONF_AVAILABLE_LANGUAGES, NULL);
  gconf_client_set_list (client, GCONF_AVAILABLE_LANGUAGES, GCONF_VALUE_STRING, list, NULL);
}

