# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# We need to include the mozilla packaging routines because we are
# very much still dependent on them
include $(MOZILLA_DIR)/toolkit/mozapps/installer/packager.mk

# This is currently only used on Windows, Linux, and Solaris
# on other platforms such as Mac will fall back to the orginal
# mozilla packaging
make-archive:
ifeq (,$(filter SunOS Linux WINNT,$(OS_ARCH)))
	$(MAKE) make-package
else
	$(MAKE) stage-package make-buildinfo-file
	@echo 'Compressing...'
ifeq (WINNT,$(OS_ARCH))
	cd $(DIST); $(CYGWIN_WRAPPER) 7z a -t7z -m0=lzma2 -mx=9 -aoa -bb3 $(PKG_BASENAME).7z $(MOZ_PKG_DIR)
else
	# Other platforms such as Linux need the Package routine to spawn a pre-complete file
  # Windows does not require this because it is dependent on generating the NSIS
  # Installer which has its own call to generate the precomplete file
	cd $(DIST)/$(MOZ_PKG_DIR); $(CREATE_PRECOMPLETE_CMD)
	cd $(DIST); XZ_OPT=-9e $(TAR) cfJv $(PKG_BASENAME).tar.xz $(MOZ_PKG_DIR)
endif
endif
