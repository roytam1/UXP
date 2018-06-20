/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSSSLSTATUS_H
#define _NSSSLSTATUS_H

#include "CertVerifier.h" // For CertificateTransparencyInfo
#include "nsISSLStatus.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIX509Cert.h"
#include "nsISerializable.h"
#include "nsIClassInfo.h"

class nsNSSCertificate;

enum class EVStatus {
  NotEV = 0,
  EV = 1,
};

class nsSSLStatus final
  : public nsISSLStatus
  , public nsISerializable
  , public nsIClassInfo
{
protected:
  virtual ~nsSSLStatus();
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISSLSTATUS
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSICLASSINFO

  nsSSLStatus();

  void SetServerCert(nsNSSCertificate* aServerCert, EVStatus aEVStatus);

  bool HasServerCert() {
    return mServerCert != nullptr;
  }

  void SetCertificateTransparencyInfo(
    const mozilla::psm::CertificateTransparencyInfo& info);

  /* public for initilization in this file */
  uint16_t mCipherSuite;
  uint16_t mProtocolVersion;
  uint16_t mCertificateTransparencyStatus;
  nsCString mKeaGroup;
  nsCString mSignatureSchemeName;

  bool mIsDomainMismatch;
  bool mIsNotValidAtThisTime;
  bool mIsUntrusted;
  bool mIsEV;

  bool mHasIsEVStatus;
  bool mHaveCipherSuiteAndProtocol;

  /* mHaveCertErrrorBits is relied on to determine whether or not a SPDY
     connection is eligible for joining in nsNSSSocketInfo::JoinConnection() */
  bool mHaveCertErrorBits;

private:
  nsCOMPtr<nsIX509Cert> mServerCert;
};

// 600cd77a-e45c-4184-bfc5-55c87dbc4511
#define NS_SSLSTATUS_CID \
{ 0x600cd77a, 0xe45c, 0x4184, \
  { 0xbf, 0xc5, 0x55, 0xc8, 0x7d, 0xbc, 0x45, 0x11 } }

#endif
