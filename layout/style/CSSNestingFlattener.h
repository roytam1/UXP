/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CSSNestingFlattener_h
#define CSSNestingFlattener_h

class nsAString;

namespace mozilla {
namespace css {

bool FlattenBasicCSSNesting(const nsAString& aInput, nsAString& aOutput);

} // namespace css
} // namespace mozilla

#endif // CSSNestingFlattener_h
