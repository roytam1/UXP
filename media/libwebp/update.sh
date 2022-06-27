#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# Usage: ./update.sh <libwebp_directory>
#
# Copies the needed files from a directory containing the original
# libwebp source.

cp $1/AUTHORS .
cp $1/COPYING .
cp $1/NEWS .
cp $1/PATENTS .
cp $1/README .
cp $1/README.mux .

mkdir -p webp
cp $1/src/webp/*.h webp

mkdir -p dec
cp $1/src/dec/*.h dec
cp $1/src/dec/*.c dec

mkdir -p demux
cp $1/src/demux/demux.c demux

mkdir -p dsp
cp $1/src/dsp/*.h dsp
cp $1/src/dsp/*.c dsp

mkdir -p enc
cp $1/src/enc/*.h enc
cp $1/src/enc/*.c enc

mkdir -p utils
cp $1/src/utils/*.h utils
cp $1/src/utils/*.c utils

find . \( -name "*.c" -o -name "*.h" \) -exec sed -i 's/#include "src\//#include "..\//g' {} \;
