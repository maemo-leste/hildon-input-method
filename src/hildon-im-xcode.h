/*
 * This file is part of hildon-input-method
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

#ifndef __HILDON_IM_XCODE_H__
#define __HILDON_IM_XCODE_H__

#include <X11/X.h>
#include <X11/Xlib.h>
#include <gmodule.h>

KeySym hildon_im_utf_to_keysym(gunichar utf_char);

void hildon_im_send_utf_via_xlib(Display *dpy, gunichar utf_char);

#endif
