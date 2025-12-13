#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set -e

# Ensure that $Date$ in the checked-out git files expands timezone-agnostically,
# so that this script's behavior is consistent when run from any time zone.
export TZ=UTC

# Also ensure GIT-INFO is consistently English.
export LANG=en_US.UTF-8
export LANGUAGE=en_US
export LC_ALL=en_US.UTF-8

icu_dir=`dirname $0`/icu

for patch in \
 bug-915735 \
 suppress-warnings.diff \
 bug-1198952-workaround-make-3.82-bug.diff \
 suppress-non-dll-interface.diff \
; do
  echo "Applying local patch $patch"
  patch -d ${icu_dir}/../../ -p1 --no-backup-if-mismatch < ${icu_dir}/../icu-patches/$patch
done

