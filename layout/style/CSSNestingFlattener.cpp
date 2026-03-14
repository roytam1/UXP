/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSSNestingFlattener.h"

#include "mozilla/Assertions.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {
namespace css {

namespace {

class CSSNestingFlattener final
{
  using SelectorList = nsTArray<nsString>;

public:
  explicit CSSNestingFlattener(const nsAString& aInput)
    : mInput(aInput)
    , mPos(0)
    , mSawNesting(false)
  {
  }

  bool Flatten(nsAString& aOutput)
  {
    nsAutoString flattened;
    if (!ProcessStylesheet(flattened, false)) {
      return false;
    }

    SkipWhitespaceAndComments();
    if (mPos != mInput.Length() || !mSawNesting) {
      return false;
    }

    aOutput.Assign(flattened);
    return true;
  }

private:
  static bool
  IsCSSWhitespace(char16_t aChar)
  {
    return aChar == ' ' || aChar == '\t' || aChar == '\r' ||
           aChar == '\n' || aChar == '\f';
  }

  bool
  AtEnd() const
  {
    return mPos >= mInput.Length();
  }

  char16_t
  Peek() const
  {
    MOZ_ASSERT(!AtEnd(), "cannot peek past end");
    return mInput.CharAt(mPos);
  }

  bool
  StartsWithComment() const
  {
    return mPos + 1 < mInput.Length() &&
           mInput.CharAt(mPos) == '/' &&
           mInput.CharAt(mPos + 1) == '*';
  }

  bool
  SkipComment()
  {
    MOZ_ASSERT(StartsWithComment(), "expected comment");

    mPos += 2;
    while (mPos + 1 < mInput.Length()) {
      if (mInput.CharAt(mPos) == '*' && mInput.CharAt(mPos + 1) == '/') {
        mPos += 2;
        return true;
      }
      ++mPos;
    }

    return false;
  }

  void
  SkipWhitespaceAndComments()
  {
    while (!AtEnd()) {
      if (IsCSSWhitespace(Peek())) {
        ++mPos;
        continue;
      }
      if (StartsWithComment()) {
        if (!SkipComment()) {
          mPos = mInput.Length();
          return;
        }
        continue;
      }
      break;
    }
  }

  bool
  SkipString(char16_t aQuote)
  {
    MOZ_ASSERT(!AtEnd() && Peek() == aQuote, "expected string start");

    ++mPos;
    while (!AtEnd()) {
      char16_t c = Peek();
      ++mPos;
      if (c == aQuote) {
        return true;
      }
      if (c == '\\' && !AtEnd()) {
        ++mPos;
        continue;
      }
      if (c == '\n' || c == '\r' || c == '\f') {
        return false;
      }
    }

    return false;
  }

  static void
  TrimWhitespace(nsAString& aText)
  {
    uint32_t start = 0;
    uint32_t end = aText.Length();

    while (start < end && IsCSSWhitespace(aText.CharAt(start))) {
      ++start;
    }
    while (end > start && IsCSSWhitespace(aText.CharAt(end - 1))) {
      --end;
    }

    if (start == 0 && end == aText.Length()) {
      return;
    }

    aText.Assign(Substring(aText, start, end - start));
  }

  bool
  SplitSelectorList(const nsAString& aSelectorText, SelectorList& aSelectors)
  {
    uint32_t itemStart = 0;
    int32_t parenDepth = 0;
    int32_t bracketDepth = 0;
    bool inComment = false;
    char16_t stringQuote = 0;

    for (uint32_t i = 0; i < aSelectorText.Length(); ++i) {
      char16_t c = aSelectorText.CharAt(i);

      if (inComment) {
        if (c == '*' && i + 1 < aSelectorText.Length() &&
            aSelectorText.CharAt(i + 1) == '/') {
          inComment = false;
          ++i;
        }
        continue;
      }

      if (stringQuote) {
        if (c == '\\') {
          ++i;
          continue;
        }
        if (c == stringQuote) {
          stringQuote = 0;
        }
        continue;
      }

      if (c == '/' && i + 1 < aSelectorText.Length() &&
          aSelectorText.CharAt(i + 1) == '*') {
        inComment = true;
        ++i;
        continue;
      }

      if (c == '"' || c == '\'') {
        stringQuote = c;
        continue;
      }

      if (c == '(') {
        ++parenDepth;
        continue;
      }
      if (c == ')' && parenDepth > 0) {
        --parenDepth;
        continue;
      }
      if (c == '[') {
        ++bracketDepth;
        continue;
      }
      if (c == ']' && bracketDepth > 0) {
        --bracketDepth;
        continue;
      }

      if (c == ',' && parenDepth == 0 && bracketDepth == 0) {
        nsAutoString selector;
        selector.Assign(Substring(aSelectorText, itemStart, i - itemStart));
        TrimWhitespace(selector);
        if (!selector.IsEmpty()) {
          aSelectors.AppendElement(selector);
        }
        itemStart = i + 1;
      }
    }

    nsAutoString selector;
    selector.Assign(Substring(aSelectorText, itemStart));
    TrimWhitespace(selector);
    if (!selector.IsEmpty()) {
      aSelectors.AppendElement(selector);
    }

    return !aSelectors.IsEmpty();
  }

