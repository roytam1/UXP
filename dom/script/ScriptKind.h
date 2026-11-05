/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScriptKind_h
#define mozilla_dom_ScriptKind_h

namespace mozilla {
namespace dom {

//////////////////////////////////////////////////////////////
// Per-request data structure
//////////////////////////////////////////////////////////////

enum class ScriptKind {
  Classic,
  Module,
  ImportMap
};

} // namespace dom
} // namespace mozilla

#endif