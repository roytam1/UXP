/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Link.h"

#include "mozilla/EventStates.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/Element.h"
#include "nsIURL.h"
#include "nsISizeOf.h"
#include "nsIDocShell.h"
#include "nsIPrefetchService.h"
#include "nsCPrefetchService.h"
#include "nsStyleLinkElement.h"

#include "nsAttrValueInlines.h"
#include "nsEscape.h"
#include "nsGkAtoms.h"
#include "nsHTMLDNSPrefetch.h"
#include "nsString.h"
#include "mozAutoDocUpdate.h"

#include "mozilla/Services.h"

#include "nsIContentPolicy.h"
#include "nsCSSParser.h"
#include "nsMimeTypes.h"
#include "nsIMediaList.h"
#include "DecoderTraits.h"
#include "imgLoader.h"

namespace mozilla {
namespace dom {

enum Destination : uint8_t
{
  DESTINATION_INVALID,
  DESTINATION_AUDIO,
  DESTINATION_AUDIOWORKLET,
  DESTINATION_DOCUMENT,
  DESTINATION_EMBED,
  DESTINATION_FONT,
  DESTINATION_FRAME,
  DESTINATION_IFRAME,
  DESTINATION_IMAGE,
  DESTINATION_JSON,
  DESTINATION_MANIFEST,
  DESTINATION_OBJECT,
  DESTINATION_REPORT,
  DESTINATION_SCRIPT,
  DESTINATION_SERVICEWORKER,
  DESTINATION_SHAREDWORKER,
  DESTINATION_STYLE,
  DESTINATION_TRACK,
  DESTINATION_VIDEO,
  DESTINATION_WEBIDENTITY,
  DESTINATION_WORKER,
  DESTINATION_XSLT,
  DESTINATION_FETCH
};

static const nsAttrValue::EnumTable kDestinationAttributeTable[] = {
  { "",              DESTINATION_INVALID       },
  { "audio",         DESTINATION_AUDIO         },
  { "font",          DESTINATION_FONT          },
  { "image",         DESTINATION_IMAGE         },
  { "script",        DESTINATION_SCRIPT        },
  { "style",         DESTINATION_STYLE         },
  { "track",         DESTINATION_TRACK         },
  { "video",         DESTINATION_VIDEO         },
  { "fetch",         DESTINATION_FETCH         },
  { "json",          DESTINATION_JSON          },
  { nullptr,         0 }
};

Link::Link(Element *aElement)
  : mElement(aElement)
  , mHistory(services::GetHistoryService())
  , mLinkState(eLinkState_NotLink)
  , mNeedsRegistration(false)
  , mRegistered(false)
{
  MOZ_ASSERT(mElement, "Must have an element");
}

Link::~Link()
{
  UnregisterFromHistory();
}

bool
Link::ElementHasHref() const
{
  return mElement->HasAttr(kNameSpaceID_None, nsGkAtoms::href) ||
         (!mElement->IsHTMLElement() &&
          mElement->HasAttr(kNameSpaceID_XLink, nsGkAtoms::href));
}

void
Link::TryDNSPrefetch()
{
  MOZ_ASSERT(mElement->IsInComposedDoc());
  if (ElementHasHref() && nsHTMLDNSPrefetch::IsAllowed(mElement->OwnerDoc())) {
    nsHTMLDNSPrefetch::PrefetchLow(this);
  }
}

void
Link::CancelDNSPrefetch(nsWrapperCache::FlagsType aDeferredFlag,
                        nsWrapperCache::FlagsType aRequestedFlag)
{
  // If prefetch was deferred, clear flag and move on
  if (mElement->HasFlag(aDeferredFlag)) {
    mElement->UnsetFlags(aDeferredFlag);
    // Else if prefetch was requested, clear flag and send cancellation
  } else if (mElement->HasFlag(aRequestedFlag)) {
    mElement->UnsetFlags(aRequestedFlag);
    // Possible that hostname could have changed since binding, but since this
    // covers common cases, most DNS prefetch requests will be canceled
    nsHTMLDNSPrefetch::CancelPrefetchLow(this, NS_ERROR_ABORT);
  }
}

void
Link::TrySpeculativeLoadFeature()
{
  MOZ_ASSERT(mElement->IsInComposedDoc());
  if (!ElementHasHref()) {
    return;
  }

  nsAutoString rel;
  if (!mElement->GetAttr(kNameSpaceID_None, nsGkAtoms::rel, rel)) {
    return;
  }

  if (!nsContentUtils::PrefetchEnabled(mElement->OwnerDoc()->GetDocShell())) {
    return;
  }

  uint32_t linkTypes = nsStyleLinkElement::ParseLinkTypes(rel,
                         mElement->NodePrincipal());

  if ((linkTypes & nsStyleLinkElement::ePREFETCH) ||
      (linkTypes & nsStyleLinkElement::eNEXT) ||
      (linkTypes & nsStyleLinkElement::ePRELOAD)) {
    nsCOMPtr<nsIPrefetchService> prefetchService(do_GetService(NS_PREFETCHSERVICE_CONTRACTID));
    if (prefetchService) {
      nsCOMPtr<nsIURI> uri(GetURI());
      if (uri) {
        nsCOMPtr<nsIDOMNode> domNode = GetAsDOMNode(mElement);
        if (linkTypes & nsStyleLinkElement::ePRELOAD) {
          nsContentPolicyType policyType;
          bool isPreloadValid = CheckPreloadAttrs(policyType, mElement);
          if (!isPreloadValid) {
            // XXX: WPTs expect that we timeout on invalid preloads including
            // those with valid destinations instead of firing an error event.
            return;
          }
          nsContentUtils::DispatchEventForPreloadURI(domNode, policyType);
        } else {
          prefetchService->PrefetchURI(uri,
                                       mElement->OwnerDoc()->GetDocumentURI(),
                                       domNode,
                                       linkTypes & nsStyleLinkElement::ePREFETCH);
        }
        return;
      }
    }
  }

  if (linkTypes & nsStyleLinkElement::ePRECONNECT) {
    nsCOMPtr<nsIURI> uri(GetURI());
    if (uri && mElement->OwnerDoc()) {
      mElement->OwnerDoc()->MaybePreconnect(uri,
        mElement->AttrValueToCORSMode(mElement->GetParsedAttr(nsGkAtoms::crossorigin)));
      return;
    }
  }

  if (linkTypes & nsStyleLinkElement::eDNS_PREFETCH) {
    if (nsHTMLDNSPrefetch::IsAllowed(mElement->OwnerDoc())) {
      nsHTMLDNSPrefetch::PrefetchLow(this);
    }
  }
}

void
Link::UpdatePreload(nsIAtom* aName,
                    const nsAttrValue* aValue,
                    const nsAttrValue* aOldValue)
{
  MOZ_ASSERT(mElement->IsInComposedDoc());

  if (!ElementHasHref()) {
    return;
  }

  nsAutoString rel;
  if (!mElement->GetAttr(kNameSpaceID_None, nsGkAtoms::rel, rel)) {
    return;
  }

  if (!nsContentUtils::PrefetchEnabled(mElement->OwnerDoc()->GetDocShell()) ||
      !nsContentUtils::IsPreloadEnabled()) {
    return;
  }

  uint32_t linkTypes = nsStyleLinkElement::ParseLinkTypes(rel,
                         mElement->NodePrincipal());

  if (!(linkTypes & nsStyleLinkElement::ePRELOAD)) {
    return;
  }

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    return;
  }

