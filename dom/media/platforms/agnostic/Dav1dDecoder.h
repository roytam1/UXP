/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(Dav1dDecoder_h_)
#define Dav1dDecoder_h_

#include "PlatformDecoderModule.h"
#include "mozilla/Span.h"

#include <stdint.h>

typedef struct Dav1dContext Dav1dContext;
typedef struct Dav1dPicture Dav1dPicture;

namespace mozilla {

class Dav1dDecoder : public MediaDataDecoder
{
public:
  explicit Dav1dDecoder(const CreateDecoderParams& aParams);

  RefPtr<InitPromise> Init() override;
  void Input(MediaRawData* aSample) override;
  void Flush() override;
  void Drain() override;
  void Shutdown() override;
  const char* GetDescriptionName() const override
  {
    return "dav1d (AV1) video decoder";
  }

  // Return true if aMimeType is a one of the strings used
  // by our demuxers to identify AV1 streams.
  static bool IsAV1(const nsACString& aMimeType);

  // Return the frame dimensions from a sequence header, when one is present.
  static nsIntSize GetFrameSize(Span<const uint8_t> aBuffer);

private:
  ~Dav1dDecoder();
  void ProcessDecode(MediaRawData* aSample);
  MediaResult DoDecode(MediaRawData* aSample);
  MediaResult DrainOutput();
  MediaResult OutputPicture(const Dav1dPicture& aPicture);
  void ProcessDrain();

  const RefPtr<layers::ImageContainer> mImageContainer;
  const RefPtr<TaskQueue> mTaskQueue;
  MediaDataDecoderCallback* mCallback;
  Atomic<bool> mIsFlushing;

  Dav1dContext* mDecoder;

  const VideoInfo& mInfo;
};

} // namespace mozilla

#endif // Dav1dDecoder_h_
