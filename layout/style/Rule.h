/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for all rule types in a CSS style sheet */

#ifndef mozilla_css_Rule_h___
#define mozilla_css_Rule_h___

#include "mozilla/StyleSheetInlines.h"
#include "mozilla/MemoryReporting.h"
#include "nsISupports.h"
#include "nsIDOMCSSRule.h"
#include "nsWrapperCache.h"

class nsIDocument;
struct nsRuleData;
template<class T> struct already_AddRefed;
class nsHTMLCSSStyleSheet;

namespace mozilla {
namespace css {
class GroupRule;

class Rule : public nsIDOMCSSRule
           , public nsWrapperCache
{
protected:
  Rule(uint32_t aLineNumber, uint32_t aColumnNumber)
    : mSheet(nullptr),
      mParentRule(nullptr),
      mLineNumber(aLineNumber),
      mColumnNumber(aColumnNumber)
  {
  }

  Rule(const Rule& aCopy)
    : mSheet(aCopy.mSheet),
      mParentRule(aCopy.mParentRule),
      mLineNumber(aCopy.mLineNumber),
      mColumnNumber(aCopy.mColumnNumber)
  {
  }

  virtual ~Rule() {}

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS(Rule)
  // Return true if this rule is known to be a cycle collection leaf, in the
  // sense that it doesn't have any outgoing owning edges.
  virtual bool IsCCLeaf() const MOZ_MUST_OVERRIDE;

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const = 0;
#endif

  // The constants in this list must maintain the following invariants:
  //   If a rule of type N must appear before a rule of type M in stylesheets
  //   then N < M
  // Note that CSSStyleSheet::RebuildChildList assumes that no other kinds of
  // rules can come between two rules of type IMPORT_RULE.
  enum {
    UNKNOWN_RULE = 0,
    STYLE_RULE = 1,
    CHARSET_RULE = 2,
    IMPORT_RULE = 3,
    MEDIA_RULE = 4,
    FONT_FACE_RULE = 5,
    PAGE_RULE = 6,
    KEYFRAMES_RULE = 7,
    KEYFRAME_RULE = 8,
    MARGIN_RULE = 9,
    NAMESPACE_RULE = 10,
    COUNTER_STYLE_RULE = 11,
    SUPPORTS_RULE = 12,
    RESERVED1_RULE = 13,
    FONT_FEATURE_VALUES_RULE = 14,
    // Internal from here down.
    VIEWPORT_RULE = 15,
    DOCUMENT_RULE = 16,
    LAYER_BLOCK_RULE = 17,
    LAYER_STATEMENT_RULE = 18,
    CONTAINER_RULE = 19
  };

  virtual int32_t GetType() const = 0;

  CSSStyleSheet* GetStyleSheet() const { return mSheet; }

  // Return the document the rule lives in, if any
  nsIDocument* GetDocument() const
  {
    CSSStyleSheet* sheet = GetStyleSheet();
    return sheet ? sheet->GetDocument() : nullptr;
  }

  virtual void SetStyleSheet(CSSStyleSheet* aSheet);

  void SetParentRule(GroupRule* aRule) {
    // We don't reference count this up reference. The group rule
    // will tell us when it's going away or when we're detached from
    // it.
    mParentRule = aRule;
  }

  uint32_t GetLineNumber() const { return mLineNumber; }
  uint32_t GetColumnNumber() const { return mColumnNumber; }

  /**
   * Clones |this|. Never returns nullptr.
   */
  virtual already_AddRefed<Rule> Clone() const = 0;

  // This is pure virtual because all of Rule's data members are non-owning and
  // thus measured elsewhere.
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const MOZ_MUST_OVERRIDE = 0;

  // WebIDL interface, aka helpers for nsIDOMCSSRule implementation.
  virtual uint16_t Type() const = 0;
  virtual void GetCssTextImpl(nsAString& aCssText) const = 0;
  // XPCOM GetCssText is OK, since it never throws.
  // XPCOM SetCssText is OK, since it never throws.
  Rule* GetParentRule() const;
  StyleSheet* GetParentStyleSheet() const { return GetStyleSheet(); }
  nsIDocument* GetParentObject() const { return GetDocument(); }

protected:
  // True if we're known-live for cycle collection purposes.
  bool IsKnownLive() const;

  // This is sometimes null (e.g., for style attributes).
  CSSStyleSheet*    mSheet;
  // When the parent GroupRule is destroyed, it will call SetParentRule(nullptr)
  // on this object. (Through SetParentRuleReference);
  GroupRule* MOZ_NON_OWNING_REF mParentRule;

  // Keep the same type so that MSVC packs them.
  uint32_t          mLineNumber;
  uint32_t          mColumnNumber;
};

} // namespace css
} // namespace mozilla

#endif /* mozilla_css_Rule_h___ */
