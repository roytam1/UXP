/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_script_ResolveResult_h
#define dom_script_ResolveResult_h

#include "mozilla/Assertions.h"  // MOZ_CRASH
#include "mozilla/NotNull.h"
#include "mozilla/Result.h"
#include "nsIURI.h"

namespace mozilla::dom {

enum class ResolveError : uint8_t
{
  ModuleResolveFailure,
  BlockedByNullEntry,
  BlockedByAfterPrefix,
  BlockedByBacktrackingPrefix,
  InvalidBareSpecifier
};

struct ResolveErrorInfo
{
  static const char* GetString(ResolveError aError) {
    switch (aError) {
      case ResolveError::ModuleResolveFailure:
        return "ModuleResolveFailure";
      case ResolveError::BlockedByNullEntry:
        return "ImportMapResolutionBlockedByNullEntry";
      case ResolveError::BlockedByAfterPrefix:
        return "ImportMapResolutionBlockedByAfterPrefix";
      case ResolveError::BlockedByBacktrackingPrefix:
        return "ImportMapResolutionBlockedByBacktrackingPrefix";
      case ResolveError::InvalidBareSpecifier:
        return "ImportMapResolveInvalidBareSpecifier";
      default:
        MOZ_CRASH("Unexpected ResolveError value");
    }
  }
};

/**
 * ResolveResult is used to store the result of 'resolving a module specifier',
 * which could be an URI on success or a ResolveError on failure.
 */
using ResolveResult =
    mozilla::Result<mozilla::NotNull<nsCOMPtr<nsIURI>>, ResolveError>;

}  // namespace mozilla::dom

#endif  // dom_script_ResolveResult_h