  bool
  SelectorHasAmpersand(const nsAString& aSelector) const
  {
    bool inComment = false;
    char16_t stringQuote = 0;

    for (uint32_t i = 0; i < aSelector.Length(); ++i) {
      char16_t c = aSelector.CharAt(i);

      if (inComment) {
        if (c == '*' && i + 1 < aSelector.Length() &&
            aSelector.CharAt(i + 1) == '/') {
          inComment = false;
          ++i;
        }
        continue;
      }

      if (stringQuote) {
        if (c == '\\') {
          ++i;
          continue;
        }
        if (c == stringQuote) {
          stringQuote = 0;
        }
        continue;
      }

      if (c == '/' && i + 1 < aSelector.Length() &&
          aSelector.CharAt(i + 1) == '*') {
        inComment = true;
        ++i;
        continue;
      }

      if (c == '"' || c == '\'') {
        stringQuote = c;
        continue;
      }

      if (c == '&') {
        return true;
      }
    }

    return false;
  }

  void
  ReplaceAmpersands(const nsAString& aSelector,
                    const nsAString& aParent,
                    nsAString& aOutput) const
  {
    bool inComment = false;
    char16_t stringQuote = 0;

    for (uint32_t i = 0; i < aSelector.Length(); ++i) {
      char16_t c = aSelector.CharAt(i);

      if (inComment) {
        aOutput.Append(c);
        if (c == '*' && i + 1 < aSelector.Length() &&
            aSelector.CharAt(i + 1) == '/') {
          aOutput.Append('/');
          inComment = false;
          ++i;
        }
        continue;
      }

      if (stringQuote) {
        aOutput.Append(c);
        if (c == '\\' && i + 1 < aSelector.Length()) {
          aOutput.Append(aSelector.CharAt(i + 1));
          ++i;
          continue;
        }
        if (c == stringQuote) {
          stringQuote = 0;
        }
        continue;
      }

      if (c == '/' && i + 1 < aSelector.Length() &&
          aSelector.CharAt(i + 1) == '*') {
        aOutput.AppendLiteral("/*");
        inComment = true;
        ++i;
        continue;
      }

      if (c == '"' || c == '\'') {
        aOutput.Append(c);
        stringQuote = c;
        continue;
      }

      if (c == '&') {
        aOutput.Append(aParent);
        continue;
      }

      aOutput.Append(c);
    }
  }

  bool
  ExpandNestedSelectors(const SelectorList& aParents,
                        const nsAString& aNestedSelectorText,
                        SelectorList& aSelectors)
  {
    SelectorList nestedSelectors;
    if (!SplitSelectorList(aNestedSelectorText, nestedSelectors)) {
      return false;
    }

    for (const nsString& nestedSelector : nestedSelectors) {
      bool hasAmpersand = SelectorHasAmpersand(nestedSelector);
      for (const nsString& parentSelector : aParents) {
        nsAutoString combined;
        if (hasAmpersand) {
          ReplaceAmpersands(nestedSelector, parentSelector, combined);
        } else {
          combined.Assign(parentSelector);
          if (!combined.IsEmpty()) {
            combined.Append(' ');
          }
          combined.Append(nestedSelector);
        }
        TrimWhitespace(combined);
        if (!combined.IsEmpty()) {
          aSelectors.AppendElement(combined);
        }
      }
    }

    return !aSelectors.IsEmpty();
  }

  static void
  AppendSelectors(const SelectorList& aSelectors, nsAString& aOutput)
  {
    for (uint32_t i = 0; i < aSelectors.Length(); ++i) {
      if (i) {
        aOutput.AppendLiteral(", ");
      }
      aOutput.Append(aSelectors[i]);
    }
  }

