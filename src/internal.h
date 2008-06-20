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

#ifndef __HILDON_IM_UI_INTERNAL_H__
#define __HILDON_IM_UI_INTERNAL_H__

#include "hildon-im-ui.h"

#define FREE_IF_SET(a) if (a) { g_free(a); a = NULL; }

#define GCONF_AVAILABLE_LANGUAGES           HILDON_IM_GCONF_DIR "/available_languages"

/* comes from Hildon IM UI */
void reload_plugins (HildonIMUI *self);
#endif
