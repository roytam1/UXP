/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(AOMDecoder_h_)
#define AOMDecoder_h_

#include "PlatformDecoderModule.h"
#include "mozilla/Span.h"

#include <stdint.h>
#include "aom/aom_decoder.h"

namespace mozilla {

class AOMDecoder : public MediaDataDecoder
{
public:
  explicit AOMDecoder(const CreateDecoderParams& aParams);

  RefPtr<InitPromise> Init() override;
  void Input(MediaRawData* aSample) override;
  void Flush() override;
  void Drain() override;
  void Shutdown() override;
  const char* GetDescriptionName() const override
  {
    return "libaom (AV1) video decoder";
  }

  // Return true if aMimeType is a one of the strings used
  // by our demuxers to identify AV1 streams.
  static bool IsAV1(const nsACString& aMimeType);

  // Return true if a sample is a keyframe.
  static bool IsKeyframe(Span<const uint8_t> aBuffer);

  // Return the frame dimensions for a sample.
  static nsIntSize GetFrameSize(Span<const uint8_t> aBuffer);

private:
  ~AOMDecoder();
  void ProcessDecode(MediaRawData* aSample);
  MediaResult DoDecode(MediaRawData* aSample);
  void ProcessDrain();

  const RefPtr<layers::ImageContainer> mImageContainer;
  const RefPtr<TaskQueue> mTaskQueue;
  MediaDataDecoderCallback* mCallback;
  Atomic<bool> mIsFlushing;

  // AOM decoder state
  aom_codec_ctx_t mCodec;

  const VideoInfo& mInfo;
};

} // namespace mozilla

#endif // AOMDecoder_h_