  static bool
  StartsNestedSelector(char16_t aChar)
  {
    switch (aChar) {
      case '.':
      case '#':
      case '[':
      case ':':
      case '&':
      case '|':
      case '>':
      case '+':
      case '~':
      case '*':
        return true;
      default:
        return false;
    }
  }

  static bool
  StartsPotentialTypeSelector(char16_t aChar)
  {
    return (aChar >= 'a' && aChar <= 'z') ||
           (aChar >= 'A' && aChar <= 'Z') ||
           aChar == '_' ||
           aChar == '\\' ||
           aChar >= 0x80;
  }

  bool
  LooksLikeTypeSelectorRule() const
  {
    if (AtEnd() || !StartsPotentialTypeSelector(Peek())) {
      return false;
    }

    uint32_t pos = mPos;
    int32_t parenDepth = 0;
    int32_t bracketDepth = 0;
    bool inComment = false;
    char16_t stringQuote = 0;

    while (pos < mInput.Length()) {
      char16_t c = mInput.CharAt(pos);

      if (inComment) {
        if (c == '*' && pos + 1 < mInput.Length() &&
            mInput.CharAt(pos + 1) == '/') {
          inComment = false;
          ++pos;
        }
        ++pos;
        continue;
      }

      if (stringQuote) {
        if (c == '\\' && pos + 1 < mInput.Length()) {
          pos += 2;
          continue;
        }
        if (c == stringQuote) {
          stringQuote = 0;
        }
        ++pos;
        continue;
      }

      if (c == '/' && pos + 1 < mInput.Length() &&
          mInput.CharAt(pos + 1) == '*') {
        inComment = true;
        pos += 2;
        continue;
      }

      if (c == '"' || c == '\'') {
        stringQuote = c;
        ++pos;
        continue;
      }

      if (c == '(') {
        ++parenDepth;
        ++pos;
        continue;
      }
      if (c == ')' && parenDepth > 0) {
        --parenDepth;
        ++pos;
        continue;
      }
      if (c == '[') {
        ++bracketDepth;
        ++pos;
        continue;
      }
      if (c == ']' && bracketDepth > 0) {
        --bracketDepth;
        ++pos;
        continue;
      }

      if (parenDepth == 0 && bracketDepth == 0) {
        if (c == '{') {
          return true;
        }
        if (c == ';' || c == '}') {
          return false;
        }
      }

      ++pos;
    }

    return false;
  }

  static bool
  IsAtRuleNameChar(char16_t aChar)
  {
    return (aChar >= 'a' && aChar <= 'z') ||
           (aChar >= 'A' && aChar <= 'Z') ||
           (aChar >= '0' && aChar <= '9') ||
           aChar == '-';
  }

  static void
  LowercaseASCII(nsACString& aText)
  {
    for (uint32_t i = 0; i < aText.Length(); ++i) {
      char c = aText.CharAt(i);
      if (c >= 'A' && c <= 'Z') {
        aText.BeginWriting()[i] = c - 'A' + 'a';
      }
    }
  }

  static bool
  ShouldProcessGroupRule(const nsACString& aName)
  {
    return aName.EqualsLiteral("media") ||
           aName.EqualsLiteral("supports") ||
           aName.EqualsLiteral("document") ||
           aName.EqualsLiteral("layer");
  }

  void
  FlushDeclarations(const SelectorList& aSelectors,
                    nsAString& aDeclarations,
                    nsAString& aOutput)
  {
    nsAutoString declarations;
    declarations.Assign(aDeclarations);
    TrimWhitespace(declarations);
    aDeclarations.Truncate();

    if (declarations.IsEmpty()) {
      return;
    }

    AppendSelectors(aSelectors, aOutput);
    aOutput.AppendLiteral(" { ");
    aOutput.Append(declarations);
    aOutput.AppendLiteral(" }\n");
  }

  bool
  ReadRawBlockBody(nsAString& aBody)
  {
    uint32_t start = mPos;
    int32_t depth = 0;

    while (!AtEnd()) {
      char16_t c = Peek();
      if (c == '"' || c == '\'') {
        if (!SkipString(c)) {
          return false;
        }
        continue;
      }
      if (StartsWithComment()) {
        if (!SkipComment()) {
          return false;
        }
        continue;
      }
      if (c == '{') {
        ++depth;
        ++mPos;
        continue;
      }
      if (c == '}') {
        if (depth == 0) {
          aBody.Assign(Substring(mInput, start, mPos - start));
          ++mPos;
          return true;
        }
        --depth;
        ++mPos;
        continue;
      }
      ++mPos;
    }

    return false;
  }