  nsCOMPtr<nsIDOMNode> domNode = GetAsDOMNode(mElement);

  nsContentPolicyType policyType;
  bool isPreloadValid = CheckPreloadAttrs(policyType, mElement);
  if (!isPreloadValid) {
    // XXX: WPTs expect that we timeout on invalid preloads including
    // those with valid destinations instead of firing an error event.
    return;
  }

  if (aName == nsGkAtoms::crossorigin) {
    CORSMode corsMode = Element::AttrValueToCORSMode(aValue);
    CORSMode oldCorsMode = Element::AttrValueToCORSMode(aOldValue);
    if (corsMode != oldCorsMode) {
      nsContentUtils::DispatchEventForPreloadURI(domNode, policyType);
    }
    return;
  }

  nsAutoString oldValue;
  if (aOldValue) {
    aOldValue->ToString(oldValue);
  } else {
    oldValue = EmptyString();
  }
  nsContentPolicyType oldPolicyType = nsIContentPolicy::TYPE_INVALID;

  if (aName == nsGkAtoms::as) {
    if (aOldValue) {
      CheckPreloadAttrs(oldPolicyType, aName, oldValue, mElement);
    }
  } else if (aName == nsGkAtoms::type) {
    if (CheckPreloadAttrs(oldPolicyType, aName, oldValue, mElement)) {
      oldPolicyType = policyType;
    } else {
      oldPolicyType = nsIContentPolicy::TYPE_INVALID;
    }
  } else if (aName == nsGkAtoms::media) {
    if (CheckPreloadAttrs(oldPolicyType, aName, oldValue, mElement)) {
      oldPolicyType = policyType;
    } else {
      oldPolicyType = nsIContentPolicy::TYPE_INVALID;
    }
  }

