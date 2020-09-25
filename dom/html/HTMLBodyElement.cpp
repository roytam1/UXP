/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLBodyElement.h"
#include "mozilla/dom/HTMLBodyElementBinding.h"
#include "nsAttrValueInlines.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsHTMLStyleSheet.h"
#include "nsIEditor.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"
#include "nsIDocShell.h"
#include "nsRuleWalker.h"
#include "nsGlobalWindow.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Body)

namespace mozilla {
namespace dom {

//----------------------------------------------------------------------

HTMLBodyElement::~HTMLBodyElement()
{
}

JSObject*
HTMLBodyElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLBodyElementBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_ISUPPORTS_INHERITED(HTMLBodyElement, nsGenericHTMLElement,
                            nsIDOMHTMLBodyElement)

NS_IMPL_ELEMENT_CLONE(HTMLBodyElement)

NS_IMETHODIMP 
HTMLBodyElement::SetBackground(const nsAString& aBackground)
{
  ErrorResult rv;
  SetBackground(aBackground, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLBodyElement::GetBackground(nsAString& aBackground)
{
  DOMString background;
  GetBackground(background);
  background.ToString(aBackground);
  return NS_OK;
}

NS_IMETHODIMP 
HTMLBodyElement::SetVLink(const nsAString& aVLink)
{
  ErrorResult rv;
  SetVLink(aVLink, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLBodyElement::GetVLink(nsAString& aVLink)
{
  DOMString vLink;
  GetVLink(vLink);
  vLink.ToString(aVLink);
  return NS_OK;
}

NS_IMETHODIMP 
HTMLBodyElement::SetALink(const nsAString& aALink)
{
  ErrorResult rv;
  SetALink(aALink, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLBodyElement::GetALink(nsAString& aALink)
{
  DOMString aLink;
  GetALink(aLink);
  aLink.ToString(aALink);
  return NS_OK;
}

NS_IMETHODIMP 
HTMLBodyElement::SetLink(const nsAString& aLink)
{
  ErrorResult rv;
  SetLink(aLink, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLBodyElement::GetLink(nsAString& aLink)
{
  DOMString link;
  GetLink(link);
  link.ToString(aLink);
  return NS_OK;
}

NS_IMETHODIMP 
HTMLBodyElement::SetText(const nsAString& aText)
{
  ErrorResult rv;
  SetText(aText, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLBodyElement::GetText(nsAString& aText)
{
  DOMString text;
  GetText(text);
  text.ToString(aText);
  return NS_OK;
}

NS_IMETHODIMP 
HTMLBodyElement::SetBgColor(const nsAString& aBgColor)
{
  ErrorResult rv;
  SetBgColor(aBgColor, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLBodyElement::GetBgColor(nsAString& aBgColor)
{
  DOMString bgColor;
  GetBgColor(bgColor);
  bgColor.ToString(aBgColor);
  return NS_OK;
}

bool
HTMLBodyElement::ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::bgcolor ||
        aAttribute == nsGkAtoms::text ||
        aAttribute == nsGkAtoms::link ||
        aAttribute == nsGkAtoms::alink ||
        aAttribute == nsGkAtoms::vlink) {
      return aResult.ParseColor(aValue);
    }
    if (aAttribute == nsGkAtoms::marginwidth ||
        aAttribute == nsGkAtoms::marginheight ||
        aAttribute == nsGkAtoms::topmargin ||
        aAttribute == nsGkAtoms::bottommargin ||
        aAttribute == nsGkAtoms::leftmargin ||
        aAttribute == nsGkAtoms::rightmargin) {
      return aResult.ParseIntWithBounds(aValue, 0);
    }
  }

  return nsGenericHTMLElement::ParseBackgroundAttribute(aNamespaceID,
                                                        aAttribute, aValue,
                                                        aResult) ||
         nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

void
HTMLBodyElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                       nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Display)) {
    // When display if first asked for, go ahead and get our colors set up.
    nsIPresShell *presShell = aData->mPresContext->GetPresShell();
    if (presShell) {
      nsIDocument *doc = presShell->GetDocument();
      if (doc) {
        nsHTMLStyleSheet* styleSheet = doc->GetAttributeStyleSheet();
        if (styleSheet) {
          const nsAttrValue* value;
          nscolor color;
          value = aAttributes->GetAttr(nsGkAtoms::link);
          if (value && value->GetColorValue(color)) {
            styleSheet->SetLinkColor(color);
          }

          value = aAttributes->GetAttr(nsGkAtoms::alink);
          if (value && value->GetColorValue(color)) {
            styleSheet->SetActiveLinkColor(color);
          }

          value = aAttributes->GetAttr(nsGkAtoms::vlink);
          if (value && value->GetColorValue(color)) {
            styleSheet->SetVisitedLinkColor(color);
          }
        }
      }
    }
  }

  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Color)) {
    nsCSSValue *colorValue = aData->ValueForColor();
    if (colorValue->GetUnit() == eCSSUnit_Null &&
        aData->mPresContext->UseDocumentColors()) {
      // color: color
      nscolor color;
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::text);
      if (value && value->GetColorValue(color))
        colorValue->SetColorValue(color);
    }
  }

  nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

nsMapRuleToAttributesFunc
HTMLBodyElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

NS_IMETHODIMP_(bool)
HTMLBodyElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::link },
    { &nsGkAtoms::vlink },
    { &nsGkAtoms::alink },
    { &nsGkAtoms::text },
    { &nsGkAtoms::marginwidth },
    { &nsGkAtoms::marginheight },
    { &nsGkAtoms::topmargin },
    { &nsGkAtoms::rightmargin },
    { &nsGkAtoms::bottommargin },
    { &nsGkAtoms::leftmargin },
    { nullptr },
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
    sBackgroundAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}

already_AddRefed<nsIEditor>
HTMLBodyElement::GetAssociatedEditor()
{
  nsCOMPtr<nsIEditor> editor = GetEditorInternal();
  if (editor) {
    return editor.forget();
  }

  // Make sure this is the actual body of the document
  if (!IsCurrentBodyElement()) {
    return nullptr;
  }

  // For designmode, try to get document's editor
  nsPresContext* presContext = GetPresContext(eForComposedDoc);
  if (!presContext) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShell> docShell = presContext->GetDocShell();
  if (!docShell) {
    return nullptr;
  }

  docShell->GetEditor(getter_AddRefs(editor));
  return editor.forget();
}

bool
HTMLBodyElement::IsEventAttributeName(nsIAtom *aName)
{
  return nsContentUtils::IsEventAttributeName(aName,
                                              EventNameType_HTML |
                                              EventNameType_HTMLBodyOrFramesetOnly);
}

nsresult
HTMLBodyElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                            nsIContent* aBindingParent,
                            bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);
  return mAttrsAndChildren.ForceMapped(this, OwnerDoc());
}

nsresult
HTMLBodyElement::AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                              const nsAttrValue* aValue,
                              const nsAttrValue* aOldValue, bool aNotify)
{
  nsresult rv = nsGenericHTMLElement::AfterSetAttr(aNameSpaceID,
                                                   aName, aValue, aOldValue,
                                                   aNotify);
  NS_ENSURE_SUCCESS(rv, rv);
  // if the last mapped attribute was removed, don't clear the
  // nsMappedAttributes, our style can still depend on the containing frame element
  if (!aValue && IsAttributeMapped(aName)) {
    nsresult rv = mAttrsAndChildren.ForceMapped(this, OwnerDoc());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

#define EVENT(name_, id_, type_, struct_) /* nothing; handled by the superclass */
// nsGenericHTMLElement::GetOnError returns
// already_AddRefed<EventHandlerNonNull> while other getters return
// EventHandlerNonNull*, so allow passing in the type to use here.
#define WINDOW_EVENT_HELPER(name_, type_)                                      \
  type_*                                                                       \
  HTMLBodyElement::GetOn##name_()                                              \
  {                                                                            \
    if (nsPIDOMWindowInner* win = OwnerDoc()->GetInnerWindow()) {              \
      nsGlobalWindow* globalWin = nsGlobalWindow::Cast(win);                   \
      return globalWin->GetOn##name_();                                        \
    }                                                                          \
    return nullptr;                                                            \
  }                                                                            \
  void                                                                         \
  HTMLBodyElement::SetOn##name_(type_* handler)                                \
  {                                                                            \
    nsPIDOMWindowInner* win = OwnerDoc()->GetInnerWindow();                    \
    if (!win) {                                                                \
      return;                                                                  \
    }                                                                          \
                                                                               \
    nsGlobalWindow* globalWin = nsGlobalWindow::Cast(win);                     \
    return globalWin->SetOn##name_(handler);                                   \
  }
#define WINDOW_EVENT(name_, id_, type_, struct_)                               \
  WINDOW_EVENT_HELPER(name_, EventHandlerNonNull)
#define BEFOREUNLOAD_EVENT(name_, id_, type_, struct_)                         \
  WINDOW_EVENT_HELPER(name_, OnBeforeUnloadEventHandlerNonNull)
#include "mozilla/EventNameList.h" // IWYU pragma: keep
#undef BEFOREUNLOAD_EVENT
#undef WINDOW_EVENT
#undef WINDOW_EVENT_HELPER
#undef EVENT

} // namespace dom
} // namespace mozilla
