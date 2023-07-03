/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Operations used to implement multiple Intl.* classes. */

#include "builtin/intl/CommonFunctions.h"

#include "mozilla/Assertions.h"

#include "jscntxt.h"
#include "jsfriendapi.h" // for GetErrorMessage, JSMSG_INTERNAL_INTL_ERROR
#include "jsobj.h"

#include "js/Value.h"
#include "vm/SelfHosting.h"
#include "vm/Stack.h"

#include "jsobjinlines.h"

bool
js::intl::InitializeObject(JSContext* cx, HandleObject obj, Handle<PropertyName*> initializer,
                           HandleValue locales, HandleValue options)
{
    FixedInvokeArgs<3> args(cx);

    args[0].setObject(*obj);
    args[1].set(locales);
    args[2].set(options);

    RootedValue thisv(cx, NullValue());
    RootedValue ignored(cx);
    if (!js::CallSelfHostedFunction(cx, initializer, thisv, args, &ignored))
        return false;

    MOZ_ASSERT(ignored.isUndefined(),
               "Unexpected return value from non-legacy Intl object initializer");
    return true;
}

bool
js::intl::LegacyIntlInitialize(JSContext* cx, HandleObject obj, Handle<PropertyName*> initializer,
                               HandleValue thisValue, HandleValue locales, HandleValue options,
                               DateTimeFormatOptions dtfOptions, MutableHandleValue result)
{
    FixedInvokeArgs<5> args(cx);

    args[0].setObject(*obj);
    args[1].set(thisValue);
    args[2].set(locales);
    args[3].set(options);
    args[4].setBoolean(dtfOptions == DateTimeFormatOptions::EnableMozExtensions);

    RootedValue thisv(cx, NullValue());
    if (!js::CallSelfHostedFunction(cx, initializer, thisv, args, result))
        return false;

    MOZ_ASSERT(result.isObject(), "Legacy Intl object initializer must return an object");
    return true;
}

/**
 * Returns the object holding the internal properties for obj.
 */
JSObject*
js::intl::GetInternalsObject(JSContext* cx, HandleObject obj)
{
    FixedInvokeArgs<1> args(cx);

    args[0].setObject(*obj);

    RootedValue v(cx, NullValue());
    if (!js::CallSelfHostedFunction(cx, cx->names().getInternals, v, args, &v))
        return nullptr;

    return &v.toObject();
}

void
js::intl::ReportInternalError(JSContext* cx)
{
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
}

const js::intl::OldStyleLanguageTagMapping
    js::intl::oldStyleLanguageTagMappings[] = {
        {"pa-PK", "pa-Arab-PK"}, {"zh-CN", "zh-Hans-CN"},
        {"zh-HK", "zh-Hant-HK"}, {"zh-SG", "zh-Hans-SG"},
        {"zh-TW", "zh-Hant-TW"},
};

