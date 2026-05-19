/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Dav1dDecoder.h"
#include "MediaResult.h"
#include "TimeUnits.h"
#include "dav1d/dav1d.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/PodOperations.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "nsError.h"
#include "prsystem.h"

#include <algorithm>
#include <errno.h>
#include <string.h>

#undef LOG
#define LOG(arg, ...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, ("Dav1dDecoder(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

namespace mozilla {

using namespace gfx;
using namespace layers;

Dav1dDecoder::Dav1dDecoder(const CreateDecoderParams& aParams)
  : mImageContainer(aParams.mImageContainer)
  , mTaskQueue(aParams.mTaskQueue)
  , mCallback(aParams.mCallback)
  , mIsFlushing(false)
  , mDecoder(nullptr)
  , mInfo(aParams.VideoConfig())
{
}

Dav1dDecoder::~Dav1dDecoder()
{
}

void
Dav1dDecoder::Shutdown()
{
  if (mDecoder) {
    dav1d_close(&mDecoder);
  }
}

RefPtr<MediaDataDecoder::InitPromise>
Dav1dDecoder::Init()
{
  int decodeThreads = 2;
  if (mInfo.mDisplay.width >= 2048) {
    decodeThreads = 8;
  } else if (mInfo.mDisplay.width >= 1024) {
    decodeThreads = 4;
  }
  decodeThreads = std::min(decodeThreads, PR_GetNumberOfProcessors());

  Dav1dSettings settings;
  dav1d_default_settings(&settings);
  settings.n_threads = decodeThreads;
  settings.logger.callback = nullptr;

  if (dav1d_open(&mDecoder, &settings) < 0) {
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
  }
  return InitPromise::CreateAndResolve(TrackInfo::kVideoTrack, __func__);
}

void
Dav1dDecoder::Flush()
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  mIsFlushing = true;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([this] () {
    if (mDecoder) {
      dav1d_flush(mDecoder);
    }
  });
  SyncRunnable::DispatchToThread(mTaskQueue, r);
  mIsFlushing = false;
}

static bool
GetPlaneSize(int aWidth, int aHeight, size_t* aSize)
{
  CheckedInt<size_t> size = aWidth;
  size *= aHeight;
  if (!size.isValid()) {
    return false;
  }
  *aSize = size.value();
  return true;
}

static void
DownshiftPlane(uint8_t* aDst, int aDstStride, const uint8_t* aSrc,
               ptrdiff_t aSrcStride, int aWidth, int aHeight, int aShift)
{
  for (int y = 0; y < aHeight; y++) {
    const uint16_t* src =
      reinterpret_cast<const uint16_t*>(aSrc + y * aSrcStride);
    uint8_t* dst = aDst + y * aDstStride;
    for (int x = 0; x < aWidth; x++) {
      dst[x] = (src[x] >> aShift) & 0xff;
    }
  }
}

