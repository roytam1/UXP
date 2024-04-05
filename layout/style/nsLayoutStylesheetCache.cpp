/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLayoutStylesheetCache.h"

#include "nsAppDirectoryServiceDefs.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/css/Loader.h"
#include "mozilla/dom/SRIMetadata.h"
#include "nsIConsoleService.h"
#include "nsIFile.h"
#include "nsNetUtil.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsIXULRuntime.h"
#include "nsPrintfCString.h"
#include "nsXULAppAPI.h"

using namespace mozilla;
using namespace mozilla::css;

static bool sNumberControlEnabled;

#define NUMBER_CONTROL_PREF "dom.forms.number"

NS_IMPL_ISUPPORTS(
  nsLayoutStylesheetCache, nsIObserver, nsIMemoryReporter)

nsresult
nsLayoutStylesheetCache::Observe(nsISupports* aSubject,
                            const char* aTopic,
                            const char16_t* aData)
{
  if (!strcmp(aTopic, "profile-before-change")) {
    mUserContentSheet = nullptr;
    mUserChromeSheet  = nullptr;
  }
  else if (!strcmp(aTopic, "profile-do-change")) {
    InitFromProfile();
  }
  else if (strcmp(aTopic, "chrome-flush-skin-caches") == 0 ||
           strcmp(aTopic, "chrome-flush-caches") == 0) {
    mScrollbarsSheet = nullptr;
    mFormsSheet = nullptr;
    mNumberControlSheet = nullptr;
  }
  else {
    NS_NOTREACHED("Unexpected observer topic.");
  }
  return NS_OK;
}

StyleSheet*
nsLayoutStylesheetCache::ScrollbarsSheet()
{
  if (!mScrollbarsSheet) {
    // Scrollbars don't need access to unsafe rules
    LoadSheetURL("chrome://global/skin/scrollbars.css",
                 &mScrollbarsSheet, eAuthorSheetFeatures, eCrash);
  }

  return mScrollbarsSheet;
}

StyleSheet*
nsLayoutStylesheetCache::FormsSheet()
{
  if (!mFormsSheet) {
    // forms.css needs access to unsafe rules
    LoadSheetURL("resource://gre-resources/forms.css",
                 &mFormsSheet, eAgentSheetFeatures, eCrash);
  }

  return mFormsSheet;
}

StyleSheet*
nsLayoutStylesheetCache::NumberControlSheet()
{
  if (!sNumberControlEnabled) {
    return nullptr;
  }

  if (!mNumberControlSheet) {
    LoadSheetURL("resource://gre-resources/number-control.css",
                 &mNumberControlSheet, eAgentSheetFeatures, eCrash);
  }

  return mNumberControlSheet;
}

StyleSheet*
nsLayoutStylesheetCache::UserContentSheet()
{
  return mUserContentSheet;
}

StyleSheet*
nsLayoutStylesheetCache::UserChromeSheet()
{
  return mUserChromeSheet;
}

StyleSheet*
nsLayoutStylesheetCache::UASheet()
{
  if (!mUASheet) {
    LoadSheetURL("resource://gre-resources/ua.css",
                 &mUASheet, eAgentSheetFeatures, eCrash);
  }

  return mUASheet;
}

StyleSheet*
nsLayoutStylesheetCache::HTMLSheet()
{
  if (!mHTMLSheet) {
    LoadSheetURL("resource://gre-resources/html.css",
                 &mHTMLSheet, eAgentSheetFeatures, eCrash);
  }

  return mHTMLSheet;
}

StyleSheet*
nsLayoutStylesheetCache::MinimalXULSheet()
{
  return mMinimalXULSheet;
}

StyleSheet*
nsLayoutStylesheetCache::XULSheet()
{
  return mXULSheet;
}

StyleSheet*
nsLayoutStylesheetCache::QuirkSheet()
{
  return mQuirkSheet;
}

StyleSheet*
nsLayoutStylesheetCache::SVGSheet()
{
  return mSVGSheet;
}

StyleSheet*
nsLayoutStylesheetCache::MathMLSheet()
{
  if (!mMathMLSheet) {
    LoadSheetURL("resource://gre-resources/mathml.css",
                 &mMathMLSheet, eAgentSheetFeatures, eCrash);
  }

  return mMathMLSheet;
}

