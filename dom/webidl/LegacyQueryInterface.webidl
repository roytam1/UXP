/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface nsISupports;
interface IID;

[Exposed=Window]
interface mixin LegacyQueryInterface {
  // Legacy QueryInterface, only exposed to chrome or XBL code on the
  // main thread.
  [Exposed=Window]
  nsISupports queryInterface(IID iid);
};

Attr includes LegacyQueryInterface;
BarProp includes LegacyQueryInterface;
BoxObject includes LegacyQueryInterface;
CaretPosition includes LegacyQueryInterface;
Comment includes LegacyQueryInterface;
Crypto includes LegacyQueryInterface;
CSSMozDocumentRule includes LegacyQueryInterface;
CSSPrimitiveValue includes LegacyQueryInterface;
CSSStyleDeclaration includes LegacyQueryInterface;
CSSStyleRule includes LegacyQueryInterface;
CSSValueList includes LegacyQueryInterface;
DOMImplementation includes LegacyQueryInterface;
DOMParser includes LegacyQueryInterface;
DOMStringMap includes LegacyQueryInterface;
DOMTokenList includes LegacyQueryInterface;
Document includes LegacyQueryInterface;
DocumentFragment includes LegacyQueryInterface;
DocumentType includes LegacyQueryInterface;
Element includes LegacyQueryInterface;
Event includes LegacyQueryInterface;
EventSource includes LegacyQueryInterface;
FileList includes LegacyQueryInterface;
FormData includes LegacyQueryInterface;
HTMLCollection includes LegacyQueryInterface;
History includes LegacyQueryInterface;
MimeTypeArray includes LegacyQueryInterface;
NamedNodeMap includes LegacyQueryInterface;
MutationObserver includes LegacyQueryInterface;
MutationRecord includes LegacyQueryInterface;
Navigator includes LegacyQueryInterface;
NodeIterator includes LegacyQueryInterface;
NodeList includes LegacyQueryInterface;
Notification includes LegacyQueryInterface;
OfflineResourceList includes LegacyQueryInterface;
PaintRequest includes LegacyQueryInterface;
PaintRequestList includes LegacyQueryInterface;
Performance includes LegacyQueryInterface;
Plugin includes LegacyQueryInterface;
PluginArray includes LegacyQueryInterface;
ProcessingInstruction includes LegacyQueryInterface;
Range includes LegacyQueryInterface;
Rect includes LegacyQueryInterface;
Selection includes LegacyQueryInterface;
SVGAnimatedEnumeration includes LegacyQueryInterface;
SVGAnimatedInteger includes LegacyQueryInterface;
SVGAnimatedNumber includes LegacyQueryInterface;
SVGAnimatedNumberList includes LegacyQueryInterface;
SVGAnimatedPreserveAspectRatio includes LegacyQueryInterface;
SVGAnimatedString includes LegacyQueryInterface;
SVGLengthList includes LegacyQueryInterface;
SVGNumberList includes LegacyQueryInterface;
SVGPathSegList includes LegacyQueryInterface;
SVGPoint includes LegacyQueryInterface;
SVGPointList includes LegacyQueryInterface;
SVGPreserveAspectRatio includes LegacyQueryInterface;
SVGRect includes LegacyQueryInterface;
SVGStringList includes LegacyQueryInterface;
SVGTransformList includes LegacyQueryInterface;
Screen includes LegacyQueryInterface;
StyleSheet includes LegacyQueryInterface;
Text includes LegacyQueryInterface;
Touch includes LegacyQueryInterface;
TouchList includes LegacyQueryInterface;
TreeColumns includes LegacyQueryInterface;
TreeWalker includes LegacyQueryInterface;
ValidityState includes LegacyQueryInterface;
WebSocket includes LegacyQueryInterface;
Window includes LegacyQueryInterface;
XMLHttpRequest includes LegacyQueryInterface;
XMLHttpRequestUpload includes LegacyQueryInterface;
XMLSerializer includes LegacyQueryInterface;
XPathEvaluator includes LegacyQueryInterface;