  // Return early for invalid preloads since we shouldn't trigger a new fetch.
  if (policyType == nsIContentPolicy::TYPE_INVALID) {
    return;
  }

  // Fire the associated event if the policy type has changed.
  if (policyType != oldPolicyType) {
    nsContentUtils::DispatchEventForPreloadURI(domNode, policyType);
  }
}

void
Link::CancelPrefetch()
{
  nsCOMPtr<nsIPrefetchService> prefetchService(do_GetService(NS_PREFETCHSERVICE_CONTRACTID));
  if (prefetchService) {
    nsCOMPtr<nsIURI> uri(GetURI());
    if (uri) {
      nsCOMPtr<nsIDOMNode> domNode = GetAsDOMNode(mElement);
      prefetchService->CancelPrefetchURI(uri, domNode);
    }
  }
}

void
Link::SetLinkState(nsLinkState aState)
{
  NS_ASSERTION(mRegistered,
               "Setting the link state of an unregistered Link!");
  NS_ASSERTION(mLinkState != aState,
               "Setting state to the currently set state!");

  // Set our current state as appropriate.
  mLinkState = aState;

  // Per IHistory interface documentation, we are no longer registered.
  mRegistered = false;

  MOZ_ASSERT(LinkState() == NS_EVENT_STATE_VISITED ||
             LinkState() == NS_EVENT_STATE_UNVISITED,
             "Unexpected state obtained from LinkState()!");

  // Tell the element to update its visited state
  mElement->UpdateState(true);
}

EventStates
Link::LinkState() const
{
  // We are a constant method, but we are just lazily doing things and have to
  // track that state.  Cast away that constness!
  Link *self = const_cast<Link *>(this);

  Element *element = self->mElement;

  // If we have not yet registered for notifications and need to,
  // due to our href changing, register now!
  if (!mRegistered && mNeedsRegistration && element->IsInComposedDoc()) {
    // Only try and register once.
    self->mNeedsRegistration = false;

    nsCOMPtr<nsIURI> hrefURI(GetURI());

    // Assume that we are not visited until we are told otherwise.
    self->mLinkState = eLinkState_Unvisited;

    // Make sure the href attribute has a valid link (bug 23209).
    // If we have a good href, register with History if available.
    if (mHistory && hrefURI) {
      nsresult rv = mHistory->RegisterVisitedCallback(hrefURI, self);
      if (NS_SUCCEEDED(rv)) {
        self->mRegistered = true;

        // And make sure we are in the document's link map.
        element->GetComposedDoc()->AddStyleRelevantLink(self);
      }
    }
  }

  // Otherwise, return our known state.
  if (mLinkState == eLinkState_Visited) {
    return NS_EVENT_STATE_VISITED;
  }

  if (mLinkState == eLinkState_Unvisited) {
    return NS_EVENT_STATE_UNVISITED;
  }

  return EventStates();
}

nsIURI*
Link::GetURI() const
{
  // If we have this URI cached, use it.
  if (mCachedURI) {
    return mCachedURI;
  }

  // Otherwise obtain it.
  Link *self = const_cast<Link *>(this);
  Element *element = self->mElement;
  mCachedURI = element->GetHrefURI();

  return mCachedURI;
}

void
Link::SetProtocol(const nsAString &aProtocol)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  nsAString::const_iterator start, end;
  aProtocol.BeginReading(start);
  aProtocol.EndReading(end);
  nsAString::const_iterator iter(start);
  (void)FindCharInReadable(':', iter, end);
  (void)uri->SetScheme(NS_ConvertUTF16toUTF8(Substring(start, iter)));

  SetHrefAttribute(uri);
}

void
Link::SetPassword(const nsAString &aPassword)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  uri->SetPassword(NS_ConvertUTF16toUTF8(aPassword));
  SetHrefAttribute(uri);
}

void
Link::SetUsername(const nsAString &aUsername)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  uri->SetUsername(NS_ConvertUTF16toUTF8(aUsername));
  SetHrefAttribute(uri);
}

