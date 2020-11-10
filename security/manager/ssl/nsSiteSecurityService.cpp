/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSiteSecurityService.h"

#include "mozilla/LinkedList.h"
#include "mozilla/Preferences.h"
#include "mozilla/Base64.h"
#include "base64.h"
#include "CertVerifier.h"
#include "nsCRTGlue.h"
#include "nsISSLStatus.h"
#include "nsISocketProvider.h"
#include "nsIURI.h"
#include "nsIX509Cert.h"
#include "nsNetUtil.h"
#include "nsNSSComponent.h"
#include "nsSecurityHeaderParser.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "pkix/pkixtypes.h"
#include "plstr.h"
#include "mozilla/Logging.h"
#include "prnetdb.h"
#include "prprf.h"
#include "ScopedNSSTypes.h"
#include "SharedCertVerifier.h"

using namespace mozilla;
using namespace mozilla::psm;

static LazyLogModule gSSSLog("nsSSService");

#define SSSLOG(args) MOZ_LOG(gSSSLog, mozilla::LogLevel::Debug, args)

////////////////////////////////////////////////////////////////////////////////

SiteHSTSState::SiteHSTSState(nsCString& aStateString)
  : mHSTSExpireTime(0)
  , mHSTSState(SecurityPropertyUnset)
  , mHSTSIncludeSubdomains(false)
{
  uint32_t hstsState = 0;
  uint32_t hstsIncludeSubdomains = 0; // PR_sscanf doesn't handle bools.
  int32_t matches = PR_sscanf(aStateString.get(), "%lld,%lu,%lu",
                              &mHSTSExpireTime, &hstsState,
                              &hstsIncludeSubdomains);
  bool valid = (matches == 3 &&
                (hstsIncludeSubdomains == 0 || hstsIncludeSubdomains == 1) &&
                ((SecurityPropertyState)hstsState == SecurityPropertyUnset ||
                 (SecurityPropertyState)hstsState == SecurityPropertySet ||
                 (SecurityPropertyState)hstsState == SecurityPropertyKnockout ||
                 (SecurityPropertyState)hstsState == SecurityPropertyNegative));
  if (valid) {
    mHSTSState = (SecurityPropertyState)hstsState;
    mHSTSIncludeSubdomains = (hstsIncludeSubdomains == 1);
  } else {
    SSSLOG(("%s is not a valid SiteHSTSState", aStateString.get()));
    mHSTSExpireTime = 0;
    mHSTSState = SecurityPropertyUnset;
    mHSTSIncludeSubdomains = false;
  }
}

SiteHSTSState::SiteHSTSState(PRTime aHSTSExpireTime,
                             SecurityPropertyState aHSTSState,
                             bool aHSTSIncludeSubdomains)

  : mHSTSExpireTime(aHSTSExpireTime)
  , mHSTSState(aHSTSState)
  , mHSTSIncludeSubdomains(aHSTSIncludeSubdomains)
{
}

void
SiteHSTSState::ToString(nsCString& aString)
{
  aString.Truncate();
  aString.AppendInt(mHSTSExpireTime);
  aString.Append(',');
  aString.AppendInt(mHSTSState);
  aString.Append(',');
  aString.AppendInt(static_cast<uint32_t>(mHSTSIncludeSubdomains));
}

////////////////////////////////////////////////////////////////////////////////

static bool
HostIsIPAddress(const char *hostname)
{
  PRNetAddr hostAddr;
  return (PR_StringToNetAddr(hostname, &hostAddr) == PR_SUCCESS);
}

nsAutoCString CanonicalizeHostname(const char* hostname)
{
  nsAutoCString canonicalizedHostname(hostname);
  ToLowerCase(canonicalizedHostname);
  while (canonicalizedHostname.Length() > 0 &&
         canonicalizedHostname.Last() == '.') {
    canonicalizedHostname.Truncate(canonicalizedHostname.Length() - 1);
  }
  return canonicalizedHostname;
}

nsSiteSecurityService::nsSiteSecurityService()
  : mUseStsService(true)
  , mPreloadListTimeOffset(0)
{
}

nsSiteSecurityService::~nsSiteSecurityService()
{
}

NS_IMPL_ISUPPORTS(nsSiteSecurityService,
                  nsIObserver,
                  nsISiteSecurityService)

