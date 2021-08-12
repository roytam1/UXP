/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EventStates.h"
#include "mozilla/dom/HTMLFormSubmission.h"
#include "mozilla/dom/HTMLObjectElement.h"
#include "mozilla/dom/HTMLObjectElementBinding.h"
#include "mozilla/dom/ElementInlines.h"
#include "nsAttrValueInlines.h"
#include "nsGkAtoms.h"
#include "nsError.h"
#include "nsIDocument.h"
#include "nsIPluginDocument.h"
#include "nsIDOMDocument.h"
#include "nsIObjectFrame.h"
#include "nsNPAPIPluginInstance.h"
#include "nsIWidget.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

HTMLObjectElement::HTMLObjectElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                                     FromParser aFromParser)
  : nsGenericHTMLFormElement(aNodeInfo),
    mIsDoneAddingChildren(!aFromParser)
{
  RegisterActivityObserver();
  SetIsNetworkCreated(aFromParser == FROM_PARSER_NETWORK);

  // <object> is always barred from constraint validation.
  SetBarredFromConstraintValidation(true);

  // By default we're in the loading state
  AddStatesSilently(NS_EVENT_STATE_LOADING);
}

HTMLObjectElement::~HTMLObjectElement()
{
  UnregisterActivityObserver();
  DestroyImageLoadingContent();
}

bool
HTMLObjectElement::IsInteractiveHTMLContent(bool aIgnoreTabindex) const
{
  return HasAttr(kNameSpaceID_None, nsGkAtoms::usemap) ||
         nsGenericHTMLFormElement::IsInteractiveHTMLContent(aIgnoreTabindex);
}

void
HTMLObjectElement::AsyncEventRunning(AsyncEventDispatcher* aEvent)
{
  nsImageLoadingContent::AsyncEventRunning(aEvent);
}

bool
HTMLObjectElement::IsDoneAddingChildren()
{
  return mIsDoneAddingChildren;
}

void
HTMLObjectElement::DoneAddingChildren(bool aHaveNotified)
{
  mIsDoneAddingChildren = true;

  // If we're already in a document, we need to trigger the load
  // Otherwise, BindToTree takes care of that.
  if (IsInComposedDoc()) {
    StartObjectLoad(aHaveNotified);
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLObjectElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLObjectElement,
                                                  nsGenericHTMLFormElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mValidity)
  nsObjectLoadingContent::Traverse(tmp, cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLObjectElement,
                                                nsGenericHTMLFormElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mValidity)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(HTMLObjectElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLObjectElement, Element)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLObjectElement)
  NS_INTERFACE_TABLE_INHERITED(HTMLObjectElement,
                               nsIDOMHTMLObjectElement,
                               imgINotificationObserver,
                               nsIRequestObserver,
                               nsIStreamListener,
                               nsIFrameLoaderOwner,
                               nsIObjectLoadingContent,
                               nsIImageLoadingContent,
                               imgIOnloadBlocker,
                               nsIChannelEventSink,
                               nsIConstraintValidation)
NS_INTERFACE_TABLE_TAIL_INHERITING(nsGenericHTMLFormElement)

NS_IMPL_ELEMENT_CLONE(HTMLObjectElement)

// nsIConstraintValidation
NS_IMPL_NSICONSTRAINTVALIDATION(HTMLObjectElement)

NS_IMETHODIMP
HTMLObjectElement::GetForm(nsIDOMHTMLFormElement **aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}

