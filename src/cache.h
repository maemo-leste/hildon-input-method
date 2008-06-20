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

#ifndef _CACHE_H_
#define _CACHE_H_

#include <glib.h>
#include <stdio.h>
#include "hildon-im-plugin.h"
/*

Cache file format:
Offset  Size  Description
0       3     'HIM'   Signature
4       1     0       Version
5       1     Number of plugins
6       ~     the plugins

0       ~     String: filename

Plugins list:
0       1     Number of languages
1       ~     Language list

HildonIMPluginInfo:
.       ~     String: description
.       ~     String: name
... and so forth

String:
0       1     Length of string (max 255)
1       L     the string

NULL values in string are not stored (only the length which is = 0).
Enums values are stored as integer.
  
*/

#define CACHE_SIGNATURE   "HIM"
#define CACHE_VERSION     0
#define CACHE_FILE        "hildon-im-plugins.cache"
#define CACHE_START_OFFSET 4

enum {
  CACHE_FILENAME,
  CACHE_FILENAME_TMP,
  CACHE_DIRECTORY
};

gchar *get_cache_file (gint);
gint   cache_get_number_of_plugins (FILE *f);
HildonIMPluginInfo *cache_get_iminfo (FILE *f);
GSList *cache_get_languages (FILE *f);
FILE  *init_cache (void);
void free_language_list (GSList *list);
gchar *cache_get_soname (FILE *f);
void free_iminfo (HildonIMPluginInfo *);
#endif