StyleSheet*
nsLayoutStylesheetCache::CounterStylesSheet()
{
  return mCounterStylesSheet;
}

StyleSheet*
nsLayoutStylesheetCache::NoScriptSheet()
{
  if (!mNoScriptSheet) {
    LoadSheetURL("resource://gre-resources/noscript.css",
                 &mNoScriptSheet, eAgentSheetFeatures, eCrash);
  }

  return mNoScriptSheet;
}

StyleSheet*
nsLayoutStylesheetCache::NoFramesSheet()
{
  if (!mNoFramesSheet) {
    LoadSheetURL("resource://gre-resources/noframes.css",
                 &mNoFramesSheet, eAgentSheetFeatures, eCrash);
  }

  return mNoFramesSheet;
}

StyleSheet*
nsLayoutStylesheetCache::ChromePreferenceSheet(nsPresContext* aPresContext)
{
  if (!mChromePreferenceSheet) {
    BuildPreferenceSheet(&mChromePreferenceSheet, aPresContext);
  }

  return mChromePreferenceSheet;
}

StyleSheet*
nsLayoutStylesheetCache::ContentPreferenceSheet(nsPresContext* aPresContext)
{
  if (!mContentPreferenceSheet) {
    BuildPreferenceSheet(&mContentPreferenceSheet, aPresContext);
  }

  return mContentPreferenceSheet;
}

StyleSheet*
nsLayoutStylesheetCache::ContentEditableSheet()
{
  if (!mContentEditableSheet) {
    LoadSheetURL("resource://gre/res/contenteditable.css",
                 &mContentEditableSheet, eAgentSheetFeatures, eCrash);
  }

  return mContentEditableSheet;
}

StyleSheet*
nsLayoutStylesheetCache::DesignModeSheet()
{
  if (!mDesignModeSheet) {
    LoadSheetURL("resource://gre/res/designmode.css",
                 &mDesignModeSheet, eAgentSheetFeatures, eCrash);
  }

  return mDesignModeSheet;
}

void
nsLayoutStylesheetCache::Shutdown()
{
  gCSSLoader = nullptr;
  gStyleCache = nullptr;
  MOZ_ASSERT(!gUserContentSheetURL, "Got the URL but never used?");
}

void
nsLayoutStylesheetCache::SetUserContentCSSURL(nsIURI* aURI)
{
  MOZ_ASSERT(XRE_IsContentProcess(), "Only used in content processes.");
  gUserContentSheetURL = aURI;
}

MOZ_DEFINE_MALLOC_SIZE_OF(LayoutStylesheetCacheMallocSizeOf)

NS_IMETHODIMP
nsLayoutStylesheetCache::CollectReports(nsIHandleReportCallback* aHandleReport,
                                        nsISupports* aData, bool aAnonymize)
{
  MOZ_COLLECT_REPORT(
    "explicit/layout/style-sheet-cache", KIND_HEAP, UNITS_BYTES,
    SizeOfIncludingThis(LayoutStylesheetCacheMallocSizeOf),
    "Memory used for some built-in style sheets.");

  return NS_OK;
}


size_t
nsLayoutStylesheetCache::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  #define MEASURE(s) n += s ? s->SizeOfIncludingThis(aMallocSizeOf) : 0;

  MEASURE(mChromePreferenceSheet);
  MEASURE(mContentEditableSheet);
  MEASURE(mContentPreferenceSheet);
  MEASURE(mCounterStylesSheet);
  MEASURE(mDesignModeSheet);
  MEASURE(mFormsSheet);
  MEASURE(mHTMLSheet);
  MEASURE(mMathMLSheet);
  MEASURE(mMinimalXULSheet);
  MEASURE(mNoFramesSheet);
  MEASURE(mNoScriptSheet);
  MEASURE(mNumberControlSheet);
  MEASURE(mQuirkSheet);
  MEASURE(mSVGSheet);
  MEASURE(mScrollbarsSheet);
  MEASURE(mUASheet);
  MEASURE(mUserChromeSheet);
  MEASURE(mUserContentSheet);
  MEASURE(mXULSheet);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - gCSSLoader

  return n;
}

