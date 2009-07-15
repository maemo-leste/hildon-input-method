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

#ifndef _HILDON_IM_WIDGET_LOADER_H_
#define _HILDON_IM_WIDGET_LOADER_H_

#include <gtk/gtkwidget.h>

/**
 * hildon_im_widget_load:
 * @library_name: name of the library that contains the widget.
 * @widget_name: name of the widget.
 * @first_property_new: propertires to create the widget.
 * 
 * Loads the specified widget, with the given properties, from the specified
 * library (which should be located with the rest of the HIM plugin libraries).
 * The library should be a #GModule with the functions dyn_WIDGETNAME_init, 
 * dyn_WIDGETNAME_exit and dyn_WIDGETNAME_create in it.
 * 
 * Returns: the loaded widget.
 */
GtkWidget* hildon_im_widget_load(const gchar *library_name,
                                 const gchar *widget_name,
                                 const gchar *first_property_new, 
                                 ...);

#endif
