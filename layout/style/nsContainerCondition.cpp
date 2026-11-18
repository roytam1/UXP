/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContainerCondition.h"

#include "nsString.h"

using namespace mozilla;

// Serialisation precedence; a child binding less tightly than its parent gets
// parentheses.  A right child of equal precedence gets them too, so that the
// printed form parses back to the same tree.
static uint8_t
ConditionPrecedence(nsContainerCondition::Kind aKind)
{
  switch (aKind) {
    case nsContainerCondition::eOr:
      return 1;
    case nsContainerCondition::eAnd:
      return 2;
    case nsContainerCondition::eNot:
      return 3;
    case nsContainerCondition::eFeature:
    case nsContainerCondition::eStyle:
      return 4;
  }
  MOZ_ASSERT_UNREACHABLE("unexpected kind");
  return 4;
}

nsContainerCondition::nsContainerCondition(Kind aKind)
  : mKind(aKind)
{
}

/* static */ already_AddRefed<nsContainerCondition>
nsContainerCondition::CreateFeature(const nsMediaExpression& aExpression)
{
  RefPtr<nsContainerCondition> node = new nsContainerCondition(eFeature);
  node->mExpression = aExpression;
  return node.forget();
}

/* static */ already_AddRefed<nsContainerCondition>
nsContainerCondition::CreateStyle(const nsAString& aText, const ContainerStyleQuery& aQuery)
{
  RefPtr<nsContainerCondition> node = new nsContainerCondition(eStyle);
  node->mStyle.AssignLiteral("style(");
  node->mStyle.Append(aText);
  node->mStyle.Append(')');
  node->mStyleQuery = aQuery;
  return node.forget();
}

/* static */ already_AddRefed<nsContainerCondition>
nsContainerCondition::CreateNot(already_AddRefed<nsContainerCondition> aOperand)
{
  RefPtr<nsContainerCondition> operand = aOperand;
  if (!operand) {
    return nullptr;
  }
  RefPtr<nsContainerCondition> node = new nsContainerCondition(eNot);
  if (!node->mOperands.AppendElement(operand)) {
    return nullptr;
  }
  return node.forget();
}

/* static */ already_AddRefed<nsContainerCondition>
nsContainerCondition::CreateJunction(
    Kind aKind, nsTArray<RefPtr<nsContainerCondition>>& aOperands)
{
  MOZ_ASSERT(aKind == eAnd || aKind == eOr);
  if (aOperands.IsEmpty()) {
    return nullptr;
  }
  if (aOperands.Length() == 1) {
    RefPtr<nsContainerCondition> only = aOperands[0].forget();
    aOperands.Clear();
    return only.forget();
  }
  for (const RefPtr<nsContainerCondition>& operand : aOperands) {
    if (!operand) {
      return nullptr;
    }
  }
  RefPtr<nsContainerCondition> node = new nsContainerCondition(aKind);
  node->mOperands.SwapElements(aOperands);
  return node.forget();
}

ContainerEval
nsContainerCondition::Matches(nsContainerConditionEvaluator& aEvaluator) const
{
  switch (mKind) {
    case eFeature:
      return aEvaluator.EvaluateFeature(mExpression);

    case eStyle:
      return aEvaluator.EvaluateStyle(*this);

    case eNot:
      MOZ_ASSERT(mOperands.Length() == 1);
      return NegateContainerEval(mOperands[0]->Matches(aEvaluator));

    case eAnd: {
      bool unknown = false;
      for (const RefPtr<nsContainerCondition>& operand : mOperands) {
        ContainerEval result = operand->Matches(aEvaluator);
        if (result == ContainerEval::False) {
          return ContainerEval::False;
        }
        if (result == ContainerEval::Unknown) {
          unknown = true;
        }
      }
      return unknown ? ContainerEval::Unknown : ContainerEval::True;
    }

    case eOr: {
      bool unknown = false;
      for (const RefPtr<nsContainerCondition>& operand : mOperands) {
        ContainerEval result = operand->Matches(aEvaluator);
        if (result == ContainerEval::True) {
          return ContainerEval::True;
        }
        if (result == ContainerEval::Unknown) {
          unknown = true;
        }
      }
      return unknown ? ContainerEval::Unknown : ContainerEval::False;
    }
  }
  MOZ_ASSERT_UNREACHABLE("unexpected kind");
  return ContainerEval::False;
}

uint32_t
nsContainerCondition::CountLeaves() const
{
  if (mKind == eFeature || mKind == eStyle) {
    return 1;
  }
  uint32_t count = 0;
  for (const RefPtr<nsContainerCondition>& operand : mOperands) {
    count += operand->CountLeaves();
  }
  return count;
}

static void
AppendConditionToString(nsAString& aString,
                        const nsContainerCondition& aCondition,
                        uint8_t aParentPrecedence, bool aRightChild)
{
  const nsContainerCondition::Kind kind = aCondition.GetKind();
  const uint8_t precedence = ConditionPrecedence(kind);
  bool parenthesize;
  if (kind == nsContainerCondition::eStyle) {
    parenthesize = false;          // style() carries its own parentheses
  } else if (kind == nsContainerCondition::eFeature) {
    parenthesize = true;           // <query-in-parens>
  } else {
    parenthesize = precedence < aParentPrecedence ||
                   (precedence == aParentPrecedence && aRightChild);
  }

  if (parenthesize) {
    aString.Append('(');
  }

  switch (aCondition.GetKind()) {
    case nsContainerCondition::eFeature:
      AppendMediaExpressionToString(aString, aCondition.GetExpression());
      break;

    case nsContainerCondition::eStyle:
      aString.Append(aCondition.GetStyleText());
      break;

    case nsContainerCondition::eNot:
      aString.AppendLiteral("not ");
      AppendConditionToString(aString, *aCondition.GetOperands()[0],
                              precedence, true);
      break;

    case nsContainerCondition::eAnd:
    case nsContainerCondition::eOr: {
      const bool isAnd = aCondition.GetKind() == nsContainerCondition::eAnd;
      const nsTArray<RefPtr<nsContainerCondition>>& operands =
        aCondition.GetOperands();
      for (uint32_t i = 0; i < operands.Length(); ++i) {
        if (i > 0) {
          if (isAnd) {
            aString.AppendLiteral(" and ");
          } else {
            aString.AppendLiteral(" or ");
          }
        }
        AppendConditionToString(aString, *operands[i], precedence, i > 0);
      }
      break;
    }
  }

  if (parenthesize) {
    aString.Append(')');
  }
}

void
nsContainerCondition::AppendToString(nsAString& aString) const
{
  AppendConditionToString(aString, *this, 0, false);
}

size_t
nsContainerCondition::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = mOperands.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (const RefPtr<nsContainerCondition>& operand : mOperands) {
    n += operand->SizeOfIncludingThis(aMallocSizeOf);
  }
  n += mStyle.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  n += mStyleQuery.mName.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  n += mStyleQuery.mValueText.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  return n;
}