void
Link::SetHost(const nsAString &aHost)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  (void)uri->SetHostPort(NS_ConvertUTF16toUTF8(aHost));
  SetHrefAttribute(uri);
}

void
Link::SetHostname(const nsAString &aHostname)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  (void)uri->SetHost(NS_ConvertUTF16toUTF8(aHostname));
  SetHrefAttribute(uri);
}

void
Link::SetPathname(const nsAString &aPathname)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (!url) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  (void)url->SetFilePath(NS_ConvertUTF16toUTF8(aPathname));
  SetHrefAttribute(uri);
}

void
Link::SetSearch(const nsAString& aSearch)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (!url) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  (void)url->SetQuery(NS_ConvertUTF16toUTF8(aSearch));
  SetHrefAttribute(uri);
}

void
Link::SetPort(const nsAString &aPort)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  nsresult rv;
  nsAutoString portStr(aPort);

  // nsIURI uses -1 as default value.
  int32_t port = -1;
  if (!aPort.IsEmpty()) {
    port = portStr.ToInteger(&rv);
    if (NS_FAILED(rv)) {
      return;
    }
  }

  (void)uri->SetPort(port);
  SetHrefAttribute(uri);
}

void
Link::SetHash(const nsAString &aHash)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  (void)uri->SetRef(NS_ConvertUTF16toUTF8(aHash));
  SetHrefAttribute(uri);
}

void
Link::GetOrigin(nsAString &aOrigin)
{
  aOrigin.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    return;
  }

  nsString origin;
  nsContentUtils::GetUTFOrigin(uri, origin);
  aOrigin.Assign(origin);
}

void
Link::GetProtocol(nsAString &_protocol)
{
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    _protocol.AssignLiteral("http");
  }
  else {
    nsAutoCString scheme;
    (void)uri->GetScheme(scheme);
    CopyASCIItoUTF16(scheme, _protocol);
  }
  _protocol.Append(char16_t(':'));
}

void
Link::GetUsername(nsAString& aUsername)
{
  aUsername.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    return;
  }

  nsAutoCString username;
  uri->GetUsername(username);
  CopyASCIItoUTF16(username, aUsername);
}

void
Link::GetPassword(nsAString &aPassword)
{
  aPassword.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    return;
  }

  nsAutoCString password;
  uri->GetPassword(password);
  CopyASCIItoUTF16(password, aPassword);
}

void
Link::GetHost(nsAString &_host)
{
  _host.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Do not throw!  Not having a valid URI should result in an empty string.
    return;
  }

  nsAutoCString hostport;
  nsresult rv = uri->GetHostPort(hostport);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(hostport, _host);
  }
}

void
Link::GetHostname(nsAString &_hostname)
{
  _hostname.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Do not throw!  Not having a valid URI should result in an empty string.
    return;
  }

  nsContentUtils::GetHostOrIPv6WithBrackets(uri, _hostname);
}

void
Link::GetPathname(nsAString &_pathname)
{
  _pathname.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (!url) {
    // Do not throw!  Not having a valid URI or URL should result in an empty
    // string.
    return;
  }

  nsAutoCString file;
  nsresult rv = url->GetFilePath(file);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(file, _pathname);
  }
}

void
Link::GetSearch(nsAString &_search)
{
  _search.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (!url) {
    // Do not throw!  Not having a valid URI or URL should result in an empty
    // string.
    return;
  }

  nsAutoCString search;
  nsresult rv = url->GetQuery(search);
  if (NS_SUCCEEDED(rv) && !search.IsEmpty()) {
    CopyUTF8toUTF16(NS_LITERAL_CSTRING("?") + search, _search);
  }
}

void
Link::GetPort(nsAString &_port)
{
  _port.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Do not throw!  Not having a valid URI should result in an empty string.
    return;
  }

  int32_t port;
  nsresult rv = uri->GetPort(&port);
  // Note that failure to get the port from the URI is not necessarily a bad
  // thing.  Some URIs do not have a port.
  if (NS_SUCCEEDED(rv) && port != -1) {
    nsAutoString portStr;
    portStr.AppendInt(port, 10);
    _port.Assign(portStr);
  }
}

