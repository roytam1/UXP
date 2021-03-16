/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// This file provides some implementation-specific test utilities. This is only
// necessary because some PSM xpcshell test utilities overlap in functionality
// with these test utilities, so the underlying implementation is shared.

#ifndef mozilla_pkix_test_pkixtestnss_h
#define mozilla_pkix_test_pkixtestnss_h

#include <keyhi.h>
#include <keythi.h>
#include "nss_scoped_ptrs.h"
#include "pkix/test/pkixtestutil.h"

namespace mozilla {
namespace pkix {
namespace test {

TestKeyPair* CreateTestKeyPair(const TestPublicKeyAlgorithm publicKeyAlg,
                               const ScopedSECKEYPublicKey& publicKey,
                               const ScopedSECKEYPrivateKey& privateKey);
}
}
}  // namespace mozilla::pkix::test

#endif  // mozilla_pkix_test_pkixtestnss_h
