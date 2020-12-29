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

KeySym hildon_im_utf_to_keysym(gunichar utf_char)
{
  KeySym sym = NoSymbol;
  char buf[6];

  if (g_unichar_to_utf8(utf_char, buf) > 0)
  {
    switch (buf[0])
    {
      case ' ':
      return XK_space;
      case '\n':
      return XK_Return;
      case '.':
      return XK_period;
      case '!':
      return XK_exclam;
      case '?':
      return XK_question;
      case ':':
      return XK_colon;
      case '-':
      return XK_minus;
      case '/':
      return XK_slash;
      case '\\':
      return XK_backslash;
      case '&':
      return XK_ampersand;
    }

    buf[1] = '\0';
    sym = XStringToKeysym(buf);
  }

  return sym;
}

void hildon_im_send_utf_via_xlib(Display *dpy, gunichar utf_char)
{
  KeySym sym;
  gboolean require_shift = g_unichar_isalpha(utf_char) && g_unichar_isupper(utf_char);

  sym = hildon_im_utf_to_keysym(utf_char);
  
  if (sym != NoSymbol)
  {
    KeyCode code =  XKeysymToKeycode(dpy, sym);
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
  else
  {
    g_warning("no KeySym for %u\n", utf_char);
  }
}