void
Link::GetHash(nsAString &_hash)
{
  _hash.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Do not throw!  Not having a valid URI should result in an empty
    // string.
    return;
  }

  nsAutoCString ref;
  nsresult rv = uri->GetRef(ref);
  if (NS_SUCCEEDED(rv) && !ref.IsEmpty()) {
    _hash.Assign(char16_t('#'));
    if (nsContentUtils::GettersDecodeURLHash()) {
      NS_UnescapeURL(ref); // XXX may result in random non-ASCII bytes!
    }
    AppendUTF8toUTF16(ref, _hash);
  }
}

void
Link::ResetLinkState(bool aNotify, bool aHasHref)
{
  nsLinkState defaultState;

  // The default state for links with an href is unvisited.
  if (aHasHref) {
    defaultState = eLinkState_Unvisited;
  } else {
    defaultState = eLinkState_NotLink;
  }

  // If !mNeedsRegstration, then either we've never registered, or we're
  // currently registered; in either case, we should remove ourself
  // from the doc and the history.
  if (!mNeedsRegistration && mLinkState != eLinkState_NotLink) {
    nsIDocument *doc = mElement->GetComposedDoc();
    if (doc && (mRegistered || mLinkState == eLinkState_Visited)) {
      // Tell the document to forget about this link if we've registered
      // with it before.
      doc->ForgetLink(this);
    }

    UnregisterFromHistory();
  }

  // If we have an href, we should register with the history.
  mNeedsRegistration = aHasHref;

  // If we've cached the URI, reset always invalidates it.
  mCachedURI = nullptr;

  // Update our state back to the default.
  mLinkState = defaultState;

  // We have to be very careful here: if aNotify is false we do NOT
  // want to call UpdateState, because that will call into LinkState()
  // and try to start off loads, etc.  But ResetLinkState is called
  // with aNotify false when things are in inconsistent states, so
  // we'll get confused in that situation.  Instead, just silently
  // update the link state on mElement. Since we might have set the
  // link state to unvisited, make sure to update with that state if
  // required.
  if (aNotify) {
    mElement->UpdateState(aNotify);
  } else {
    if (mLinkState == eLinkState_Unvisited) {
      mElement->UpdateLinkState(NS_EVENT_STATE_UNVISITED);
    } else {
      mElement->UpdateLinkState(EventStates());
    }
  }
}

void
Link::UnregisterFromHistory()
{
  // If we are not registered, we have nothing to do.
  if (!mRegistered) {
    return;
  }

  NS_ASSERTION(mCachedURI, "mRegistered is true, but we have no cached URI?!");

  // And tell History to stop tracking us.
  if (mHistory) {
    nsresult rv = mHistory->UnregisterVisitedCallback(mCachedURI, this);
    NS_ASSERTION(NS_SUCCEEDED(rv), "This should only fail if we misuse the API!");
    if (NS_SUCCEEDED(rv)) {
      mRegistered = false;
    }
  }
}

already_AddRefed<nsIURI>
Link::GetURIToMutate()
{
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    return nullptr;
  }
  nsCOMPtr<nsIURI> clone;
  (void)uri->Clone(getter_AddRefs(clone));
  return clone.forget();
}

void
Link::SetHrefAttribute(nsIURI *aURI)
{
  NS_ASSERTION(aURI, "Null URI is illegal!");

  // if we change this code to not reserialize we need to do something smarter
  // in SetProtocol because changing the protocol of an URI can change the
  // "nature" of the nsIURL/nsIURI implementation.
  nsAutoCString href;
  (void)aURI->GetSpec(href);
  (void)mElement->SetAttr(kNameSpaceID_None, nsGkAtoms::href,
                          NS_ConvertUTF8toUTF16(href), true);
}

