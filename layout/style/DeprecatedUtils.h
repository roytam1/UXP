/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* some deprecated utilities */

#ifndef mozilla_DeprecatedUtils_h
#define mozilla_DeprecatedUtils_h

#include "mozilla/TypeTraits.h"

/**
 * Macro used in a base class of |geckotype_|.
 */
#define MOZ_DECL_DEPRECATED_METHODS(geckotype_)  \
  inline geckotype_* AsGecko();                         \
  inline const geckotype_* AsGecko() const;

/**
 * Macro used in inline header of class |type_| with its Gecko
 * subclass named |geckotype_| for implementing the inline methods
 * defined by MOZ_DECL_DEPRECATED_METHODS.
 */
#define MOZ_DEFINE_DEPRECATED_METHODS(type_, geckotype_) \
  geckotype_* type_::AsGecko() {                                \
    return static_cast<geckotype_*>(this);                      \
  }                                                             \
  const geckotype_* type_::AsGecko() const {                    \
    return static_cast<const geckotype_*>(this);                \
  }

#endif // mozilla_DeprecatedUtils_h
