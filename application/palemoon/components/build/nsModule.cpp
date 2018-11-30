/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"

#include "nsBrowserCompsCID.h"
#include "DirectoryProvider.h"

#if defined(XP_WIN)
#include "nsWindowsShellService.h"
#elif defined(XP_MACOSX)
#include "nsMacShellService.h"
#elif defined(MOZ_WIDGET_GTK)
#include "nsGNOMEShellService.h"
#endif

#include "rdf.h"
#include "nsFeedSniffer.h"

#include "nsNetCID.h"

using namespace mozilla::browser;

/////////////////////////////////////////////////////////////////////////////

NS_GENERIC_FACTORY_CONSTRUCTOR(DirectoryProvider)
#if defined(XP_WIN)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWindowsShellService)
#elif defined(XP_MACOSX)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacShellService)
#elif defined(MOZ_WIDGET_GTK)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsGNOMEShellService, Init)
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(nsFeedSniffer)

NS_DEFINE_NAMED_CID(NS_BROWSERDIRECTORYPROVIDER_CID);
#if defined(XP_WIN)
NS_DEFINE_NAMED_CID(NS_SHELLSERVICE_CID);
#elif defined(MOZ_WIDGET_GTK)
NS_DEFINE_NAMED_CID(NS_SHELLSERVICE_CID);
#endif
NS_DEFINE_NAMED_CID(NS_FEEDSNIFFER_CID);
#ifdef XP_MACOSX
NS_DEFINE_NAMED_CID(NS_SHELLSERVICE_CID);
#endif

static const mozilla::Module::CIDEntry kBrowserCIDs[] = {
    { &kNS_BROWSERDIRECTORYPROVIDER_CID, false, nullptr, DirectoryProviderConstructor },
#if defined(XP_WIN)
    { &kNS_SHELLSERVICE_CID, false, nullptr, nsWindowsShellServiceConstructor },
#elif defined(MOZ_WIDGET_GTK)
    { &kNS_SHELLSERVICE_CID, false, nullptr, nsGNOMEShellServiceConstructor },
#endif
    { &kNS_FEEDSNIFFER_CID, false, nullptr, nsFeedSnifferConstructor },
#ifdef XP_MACOSX
    { &kNS_SHELLSERVICE_CID, false, nullptr, nsMacShellServiceConstructor },
#endif
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kBrowserContracts[] = {
    { NS_BROWSERDIRECTORYPROVIDER_CONTRACTID, &kNS_BROWSERDIRECTORYPROVIDER_CID },
#if defined(XP_WIN)
    { NS_SHELLSERVICE_CONTRACTID, &kNS_SHELLSERVICE_CID },
#elif defined(MOZ_WIDGET_GTK)
    { NS_SHELLSERVICE_CONTRACTID, &kNS_SHELLSERVICE_CID },
#endif
    { NS_FEEDSNIFFER_CONTRACTID, &kNS_FEEDSNIFFER_CID },
#ifdef XP_MACOSX
    { NS_SHELLSERVICE_CONTRACTID, &kNS_SHELLSERVICE_CID },
#endif
    { nullptr }
};

static const mozilla::Module::CategoryEntry kBrowserCategories[] = {
    { XPCOM_DIRECTORY_PROVIDER_CATEGORY, "browser-directory-provider", NS_BROWSERDIRECTORYPROVIDER_CONTRACTID },
    { NS_CONTENT_SNIFFER_CATEGORY, "Feed Sniffer", NS_FEEDSNIFFER_CONTRACTID },
    { nullptr }
};

static const mozilla::Module kBrowserModule = {
    mozilla::Module::kVersion,
    kBrowserCIDs,
    kBrowserContracts,
    kBrowserCategories
};

NSMODULE_DEFN(nsBrowserCompsModule) = &kBrowserModule;
