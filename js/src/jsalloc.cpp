/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsalloc.h"

#include "jscntxt.h"

using namespace js;

void*
TempAllocPolicy::onOutOfMemory(AllocFunction allocFunc, size_t nbytes, void* reallocPtr)
{
    return static_cast<ExclusiveContext*>(cx_)->onOutOfMemory(allocFunc, nbytes, reallocPtr);
}

void
TempAllocPolicy::reportAllocOverflow() const
{
    ReportAllocationOverflow(static_cast<ExclusiveContext*>(cx_));
}