nsresult
nsSiteSecurityService::Init()
{
  // Don't access Preferences off the main thread.
  if (!NS_IsMainThread()) {
    NS_NOTREACHED("nsSiteSecurityService initialized off main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  mUseStsService = mozilla::Preferences::GetBool(
    "network.stricttransportsecurity.enabled", true);
  mozilla::Preferences::AddStrongObserver(this,
    "network.stricttransportsecurity.enabled");
  mPreloadListTimeOffset = mozilla::Preferences::GetInt(
    "test.currentTimeOffsetSeconds", 0);
  mozilla::Preferences::AddStrongObserver(this,
    "test.currentTimeOffsetSeconds");
  mSiteStateStorage =
    mozilla::DataStorage::Get(NS_LITERAL_STRING("SiteSecurityServiceState.txt"));
  bool storageWillPersist = false;
  nsresult rv = mSiteStateStorage->Init(storageWillPersist);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  // This is not fatal. There are some cases where there won't be a
  // profile directory (e.g. running xpcshell). There isn't the
  // expectation that site information will be presisted in those cases.
  if (!storageWillPersist) {
    NS_WARNING("site security information will not be persisted");
  }

  return NS_OK;
}

nsresult
nsSiteSecurityService::GetHost(nsIURI* aURI, nsACString& aResult)
{
  nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(aURI);
  if (!innerURI) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString host;
  nsresult rv = innerURI->GetAsciiHost(host);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aResult.Assign(CanonicalizeHostname(host.get()));
  if (aResult.IsEmpty()) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

static void
SetStorageKey(nsAutoCString& storageKey, nsCString& hostname, uint32_t aType)
{
  storageKey = hostname;
  switch (aType) {
    case nsISiteSecurityService::HEADER_HSTS:
      storageKey.AppendLiteral(":HSTS");
      break;
    default:
      NS_ASSERTION(false, "SSS:SetStorageKey got invalid type");
  }
}

// Expire times are in millis.  Since Headers max-age is in seconds, and
// PR_Now() is in micros, normalize the units at milliseconds.
static int64_t
ExpireTimeFromMaxAge(uint64_t maxAge)
{
  return (PR_Now() / PR_USEC_PER_MSEC) + ((int64_t)maxAge * PR_MSEC_PER_SEC);
}

nsresult
nsSiteSecurityService::SetHSTSState(uint32_t aType,
                                    nsIURI* aSourceURI,
                                    int64_t maxage,
                                    bool includeSubdomains,
                                    uint32_t flags,
                                    SecurityPropertyState aHSTSState)
{
  // Exit early if STS not enabled
  if (!mUseStsService) {
    return NS_OK;
  }

  // If max-age is zero, the host is no longer considered HSTS.
  if (maxage == 0) {
    return RemoveState(aType, aSourceURI, flags);
  }

  MOZ_ASSERT((aHSTSState == SecurityPropertySet ||
              aHSTSState == SecurityPropertyNegative),
      "HSTS State must be SecurityPropertySet or SecurityPropertyNegative");

  int64_t expiretime = ExpireTimeFromMaxAge(maxage);
  SiteHSTSState siteState(expiretime, aHSTSState, includeSubdomains);
  nsAutoCString stateString;
  siteState.ToString(stateString);
  nsAutoCString hostname;
  nsresult rv = GetHost(aSourceURI, hostname);
  NS_ENSURE_SUCCESS(rv, rv);
  SSSLOG(("SSS: setting state for %s", hostname.get()));
  bool isPrivate = flags & nsISocketProvider::NO_PERMANENT_STORAGE;
  mozilla::DataStorageType storageType = isPrivate
                                         ? mozilla::DataStorage_Private
                                         : mozilla::DataStorage_Persistent;
  nsAutoCString storageKey;
  SetStorageKey(storageKey, hostname, aType);
  rv = mSiteStateStorage->Put(storageKey, stateString, storageType);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsSiteSecurityService::RemoveState(uint32_t aType, nsIURI* aURI, uint32_t aFlags)
{
   // Child processes are not allowed direct access to this.
   if (!XRE_IsParentProcess()) {
     MOZ_CRASH("Child process: no direct access to nsISiteSecurityService::RemoveState");
   }

  // Only HSTS is supported at the moment.
  NS_ENSURE_TRUE(aType == nsISiteSecurityService::HEADER_HSTS,
                 NS_ERROR_NOT_IMPLEMENTED);

  nsAutoCString hostname;
  nsresult rv = GetHost(aURI, hostname);
  NS_ENSURE_SUCCESS(rv, rv);

  bool isPrivate = aFlags & nsISocketProvider::NO_PERMANENT_STORAGE;
  mozilla::DataStorageType storageType = isPrivate
                                         ? mozilla::DataStorage_Private
                                         : mozilla::DataStorage_Persistent;
  SSSLOG(("SSS: removing entry for %s", hostname.get()));
  nsAutoCString storageKey;
  SetStorageKey(storageKey, hostname, aType);
  mSiteStateStorage->Remove(storageKey, storageType);

  return NS_OK;
}

NS_IMETHODIMP
nsSiteSecurityService::ProcessHeader(uint32_t aType,
                                     nsIURI* aSourceURI,
                                     const char* aHeader,
                                     nsISSLStatus* aSSLStatus,
                                     uint32_t aFlags,
                                     uint64_t* aMaxAge,
                                     bool* aIncludeSubdomains,
                                     uint32_t* aFailureResult)
{
  if (aFailureResult) {
    *aFailureResult = nsISiteSecurityService::ERROR_UNKNOWN;
  }
  NS_ENSURE_TRUE(aType == nsISiteSecurityService::HEADER_HSTS ||
                 aType == nsISiteSecurityService::HEADER_HPKP,
                 NS_ERROR_NOT_IMPLEMENTED);

  NS_ENSURE_ARG(aSSLStatus);
  return ProcessHeaderInternal(aType, aSourceURI, aHeader, aSSLStatus, aFlags,
                               aMaxAge, aIncludeSubdomains, aFailureResult);
}

NS_IMETHODIMP
nsSiteSecurityService::UnsafeProcessHeader(uint32_t aType,
                                           nsIURI* aSourceURI,
                                           const char* aHeader,
                                           uint32_t aFlags,
                                           uint64_t* aMaxAge,
                                           bool* aIncludeSubdomains,
                                           uint32_t* aFailureResult)
{
  return ProcessHeaderInternal(aType, aSourceURI, aHeader, nullptr, aFlags,
                               aMaxAge, aIncludeSubdomains, aFailureResult);
}

nsresult
nsSiteSecurityService::ProcessHeaderInternal(uint32_t aType,
                                             nsIURI* aSourceURI,
                                             const char* aHeader,
                                             nsISSLStatus* aSSLStatus,
                                             uint32_t aFlags,
                                             uint64_t* aMaxAge,
                                             bool* aIncludeSubdomains,
                                             uint32_t* aFailureResult)
{
  if (aFailureResult) {
    *aFailureResult = nsISiteSecurityService::ERROR_UNKNOWN;
  }
  // Only HSTS is supported at the moment.
  NS_ENSURE_TRUE(aType == nsISiteSecurityService::HEADER_HSTS,
                 NS_ERROR_NOT_IMPLEMENTED);

  if (aMaxAge != nullptr) {
    *aMaxAge = 0;
  }

  if (aIncludeSubdomains != nullptr) {
    *aIncludeSubdomains = false;
  }

  if (aSSLStatus) {
    bool tlsIsBroken = false;
    bool trustcheck;
    nsresult rv;
    rv = aSSLStatus->GetIsDomainMismatch(&trustcheck);
    NS_ENSURE_SUCCESS(rv, rv);
    tlsIsBroken = tlsIsBroken || trustcheck;

    rv = aSSLStatus->GetIsNotValidAtThisTime(&trustcheck);
    NS_ENSURE_SUCCESS(rv, rv);
    tlsIsBroken = tlsIsBroken || trustcheck;

    rv = aSSLStatus->GetIsUntrusted(&trustcheck);
    NS_ENSURE_SUCCESS(rv, rv);
    tlsIsBroken = tlsIsBroken || trustcheck;
    if (tlsIsBroken) {
       SSSLOG(("SSS: discarding header from untrustworthy connection"));
       if (aFailureResult) {
         *aFailureResult = nsISiteSecurityService::ERROR_UNTRUSTWORTHY_CONNECTION;
       }
      return NS_ERROR_FAILURE;
    }
  }

  nsAutoCString host;
  nsresult rv = GetHost(aSourceURI, host);
  NS_ENSURE_SUCCESS(rv, rv);
  if (HostIsIPAddress(host.get())) {
    /* Don't process headers if a site is accessed by IP address. */
    return NS_OK;
  }

  switch (aType) {
    case nsISiteSecurityService::HEADER_HSTS:
      rv = ProcessSTSHeader(aSourceURI, aHeader, aFlags, aMaxAge,
                            aIncludeSubdomains, aFailureResult);
      break;
    default:
      MOZ_CRASH("unexpected header type");
  }
  return rv;
}

static uint32_t
ParseSSSHeaders(uint32_t aType,
                const char* aHeader,
                bool& foundIncludeSubdomains,
                bool& foundMaxAge,
                bool& foundUnrecognizedDirective,
                uint64_t& maxAge,
                nsTArray<nsCString>& sha256keys)
{
  // "Strict-Transport-Security" ":" OWS
  //      STS-d  *( OWS ";" OWS STS-d  OWS)
  //
  //  ; STS directive
  //  STS-d      = maxAge / includeSubDomains
  //
  //  maxAge     = "max-age" "=" delta-seconds v-ext
  //
  //  includeSubDomains = [ "includeSubDomains" ]
  //

  //  All directives must appear only once.
  //  Directive names are case-insensitive.
  //  The entire header is invalid if a directive not conforming to the
  //  syntax is encountered.
  //  Unrecognized directives (that are otherwise syntactically valid) are
  //  ignored, and the rest of the header is parsed as normal.

  NS_NAMED_LITERAL_CSTRING(max_age_var, "max-age");
  NS_NAMED_LITERAL_CSTRING(include_subd_var, "includesubdomains");

  nsSecurityHeaderParser parser(aHeader);
  nsresult rv = parser.Parse();
  if (NS_FAILED(rv)) {
    SSSLOG(("SSS: could not parse header"));
    return nsISiteSecurityService::ERROR_COULD_NOT_PARSE_HEADER;
  }
  mozilla::LinkedList<nsSecurityHeaderDirective>* directives = parser.GetDirectives();

  for (nsSecurityHeaderDirective* directive = directives->getFirst();
       directive != nullptr; directive = directive->getNext()) {
    SSSLOG(("SSS: found directive %s\n", directive->mName.get()));
    if (directive->mName.Length() == max_age_var.Length() &&
        directive->mName.EqualsIgnoreCase(max_age_var.get(),
                                          max_age_var.Length())) {
      if (foundMaxAge) {
        SSSLOG(("SSS: found two max-age directives"));
        return nsISiteSecurityService::ERROR_MULTIPLE_MAX_AGES;
      }

      SSSLOG(("SSS: found max-age directive"));
      foundMaxAge = true;

      size_t len = directive->mValue.Length();
      for (size_t i = 0; i < len; i++) {
        char chr = directive->mValue.CharAt(i);
        if (chr < '0' || chr > '9') {
          SSSLOG(("SSS: invalid value for max-age directive"));
          return nsISiteSecurityService::ERROR_INVALID_MAX_AGE;
        }
      }

      if (PR_sscanf(directive->mValue.get(), "%llu", &maxAge) != 1) {
        SSSLOG(("SSS: could not parse delta-seconds"));
        return nsISiteSecurityService::ERROR_INVALID_MAX_AGE;
      }

      SSSLOG(("SSS: parsed delta-seconds: %llu", maxAge));
    } else if (directive->mName.Length() == include_subd_var.Length() &&
               directive->mName.EqualsIgnoreCase(include_subd_var.get(),
                                                 include_subd_var.Length())) {
      if (foundIncludeSubdomains) {
        SSSLOG(("SSS: found two includeSubdomains directives"));
        return nsISiteSecurityService::ERROR_MULTIPLE_INCLUDE_SUBDOMAINS;
      }

      SSSLOG(("SSS: found includeSubdomains directive"));
      foundIncludeSubdomains = true;

      if (directive->mValue.Length() != 0) {
        SSSLOG(("SSS: includeSubdomains directive unexpectedly had value '%s'",
                directive->mValue.get()));
        return nsISiteSecurityService::ERROR_INVALID_INCLUDE_SUBDOMAINS;
      }
    } else {
      SSSLOG(("SSS: ignoring unrecognized directive '%s'",
              directive->mName.get()));
      foundUnrecognizedDirective = true;
    }
  }
  return nsISiteSecurityService::Success;
}

nsresult
nsSiteSecurityService::ProcessSTSHeader(nsIURI* aSourceURI,
                                        const char* aHeader,
                                        uint32_t aFlags,
                                        uint64_t* aMaxAge,
                                        bool* aIncludeSubdomains,
                                        uint32_t* aFailureResult)
{
  if (aFailureResult) {
    *aFailureResult = nsISiteSecurityService::ERROR_UNKNOWN;
  }
  SSSLOG(("SSS: processing HSTS header '%s'", aHeader));

  const uint32_t aType = nsISiteSecurityService::HEADER_HSTS;
  bool foundMaxAge = false;
  bool foundIncludeSubdomains = false;
  bool foundUnrecognizedDirective = false;
  uint64_t maxAge = 0;
  nsTArray<nsCString> unusedSHA256keys; // Required for sane internal interface

  uint32_t sssrv = ParseSSSHeaders(aType, aHeader, foundIncludeSubdomains,
                                   foundMaxAge, foundUnrecognizedDirective,
                                   maxAge, unusedSHA256keys);
  if (sssrv != nsISiteSecurityService::Success) {
    if (aFailureResult) {
      *aFailureResult = sssrv;
    }
    return NS_ERROR_FAILURE;
  }

  // after processing all the directives, make sure we came across max-age
  // somewhere.
  if (!foundMaxAge) {
    SSSLOG(("SSS: did not encounter required max-age directive"));
    if (aFailureResult) {
      *aFailureResult = nsISiteSecurityService::ERROR_NO_MAX_AGE;
    }
    return NS_ERROR_FAILURE;
  }

  // record the successfully parsed header data.
  nsresult rv = SetHSTSState(aType, aSourceURI, maxAge, foundIncludeSubdomains,
                             aFlags, SecurityPropertySet);
  if (NS_FAILED(rv)) {
    SSSLOG(("SSS: failed to set STS state"));
    if (aFailureResult) {
      *aFailureResult = nsISiteSecurityService::ERROR_COULD_NOT_SAVE_STATE;
    }
    return rv;
  }

  if (aMaxAge != nullptr) {
    *aMaxAge = maxAge;
  }

  if (aIncludeSubdomains != nullptr) {
    *aIncludeSubdomains = foundIncludeSubdomains;
  }

  return foundUnrecognizedDirective
           ? NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA
           : NS_OK;
}

NS_IMETHODIMP
nsSiteSecurityService::IsSecureURI(uint32_t aType, nsIURI* aURI,
                                   uint32_t aFlags, bool* aCached,
                                   bool* aResult)
{
  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG(aResult);

  // Only HSTS is supported at the moment.
  NS_ENSURE_TRUE(aType == nsISiteSecurityService::HEADER_HSTS,
                 NS_ERROR_NOT_IMPLEMENTED);

  nsAutoCString hostname;
  nsresult rv = GetHost(aURI, hostname);
  NS_ENSURE_SUCCESS(rv, rv);

  // Exit early if STS not enabled
  if (!mUseStsService) {
    *aResult = false;
    return NS_OK;
  }

  /* An IP address never qualifies as a secure URI. */
  if (HostIsIPAddress(hostname.get())) {
    *aResult = false;
    return NS_OK;
  }

  return IsSecureHost(aType, hostname.get(), aFlags, aCached, aResult);
}

NS_IMETHODIMP
nsSiteSecurityService::IsSecureHost(uint32_t aType, const char* aHost,
                                    uint32_t aFlags, bool* aCached,
                                    bool* aResult)
{
  NS_ENSURE_ARG(aHost);
  NS_ENSURE_ARG(aResult);

  // Only HSTS is supported at the moment.
  NS_ENSURE_TRUE(aType == nsISiteSecurityService::HEADER_HSTS,
                 NS_ERROR_NOT_IMPLEMENTED);

  // set default in case if we can't find any STS information
  *aResult = false;
  if (aCached) {
    *aCached = false;
  }

  // Exit early if checking HSTS and STS not enabled
  if (!mUseStsService && aType == nsISiteSecurityService::HEADER_HSTS) {
    return NS_OK;
  }

  // An IP address never qualifies as a secure URI.
  if (HostIsIPAddress(aHost)) {
    return NS_OK;
  }

  // Canonicalize the passed host name
  nsAutoCString host(CanonicalizeHostname(aHost));
  
  // First check the exact host. This involves first checking for an entry in
  // site security storage. If that entry exists, we don't want to check
  // in the preload list. We only want to use the stored value if it is not a
  // knockout entry, however.
  // Additionally, if it is a knockout entry, we want to stop looking for data
  // on the host, because the knockout entry indicates "we have no information
  // regarding the security status of this host".
  bool isPrivate = aFlags & nsISocketProvider::NO_PERMANENT_STORAGE;
  mozilla::DataStorageType storageType = isPrivate
                                         ? mozilla::DataStorage_Private
                                         : mozilla::DataStorage_Persistent;
  nsAutoCString storageKey;
  SetStorageKey(storageKey, host, aType);
  nsCString value = mSiteStateStorage->Get(storageKey, storageType);
  SiteHSTSState siteState(value);
  if (siteState.mHSTSState != SecurityPropertyUnset) {
    SSSLOG(("Found entry for %s", host.get()));
    bool expired = siteState.IsExpired(aType);
    if (!expired) {
      if (aCached) {
        *aCached = true;
      }
      if (siteState.mHSTSState == SecurityPropertySet) {
        *aResult = true;
        return NS_OK;
      } else if (siteState.mHSTSState == SecurityPropertyNegative) {
        *aResult = false;
        return NS_OK;
      }
    }

    // If the entry is expired we can remove it.
    if (expired) {
      mSiteStateStorage->Remove(storageKey, storageType);
    }
  }

  SSSLOG(("no HSTS data for %s found, walking up domain", host.get()));
  const char *subdomain;

  uint32_t offset = 0;
  for (offset = host.FindChar('.', offset) + 1;
       offset > 0;
       offset = host.FindChar('.', offset) + 1) {

    subdomain = host.get() + offset;

    // If we get an empty string, don't continue.
    if (strlen(subdomain) < 1) {
      break;
    }

    // Do the same thing as with the exact host, except now we're looking at
    // ancestor domains of the original host. So, we have to look at the
    // include subdomains flag (although we still have to check for a
    // SecurityPropertySet flag first to check that this is a secure host and
    // not a knockout entry - and again, if it is a knockout entry, we stop
    // looking for data on it and skip to the next higher up ancestor domain).
    nsCString subdomainString(subdomain);
    nsAutoCString storageKey;
    SetStorageKey(storageKey, subdomainString, aType);
    value = mSiteStateStorage->Get(storageKey, storageType);
    SiteHSTSState siteState(value);
    if (siteState.mHSTSState != SecurityPropertyUnset) {
      SSSLOG(("Found entry for %s", subdomain));
      bool expired = siteState.IsExpired(aType);
      if (!expired) {
        if (aCached) {
          *aCached = true;
        }
        if (siteState.mHSTSState == SecurityPropertySet) {
          *aResult = siteState.mHSTSIncludeSubdomains;
          break;
        } else if (siteState.mHSTSState == SecurityPropertyNegative) {
          *aResult = false;
          break;
        }
      }

      // If the entry is expired we can remove it.
      if (expired) {
        mSiteStateStorage->Remove(storageKey, storageType);
      }
    }

    SSSLOG(("no HSTS data for %s found, walking up domain", subdomain));
  }

  // Use whatever we ended up with, which defaults to false.
  return NS_OK;
}

NS_IMETHODIMP
nsSiteSecurityService::ClearAll()
{
  return mSiteStateStorage->Clear();
}

//------------------------------------------------------------
// nsSiteSecurityService::nsIObserver
//------------------------------------------------------------

NS_IMETHODIMP
nsSiteSecurityService::Observe(nsISupports *subject,
                               const char *topic,
                               const char16_t *data)
{
  // Don't access Preferences off the main thread.
  if (!NS_IsMainThread()) {
    NS_NOTREACHED("Preferences accessed off main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  if (strcmp(topic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0) {
    mUseStsService = mozilla::Preferences::GetBool(
      "network.stricttransportsecurity.enabled", true);
    mPreloadListTimeOffset =
      mozilla::Preferences::GetInt("test.currentTimeOffsetSeconds", 0);
  }

  return NS_OK;
}
