/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LogicalTypes_h
#define mozilla_LogicalTypes_h

namespace mozilla {

// Logical axis, edge, side and corner constants for use in various places.
enum LogicalAxis {
  eLogicalAxisBlock  = 0x0,
  eLogicalAxisInline = 0x1
};
enum LogicalEdge {
  eLogicalEdgeStart  = 0x0,
  eLogicalEdgeEnd    = 0x1
};
enum LogicalSide {
  eLogicalSideBStart = (eLogicalAxisBlock  << 1) | eLogicalEdgeStart,  // 0x0
  eLogicalSideBEnd   = (eLogicalAxisBlock  << 1) | eLogicalEdgeEnd,    // 0x1
  eLogicalSideIStart = (eLogicalAxisInline << 1) | eLogicalEdgeStart,  // 0x2
  eLogicalSideIEnd   = (eLogicalAxisInline << 1) | eLogicalEdgeEnd     // 0x3
};

enum LogicalCorner
{
  eLogicalCornerBStartIStart = 0,
  eLogicalCornerBStartIEnd   = 1,
  eLogicalCornerBEndIEnd     = 2,
  eLogicalCornerBEndIStart   = 3
};

} // namespace mozilla

#endif // mozilla_LogicalTypes_h
