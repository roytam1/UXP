/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCSPService_h___
#define nsCSPService_h___

#include "nsXPCOM.h"
#include "nsIContentPolicy.h"
#include "nsIChannel.h"
#include "nsIChannelEventSink.h"
#include "nsDataHashtable.h"

#define CSPSERVICE_CONTRACTID "@mozilla.org/cspservice;1"
#define CSPSERVICE_CID \
  { 0x83d284d6, 0xf280, 0x48ae, \
    { 0xa5, 0x6b, 0x0f, 0x28, 0x11, 0x29, 0x83, 0x1b } }
class CSPService : public nsIContentPolicy,
                   public nsIChannelEventSink
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPOLICY
  NS_DECL_NSICHANNELEVENTSINK

  CSPService();
  static bool sCSPEnabled;
  static bool sCSPReportingEnabled;

protected:
  virtual ~CSPService();

private:
  // Maps origins to app status.
  nsDataHashtable<nsCStringHashKey, uint16_t> mAppStatusCache;
};
#endif /* nsCSPService_h___ */