nsresult
HTMLObjectElement::BindToTree(nsIDocument *aDocument,
                              nsIContent *aParent,
                              nsIContent *aBindingParent,
                              bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLFormElement::BindToTree(aDocument, aParent,
                                                     aBindingParent,
                                                     aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsObjectLoadingContent::BindToTree(aDocument, aParent,
                                          aBindingParent,
                                          aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  // Don't kick off load from being bound to a plugin document - the plugin
  // document will call nsObjectLoadingContent::InitializeFromChannel() for the
  // initial load.
  nsCOMPtr<nsIPluginDocument> pluginDoc = do_QueryInterface(aDocument);

  // If we already have all the children, start the load.
  if (mIsDoneAddingChildren && !pluginDoc) {
    void (HTMLObjectElement::*start)() = &HTMLObjectElement::StartObjectLoad;
    nsContentUtils::AddScriptRunner(NewRunnableMethod(this, start));
  }

  return NS_OK;
}

void
HTMLObjectElement::UnbindFromTree(bool aDeep,
                                  bool aNullParent)
{
  nsObjectLoadingContent::UnbindFromTree(aDeep, aNullParent);
  nsGenericHTMLFormElement::UnbindFromTree(aDeep, aNullParent);
}



nsresult
HTMLObjectElement::AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue, bool aNotify)
{
  nsresult rv = AfterMaybeChangeAttr(aNamespaceID, aName, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  return nsGenericHTMLFormElement::AfterSetAttr(aNamespaceID, aName, aValue,
                                                aOldValue, aNotify);
}

nsresult
HTMLObjectElement::OnAttrSetButNotChanged(int32_t aNamespaceID, nsIAtom* aName,
                                          const nsAttrValueOrString& aValue,
                                          bool aNotify)
{
  nsresult rv = AfterMaybeChangeAttr(aNamespaceID, aName, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  return nsGenericHTMLFormElement::OnAttrSetButNotChanged(aNamespaceID, aName,
                                                          aValue, aNotify);
}

nsresult
HTMLObjectElement::AfterMaybeChangeAttr(int32_t aNamespaceID, nsIAtom* aName,
                                        bool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None) {
    // if aNotify is false, we are coming from the parser or some such place;
    // we'll get bound after all the attributes have been set, so we'll do the
    // object load from BindToTree/DoneAddingChildren.
    // Skip the LoadObject call in that case.
    // We also don't want to start loading the object when we're not yet in
    // a document, just in case that the caller wants to set additional
    // attributes before inserting the node into the document.
    if (aNotify && IsInComposedDoc() && mIsDoneAddingChildren &&
        aName == nsGkAtoms::data) {
      nsContentUtils::AddScriptRunner(NS_NewRunnableFunction(
          [self = RefPtr<HTMLObjectElement>(this), aNotify]() {
            if (self->IsInComposedDoc()) {
              self->LoadObject(aNotify, true);
            }
          }));
      return NS_OK;
    }
  }

  return NS_OK;
}

bool
HTMLObjectElement::IsFocusableForTabIndex()
{
  nsIDocument* doc = GetComposedDoc();
  if (!doc || doc->HasFlag(NODE_IS_EDITABLE)) {
    return false;
  }

  return IsEditableRoot() ||
         (Type() == eType_Document &&
          nsContentUtils::IsSubDocumentTabbable(this));
}

bool
HTMLObjectElement::IsHTMLFocusable(bool aWithMouse,
                                   bool *aIsFocusable, int32_t *aTabIndex)
{
  // TODO: this should probably be managed directly by IsHTMLFocusable.
  // See bug 597242.
  nsIDocument *doc = GetComposedDoc();
  if (!doc || doc->HasFlag(NODE_IS_EDITABLE)) {
    if (aTabIndex) {
      GetTabIndex(aTabIndex);
    }

    *aIsFocusable = false;

    return false;
  }

  // This method doesn't call nsGenericHTMLFormElement intentionally.
  // TODO: It should probably be changed when bug 597242 will be fixed.
  if (Type() == eType_Plugin || IsEditableRoot() ||
      (Type() == eType_Document && nsContentUtils::IsSubDocumentTabbable(this))) {
    // Has plugin content: let the plugin decide what to do in terms of
    // internal focus from mouse clicks
    if (aTabIndex) {
      GetTabIndex(aTabIndex);
    }

    *aIsFocusable = true;

    return false;
  }

  // TODO: this should probably be managed directly by IsHTMLFocusable.
  // See bug 597242.
  const nsAttrValue* attrVal = mAttrsAndChildren.GetAttr(nsGkAtoms::tabindex);

  *aIsFocusable = attrVal && attrVal->Type() == nsAttrValue::eInteger;

  if (aTabIndex && *aIsFocusable) {
    *aTabIndex = attrVal->GetIntegerValue();
  }

  return false;
}

nsIContent::IMEState
HTMLObjectElement::GetDesiredIMEState()
{
  if (Type() == eType_Plugin) {
    return IMEState(IMEState::PLUGIN);
  }
   
  return nsGenericHTMLFormElement::GetDesiredIMEState();
}

NS_IMETHODIMP
HTMLObjectElement::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP
HTMLObjectElement::SubmitNamesValues(HTMLFormSubmission *aFormSubmission)
{
  nsAutoString name;
  if (!GetAttr(kNameSpaceID_None, nsGkAtoms::name, name)) {
    // No name, don't submit.

    return NS_OK;
  }

  nsIFrame* frame = GetPrimaryFrame();

  nsIObjectFrame *objFrame = do_QueryFrame(frame);
  if (!objFrame) {
    // No frame, nothing to submit.

    return NS_OK;
  }

  RefPtr<nsNPAPIPluginInstance> pi;
  objFrame->GetPluginInstance(getter_AddRefs(pi));
  if (!pi)
    return NS_OK;

  nsAutoString value;
  nsresult rv = pi->GetFormValue(value);
  NS_ENSURE_SUCCESS(rv, rv);

  return aFormSubmission->AddNameValuePair(name, value);
}

NS_IMPL_STRING_ATTR(HTMLObjectElement, Align, align)
NS_IMPL_STRING_ATTR(HTMLObjectElement, Archive, archive)
NS_IMPL_STRING_ATTR(HTMLObjectElement, Border, border)
NS_IMPL_STRING_ATTR(HTMLObjectElement, Code, code)
NS_IMPL_URI_ATTR(HTMLObjectElement, CodeBase, codebase)
NS_IMPL_STRING_ATTR(HTMLObjectElement, CodeType, codetype)
NS_IMPL_URI_ATTR_WITH_BASE(HTMLObjectElement, Data, data, codebase)
NS_IMPL_BOOL_ATTR(HTMLObjectElement, Declare, declare)
NS_IMPL_STRING_ATTR(HTMLObjectElement, Height, height)
NS_IMPL_INT_ATTR(HTMLObjectElement, Hspace, hspace)
NS_IMPL_STRING_ATTR(HTMLObjectElement, Name, name)
NS_IMPL_STRING_ATTR(HTMLObjectElement, Standby, standby)
NS_IMPL_STRING_ATTR(HTMLObjectElement, Type, type)
NS_IMPL_STRING_ATTR(HTMLObjectElement, UseMap, usemap)
NS_IMPL_INT_ATTR(HTMLObjectElement, Vspace, vspace)
NS_IMPL_STRING_ATTR(HTMLObjectElement, Width, width)

int32_t
HTMLObjectElement::TabIndexDefault()
{
  return IsFocusableForTabIndex() ? 0 : -1;
}

NS_IMETHODIMP
HTMLObjectElement::GetContentDocument(nsIDOMDocument **aContentDocument)
{
  NS_ENSURE_ARG_POINTER(aContentDocument);

  nsCOMPtr<nsIDOMDocument> domDoc =
    do_QueryInterface(GetContentDocument(*nsContentUtils::SubjectPrincipal()));
  domDoc.forget(aContentDocument);
  return NS_OK;
}

nsPIDOMWindowOuter*
HTMLObjectElement::GetContentWindow(nsIPrincipal& aSubjectPrincipal)
{
  nsIDocument* doc = GetContentDocument(aSubjectPrincipal);
  if (doc) {
    return doc->GetWindow();
  }

  return nullptr;
}

bool
HTMLObjectElement::ParseAttribute(int32_t aNamespaceID,
                                  nsIAtom *aAttribute,
                                  const nsAString &aValue,
                                  nsAttrValue &aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::align) {
      return ParseAlignValue(aValue, aResult);
    }
    if (ParseImageAttribute(aAttribute, aValue, aResult)) {
      return true;
    }
  }

  return nsGenericHTMLFormElement::ParseAttribute(aNamespaceID, aAttribute,
                                                  aValue, aResult);
}

