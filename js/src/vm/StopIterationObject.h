/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_StopIterationObject_h
#define vm_StopIterationObject_h

#include "jsobj.h"

namespace js {

class StopIterationObject : public JSObject
{
  public:
    static const Class class_;
};

} // namespace js

#endif /* vm_StopIterationObject_h */
