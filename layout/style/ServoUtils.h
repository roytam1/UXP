/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* some utilities for stylo */

#ifndef mozilla_ServoUtils_h
#define mozilla_ServoUtils_h

#include "mozilla/TypeTraits.h"

/**
 * Macro used in a base class of |geckotype_| and |servotype_|.
 * The class should define |StyleBackendType mType;| itself.
 */
#define MOZ_DECL_STYLO_METHODS(geckotype_)  \
  inline geckotype_* AsGecko();                         \
  inline const geckotype_* AsGecko() const;

/**
 * Macro used in inline header of class |type_| with its Gecko and Servo
 * subclasses named |geckotype_| and |servotype_| correspondingly for
 * implementing the inline methods defined by MOZ_DECL_STYLO_METHODS.
 */
#define MOZ_DEFINE_STYLO_METHODS(type_, geckotype_) \
  geckotype_* type_::AsGecko() {                                \
    return static_cast<geckotype_*>(this);                      \
  }                                                             \
  const geckotype_* type_::AsGecko() const {                    \
    return static_cast<const geckotype_*>(this);                \
  }

#endif // mozilla_ServoUtils_h