nsLayoutStylesheetCache::nsLayoutStylesheetCache()
{
  nsCOMPtr<nsIObserverService> obsSvc =
    mozilla::services::GetObserverService();
  NS_ASSERTION(obsSvc, "No global observer service?");

  if (obsSvc) {
    obsSvc->AddObserver(this, "profile-before-change", false);
    obsSvc->AddObserver(this, "profile-do-change", false);
    obsSvc->AddObserver(this, "chrome-flush-skin-caches", false);
    obsSvc->AddObserver(this, "chrome-flush-caches", false);
  }

  InitFromProfile();

  // And make sure that we load our UA sheets.  No need to do this
  // per-profile, since they're profile-invariant.
  LoadSheetURL("resource://gre-resources/counterstyles.css",
               &mCounterStylesSheet, eAgentSheetFeatures, eCrash);
  LoadSheetURL("chrome://global/content/minimal-xul.css",
               &mMinimalXULSheet, eAgentSheetFeatures, eCrash);
  LoadSheetURL("resource://gre-resources/quirk.css",
               &mQuirkSheet, eAgentSheetFeatures, eCrash);
  LoadSheetURL("resource://gre/res/svg.css",
               &mSVGSheet, eAgentSheetFeatures, eCrash);
  LoadSheetURL("chrome://global/content/xul.css",
               &mXULSheet, eAgentSheetFeatures, eCrash);

  if (gUserContentSheetURL) {
    MOZ_ASSERT(XRE_IsContentProcess(), "Only used in content processes.");
    LoadSheet(gUserContentSheetURL, &mUserContentSheet, eUserSheetFeatures, eLogToConsole);
    gUserContentSheetURL = nullptr;
  }

  // The remaining sheets are created on-demand do to their use being rarer
  // (which helps save memory for Firefox OS apps) or because they need to
  // be re-loadable in DependentPrefChanged.
}

nsLayoutStylesheetCache::~nsLayoutStylesheetCache()
{
  mozilla::UnregisterWeakMemoryReporter(this);
}

void
nsLayoutStylesheetCache::InitMemoryReporter()
{
  mozilla::RegisterWeakMemoryReporter(this);
}

/* static */ nsLayoutStylesheetCache*
nsLayoutStylesheetCache::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  bool mustInit = !gStyleCache;
  auto& cache = gStyleCache;

  if (!cache) {
    cache = new nsLayoutStylesheetCache();
    cache->InitMemoryReporter();
  }

  if (mustInit) {
    // Initialization that only needs to be done once for both
    // nsLayoutStylesheetCaches.

    Preferences::AddBoolVarCache(&sNumberControlEnabled, NUMBER_CONTROL_PREF,
                                 true);

    // For each pref that controls a CSS feature that a UA style sheet depends
    // on (such as a pref that enables a property that a UA style sheet uses),
    // register DependentPrefChanged as a callback to ensure that the relevant
    // style sheets will be re-parsed.
    // Preferences::RegisterCallback(&DependentPrefChanged,
    //                               "layout.css.example-pref.enabled");
    Preferences::RegisterCallback(&DependentPrefChanged,
                                  "dom.details_element.enabled");
  }

  return cache;
}

void
nsLayoutStylesheetCache::InitFromProfile()
{
  nsCOMPtr<nsIXULRuntime> appInfo = do_GetService("@mozilla.org/xre/app-info;1");
  if (appInfo) {
    bool inSafeMode = false;
    appInfo->GetInSafeMode(&inSafeMode);
    if (inSafeMode)
      return;
  }
  nsCOMPtr<nsIFile> contentFile;
  nsCOMPtr<nsIFile> chromeFile;

  NS_GetSpecialDirectory(NS_APP_USER_CHROME_DIR,
                         getter_AddRefs(contentFile));
  if (!contentFile) {
    // if we don't have a profile yet, that's OK!
    return;
  }

  contentFile->Clone(getter_AddRefs(chromeFile));
  if (!chromeFile) return;

  contentFile->Append(NS_LITERAL_STRING("userContent.css"));
  chromeFile->Append(NS_LITERAL_STRING("userChrome.css"));

  LoadSheetFile(contentFile, &mUserContentSheet, eUserSheetFeatures, eLogToConsole);
  LoadSheetFile(chromeFile, &mUserChromeSheet, eUserSheetFeatures, eLogToConsole);
}

void
nsLayoutStylesheetCache::LoadSheetURL(const char* aURL,
                                      RefPtr<StyleSheet>* aSheet,
                                      SheetParsingMode aParsingMode,
                                      FailureAction aFailureAction)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), aURL);
  LoadSheet(uri, aSheet, aParsingMode, aFailureAction);
  if (!aSheet) {
    NS_ERROR(nsPrintfCString("Could not load %s", aURL).get());
  }
}