  bool
  ReadQualifiedRulePrelude(nsAString& aPrelude)
  {
    uint32_t start = mPos;
    int32_t parenDepth = 0;
    int32_t bracketDepth = 0;

    while (!AtEnd()) {
      char16_t c = Peek();
      if (c == '"' || c == '\'') {
        if (!SkipString(c)) {
          return false;
        }
        continue;
      }
      if (StartsWithComment()) {
        if (!SkipComment()) {
          return false;
        }
        continue;
      }
      if (c == '(') {
        ++parenDepth;
        ++mPos;
        continue;
      }
      if (c == ')' && parenDepth > 0) {
        --parenDepth;
        ++mPos;
        continue;
      }
      if (c == '[') {
        ++bracketDepth;
        ++mPos;
        continue;
      }
      if (c == ']' && bracketDepth > 0) {
        --bracketDepth;
        ++mPos;
        continue;
      }
      if (c == '{' && parenDepth == 0 && bracketDepth == 0) {
        aPrelude.Assign(Substring(mInput, start, mPos - start));
        TrimWhitespace(aPrelude);
        ++mPos;
        return !aPrelude.IsEmpty();
      }
      if ((c == ';' || c == '}') && parenDepth == 0 && bracketDepth == 0) {
        return false;
      }
      ++mPos;
    }

    return false;
  }

  bool
  ReadAtRulePrelude(nsAString& aPrelude, nsACString& aName, bool& aHasBlock)
  {
    MOZ_ASSERT(!AtEnd() && Peek() == '@', "expected at-rule");

    uint32_t start = mPos;
    ++mPos;
    aName.Truncate();
    while (!AtEnd() && IsAtRuleNameChar(Peek())) {
      char16_t c = Peek();
      aName.Append(char(c <= 0x7f ? c : '?'));
      ++mPos;
    }
    LowercaseASCII(aName);

    int32_t parenDepth = 0;
    int32_t bracketDepth = 0;
    while (!AtEnd()) {
      char16_t c = Peek();
      if (c == '"' || c == '\'') {
        if (!SkipString(c)) {
          return false;
        }
        continue;
      }
      if (StartsWithComment()) {
        if (!SkipComment()) {
          return false;
        }
        continue;
      }
      if (c == '(') {
        ++parenDepth;
        ++mPos;
        continue;
      }
      if (c == ')' && parenDepth > 0) {
        --parenDepth;
        ++mPos;
        continue;
      }
      if (c == '[') {
        ++bracketDepth;
        ++mPos;
        continue;
      }
      if (c == ']' && bracketDepth > 0) {
        --bracketDepth;
        ++mPos;
        continue;
      }
      if (parenDepth == 0 && bracketDepth == 0) {
        if (c == ';') {
          aPrelude.Assign(Substring(mInput, start, mPos - start));
          TrimWhitespace(aPrelude);
          ++mPos;
          aHasBlock = false;
          return true;
        }
        if (c == '{') {
          aPrelude.Assign(Substring(mInput, start, mPos - start));
          TrimWhitespace(aPrelude);
          ++mPos;
          aHasBlock = true;
          return true;
        }
      }
      ++mPos;
    }

    return false;
  }

  bool
  ConsumeDeclaration(nsAString& aDeclaration)
  {
    uint32_t start = mPos;
    int32_t parenDepth = 0;
    int32_t bracketDepth = 0;
    int32_t braceDepth = 0;

    while (!AtEnd()) {
      char16_t c = Peek();
      if (c == '"' || c == '\'') {
        if (!SkipString(c)) {
          return false;
        }
        continue;
      }
      if (StartsWithComment()) {
        if (!SkipComment()) {
          return false;
        }
        continue;
      }
      if (c == '(') {
        ++parenDepth;
        ++mPos;
        continue;
      }
      if (c == ')' && parenDepth > 0) {
        --parenDepth;
        ++mPos;
        continue;
      }
      if (c == '[') {
        ++bracketDepth;
        ++mPos;
        continue;
      }
      if (c == ']' && bracketDepth > 0) {
        --bracketDepth;
        ++mPos;
        continue;
      }
      if (c == '{') {
        ++braceDepth;
        ++mPos;
        continue;
      }
      if (c == '}') {
        if (braceDepth == 0 && parenDepth == 0 && bracketDepth == 0) {
          break;
        }
        if (braceDepth > 0) {
          --braceDepth;
        }
        ++mPos;
        continue;
      }
      if (c == ';' && parenDepth == 0 && bracketDepth == 0 &&
          braceDepth == 0) {
        ++mPos;
        break;
      }
      ++mPos;
    }

    aDeclaration.Assign(Substring(mInput, start, mPos - start));
    TrimWhitespace(aDeclaration);
    if (aDeclaration.IsEmpty()) {
      return false;
    }
    if (aDeclaration.Last() != ';') {
      aDeclaration.Append(';');
    }
    return true;
  }

