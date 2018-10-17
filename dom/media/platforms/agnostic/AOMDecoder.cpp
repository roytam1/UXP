/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AOMDecoder.h"
#include "MediaResult.h"
#include "TimeUnits.h"
#include "aom/aomdx.h"
#include "gfx2DGlue.h"
#include "mozilla/PodOperations.h"
#include "mozilla/SyncRunnable.h"
#include "nsError.h"
#include "prsystem.h"

#include <algorithm>

#undef LOG
#define LOG(arg, ...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, ("AOMDecoder(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

namespace mozilla {

using namespace gfx;
using namespace layers;

AOMDecoder::AOMDecoder(const CreateDecoderParams& aParams)
  : mImageContainer(aParams.mImageContainer)
  , mTaskQueue(aParams.mTaskQueue)
  , mCallback(aParams.mCallback)
  , mIsFlushing(false)
  , mInfo(aParams.VideoConfig())
{
  PodZero(&mCodec);
}

AOMDecoder::~AOMDecoder()
{
}

void
AOMDecoder::Shutdown()
{
  aom_codec_destroy(&mCodec);
}

RefPtr<MediaDataDecoder::InitPromise>
AOMDecoder::Init()
{
  int decode_threads = 2;
  aom_codec_iface_t* dx = aom_codec_av1_dx();
  if (mInfo.mDisplay.width >= 2048) {
    decode_threads = 8;
  }
  else if (mInfo.mDisplay.width >= 1024) {
    decode_threads = 4;
  }
  decode_threads = std::min(decode_threads, PR_GetNumberOfProcessors());

  aom_codec_dec_cfg_t config;
  PodZero(&config);
  config.threads = decode_threads;
  config.w = config.h = 0; // set after decode

  aom_codec_flags_t flags = 0;
  
  if (!dx || aom_codec_dec_init(&mCodec, dx, &config, flags)) {
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
  }
  return InitPromise::CreateAndResolve(TrackInfo::kVideoTrack, __func__);
}

void
AOMDecoder::Flush()
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  mIsFlushing = true;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([this] () {
    // nothing to do for now.
  });
  SyncRunnable::DispatchToThread(mTaskQueue, r);
  mIsFlushing = false;
}

MediaResult
AOMDecoder::DoDecode(MediaRawData* aSample)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

#if defined(DEBUG)
  NS_ASSERTION(IsKeyframe(*aSample) == aSample->mKeyframe,
               "AOM Decode Keyframe error sample->mKeyframe and si.si_kf out of sync");
