#!/bin/sh

set -x
libtoolize --automake
aclocal
gtkdocize || exit 1
autoconf
autoheader
automake --add-missing --foreign