  bool
  ParseAtRule(nsAString& aOutput, const SelectorList* aParents)
  {
    nsAutoString prelude;
    nsAutoCString name;
    bool hasBlock = false;
    if (!ReadAtRulePrelude(prelude, name, hasBlock)) {
      return false;
    }

    if (!hasBlock) {
      aOutput.Append(prelude);
      aOutput.AppendLiteral(";\n");
      return true;
    }

    if (!ShouldProcessGroupRule(name)) {
      nsAutoString body;
      if (!ReadRawBlockBody(body)) {
        return false;
      }
      aOutput.Append(prelude);
      aOutput.AppendLiteral(" {");
      aOutput.Append(body);
      aOutput.AppendLiteral("}\n");
      return true;
    }

    nsAutoString inner;
    if (aParents) {
      mSawNesting = true;
      if (!ProcessStyleContext(*aParents, inner)) {
        return false;
      }
    } else {
      if (!ProcessStylesheet(inner, true)) {
        return false;
      }
    }

    aOutput.Append(prelude);
    aOutput.AppendLiteral(" {\n");
    aOutput.Append(inner);
    aOutput.AppendLiteral("}\n");
    return true;
  }

  bool
  ParseQualifiedRule(nsAString& aOutput, const SelectorList* aParents)
  {
    nsAutoString prelude;
    if (!ReadQualifiedRulePrelude(prelude)) {
      return false;
    }

    SelectorList selectors;
    if (aParents) {
      mSawNesting = true;
      if (!ExpandNestedSelectors(*aParents, prelude, selectors)) {
        return false;
      }
    } else if (!SplitSelectorList(prelude, selectors)) {
      return false;
    }

    return ProcessStyleContext(selectors, aOutput);
  }

  bool
  ProcessStyleContext(const SelectorList& aSelectors, nsAString& aOutput)
  {
    nsAutoString declarations;

    while (!AtEnd()) {
      SkipWhitespaceAndComments();
      if (AtEnd()) {
        return false;
      }

      char16_t c = Peek();
      if (c == '}') {
        ++mPos;
        FlushDeclarations(aSelectors, declarations, aOutput);
        return true;
      }

      if (c == '@') {
        FlushDeclarations(aSelectors, declarations, aOutput);
        if (!ParseAtRule(aOutput, &aSelectors)) {
          return false;
        }
        continue;
      }

      if (StartsNestedSelector(c) || LooksLikeTypeSelectorRule()) {
        FlushDeclarations(aSelectors, declarations, aOutput);
        if (!ParseQualifiedRule(aOutput, &aSelectors)) {
          return false;
        }
        continue;
      }

      nsAutoString declaration;
      if (!ConsumeDeclaration(declaration)) {
        return false;
      }
      if (!declarations.IsEmpty()) {
        declarations.Append(' ');
      }
      declarations.Append(declaration);
    }

    return false;
  }

  bool
  ProcessStylesheet(nsAString& aOutput, bool aStopAtBlockEnd)
  {
    while (!AtEnd()) {
      SkipWhitespaceAndComments();
      if (AtEnd()) {
        return !aStopAtBlockEnd;
      }

      if (Peek() == '}') {
        if (!aStopAtBlockEnd) {
          return false;
        }
        ++mPos;
        return true;
      }

      if (Peek() == '@') {
        if (!ParseAtRule(aOutput, nullptr)) {
          return false;
        }
      } else {
        if (!ParseQualifiedRule(aOutput, nullptr)) {
          return false;
        }
      }
    }

    return !aStopAtBlockEnd;
  }

  const nsAString& mInput;
  uint32_t mPos;
  bool mSawNesting;
};

} // namespace

bool
FlattenBasicCSSNesting(const nsAString& aInput, nsAString& aOutput)
{
  CSSNestingFlattener flattener(aInput);
  return flattener.Flatten(aOutput);
}

} // namespace css
} // namespace mozilla