MediaResult
Dav1dDecoder::OutputPicture(const Dav1dPicture& aPicture)
{
  const int width = aPicture.p.w;
  const int height = aPicture.p.h;
  if (width <= 0 || height <= 0) {
    return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                       RESULT_DETAIL("dav1d returned invalid AV1 picture size"));
  }

  int chromaWidth = width;
  int chromaHeight = height;
  switch (aPicture.p.layout) {
    case DAV1D_PIXEL_LAYOUT_I420:
      chromaWidth = (width + 1) >> 1;
      chromaHeight = (height + 1) >> 1;
      break;
    case DAV1D_PIXEL_LAYOUT_I422:
      chromaWidth = (width + 1) >> 1;
      chromaHeight = height;
      break;
    case DAV1D_PIXEL_LAYOUT_I444:
      break;
    default:
      return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                         RESULT_DETAIL("dav1d returned unsupported AV1 pixel layout: %d",
                                       int(aPicture.p.layout)));
  }

  const int planeWidths[3] = { width, chromaWidth, chromaWidth };
  const int planeHeights[3] = { height, chromaHeight, chromaHeight };
  const ptrdiff_t srcStrides[3] = {
    aPicture.stride[0],
    aPicture.stride[1],
    aPicture.stride[1]
  };
  uint8_t* planeData[3] = {
    static_cast<uint8_t*>(aPicture.data[0]),
    static_cast<uint8_t*>(aPicture.data[1]),
    static_cast<uint8_t*>(aPicture.data[2])
  };
  uint32_t planeStrides[3] = { 0, 0, 0 };
  UniquePtr<uint8_t[]> downshifted[3];

  if (aPicture.p.bpc > 8) {
    const int shift = aPicture.p.bpc - 8;
    for (int plane = 0; plane < 3; plane++) {
      size_t planeSize;
      if (!GetPlaneSize(planeWidths[plane], planeHeights[plane], &planeSize)) {
        return MediaResult(NS_ERROR_OUT_OF_MEMORY,
                           RESULT_DETAIL("AV1 downshift plane size overflow"));
      }
      downshifted[plane] = MakeUniqueFallible<uint8_t[]>(planeSize);
      if (!downshifted[plane]) {
        return MediaResult(NS_ERROR_OUT_OF_MEMORY,
                           RESULT_DETAIL("Couldn't allocate AV1 conversion buffer"));
      }
      DownshiftPlane(downshifted[plane].get(), planeWidths[plane],
                     planeData[plane], srcStrides[plane],
                     planeWidths[plane], planeHeights[plane], shift);
      planeData[plane] = downshifted[plane].get();
      planeStrides[plane] = planeWidths[plane];
    }
  } else {
    for (int plane = 0; plane < 3; plane++) {
      CheckedInt<uint32_t> stride(srcStrides[plane]);
      if (srcStrides[plane] < 0 || !stride.isValid()) {
        return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                           RESULT_DETAIL("dav1d returned invalid AV1 stride"));
      }
      planeStrides[plane] = stride.value();
    }
  }

  VideoData::YCbCrBuffer b;
  for (int plane = 0; plane < 3; plane++) {
    b.mPlanes[plane].mData = planeData[plane];
    b.mPlanes[plane].mStride = planeStrides[plane];
    b.mPlanes[plane].mHeight = planeHeights[plane];
    b.mPlanes[plane].mWidth = planeWidths[plane];
    b.mPlanes[plane].mOffset = 0;
    b.mPlanes[plane].mSkip = 0;
  }

  if (aPicture.seq_hdr) {
    switch (aPicture.seq_hdr->mtrx) {
      case DAV1D_MC_BT601:
      case DAV1D_MC_BT470BG:
        b.mYUVColorSpace = YUVColorSpace::BT601;
        break;
      case DAV1D_MC_BT709:
        b.mYUVColorSpace = YUVColorSpace::BT709;
        break;
      case DAV1D_MC_IDENTITY:
        b.mYUVColorSpace = YUVColorSpace::IDENTITY;
        break;
      default:
        LOG("Unhandled colorspace %d", aPicture.seq_hdr->mtrx);
        break;
    }
    b.mColorRange = aPicture.seq_hdr->color_range ? ColorRange::FULL
                                                  : ColorRange::LIMITED;
  }

  const int64_t time =
    aPicture.m.timestamp == INT64_MIN ? 0 : aPicture.m.timestamp;
  const int64_t duration = aPicture.m.duration;
  const int64_t offset = aPicture.m.offset;
  const bool keyframe =
    aPicture.frame_hdr &&
    aPicture.frame_hdr->frame_type == DAV1D_FRAME_TYPE_KEY;

  RefPtr<VideoData> v =
    VideoData::CreateAndCopyData(mInfo,
                                 mImageContainer,
                                 offset,
                                 time,
                                 duration,
                                 b,
                                 keyframe,
                                 time,
                                 mInfo.ScaledImageRect(width, height));

  if (!v) {
    LOG("Image allocation error source %dx%d display %ux%u picture %ux%u",
        width, height, mInfo.mDisplay.width, mInfo.mDisplay.height,
        mInfo.mImage.width, mInfo.mImage.height);
    return MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__);
  }
  mCallback->Output(v);
  return NS_OK;
}

