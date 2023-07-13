/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/BigIntType.h"

#include "mozilla/FloatingPoint.h"
#include "mozilla/HashFunctions.h"

#include "jsapi.h"
#include "jscntxt.h"

#include "builtin/BigInt.h"
#include "gc/Allocator.h"
#include "gc/Tracer.h"
#include "vm/SelfHosting.h"

using namespace js;

BigInt*
BigInt::create(ExclusiveContext* cx)
{
    BigInt* x = Allocate<BigInt>(cx);
    if (!x)
        return nullptr;
    return x;
}

BigInt*
BigInt::create(ExclusiveContext* cx, double d)
{
    return nullptr;
}

// BigInt proposal section 5.1.1
static bool
IsInteger(double d)
{
    // Step 1 is an assertion checked by the caller.
    // Step 2.
    if (!mozilla::IsFinite(d))
        return false;

    // Step 3.
    double i = JS::ToInteger(d);

    // Step 4.
    if (i != d)
        return false;

    // Step 5.
    return true;
}

// BigInt proposal section 5.1.2
BigInt*
js::NumberToBigInt(ExclusiveContext* cx, double d)
{
    // Step 1 is an assertion checked by the caller.
    // Step 2.
    if (!IsInteger(d)) {
        if(cx->isJSContext()) {
            JS_ReportErrorNumberASCII(cx->asJSContext(), GetErrorMessage, nullptr,
                                      JSMSG_NUMBER_TO_BIGINT);
        }
        return nullptr;
    }

    // Step 3.
    return BigInt::create(cx, d);
}

BigInt*
BigInt::copy(ExclusiveContext* cx, HandleBigInt x)
{
    BigInt* bi = create(cx);
    if (!bi)
        return nullptr;
    return bi;
}

// BigInt proposal section 7.3
BigInt*
js::ToBigInt(ExclusiveContext* cx, HandleValue val)
{
    RootedValue v(cx, val);

    if(cx->isJSContext()) {
        // Step 1.
        if (!ToPrimitive(cx->asJSContext(), JSTYPE_NUMBER, &v))
        return nullptr;

        // Step 2.
        // Boolean and string conversions are not yet supported.
        if (v.isBigInt())
            return v.toBigInt();

        JS_ReportErrorNumberASCII(cx->asJSContext(), GetErrorMessage, nullptr, JSMSG_NOT_BIGINT);
    }   
    return nullptr;
}

JSLinearString*
BigInt::toString(ExclusiveContext* cx, BigInt* x, uint8_t radix)
{
    return nullptr;
}

void
BigInt::finalize(js::FreeOp* fop)
{
    return;
}

JSAtom*
js::BigIntToAtom(ExclusiveContext* cx, BigInt* bi)
{
    JSString* str = BigInt::toString(cx, bi, 10);
    if (!str)
        return nullptr;
    return AtomizeString(cx, str);
}

bool
BigInt::toBoolean()
{
    return false;
}

js::HashNumber
BigInt::hash()
{
    return 0;
}

size_t
BigInt::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    return 0;
}

JS::ubi::Node::Size
JS::ubi::Concrete<BigInt>::size(mozilla::MallocSizeOf mallocSizeOf) const
{
    MOZ_ASSERT(get().isTenured());
    return js::gc::Arena::thingSize(get().asTenured().getAllocKind());
}
