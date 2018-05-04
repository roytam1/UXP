dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl Set the MOZ_ICU_VERSION variable to denote the current version of the
dnl ICU library, as well as a few other things.

AC_DEFUN([MOZ_CONFIG_ICU], [

ICU_LIB_NAMES=
MOZ_SYSTEM_ICU=
MOZ_ARG_WITH_BOOL(system-icu,
[  --with-system-icu
                          Use system ICU (located with pkgconfig)],
    MOZ_SYSTEM_ICU=1)

if test -n "$MOZ_SYSTEM_ICU"; then
    PKG_CHECK_MODULES(MOZ_ICU, icu-i18n >= 58.1)
    CFLAGS="$CFLAGS $MOZ_ICU_CFLAGS"
    CXXFLAGS="$CXXFLAGS $MOZ_ICU_CFLAGS"
fi

AC_SUBST(MOZ_SYSTEM_ICU)

dnl We always use ICU.
USE_ICU=1

dnl Settings for the implementation of the ECMAScript Internationalization API
if test -n "$USE_ICU"; then
    icudir="$_topsrcdir/intl/icu/source"
    if test ! -d "$icudir"; then
        icudir="$_topsrcdir/../../intl/icu/source"
        if test ! -d "$icudir"; then
            AC_MSG_ERROR([Cannot find the ICU directory])
        fi
    fi

    version=`sed -n 's/^[[[:space:]]]*#[[:space:]]*define[[:space:]][[:space:]]*U_ICU_VERSION_MAJOR_NUM[[:space:]][[:space:]]*\([0-9][0-9]*\)[[:space:]]*$/\1/p' "$icudir/common/unicode/uvernum.h"`
    if test x"$version" = x; then
       AC_MSG_ERROR([cannot determine icu version number from uvernum.h header file $lineno])
    fi
    MOZ_ICU_VERSION="$version"

    # TODO: the l is actually endian-dependent
    # We could make this set as 'l' or 'b' for little or big, respectively,
    # but we'd need to check in a big-endian version of the file.
    ICU_DATA_FILE="icudt${version}l.dat"

    dnl We won't build ICU data as a separate file when building
    dnl JS standalone so that embedders don't have to deal with it.
    dnl We also don't do it on Windows because sometimes the file goes
    dnl missing -- possibly due to overzealous antivirus software? --
    dnl which prevents the browser from starting up :(
    if test -z "$JS_STANDALONE" -a -z "$MOZ_SYSTEM_ICU" -a "$OS_TARGET" != WINNT -a "$MOZ_WIDGET_TOOLKIT" != "android"; then
        MOZ_ICU_DATA_ARCHIVE=1
    else
        MOZ_ICU_DATA_ARCHIVE=
    fi
fi

AC_SUBST(MOZ_ICU_VERSION)
AC_SUBST(ENABLE_INTL_API)
AC_SUBST(USE_ICU)
AC_SUBST(ICU_DATA_FILE)
AC_SUBST(MOZ_ICU_DATA_ARCHIVE)

if test -n "$USE_ICU" -a -z "$MOZ_SYSTEM_ICU"; then
    if test -z "$YASM" -a -z "$GNU_AS" -a "$COMPILE_ENVIRONMENT"; then
      AC_MSG_ERROR([Building ICU requires either yasm or a GNU assembler. If you do not have either of those available for this platform you must use --without-intl-api])
    fi
    dnl We build ICU as a static library.
    AC_DEFINE(U_STATIC_IMPLEMENTATION)
    dnl Source files that use ICU should have control over which parts of the ICU
    dnl namespace they want to use.
    AC_DEFINE(U_USING_ICU_NAMESPACE,0)
fi
])