void
nsLayoutStylesheetCache::LoadSheetFile(nsIFile* aFile,
                                       RefPtr<StyleSheet>* aSheet,
                                       SheetParsingMode aParsingMode,
                                       FailureAction aFailureAction)
{
  bool exists = false;
  aFile->Exists(&exists);

  if (!exists) return;

  nsCOMPtr<nsIURI> uri;
  NS_NewFileURI(getter_AddRefs(uri), aFile);

  LoadSheet(uri, aSheet, aParsingMode, aFailureAction);
}

static void
ErrorLoadingSheet(nsIURI* aURI, const char* aMsg, FailureAction aFailureAction)
{
  nsPrintfCString errorMessage("%s loading built-in stylesheet '%s'",
                               aMsg,
                               aURI ? aURI->GetSpecOrDefault().get() : "");
  if (aFailureAction == eLogToConsole) {
    nsCOMPtr<nsIConsoleService> cs = do_GetService(NS_CONSOLESERVICE_CONTRACTID);
    if (cs) {
      cs->LogStringMessage(NS_ConvertUTF8toUTF16(errorMessage).get());
      return;
    }
  }

  NS_RUNTIMEABORT(errorMessage.get());
}

void
nsLayoutStylesheetCache::LoadSheet(nsIURI* aURI,
                                   RefPtr<StyleSheet>* aSheet,
                                   SheetParsingMode aParsingMode,
                                   FailureAction aFailureAction)
{
  if (!aURI) {
    ErrorLoadingSheet(aURI, "null URI", eCrash);
    return;
  }

  if (!gCSSLoader) {
    gCSSLoader = new mozilla::css::Loader();
    if (!gCSSLoader) {
      ErrorLoadingSheet(aURI, "no Loader", eCrash);
      return;
    }
  }

  nsresult rv = gCSSLoader->LoadSheetSync(aURI, aParsingMode, true, aSheet);
  if (NS_FAILED(rv)) {
    ErrorLoadingSheet(aURI,
      nsPrintfCString("LoadSheetSync failed with error %x", rv).get(),
      aFailureAction);
  }
}

/* static */ void
nsLayoutStylesheetCache::InvalidateSheet(RefPtr<StyleSheet>* aSheet)
{
  MOZ_ASSERT(gCSSLoader, "pref changed before we loaded a sheet?");

  const bool hasSheet = aSheet && *aSheet;
  if (hasSheet && gCSSLoader) {
    nsIURI* uri = (*aSheet)->GetSheetURI();
    gCSSLoader->ObsoleteSheet(uri);
    *aSheet = nullptr;
  }
}

/* static */ void
nsLayoutStylesheetCache::DependentPrefChanged(const char* aPref, void* aData)
{
  MOZ_ASSERT(gStyleCache, "pref changed after shutdown?");

  // Cause any UA style sheets whose parsing depends on the value of prefs
  // to be re-parsed by dropping the sheet from gCSSLoader's cache
  // then setting our cached sheet pointer to null.  This will only work for
  // sheets that are loaded lazily.

#define INVALIDATE(sheet_) \
  InvalidateSheet(gStyleCache ? &gStyleCache->sheet_ : nullptr);

  // INVALIDATE(mUASheet);  // for layout.css.example-pref.enabled
  INVALIDATE(mHTMLSheet); // for dom.details_element.enabled

#undef INVALIDATE
}

/* static */ void
nsLayoutStylesheetCache::InvalidatePreferenceSheets()
{
  if (gStyleCache) {
    gStyleCache->mContentPreferenceSheet = nullptr;
    gStyleCache->mChromePreferenceSheet = nullptr;
  }
}

void
nsLayoutStylesheetCache::BuildPreferenceSheet(RefPtr<StyleSheet>* aSheet,
                                              nsPresContext* aPresContext)
{
  *aSheet = new CSSStyleSheet(eAgentSheetFeatures, CORS_NONE,
                              mozilla::net::RP_Default);

  StyleSheet* sheet = *aSheet;

  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "about:PreferenceStyleSheet", nullptr);
  MOZ_ASSERT(uri, "URI creation shouldn't fail");

  sheet->SetURIs(uri, uri, uri);
  sheet->SetComplete();

  static const uint32_t kPreallocSize = 1024;

  nsString sheetText;
  sheetText.SetCapacity(kPreallocSize);

