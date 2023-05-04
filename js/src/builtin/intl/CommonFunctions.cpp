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
js::intl::CreateDefaultOptions(JSContext* cx, MutableHandleValue defaultOptions)
{
    RootedObject options(cx, NewObjectWithGivenProto<PlainObject>(cx, nullptr));
    if (!options)
        return false;
    defaultOptions.setObject(*options);
    return true;
}

bool
js::intl::InitializeObject(JSContext* cx, HandleObject obj, Handle<PropertyName*> initializer,
                           HandleValue locales, HandleValue options)
{
    RootedValue initializerValue(cx);
    if (!GlobalObject::getIntrinsicValue(cx, cx->global(), initializer, &initializerValue))
        return false;
    MOZ_ASSERT(initializerValue.isObject());
    MOZ_ASSERT(initializerValue.toObject().is<JSFunction>());

    FixedInvokeArgs<3> args(cx);

    args[0].setObject(*obj);
    args[1].set(locales);
    args[2].set(options);

    RootedValue thisv(cx, NullValue());
    RootedValue ignored(cx);
    return js::Call(cx, initializerValue, thisv, args, &ignored);
}

/**
 * Returns the object holding the internal properties for obj.
 */
JSObject*
js::intl::GetInternalsObject(JSContext* cx, HandleObject obj)
{
    RootedValue getInternalsValue(cx);
    if (!GlobalObject::getIntrinsicValue(cx, cx->global(), cx->names().getInternals,
                                         &getInternalsValue))
    {
        return nullptr;
    }
    MOZ_ASSERT(getInternalsValue.isObject());
    MOZ_ASSERT(getInternalsValue.toObject().is<JSFunction>());

    FixedInvokeArgs<1> args(cx);

    args[0].setObject(*obj);

    RootedValue v(cx, NullValue());
    if (!js::Call(cx, getInternalsValue, v, args, &v))
        return nullptr;

    return &v.toObject();
}

void
js::intl::ReportInternalError(JSContext* cx)
{
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
}

bool
js::intl::GetAvailableLocales(JSContext* cx, CountAvailable countAvailable,
                              GetAvailable getAvailable, MutableHandleValue result)
{
    RootedObject locales(cx, NewObjectWithGivenProto<PlainObject>(cx, nullptr));
    if (!locales)
        return false;

    uint32_t count = countAvailable();
    RootedValue t(cx, BooleanValue(true));
    for (uint32_t i = 0; i < count; i++) {
        const char* locale = getAvailable(i);
        auto lang = DuplicateString(cx, locale);
        if (!lang)
            return false;
        char* p;
        while ((p = strchr(lang.get(), '_')))
            *p = '-';
        RootedAtom a(cx, Atomize(cx, lang.get(), strlen(lang.get())));
        if (!a)
            return false;
        if (!DefineProperty(cx, locales, a->asPropertyName(), t, nullptr, nullptr,
                            JSPROP_ENUMERATE))
        {
            return false;
        }
    }

    result.setObject(*locales);
    return true;
}
