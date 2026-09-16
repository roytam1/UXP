/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFVP9MFTManager.h"
#include "WMFVideoMFTManager.h"
#include "ImageContainer.h"
#include "Layers.h"
#include "VideoUtils.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/Logging.h"
#include "nsPrintfCString.h"

namespace mozilla {

struct VP9ShownFrame {
  RefPtr<layers::Image> mImage;
  nsIntRect mPicture;
  bool mKeyframe;
};

WMFVP9MFTManager::WMFVP9MFTManager(
                          const VideoInfo& aConfig,
                          layers::KnowsCompositor* aCompositor,
                          layers::ImageContainer* aContainer,
                          bool aDXVAEnabled)
  : mVideoInfo(aConfig)
  , mKnowsCompositor(aCompositor)
  , mImageContainer(aContainer)
  , mDXVAEnabled(aDXVAEnabled)
  , mIsValid(false)
{}

WMFVP9MFTManager::~WMFVP9MFTManager()
{
  Shutdown();
}

bool
WMFVP9MFTManager::Init()
{
  if (!IsWin7SP1OrLater()) {
    mFailureReason.AssignLiteral("VP9 DXVA2 requires Windows 7 SP1 or later");
  } else if (!mDXVAEnabled) {
    mFailureReason.AssignLiteral("Hardware video decoding is disabled or blocklisted");
  } else if (!mKnowsCompositor) {
    mFailureReason.AssignLiteral("No compositor available for VP9 DXVA2");
  } else if (mVideoInfo.HasAlpha()) {
    mFailureReason.AssignLiteral("VP9 DXVA2 cannot decode streams with alpha");
  } else if (mVideoInfo.mBitDepth != 8) {
    mFailureReason = nsPrintfCString(
      "VP9 DXVA2 supports profile 0 only; stream is %u-bit", mVideoInfo.mBitDepth);
  } else if (mVideoInfo.mImage.width < 16 || mVideoInfo.mImage.height < 16 ||
             mVideoInfo.mImage.width > 8192 || mVideoInfo.mImage.height > 8192) {
    mFailureReason = nsPrintfCString(
      "VP9 DXVA2 does not accept a coded size of %dx%d",
      mVideoInfo.mImage.width, mVideoInfo.mImage.height);
  }
  if (!mFailureReason.IsEmpty()) {
    return false;
  }

  auto backend = mKnowsCompositor->GetCompositorBackendType();
  if (backend != layers::LayersBackend::LAYERS_D3D9 &&
      backend != layers::LayersBackend::LAYERS_D3D11) {
    mFailureReason = nsPrintfCString(
      "VP9 DXVA2 needs a Direct3D compositor; layers backend is %d", int(backend));
    return false;
  }

  mDXVA2Manager = WMFVideoMFTManager::CreateVP9DXVA(mKnowsCompositor,
    mFailureReason, DXVAVP9Manager::GetVP9DecoderGUID());
  if (!mDXVA2Manager) {
    if (mFailureReason.IsEmpty()) {
      mFailureReason.AssignLiteral(
        "Could not create a D3D9 DXVA2 device for the VP9 profile-0 GUID");
    }
    return false;
  }

  mVP9Decoder = new DXVAVP9Manager();
  if (!mVP9Decoder->Init(mDXVA2Manager->GetDXVADeviceManager(),
        mVideoInfo.mImage.width, mVideoInfo.mImage.height, mFailureReason)) {
    Shutdown();
    return false;
  }

  mIsValid = true;
  mFailureReason.AssignLiteral("Using VP9 DXVA2 profile 0");
  return true;
}

HRESULT
WMFVP9MFTManager::Input(MediaRawData* aSample)
{
  if (!mIsValid) { return E_UNEXPECTED; }
  if (!aSample || aSample->Size() > UINT32_MAX) { return E_INVALIDARG; }
  CheckedInt<int64_t> sampleEnd = CheckedInt<int64_t>(aSample->mTime) + aSample->mDuration;
  if (!sampleEnd.isValid() || aSample->mDuration < 0) { return E_INVALIDARG; }
  uint32_t offsets[8], sizes[8], count;
  if (!VP9SplitSuperframe(aSample->Data(), uint32_t(aSample->Size()), offsets, sizes, count)) {
    return E_INVALIDARG;
  }
  AutoTArray<VP9ShownFrame, 2> shown;
  for (uint32_t i = 0; i < count; ++i) {
    RefPtr<IDirect3DSurface9> surface;
    bool display = false;
    VP9FrameHeader header;
    HRESULT hr = mVP9Decoder->DecodeFrame(aSample->Data() + offsets[i], sizes[i],
      getter_AddRefs(surface), &display, &header);
    if (FAILED(hr)) { Flush(); return hr; }
    if (!display) { continue; }
    if (mSeekTargetThreshold.isSome() &&
        sampleEnd.value() < mSeekTargetThreshold.ref().ToMicroseconds()) {
      continue;
    }
    nsIntRect picture = mVideoInfo.ScaledImageRect(header.frameWidth, header.frameHeight);
    RefPtr<layers::Image> image;
    hr = mDXVA2Manager->CopySurfaceToImage(surface, picture, getter_AddRefs(image));
    if (FAILED(hr)) { Flush(); return hr; }
    VP9ShownFrame* entry = shown.AppendElement();
    entry->mImage = image;
    entry->mPicture = picture;
    entry->mKeyframe = !header.showExistingFrame && header.frameType == 0;
  }
  if (shown.IsEmpty()) { return S_OK; }
  mSeekTargetThreshold.reset();

  const uint32_t shownCount = shown.Length();
  const int64_t slice = aSample->mDuration / shownCount;
  for (uint32_t i = 0; i < shownCount; ++i) {
    const int64_t time = aSample->mTime + int64_t(i) * slice;
    const int64_t duration = (i + 1 == shownCount)
      ? (aSample->mTime + aSample->mDuration - time)
      : slice;
    RefPtr<VideoData> frame = VideoData::CreateFromImage(mVideoInfo,
      aSample->mOffset, time, duration, shown[i].mImage,
      shown[i].mKeyframe, -1, shown[i].mPicture);
    if (!frame) { return E_OUTOFMEMORY; }
    mPendingFrames.AppendElement(frame);
  }
  return S_OK;
}

HRESULT
WMFVP9MFTManager::Output(int64_t aStreamOffset,
                         RefPtr<MediaData>& aOutput)
{
  aOutput = nullptr;
  if (mPendingFrames.IsEmpty()) { return MF_E_TRANSFORM_NEED_MORE_INPUT; }
  aOutput = mPendingFrames[0];
  mPendingFrames.RemoveElementAt(0);
  return S_OK;
}

void
WMFVP9MFTManager::Flush()
{
  mPendingFrames.Clear();
  mSeekTargetThreshold.reset();
  if (mVP9Decoder) { mVP9Decoder->Flush(); }
}

void
WMFVP9MFTManager::Shutdown()
{
  mPendingFrames.Clear();
  mVP9Decoder = nullptr;
  if (mDXVA2Manager) { DeleteOnMainThread(mDXVA2Manager); }
  mIsValid = false;
}

bool
WMFVP9MFTManager::IsHardwareAccelerated(nsACString& aReason) const
{
  aReason = mFailureReason;
  return mIsValid;
}

} // namespace mozilla
