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

#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#include "hildon-im-xcode.h"
#include "hildon-im-xcode-keysyms.h"

static size_t hildon_im_find_code(gunichar utf_char)
{
	size_t i = 0;
	for (; i < sizeof(hildon_im_utfcode)/sizeof(gunichar) && hildon_im_utfcode[i]; ++i) 
		if (hildon_im_utfcode[i] == utf_char) 
			break;
	return i;
}

static KeySym hildon_im_utf_to_keysym(gunichar utf_char)
{
  size_t codeindex = hildon_im_find_code(utf_char);

  switch (utf_char)
  {
    case ' ':
    return XK_space;
    case '\n':
    return XK_Return;
  }
  
  if (hildon_im_xsymcode[codeindex])
    return hildon_im_xsymcode[codeindex];
  else
    return NoSymbol;
}

static KeySym hildon_im_utf_to_keysym_heuristic(gunichar utf_char)
{
    return utf_char + 0x01000000;
}

void hildon_im_send_utf_via_xlib(Display *dpy, gunichar utf_char)
{
  KeySym sym;
  gboolean require_shift = g_unichar_isalpha(utf_char) && g_unichar_isupper(utf_char);

  sym = hildon_im_utf_to_keysym(utf_char);
  
  if (sym == NoSymbol)
    sym = hildon_im_utf_to_keysym_heuristic(utf_char);
  
  KeyCode code = XKeysymToKeycode(dpy, sym);
  if(!code && sym != hildon_im_utf_to_keysym_heuristic(utf_char))
  {
    sym = hildon_im_utf_to_keysym_heuristic(utf_char);
    code = XKeysymToKeycode(dpy, sym);
  }
  if(code)
  {
    if (require_shift)
      XTestFakeKeyEvent(dpy,  XKeysymToKeycode(dpy, XK_Shift_L), True, CurrentTime);
    
    XTestFakeKeyEvent(dpy, code, True, CurrentTime);
    XTestFakeKeyEvent(dpy, code, False, CurrentTime);

    if (require_shift)
      XTestFakeKeyEvent(dpy,  XKeysymToKeycode(dpy, XK_Shift_L), False, CurrentTime);
  }
  else
    g_warning("no KeyCode for %u\n", utf_char);
}
