#!/bin/sh
# This script should apply all relevant patches to the expat source if
# deviating from upstream.
for patchfile in $(ls *.patch)
do
  patch -p1 < $patchfile
done
