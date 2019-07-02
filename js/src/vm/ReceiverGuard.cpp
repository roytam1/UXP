/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ReceiverGuard.h"

#include "builtin/TypedObject.h"
#include "jsobjinlines.h"

using namespace js;

ReceiverGuard::ReceiverGuard(JSObject* obj)
  : group(nullptr), shape(nullptr)
{
    if (obj) {
        if (obj->is<TypedObject>()) {
            group = obj->group();
        } else {
            shape = obj->maybeShape();
        }
    }
}

ReceiverGuard::ReceiverGuard(ObjectGroup* group, Shape* shape)
  : group(group), shape(shape)
{
    if (group) {
        const Class* clasp = group->clasp();
        if (IsTypedObjectClass(clasp)) {
            this->shape = nullptr;
        } else {
            this->group = nullptr;
        }
    }
}

/* static */ int32_t
HeapReceiverGuard::keyBits(JSObject* obj)
{
    if (obj->is<TypedObject>()) {
        // Only the group needs to be guarded for typed objects.
        return 2;
    }
    // Other objects only need the shape to be guarded.
    return 3;
}

void
HeapReceiverGuard::trace(JSTracer* trc)
{
    TraceNullableEdge(trc, &shape_, "receiver_guard_shape");
    TraceNullableEdge(trc, &group_, "receiver_guard_group");
}
