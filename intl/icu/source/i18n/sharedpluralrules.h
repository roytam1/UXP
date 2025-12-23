// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
* Copyright (C) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
******************************************************************************
* sharedpluralrules.h
*/

#ifndef __SHARED_PLURALRULES_H__
#define __SHARED_PLURALRULES_H__

#include "unicode/utypes.h"
#include "sharedobject.h"
#include "unifiedcache.h"

U_NAMESPACE_BEGIN

class PluralRules;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4251)
#pragma warning(disable:4275)
#endif

class U_I18N_API SharedPluralRules : public SharedObject {

#ifdef _MSC_VER
#pragma warning(pop)
#endif

public:
    SharedPluralRules(PluralRules *prToAdopt) : ptr(prToAdopt) { }
    virtual ~SharedPluralRules();
    const PluralRules *operator->() const { return ptr; }
    const PluralRules &operator*() const { return *ptr; }
private:
    PluralRules *ptr;
    SharedPluralRules(const SharedPluralRules &) = delete;
    SharedPluralRules &operator=(const SharedPluralRules &) =delete;
};

template<> U_I18N_API
const SharedPluralRules *LocaleCacheKey<SharedPluralRules>::createObject(
        const void * /*unused*/, UErrorCode &status) const;

U_NAMESPACE_END

#endif