MediaResult
Dav1dDecoder::DrainOutput()
{
  while (true) {
    Dav1dPicture picture;
    PodZero(&picture);
    const int res = dav1d_get_picture(mDecoder, &picture);
    if (res == DAV1D_ERR(EAGAIN)) {
      return NS_OK;
    }
    if (res < 0) {
      return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                         RESULT_DETAIL("dav1d error getting AV1 picture: %d",
                                       res));
    }

    MediaResult rv = OutputPicture(picture);
    dav1d_picture_unref(&picture);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
}

MediaResult
Dav1dDecoder::DoDecode(MediaRawData* aSample)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

  Dav1dData data;
  PodZero(&data);
  uint8_t* dst = dav1d_data_create(&data, aSample->Size());
  if (!dst) {
    return MediaResult(NS_ERROR_OUT_OF_MEMORY,
                       RESULT_DETAIL("Couldn't allocate dav1d AV1 input buffer"));
  }
  memcpy(dst, aSample->Data(), aSample->Size());
  data.m.timestamp = aSample->mTime;
  data.m.duration = aSample->mDuration;
  data.m.offset = aSample->mOffset;
  data.m.size = aSample->Size();

  while (data.sz) {
    const int res = dav1d_send_data(mDecoder, &data);
    if (res == DAV1D_ERR(EAGAIN)) {
      MediaResult rv = DrainOutput();
      if (NS_FAILED(rv)) {
        dav1d_data_unref(&data);
        return rv;
      }
      continue;
    }
    if (res < 0) {
      dav1d_data_unref(&data);
      return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                         RESULT_DETAIL("dav1d error decoding AV1 sample: %d",
                                       res));
    }
  }

  dav1d_data_unref(&data);
  return DrainOutput();
}

void
Dav1dDecoder::ProcessDecode(MediaRawData* aSample)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  if (mIsFlushing) {
    return;
  }
  MediaResult rv = DoDecode(aSample);
  if (NS_FAILED(rv)) {
    mCallback->Error(rv);
  } else {
    mCallback->InputExhausted();
  }
}

void
Dav1dDecoder::Input(MediaRawData* aSample)
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  mTaskQueue->Dispatch(NewRunnableMethod<RefPtr<MediaRawData>>(
                       this, &Dav1dDecoder::ProcessDecode, aSample));
}

void
Dav1dDecoder::ProcessDrain()
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  MediaResult rv = DrainOutput();
  if (NS_FAILED(rv)) {
    mCallback->Error(rv);
  } else {
    mCallback->DrainComplete();
  }
}

void
Dav1dDecoder::Drain()
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  mTaskQueue->Dispatch(NewRunnableMethod(this, &Dav1dDecoder::ProcessDrain));
}

/* static */
bool
Dav1dDecoder::IsAV1(const nsACString& aMimeType)
{
  return aMimeType.EqualsLiteral("video/webm; codecs=av1") ||
         aMimeType.EqualsLiteral("video/av1");
}

/* static */
nsIntSize
Dav1dDecoder::GetFrameSize(Span<const uint8_t> aBuffer)
{
  Dav1dSequenceHeader seqHdr;
  PodZero(&seqHdr);
  if (dav1d_parse_sequence_header(&seqHdr,
                                  aBuffer.Elements(),
                                  aBuffer.Length()) < 0) {
    return nsIntSize(0, 0);
  }
  return nsIntSize(seqHdr.max_width, seqHdr.max_height);
}

} // namespace mozilla
#undef LOG
