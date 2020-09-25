/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsProxyRelease.h"
#include "nsThreadUtils.h"

namespace detail {

/* static */ void
ProxyReleaseChooser<true>::ProxyReleaseISupports(nsIEventTarget* aTarget,
                                                 nsISupports* aDoomed,
                                                 bool aAlwaysProxy)
{
  ::detail::ProxyRelease<nsISupports>(aTarget, dont_AddRef(aDoomed),
                                      aAlwaysProxy);
}

} // namespace detail
