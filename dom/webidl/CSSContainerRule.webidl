/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://drafts.csswg.org/css-conditional-5/#csscontainerrule
 */
[Exposed=Window, Pref="layout.css.container-queries.enabled"]
interface CSSContainerRule : CSSConditionRule {
  readonly attribute DOMString containerName;
  readonly attribute DOMString containerQuery;
};