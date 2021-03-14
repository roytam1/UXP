/*- *- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "pkix/Result.h"
#include "pkix/pkixutil.h"

namespace mozilla { namespace pkix {

const char*
MapResultToName(Result result)
{
  switch (result)
  {
#define MOZILLA_PKIX_MAP(mozilla_pkix_result, value, nss_result) \
    case Result::mozilla_pkix_result: return "Result::" #mozilla_pkix_result;

    MOZILLA_PKIX_MAP_LIST

#undef MOZILLA_PKIX_MAP

    MOZILLA_PKIX_UNREACHABLE_DEFAULT_ENUM
  }
}

} } // namespace mozilla::pkix