#define NS_GET_R_G_B(color_) \
  NS_GET_R(color_), NS_GET_G(color_), NS_GET_B(color_)

  sheetText.AppendLiteral(
      "@namespace url(http://www.w3.org/1999/xhtml);\n"
      "@namespace svg url(http://www.w3.org/2000/svg);\n");

  // Rules for link styling.
  nscolor linkColor = aPresContext->DefaultLinkColor();
  nscolor activeColor = aPresContext->DefaultActiveLinkColor();
  nscolor visitedColor = aPresContext->DefaultVisitedLinkColor();

  sheetText.AppendPrintf(
      "*|*:link { color: #%02x%02x%02x; }\n"
      "*|*:any-link:active { color: #%02x%02x%02x; }\n"
      "*|*:visited { color: #%02x%02x%02x; }\n",
      NS_GET_R_G_B(linkColor),
      NS_GET_R_G_B(activeColor),
      NS_GET_R_G_B(visitedColor));

  bool underlineLinks =
    aPresContext->GetCachedBoolPref(kPresContext_UnderlineLinks);
  sheetText.AppendPrintf(
      "*|*:any-link%s { text-decoration: %s; }\n",
      underlineLinks ? ":not(svg|a)" : "",
      underlineLinks ? "underline" : "none");

  // Rules for focus styling.

  bool focusRingOnAnything = aPresContext->GetFocusRingOnAnything();
  uint8_t focusRingWidth = aPresContext->FocusRingWidth();
  uint8_t focusRingStyle = aPresContext->GetFocusRingStyle();

  if ((focusRingWidth != 1 && focusRingWidth <= 4) || focusRingOnAnything) {
    if (focusRingWidth != 1) {
      // If the focus ring width is different from the default, fix buttons
      // with rings.
      sheetText.AppendPrintf(
          "button::-moz-focus-inner, input[type=\"reset\"]::-moz-focus-inner, "
          "input[type=\"button\"]::-moz-focus-inner, "
          "input[type=\"submit\"]::-moz-focus-inner { "
          "border: %dpx %s transparent !important; }\n",
          focusRingWidth,
          focusRingStyle == 0 ? "solid" : "dotted");

      sheetText.AppendLiteral(
          "button:focus::-moz-focus-inner, "
          "input[type=\"reset\"]:focus::-moz-focus-inner, "
          "input[type=\"button\"]:focus::-moz-focus-inner, "
          "input[type=\"submit\"]:focus::-moz-focus-inner { "
          "border-color: ButtonText !important; }\n");
    }

    sheetText.AppendPrintf(
        "%s { outline: %dpx %s !important; %s}\n",
        focusRingOnAnything ?
          ":focus" :
          "*|*:link:focus, *|*:visited:focus",
        focusRingWidth,
        focusRingStyle == 0 ? // solid
          "solid -moz-mac-focusring" : "dotted WindowText",
        focusRingStyle == 0 ? // solid
          "-moz-outline-radius: 3px; outline-offset: 1px; " : "");
  }

  if (aPresContext->GetUseFocusColors()) {
    nscolor focusText = aPresContext->FocusTextColor();
    nscolor focusBG = aPresContext->FocusBackgroundColor();
    sheetText.AppendPrintf(
        "*:focus, *:focus > font { color: #%02x%02x%02x !important; "
        "background-color: #%02x%02x%02x !important; }\n",
        NS_GET_R_G_B(focusText),
        NS_GET_R_G_B(focusBG));
  }

  NS_ASSERTION(sheetText.Length() <= kPreallocSize,
               "kPreallocSize should be big enough to build preference style "
               "sheet without reallocation");

  sheet->AsConcrete()->ReparseSheet(sheetText);

#undef NS_GET_R_G_B
}

mozilla::StaticRefPtr<nsLayoutStylesheetCache>
nsLayoutStylesheetCache::gStyleCache;

mozilla::StaticRefPtr<mozilla::css::Loader>
nsLayoutStylesheetCache::gCSSLoader;

mozilla::StaticRefPtr<nsIURI>
nsLayoutStylesheetCache::gUserContentSheetURL;
