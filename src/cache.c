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

#include <glib.h>
#include "internal.h"
#include "cache.h"
#include "config.h"
#include "hildon-im-plugin.h"

gchar *
get_cache_file (gint mode)
{
	gchar *retval = NULL;

	switch (mode)
	{
		case CACHE_FILENAME_TMP:
			retval = g_strconcat (PREFIX, G_DIR_SEPARATOR_S, 
				"lib", G_DIR_SEPARATOR_S, 
				IM_PLUGIN_DIR, G_DIR_SEPARATOR_S,
				CACHE_FILE, ".tmp", NULL);	
			break;
		case CACHE_FILENAME:
			retval = g_build_filename (PREFIX, "lib", 
				IM_PLUGIN_DIR, CACHE_FILE, NULL);
			break;
		case CACHE_DIRECTORY:
			retval = g_build_filename (PREFIX, "lib", 
				IM_PLUGIN_DIR, NULL);
			break;
	}
	return retval;
}

static gboolean
cache_read_byte (FILE *f, gchar *value)
{
	return (fread (value, 1, 1, f) == 1);
}

static gboolean
cache_read_int (FILE *f, gint *value)
{
	return (fread (value, 1, sizeof (gint), f) == sizeof (gint));
}

static gboolean
cache_read_string (FILE *f, gchar **value)
{
	gboolean retval = FALSE;
	gchar size;

	retval = cache_read_byte (f, &size);
	if (retval == FALSE)
		return FALSE;

	if (size == 0)
	{
		*value = NULL;
		return TRUE;
	}

	*value = g_malloc0 (size + 1);
	if (fread (*value, 1, size, f) == size)
		return TRUE;
	else
		g_free (*value);

	*value = NULL;
	return retval;
}

#define SIG_LENGTH sizeof (CACHE_SIGNATURE) -1
static gboolean
cache_read_header (FILE *f)
{
	gchar buf [SIG_LENGTH];

	if (fread (buf, 1, SIG_LENGTH, f) != SIG_LENGTH) 
	{
		g_warning ("Invalid signature read");
		return FALSE;
	}

	if (g_ascii_strncasecmp (buf, CACHE_SIGNATURE, SIG_LENGTH) == 0)
		return TRUE;

	g_warning ("Invalid signature found in cache file");
	return FALSE;
}

static gboolean
cache_read_version (FILE *f)
{
	gchar version;

	if (cache_read_byte (f, &version) == FALSE)
		return FALSE;

	if (version != CACHE_VERSION)
	{
		g_warning ("Mismatch version %d (found) vs %d (expected)", 
				version, CACHE_VERSION);
		return FALSE;
	}

	return TRUE;
}

GSList *
cache_get_languages (FILE *f)
{
	gint i;
	gchar num_languages;
	GSList *retval = NULL;
	gchar *s;

	if (cache_read_byte (f, &num_languages) == FALSE)
		return retval;

	for (i = 0; i < num_languages; i ++)
	{
		if (cache_read_string (f, &s) == TRUE)
		{
			if (s != NULL)
			{
				retval = g_slist_prepend (retval, s);
			}
		} else {
			g_warning ("Failed reading languages");
			break;
		}
	}

	if (retval)
		retval = g_slist_reverse (retval);

	return retval;
}

void
free_iminfo (HildonIMPluginInfo *info)
{
  if (info == NULL)
    return;

  FREE_IF_SET (info->description);
	FREE_IF_SET (info->name);
	FREE_IF_SET (info->menu_title);
	FREE_IF_SET (info->gettext_domain);
	FREE_IF_SET (info->special_plugin);
	FREE_IF_SET (info->ossohelp_id);
	
	g_free (info);
}

HildonIMPluginInfo *
cache_get_iminfo (FILE *f)
{
	HildonIMPluginInfo *info;
	gboolean ok = TRUE;
	gchar test;
	gint  value;


	info = g_malloc0 (sizeof (HildonIMPluginInfo));
	if (info == NULL)
		return FALSE;

	ok = cache_read_string (f, &info->description);
	ok &= cache_read_string (f, &info->name);
	ok &= cache_read_string (f, &info->menu_title);
	ok &= cache_read_string (f, &info->gettext_domain);
	ok &= cache_read_byte (f, &test);
	info->visible_in_menu = (test != 0);
	ok &= cache_read_byte (f, &test);
	info->cached = (test != 0);
	ok &= cache_read_int (f, &info->type);
	ok &= cache_read_int (f, &info->group);
	ok &= cache_read_int (f, &info->priority);
	ok &= cache_read_string (f, &info->special_plugin);
	ok &= cache_read_string (f, &info->ossohelp_id);
	ok &= cache_read_byte (f, &test);
	info->disable_common_buttons = (test != 0);
	ok &= cache_read_int (f, &info->height);
	ok &= cache_read_int (f, &value);
	info->trigger = value;

	if (ok)
		return info;

  free_iminfo (info);
	g_warning ("Failed loading HildonIMPluginInfo");
	return NULL;
}

FILE *
init_cache ()
{
	gchar *filename;
	FILE *f;

	filename = get_cache_file (CACHE_FILENAME);
	if (filename)
	{
		f = fopen (filename, "r");
		if (f)
		{
			gboolean result;

			result = cache_read_header (f);
			result &= cache_read_version (f);

			g_free (filename);

			if (result == FALSE)
				return NULL;

			return f;
		} else {
			g_warning ("Couldn't open %s for reading. Forgot "
					"to run hildon-im-recache?", filename);
			g_free (filename);
		}
	}
	return NULL;
}

gint
cache_get_number_of_plugins (FILE *f)
{
	gchar retval;

	if (cache_read_byte (f, &retval) == FALSE)
		return 0;

	return (gint) retval;
}

gchar *
cache_get_soname (FILE *f)
{
	gchar *retval;

	if (cache_read_string (f, &retval) == FALSE)
		return NULL;

	return retval;
}

void
free_language_list (GSList *list)
{
	if (list == NULL)
		return;

	g_slist_foreach (list, (GFunc)g_free, NULL);
  g_slist_free (list);
}

