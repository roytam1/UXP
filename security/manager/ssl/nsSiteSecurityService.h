/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsSiteSecurityService_h__
#define __nsSiteSecurityService_h__

#include "mozilla/DataStorage.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsISiteSecurityService.h"
#include "nsString.h"
#include "nsTArray.h"
#include "pkix/pkixtypes.h"
#include "prtime.h"

class nsIURI;
class nsISSLStatus;

// 91ea3803-9c79-45d9-97bf-88bc80269236
#define NS_SITE_SECURITY_SERVICE_CID \
  { 0x91ea3803, 0x9c79, 0x45d9, \
    { 0x97, 0xbf, 0x88, 0xbc, 0x80, 0x26, 0x92, 0x36 } }

/**
 * SecurityPropertyState: A utility enum for representing the different states
 * a security property can be in.
 * SecurityPropertySet and SecurityPropertyUnset correspond to indicating
 * a site has or does not have the security property in question, respectively.
 * SecurityPropertyKnockout indicates a value on a preloaded list is being
 * overridden, and the associated site does not have the security property
 * in question.
 */
enum SecurityPropertyState {
  SecurityPropertyUnset = 0,
  SecurityPropertySet = 1,
  SecurityPropertyKnockout = 2,
  SecurityPropertyNegative = 3,
};

/**
 * SiteHSTSState: A utility class that encodes/decodes a string describing
 * the security state of a site. Currently only handles HSTS.
 * HSTS state consists of:
 *  - Expiry time (PRTime (aka int64_t) in milliseconds)
 *  - A state flag (SecurityPropertyState, default SecurityPropertyUnset)
 *  - An include subdomains flag (bool, default false)
 */
class SiteHSTSState
{
public:
  explicit SiteHSTSState(nsCString& aStateString);
  SiteHSTSState(PRTime aHSTSExpireTime, SecurityPropertyState aHSTSState,
                bool aHSTSIncludeSubdomains);

  PRTime mHSTSExpireTime;
  SecurityPropertyState mHSTSState;
  bool mHSTSIncludeSubdomains;

  bool IsExpired(uint32_t aType)
  {
    // If mHSTSExpireTime is 0, this entry never expires (this is the case for
    // knockout entries).
    if (mHSTSExpireTime == 0) {
      return false;
    }

    PRTime now = PR_Now() / PR_USEC_PER_MSEC;
    if (now > mHSTSExpireTime) {
      return true;
    }

    return false;
  }

  void ToString(nsCString &aString);
};

class nsSiteSecurityService : public nsISiteSecurityService
                            , public nsIObserver
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSISITESECURITYSERVICE

  nsSiteSecurityService();
  nsresult Init();

protected:
  virtual ~nsSiteSecurityService();

private:
  nsresult GetHost(nsIURI *aURI, nsACString &aResult);
  nsresult SetHSTSState(uint32_t aType, nsIURI* aSourceURI, int64_t maxage,
                        bool includeSubdomains, uint32_t flags,
                        SecurityPropertyState aHSTSState);
  nsresult ProcessHeaderInternal(uint32_t aType, nsIURI* aSourceURI,
                                 const char* aHeader, nsISSLStatus* aSSLStatus,
                                 uint32_t aFlags, uint64_t* aMaxAge,
                                 bool* aIncludeSubdomains,
                                 uint32_t* aFailureResult);
  nsresult ProcessSTSHeader(nsIURI* aSourceURI, const char* aHeader,
                            uint32_t flags, uint64_t* aMaxAge,
                            bool* aIncludeSubdomains, uint32_t* aFailureResult);

  bool mUseStsService;
  int64_t mPreloadListTimeOffset;
  RefPtr<mozilla::DataStorage> mSiteStateStorage;
};

#endif // __nsSiteSecurityService_h__
