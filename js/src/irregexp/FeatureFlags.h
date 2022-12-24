/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef irregexp_FeatureFlags_h
#define irregexp_FeatureFlags_h

namespace js {

namespace irregexp {
 
// Feature flag to treat /../v as /../u   (https://v8.dev/features/regexp-v-flag)
// We don't support Set Notation or the changed Case Insenstive handling
// but we have Property Sequences and want them in unit test runs.
static const bool kParseFlagUnicodeSetsAsUnicode = false;

} } // namespace js::irregexp

#endif // irregexp_FeatureFlags_h
