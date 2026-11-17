/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Immutable tree representation of a container query condition.
 *
 * A media query stores its conditions as a flat conjunction of
 * nsMediaExpression with a single negation flag on the query, so it cannot
 * represent "not (width > 10px) and (height > 10px)": the negation applies to
 * the whole query rather than to one operand.  Container queries are parsed
 * into this tree instead, which keeps arbitrary nesting of not/and/or, and
 * matching is a plain recursive walk over the nodes.
 *
 * Nodes are immutable once built.  The parser hands out already_AddRefed<> and
 * consumers only ever hold const pointers, so a tree can be shared by rule and
 * sheet cloning without being copied.
 */

#ifndef nsContainerCondition_h_
#define nsContainerCondition_h_

#include "nsTArray.h"
#include "nsIMediaList.h"
#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/RefCounted.h"
#include "mozilla/RefPtr.h"

/**
 * Three-valued result of matching one node.  Unknown means the feature cannot
 * be answered by the selected container at all -- a block-axis feature against
 * an inline-size container, an unsupported feature name, a missing value -- and
 * is kept distinct from False so that "not (unknown)" stays unknown instead of
 * turning into a match.
 */
enum class ContainerEval : uint8_t {
  False,
  True,
  Unknown
};

inline ContainerEval
NegateContainerEval(ContainerEval aEval)
{
  switch (aEval) {
    case ContainerEval::False:
      return ContainerEval::True;
    case ContainerEval::True:
      return ContainerEval::False;
    case ContainerEval::Unknown:
      return ContainerEval::Unknown;
  }
  return ContainerEval::Unknown;
}

/**
 * Evaluates one leaf against the container the caller selected.  Implemented by
 * the rule processor, which owns container resolution and the size logic.
 */
class nsContainerConditionEvaluator
{
public:
  virtual ContainerEval EvaluateFeature(const nsMediaExpression& aExpression) = 0;

protected:
  virtual ~nsContainerConditionEvaluator() {}
};

class nsContainerCondition final
{
public:
  enum Kind : uint8_t { eFeature, eNot, eAnd, eOr };

  NS_INLINE_DECL_REFCOUNTING(nsContainerCondition)

  static already_AddRefed<nsContainerCondition>
  CreateFeature(const nsMediaExpression& aExpression);
  static already_AddRefed<nsContainerCondition>
  CreateNot(already_AddRefed<nsContainerCondition> aOperand);
  // Consumes aOperands.  Returns the single operand unwrapped when only one is
  // given, so a lone condition never grows a junction node.
  static already_AddRefed<nsContainerCondition>
  CreateJunction(Kind aKind, nsTArray<RefPtr<nsContainerCondition>>& aOperands);

  Kind GetKind() const { return mKind; }

  const nsMediaExpression& GetExpression() const
  {
    MOZ_ASSERT(mKind == eFeature);
    return mExpression;
  }

  const nsTArray<RefPtr<nsContainerCondition>>& GetOperands() const
  {
    return mOperands;
  }

  ContainerEval Matches(nsContainerConditionEvaluator& aEvaluator) const;

  void AppendToString(nsAString& aString) const;

  uint32_t CountLeaves() const;

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return SizeOfExcludingThis(aMallocSizeOf) + aMallocSizeOf(this);
  }

private:
  explicit nsContainerCondition(Kind aKind);
  ~nsContainerCondition() {}

  Kind mKind;
  // Only meaningful for eFeature.
  nsMediaExpression mExpression;
  // Exactly one operand for eNot, two or more for eAnd and eOr.
  nsTArray<RefPtr<nsContainerCondition>> mOperands;
};

#endif /* nsContainerCondition_h_ */