#endif

  if (aom_codec_err_t r = aom_codec_decode(&mCodec, aSample->Data(), aSample->Size(), nullptr, 0)) {
    LOG("AOM Decode error: %s", aom_codec_err_to_string(r));
    return MediaResult(
      NS_ERROR_DOM_MEDIA_DECODE_ERR,
      RESULT_DETAIL("AOM error decoding AV1 sample: %s", aom_codec_err_to_string(r)));
  }

  aom_codec_iter_t iter = nullptr;
  aom_image_t *img;

  while ((img = aom_codec_get_frame(&mCodec, &iter))) {
    NS_ASSERTION(img->fmt == AOM_IMG_FMT_I420 ||
                 img->fmt == AOM_IMG_FMT_I444,
                 "WebM image format not I420 or I444");

    // Chroma shifts are rounded down as per the decoding examples in the SDK
    VideoData::YCbCrBuffer b;
    b.mPlanes[0].mData = img->planes[0];
    b.mPlanes[0].mStride = img->stride[0];
    b.mPlanes[0].mHeight = img->d_h;
    b.mPlanes[0].mWidth = img->d_w;
    b.mPlanes[0].mOffset = b.mPlanes[0].mSkip = 0;

    b.mPlanes[1].mData = img->planes[1];
    b.mPlanes[1].mStride = img->stride[1];
    b.mPlanes[1].mOffset = b.mPlanes[1].mSkip = 0;

    b.mPlanes[2].mData = img->planes[2];
    b.mPlanes[2].mStride = img->stride[2];
    b.mPlanes[2].mOffset = b.mPlanes[2].mSkip = 0;

    if (img->fmt == AOM_IMG_FMT_I420) {
      b.mPlanes[1].mHeight = (img->d_h + 1) >> img->y_chroma_shift;
      b.mPlanes[1].mWidth = (img->d_w + 1) >> img->x_chroma_shift;

      b.mPlanes[2].mHeight = (img->d_h + 1) >> img->y_chroma_shift;
      b.mPlanes[2].mWidth = (img->d_w + 1) >> img->x_chroma_shift;
    } else if (img->fmt == AOM_IMG_FMT_I444) {
      b.mPlanes[1].mHeight = img->d_h;
      b.mPlanes[1].mWidth = img->d_w;

      b.mPlanes[2].mHeight = img->d_h;
      b.mPlanes[2].mWidth = img->d_w;
    } else {
      LOG("AOM Unknown image format");
      return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                         RESULT_DETAIL("AOM Unknown image format"));
    }

    RefPtr<VideoData> v;
    v = VideoData::CreateAndCopyData(mInfo,
                                     mImageContainer,
                                     aSample->mOffset,
                                     aSample->mTime,
                                     aSample->mDuration,
                                     b,
                                     aSample->mKeyframe,
                                     aSample->mTimecode,
                                     mInfo.ScaledImageRect(img->d_w,
                                                           img->d_h));

    if (!v) {
      LOG(
        "Image allocation error source %ux%u display %ux%u picture %ux%u",
        img->d_w, img->d_h, mInfo.mDisplay.width, mInfo.mDisplay.height,
        mInfo.mImage.width, mInfo.mImage.height);
      return MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__);
    }
    mCallback->Output(v);
  }
  return NS_OK;
}

void
AOMDecoder::ProcessDecode(MediaRawData* aSample)
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
AOMDecoder::Input(MediaRawData* aSample)
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  mTaskQueue->Dispatch(NewRunnableMethod<RefPtr<MediaRawData>>(
                       this, &AOMDecoder::ProcessDecode, aSample));
}

void
AOMDecoder::ProcessDrain()
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  mCallback->DrainComplete();
}

void
AOMDecoder::Drain()
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  mTaskQueue->Dispatch(NewRunnableMethod(this, &AOMDecoder::ProcessDrain));
}

/* static */
bool
AOMDecoder::IsAV1(const nsACString& aMimeType)
{
  return aMimeType.EqualsLiteral("video/webm; codecs=av1")
         || aMimeType.EqualsLiteral("video/av1");
}

/* static */
bool
AOMDecoder::IsSupportedCodec(const nsAString& aCodecType)
{
  // While AV1 is under development, we describe support
  // for a specific aom commit hash so sites can check
  // compatibility.
  auto version = NS_ConvertASCIItoUTF16("av1.experimental.");
  version.AppendLiteral("4d668d7feb1f8abd809d1bca0418570a7f142a36");
  return aCodecType.EqualsLiteral("av1") ||
         aCodecType.Equals(version);
}

/* static */
bool
AOMDecoder::IsKeyframe(Span<const uint8_t> aBuffer) {
  aom_codec_stream_info_t info;
  PodZero(&info);

  aom_codec_peek_stream_info(aom_codec_av1_dx(),
                             aBuffer.Elements(),
                             aBuffer.Length(),
                             &info);

  return bool(info.is_kf);
}

/* static */
nsIntSize
AOMDecoder::GetFrameSize(Span<const uint8_t> aBuffer) {
  aom_codec_stream_info_t info;
  PodZero(&info);

  aom_codec_peek_stream_info(aom_codec_av1_dx(),
                             aBuffer.Elements(),
                             aBuffer.Length(),
                             &info);

  return nsIntSize(info.w, info.h);
}

} // namespace mozilla
#undef LOG