bool
Link::CheckPreloadAttrs(nsContentPolicyType& aPolicyType,
                        const nsAString& aDestination,
                        const nsAString& aType,
                        const nsAString& aMedia,
                        nsIDocument* aDocument)
{
  nsString mimeType;
  nsString params;
  nsContentUtils::SplitMimeType(aType, mimeType, params);
  ToLowerCase(mimeType);

  nsAttrValue destinationAttr;
  ParseDestinationValue(aDestination, destinationAttr);

  aPolicyType = DestinationToContentPolicy(destinationAttr);
  if (aPolicyType == nsIContentPolicy::TYPE_INVALID) {
    return false;
  }

  // Check if media attribute is valid.
  if (!aMedia.IsEmpty()) {
    nsCSSParser cssParser;
    RefPtr<nsMediaList> mediaList = new nsMediaList();
    cssParser.ParseMediaList(aMedia, nullptr, 0, mediaList, false);

    nsIPresShell* shell = aDocument->GetShell();
    if (!shell) {
      return false;
    }

    nsPresContext* presContext = shell->GetPresContext();
    if (!presContext) {
      return false;
    }
    if (!mediaList->Matches(presContext, nullptr)) {
      return false;
    }
  }

  if (mimeType.IsEmpty()) {
    return true;
  }

  switch (aPolicyType) {
  case nsIContentPolicy::TYPE_OTHER:
    if (destinationAttr.GetEnumValue() == DESTINATION_JSON) {
      return nsContentUtils::IsJSONMIMEType(mimeType);
    }
    return true;
  case nsIContentPolicy::TYPE_MEDIA:
    if (destinationAttr.GetEnumValue() == DESTINATION_TRACK) {
      return mimeType.EqualsASCII(TEXT_VTT);
    }
    return DecoderTraits::IsSupportedInVideoDocument(NS_ConvertUTF16toUTF8(mimeType));
  case nsIContentPolicy::TYPE_FONT:
    return nsContentUtils::IsFontMIMEType(mimeType);
  case nsIContentPolicy::TYPE_IMAGE:
    return imgLoader::SupportImageWithMimeType(NS_ConvertUTF16toUTF8(mimeType).get(),
                                               AcceptedMimeTypes::IMAGES_AND_DOCUMENTS);
  case nsIContentPolicy::TYPE_SCRIPT:
    return nsContentUtils::IsJavascriptMIMEType(mimeType);
  case nsIContentPolicy::TYPE_STYLESHEET:
    return mimeType.EqualsASCII(TEXT_CSS);
  default:
    return false;
  }
}

bool
Link::CheckPreloadAttrs(nsContentPolicyType& aPolicyType,
                        nsIAtom* aName,
                        const nsAString& aValue,
                        Element* aElement)
{
  nsAutoString destination;
  if (aName == nsGkAtoms::as) {
    destination = aValue;
  } else {
    aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::as, destination);
  }

  nsAutoString type;
  if (aName == nsGkAtoms::type) {
    type = aValue;
  } else {
    aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::type, type);
  }

  nsAutoString media;
  if (aName == nsGkAtoms::media) {
    media = aValue;
  } else {
    aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::media, media);
  }

  return CheckPreloadAttrs(aPolicyType,
                           destination,
                           type,
                           media,
                           aElement->OwnerDoc());
}

bool
Link::CheckPreloadAttrs(nsContentPolicyType& aPolicyType,
                        Element* aElement)
{
  nsAutoString unused;
  return CheckPreloadAttrs(aPolicyType,
                           nullptr,
                           unused,
                           aElement);
}

/* static */ void
Link::ParseDestinationValue(const nsAString& aValue,
                            nsAttrValue& aResult)
{
  // Invalid values are treated as an empty string.
  aResult.ParseEnumValue(aValue,
                         kDestinationAttributeTable,
                         false,
                         &kDestinationAttributeTable[0]);
}

/* static */ nsContentPolicyType
Link::DestinationToContentPolicy(const nsAttrValue& aValue)
{
  switch (aValue.GetEnumValue()) {
  case DESTINATION_AUDIO:
  case DESTINATION_TRACK:
  case DESTINATION_VIDEO:
    return nsIContentPolicy::TYPE_MEDIA;
  case DESTINATION_FONT:
    return nsIContentPolicy::TYPE_FONT;
  case DESTINATION_IMAGE:
    return nsIContentPolicy::TYPE_IMAGE;
  case DESTINATION_SCRIPT:
    return nsIContentPolicy::TYPE_SCRIPT;
  case DESTINATION_STYLE:
    return nsIContentPolicy::TYPE_STYLESHEET;
  case DESTINATION_JSON:
  case DESTINATION_FETCH:
    return nsIContentPolicy::TYPE_OTHER;
  case DESTINATION_INVALID:
  default:
    return nsIContentPolicy::TYPE_INVALID;
  }
}

size_t
Link::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;

  if (mCachedURI) {
    nsCOMPtr<nsISizeOf> iface = do_QueryInterface(mCachedURI);
    if (iface) {
      n += iface->SizeOfIncludingThis(aMallocSizeOf);
    }
  }

  // The following members don't need to be measured:
  // - mElement, because it is a pointer-to-self used to avoid QIs
  // - mHistory, because it is non-owning

  return n;
}

} // namespace dom
} // namespace mozilla
