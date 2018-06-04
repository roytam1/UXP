# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

installer:
	@$(MAKE) -C application/basilisk/installer installer

package:
	@$(MAKE) -C application/basilisk/installer

package-compare:
	@$(MAKE) -C application/basilisk/installer package-compare

stage-package:
	@$(MAKE) -C application/basilisk/installer stage-package

sdk:
	@$(MAKE) -C application/basilisk/installer make-sdk

install::
	@$(MAKE) -C application/basilisk/installer install

clean::
	@$(MAKE) -C application/basilisk/installer clean

distclean::
	@$(MAKE) -C application/basilisk/installer distclean

source-package::
	@$(MAKE) -C application/basilisk/installer source-package

upload::
	@$(MAKE) -C application/basilisk/installer upload

source-upload::
	@$(MAKE) -C application/basilisk/installer source-upload

hg-bundle::
	@$(MAKE) -C application/basilisk/installer hg-bundle

l10n-check::
	@$(MAKE) -C application/basilisk/locales l10n-check

ifdef ENABLE_TESTS
# Implemented in testing/testsuite-targets.mk

mochitest-browser-chrome:
	$(RUN_MOCHITEST) --flavor=browser
	$(CHECK_TEST_ERROR)

mochitest:: mochitest-browser-chrome

.PHONY: mochitest-browser-chrome

endif
