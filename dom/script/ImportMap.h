/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_script_ImportMap_h
#define dom_script_ImportMap_h

#include <functional>
#include <map>

#include "js/SourceBufferHolder.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Logging.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "ResolveResult.h"

struct JSContext;
class nsIScriptElement;
class nsIURI;

namespace mozilla {
namespace dom {

class LoadedScript;
class ScriptLoadRequest;
class ScriptLoader;

/**
 * A helper class to report warning to ScriptLoader.
 */
class ReportWarningHelper {
 public:
  ReportWarningHelper(ScriptLoader* aLoader, ScriptLoadRequest* aRequest)
      : mLoader(aLoader), mRequest(aRequest) {}

  void Report(const char* aMessageName,
              const nsTArray<nsString>& aParams = nsTArray<nsString>()) const;

 private:
  RefPtr<ScriptLoader> mLoader;
  ScriptLoadRequest* mRequest;
};

// Specifier map from import maps.
// https://html.spec.whatwg.org/multipage/webappapis.html#specifier-map
using SpecifierMap =
    std::map<nsString, nsCOMPtr<nsIURI>, std::greater<nsString>>;

// Scope map from import maps.
// https://html.spec.whatwg.org/multipage/webappapis.html#import-map-scopes
using ScopeMap = std::map<nsCString, mozilla::UniquePtr<SpecifierMap>,
                          std::greater<nsCString>>;

/**
 * Implementation of Import maps.
 * https://html.spec.whatwg.org/multipage/webappapis.html#import-maps
 */
class ImportMap {
 public:
  ImportMap(mozilla::UniquePtr<SpecifierMap> aImports,
            mozilla::UniquePtr<ScopeMap> aScopes)
      : mImports(std::move(aImports)), mScopes(std::move(aScopes)) {}

  /**
   * Parse the JSON string from the Import map script.
   * This function will throw TypeError if there's any invalid key or value in
   * the JSON text according to the spec.
   *
   * See https://html.spec.whatwg.org/multipage/webappapis.html#parse-an-import-map-string
   */
  static mozilla::UniquePtr<ImportMap> ParseString(
      JSContext* aCx, JS::SourceBufferHolder& aInput, nsIURI* aBaseURL,
      const ReportWarningHelper& aWarning);

  // Implements
  // https://html.spec.whatwg.org/multipage/webappapis.html#resolve-a-module-specifier
  static ResolveResult ResolveModuleSpecifier(ImportMap* aImportMap,
                                              ScriptLoader* aLoader,
                                              LoadedScript* aScript,
                                              const nsAString& aSpecifier);

  // Logging
  static mozilla::LazyLogModule gImportMapLog;

 private:
  /**
   * https://html.spec.whatwg.org/multipage/webappapis.html#import-map
   *
   * A import map is a struct with two items:
   * 1. imports, a specifier map, and
   * 2. scopes, an ordered map of URLs to specifier maps.
   */
  mozilla::UniquePtr<SpecifierMap> mImports;
  mozilla::UniquePtr<ScopeMap> mScopes;
};

} // namespace dom
} // namespace mozilla

#endif // dom_script_ImportMap_h