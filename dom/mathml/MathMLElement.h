/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MathMLElement_h_
#define mozilla_dom_MathMLElement_h_

#include "mozilla/Attributes.h"
#include "mozilla/dom/ElementInlines.h"
#include "nsMappedAttributeElement.h"
#include "nsIDOMElement.h"
#include "Link.h"
#include "mozilla/dom/DOMRect.h"

class nsCSSValue;

namespace mozilla {
class EventChainPostVisitor;
class EventChainPreVisitor;
namespace dom {

typedef nsMappedAttributeElement MathMLElementBase;

/*
 * The base class for MathML elements.
 */
class MathMLElement final : public MathMLElementBase,
                            public nsIDOMElement,
                            public mozilla::dom::Link
{
public:
  explicit MathMLElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  explicit MathMLElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  // Implementation of nsISupports is inherited from MathMLElementBase
  NS_DECL_ISUPPORTS_INHERITED

  NS_IMPL_FROMCONTENT(MathMLElement, kNameSpaceID_MathML)

  // Forward implementations of parent interfaces of MathMLElement to 
  // our base class
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                      nsIContent* aBindingParent,
                      bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult) override;

  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const override;

  enum {
    PARSE_ALLOW_UNITLESS = 0x01, // unitless 0 will be turned into 0px
    PARSE_ALLOW_NEGATIVE = 0x02,
    PARSE_SUPPRESS_WARNINGS = 0x04,
    CONVERT_UNITLESS_TO_PERCENT = 0x08
  };
  static bool ParseNamedSpaceValue(const nsString& aString,
                                   nsCSSValue&     aCSSValue,
                                   uint32_t        aFlags);

  static bool ParseNumericValue(const nsString& aString,
                                nsCSSValue&     aCSSValue,
                                uint32_t        aFlags,
                                nsIDocument*    aDocument);

  static void MapMathMLAttributesInto(const nsMappedAttributes* aAttributes, 
                                      nsRuleData* aRuleData);
  
  virtual nsresult GetEventTargetParent(
                     mozilla::EventChainPreVisitor& aVisitor) override;
  virtual nsresult PostHandleEvent(
                     mozilla::EventChainPostVisitor& aVisitor) override;
  nsresult Clone(mozilla::dom::NodeInfo*, nsINode**) const override;
  virtual mozilla::EventStates IntrinsicState() const override;
  virtual bool IsNodeOfType(uint32_t aFlags) const override;

  // Set during reflow as necessary. Does a style change notification,
  // aNotify must be true.
  void SetIncrementScriptLevel(bool aIncrementScriptLevel, bool aNotify);
  bool GetIncrementScriptLevel() const {
    return mIncrementScriptLevel;
  }

  int32_t TabIndexDefault() final;
  virtual bool IsFocusableInternal(int32_t* aTabIndex, bool aWithMouse) override;
  virtual bool IsLink(nsIURI** aURI) const override;
  virtual void GetLinkTarget(nsAString& aTarget) override;
  virtual already_AddRefed<nsIURI> GetHrefURI() const override;

  virtual nsIDOMNode* AsDOMNode() override { return this; }

  void RecompileScriptEventListeners() override;
  bool IsEventAttributeNameInternal(nsIAtom* aName);

protected:
  virtual ~MathMLElement() {}

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsresult BeforeSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                         const nsAttrValueOrString* aValue, bool aNotify) override;
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;

private:
  bool mIncrementScriptLevel;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MathMLElement_h_
