/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsMenuSeparator_h__
#define __nsMenuSeparator_h__

#include "mozilla/Attributes.h"

#include "nsMenuObject.h"

class nsIContent;
class nsIAtom;
class nsMenuContainer;

// Menu separator class
class nsMenuSeparator final : public nsMenuObject {
public:
    nsMenuSeparator(nsMenuContainer* aParent, nsIContent* aContent);
    ~nsMenuSeparator();

    nsMenuObject::EType Type() const override;

private:
    void InitializeNativeData() override;
    void Update(nsStyleContext* aStyleContext) override;
    bool IsCompatibleWithNativeData(DbusmenuMenuitem* aNativeData) const override;
    nsMenuObject::PropertyFlags SupportedProperties() const override;

    void OnAttributeChanged(nsIContent* aContent, nsIAtom* aAttribute) override;
};

#endif /* __nsMenuSeparator_h__ */
