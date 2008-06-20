/*
 * This file is part of hildon-input-method
 *
 * Copyright (C) 2007 Nokia Corporation.
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

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <glib.h>
#include "cache.h"
#include "hildon-im-plugin.h"

static gboolean
write_int (FILE *f, gint i)
{
  return (fwrite (&i, 1, sizeof (gint), f) == sizeof (gint));
}

static gboolean
write_byte (FILE *f, gchar i)
{
  return (fwrite (&i, 1, 1, f) == 1);
}

static gboolean
write_string (FILE *f, gchar *s)
{
  gboolean retval = FALSE;
  gint size;
  
  if (s == NULL)
    size = 0;
  else 
    size = strlen (s);

  retval = write_byte (f, (gchar) size);
  if (size && retval)
  {
    retval &= (fwrite (s, 1, size, f) == size);
  }

  return retval;
}

static gint        
lang_compare (gconstpointer a, gconstpointer b)
{
  if (a == NULL || b == NULL)
  {
    return -1;
  }

  return strcmp (a, b);
}

static void
free_array (gchar **langs)
{
  gchar **_langs = langs;

  if (langs == NULL)
  {
    return;
  }

  while (*_langs != NULL)
  {
    g_free (*_langs);
    _langs ++;
  }
  g_free (langs);
}

static GSList *
add_languages (GSList *main, gchar **sub)
{
  GSList *retval = NULL;
  gchar **_sub = sub;

  if (sub == NULL)
  {
    return NULL;
  }

  while (*_sub != NULL)
  {
    if (main == NULL)
    {
      main = g_slist_prepend (main, g_strdup (*_sub));
    }
    else 
    {
      if (g_slist_find_custom (main, *_sub, lang_compare) == NULL)
      {
        main = g_slist_prepend (main, g_strdup (*_sub));
      }
    }
    _sub ++;
  }

  retval = main;
  if (retval)
    retval = g_slist_reverse (retval);
  return retval;
}

#define _EXIT { if (soname) g_free (soname); \
  free_language_list (language_list); \
  if (handle) dlclose (handle); \
  return FALSE; }

#define _EXIT_BUT_OK { if (soname) g_free (soname); \
  free_language_list (language_list); \
  if (handle) dlclose (handle); \
  return TRUE; }


static gboolean
cache_file (const gchar *dir, 
    const gchar *name, FILE *file, gboolean *valid)
{
  gboolean           retval = TRUE;
  gboolean           should_be_free;
  gchar             *soname;
  gchar             **sub;
  GSList            *language_list = NULL, *iter;

  void              *handle = NULL;
  typedef gchar     **(*get_lang_func)(gboolean *);
  typedef const     HildonIMPluginInfo *(*im_info_func)(void);
  
  get_lang_func     getlangfunc;
  im_info_func      infofunc;

  const HildonIMPluginInfo *plugin_info;

  *valid = FALSE;
  
  if (!g_str_has_suffix (name, ".so"))
    return TRUE;

  g_print ("Processing: %s\n", name);
  soname = g_build_filename (dir, name, NULL);

  handle = dlopen (soname, RTLD_LAZY);
  if (handle == NULL)
  {
    g_warning("Opening %s failed! (%s)", soname, dlerror());
    _EXIT;
  }

  getlangfunc = G_GNUC_EXTENSION (get_lang_func)
          dlsym(handle, "hildon_im_plugin_get_available_languages");
  if (getlangfunc == NULL)
  {
    g_warning ("No available languages in %s! "
        "This is not HIM plugin (probably it's a TIS plugin)", name);
    _EXIT_BUT_OK;
  }

  sub = (*getlangfunc) (&should_be_free);
  if (sub != NULL)
  {
    language_list = add_languages (language_list, sub);   
    if (should_be_free == TRUE)
    {
      free_array (sub);
    }
  }

  infofunc = G_GNUC_EXTENSION (im_info_func)
          dlsym(handle, "hildon_im_plugin_get_info");
  if (infofunc == NULL)
  {
    g_warning("No get_info in %s!", name);
    _EXIT;
  }

  plugin_info = (*infofunc)();

  retval &= write_string (file, soname);

  /* Number of languages */
  if (retval)
    retval &= write_byte (file, g_slist_length (language_list));
  else
    _EXIT;

  /* Language list */
  iter = language_list;
  while (iter)
  {
    gchar *data = (gchar *) iter->data;
    
    retval &= write_string (file, data);
    if (retval == FALSE) 
    {
      g_warning ("Failed writing the language_list");
      _EXIT;
    }

    iter = g_slist_next (iter);
  }

  retval &= write_string (file, plugin_info->description);
  retval &= write_string (file, plugin_info->name);
  retval &= write_string (file, plugin_info->menu_title);
  retval &= write_string (file, plugin_info->gettext_domain);
  retval &= write_byte (file, plugin_info->visible_in_menu);
  retval &= write_byte (file, plugin_info->cached);
  retval &= write_int (file, plugin_info->type);
  retval &= write_int (file, plugin_info->group);
  retval &= write_int (file, plugin_info->priority);
  retval &= write_string (file, plugin_info->special_plugin);
  retval &= write_string (file, plugin_info->ossohelp_id);
  retval &= write_byte (file, plugin_info->disable_common_buttons);
  retval &= write_int (file, plugin_info->height);
  retval &= write_int (file, plugin_info->trigger);

  free_language_list (language_list);
  dlclose(handle); /* Close it for now. */  
  g_free (soname);

  *valid = TRUE;
  return retval;
}

