/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpNTLMAuth_h__
#define nsHttpNTLMAuth_h__

#include "nsIHttpAuthenticator.h"

namespace mozilla { namespace net {

class nsHttpNTLMAuth : public nsIHttpAuthenticator
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIHTTPAUTHENTICATOR

    nsHttpNTLMAuth() {}

private:
    virtual ~nsHttpNTLMAuth() {}

    // This flag indicates whether we are using the native NTLM implementation
    // or the internal one.
    bool mUseNative;

    // Whether the prefs let this host use the logged-in user's identity. Set by
    // ChallengeReceived, read by GenerateCredentials.
    bool mAllowDefaultCredentials = false;
};

} // namespace net
} // namespace mozilla

#endif // !nsHttpNTLMAuth_h__
