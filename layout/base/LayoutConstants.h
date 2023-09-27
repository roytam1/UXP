/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* constants used throughout the Layout module */

#ifndef LayoutConstants_h___
#define LayoutConstants_h___

#include "nsSize.h"  // for NS_MAXSIZE

/**
 * Constant used to indicate an unconstrained size.
 */
#define NS_UNCONSTRAINEDSIZE NS_MAXSIZE

// NOTE: There are assumptions all over that these have the same value,
//       namely NS_UNCONSTRAINEDSIZE.
#define NS_INTRINSICSIZE    NS_UNCONSTRAINEDSIZE
#define NS_AUTOHEIGHT       NS_UNCONSTRAINEDSIZE
#define NS_AUTOOFFSET       NS_UNCONSTRAINEDSIZE

// +1 is to avoid clamped huge margin values being processed as auto margins
#define NS_AUTOMARGIN       (NS_UNCONSTRAINEDSIZE + 1)

#define NS_INTRINSIC_WIDTH_UNKNOWN nscoord_MIN

// The fallback size of width is 300px and the aspect-ratio is 2:1, based on the
// spec definition in CSS2 section 10.3.2
#define REPLACED_ELEM_FALLBACK_PX_WIDTH 300
#define REPLACED_ELEM_FALLBACK_PX_HEIGHT 150

#endif // LayoutConstants_h___
