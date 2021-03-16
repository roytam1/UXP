/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef mozilla_pkix_pkixcheck_h
#define mozilla_pkix_pkixcheck_h

#include "pkix/pkixtypes.h"

namespace mozilla {
namespace pkix {

class BackCert;

Result CheckIssuerIndependentProperties(TrustDomain& trustDomain,
                                        const BackCert& cert, Time time,
                                        KeyUsage requiredKeyUsageIfPresent,
                                        KeyPurposeId requiredEKUIfPresent,
                                        const CertPolicyId& requiredPolicy,
                                        unsigned int subCACount,
                                        /*out*/ TrustLevel& trustLevel);

Result CheckNameConstraints(Input encodedNameConstraints,
                            const BackCert& firstChild,
                            KeyPurposeId requiredEKUIfPresent);

Result CheckIssuer(Input encodedIssuer);

// ParseValidity and CheckValidity are usually used together.  First you parse
// the dates from the DER Validity sequence, then you compare them to the time
// at which you are validating.  They are separate so that the notBefore and
// notAfter times can be used for other things before they are checked against
// the time of validation.
Result ParseValidity(Input encodedValidity,
                     /*optional out*/ Time* notBeforeOut = nullptr,
                     /*optional out*/ Time* notAfterOut = nullptr);
Result CheckValidity(Time time, Time notBefore, Time notAfter);

// Check that a subject has TLS Feature (rfc7633) requirements that match its
// potential issuer
Result CheckTLSFeatures(const BackCert& subject, BackCert& potentialIssuer);
}
}  // namespace mozilla::pkix

#endif  // mozilla_pkix_pkixcheck_h