void
HTMLObjectElement::MapAttributesIntoRule(const nsMappedAttributes *aAttributes,
                                         nsRuleData *aData)
{
  nsGenericHTMLFormElement::MapImageAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLFormElement::MapImageBorderAttributeInto(aAttributes, aData);
  nsGenericHTMLFormElement::MapImageMarginAttributeInto(aAttributes, aData);
  nsGenericHTMLFormElement::MapImageSizeAttributesInto(aAttributes, aData);
  nsGenericHTMLFormElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
HTMLObjectElement::IsAttributeMapped(const nsIAtom *aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
    sCommonAttributeMap,
    sImageMarginSizeAttributeMap,
    sImageBorderAttributeMap,
    sImageAlignAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}


nsMapRuleToAttributesFunc
HTMLObjectElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

void
HTMLObjectElement::StartObjectLoad(bool aNotify)
{
  // BindToTree can call us asynchronously, and we may be removed from the tree
  // in the interim
  if (!IsInComposedDoc() || !OwnerDoc()->IsActive()) {
    return;
  }

  LoadObject(aNotify);
  SetIsNetworkCreated(false);
}

EventStates
HTMLObjectElement::IntrinsicState() const
{
  return nsGenericHTMLFormElement::IntrinsicState() | ObjectState();
}

uint32_t
HTMLObjectElement::GetCapabilities() const
{
  return nsObjectLoadingContent::GetCapabilities() | eSupportClassID;
}

void
HTMLObjectElement::DestroyContent()
{
  nsObjectLoadingContent::DestroyContent();
  nsGenericHTMLFormElement::DestroyContent();
}

nsresult
HTMLObjectElement::CopyInnerTo(Element* aDest)
{
  nsresult rv = nsGenericHTMLFormElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDest->OwnerDoc()->IsStaticDocument()) {
    CreateStaticClone(static_cast<HTMLObjectElement*>(aDest));
  }

  return rv;
}

JSObject*
HTMLObjectElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  JS::Rooted<JSObject*> obj(aCx,
    HTMLObjectElementBinding::Wrap(aCx, this, aGivenProto));
  if (!obj) {
    return nullptr;
  }
  SetupProtoChain(aCx, obj);
  return obj;
}

} // namespace dom
} // namespace mozilla

NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Object)
