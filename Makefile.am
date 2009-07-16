# This file is part of hildon-input-method
#
# Copyright (C) 2007 Nokia Corporation.
#
# Contact: Mohammad Anwari <Mohammad.Anwari@nokia.com>
#

SUBDIRS = src docs

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = hildon-input-method-ui-3.0.pc 

man_MANS = hildon-input-method.1

configdir = $(sysconfdir)/hildon

afinitdir=$(sysconfdir)/osso-af-init
afinit_SCRIPTS = keyboard.sh keyboard.defs

schemadir = @GCONF_SCHEMA_FILE_DIR@
schema_DATA = hildon-input-method-ui3.schemas

deb: dist
	-mkdir debian-build
	cd debian-build && \
	tar zxvf ../$(PACKAGE)-$(VERSION).tar.gz && \
	cd $(PACKAGE)-$(VERSION) && dpkg-buildpackage -rfakeroot
