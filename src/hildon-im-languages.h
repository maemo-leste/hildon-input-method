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

#ifndef _HILDON_IM_LANGUAGES_H_
#define _HILDON_IM_LANGUAGES_H_

typedef struct _HildonIMLanguage HildonIMLanguage;

struct _HildonIMLanguage
{
  gchar *language_code;
  gchar *description;
};

gchar *hildon_im_get_language_description (const char *lang);
void hildon_im_populate_available_languages (GSList *list);
GSList *hildon_im_get_available_languages (void);
void hildon_im_free_available_languages (GSList *list);

#endif
