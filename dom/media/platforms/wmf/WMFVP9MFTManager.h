/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WMFVP9MFTManager_h_
#define WMFVP9MFTManager_h_
#include "WMF.h"
#include "WMFMediaDataDecoder.h"
#include "DXVA2Manager.h"
#include "DXVAVP9Manager.h"
#include "MediaInfo.h"
#include "nsTArray.h"

namespace mozilla {

class WMFVP9MFTManager : public MFTManager {
public:
  WMFVP9MFTManager(const VideoInfo& aConfig,
                   layers::KnowsCompositor* aCompositor,
                   layers::ImageContainer* aContainer,
                   bool aDXVAEnabled);
  ~WMFVP9MFTManager();

  bool Init();

  HRESULT Input(MediaRawData* aSample) override;

  HRESULT Output(int64_t aStreamOffset,
                 RefPtr<MediaData>& aOutput) override;

  void Flush() override;

  void Drain() override {}

  void Shutdown() override;

  bool IsHardwareAccelerated(nsACString& aReason) const override;

  TrackInfo::TrackType GetType() override {
    return TrackInfo::kVideoTrack;
  }

  const char* GetDescriptionName() const override {
    return "VP9 DXVA2 profile 0 hardware decoder";
  }

private:

  VideoInfo mVideoInfo;

  RefPtr<layers::KnowsCompositor> mKnowsCompositor;
  RefPtr<layers::ImageContainer> mImageContainer;

  nsAutoPtr<DXVA2Manager> mDXVA2Manager;

  nsAutoPtr<DXVAVP9Manager> mVP9Decoder;

  nsTArray<RefPtr<MediaData>> mPendingFrames;

  nsCString mFailureReason;

  bool mDXVAEnabled;
  bool mIsValid;
};

} // namespace mozilla

#endif // WMFVP9MFTManager_h_