static gboolean
generate_cache (void)
{
  FILE *f;
  gchar *filename, *target_filename;
  gboolean retval = FALSE;

  filename = get_cache_file (CACHE_FILENAME_TMP);
  if (filename)
  {
    f = fopen (filename, "w");
    if (f) 
    {
      const gchar *entry;
      gchar *dirname;
      GDir *dir;
      gint num_plugins = 0;

      /* Signature */
      retval = (fwrite (CACHE_SIGNATURE, 
            1, sizeof (CACHE_SIGNATURE) - 1, f) ==
          (sizeof (CACHE_SIGNATURE) - 1));

      retval &= write_byte (f, CACHE_VERSION);
      retval &= write_byte (f, 0);
      if (retval)
      {
        dirname = get_cache_file (CACHE_DIRECTORY);
        if (dirname)
        {
          dir = g_dir_open (dirname, 0, NULL);
          if (dir) 
          {
            while ((entry = g_dir_read_name (dir)) != NULL)
            {             
              gboolean valid;
              if (cache_file (dirname, entry, f, &valid) == FALSE)
              {
                g_warning ("Unable writing file %s when caching %s",
                    filename, entry);
                retval = FALSE;
                break;
              } else {
                retval = TRUE;
                if (valid)
                  num_plugins ++;
              }
            }
            g_dir_close (dir);
          } else 
          {
            g_warning ("Unable to open directory %s", dirname);
            retval = FALSE;
          }
          g_free (dirname);
        } else {
          retval = FALSE;
        }       
      }

      if (retval)
      {
        if (fseek (f, CACHE_START_OFFSET, SEEK_SET) != 0)
          retval = FALSE;

        g_print ("Number of plugins processed: %d\n", num_plugins);
        retval &= write_byte (f, (gchar) num_plugins);

        fclose (f);
        target_filename = get_cache_file (CACHE_FILENAME);

        if (rename (filename, target_filename) != 0)
          retval = FALSE;
      }
    } else {
      g_warning ("Unable to open file %s for writing", filename);
      retval = FALSE;
    }

    g_free (filename);
  }
  return retval;
}

int
main (void)
{
  if (generate_cache () == FALSE)
    return -1;

  return 0;
}
