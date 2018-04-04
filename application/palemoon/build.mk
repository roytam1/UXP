# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

installer:
	@$(MAKE) -C application/palemoon/installer installer

package:
	@$(MAKE) -C application/palemoon/installer

package-compare:
	@$(MAKE) -C application/palemoon/installer package-compare

stage-package:
	@$(MAKE) -C application/palemoon/installer stage-package

install::
	@$(MAKE) -C application/palemoon/installer install

clean::
	@$(MAKE) -C application/palemoon/installer clean

distclean::
	@$(MAKE) -C application/palemoon/installer distclean

source-package::
	@$(MAKE) -C application/palemoon/installer source-package

upload::
	@$(MAKE) -C application/palemoon/installer upload

source-upload::
	@$(MAKE) -C application/palemoon/installer source-upload

hg-bundle::
	@$(MAKE) -C application/palemoon/installer hg-bundle

l10n-check::
	@$(MAKE) -C application/palemoon/locales l10n-check

ifdef ENABLE_TESTS
# Implemented in testing/testsuite-targets.mk

mochitest-browser-chrome:
	$(RUN_MOCHITEST) --browser-chrome
	$(CHECK_TEST_ERROR)

mochitest:: mochitest-browser-chrome

.PHONY: mochitest-browser-chrome

mochitest-metro-chrome:
	$(RUN_MOCHITEST) --metro-immersive --browser-chrome
	$(CHECK_TEST_ERROR)


endif
