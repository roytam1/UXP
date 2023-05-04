/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Preferences.h"
#ifdef MOZ_AV1
#include "AOMDecoder.h"
#endif
#include "MediaPrefs.h"
#include "MediaDecoderStateMachine.h"
#include "WebMDemuxer.h"
#include "WebMDecoder.h"
#include "PDMFactory.h"
#include "VideoUtils.h"
#include "nsContentTypeParser.h"

namespace mozilla {

MediaDecoderStateMachine* WebMDecoder::CreateStateMachine()
{
  mReader =
    new MediaFormatReader(this, new WebMDemuxer(GetResource()), GetVideoFrameContainer());
  return new MediaDecoderStateMachine(this, mReader);
}

/* static */
bool
WebMDecoder::IsEnabled()
{
  return Preferences::GetBool("media.webm.enabled");
}

/* static */
bool
WebMDecoder::CanHandleMediaType(const nsACString& aMIMETypeExcludingCodecs,
                                const nsAString& aCodecs)
{
  if (!IsEnabled()) {
    return false;
  }

  const bool isWebMAudio = aMIMETypeExcludingCodecs.EqualsASCII("audio/webm");
  const bool isWebMVideo = aMIMETypeExcludingCodecs.EqualsASCII("video/webm");
  const bool isMatroskaAudio = aMIMETypeExcludingCodecs.EqualsASCII("audio/x-matroska") ;
  const bool isMatroskaVideo = aMIMETypeExcludingCodecs.EqualsASCII("video/x-matroska") ;

  if (!isWebMAudio && !isWebMVideo && !isMatroskaAudio && !isMatroskaVideo) {
    return false;
  }

  nsTArray<nsCString> codecMimes;
  if (aCodecs.IsEmpty()) {
    // WebM guarantees that the only codecs it contained are vp8, vp9, opus or vorbis.
    return true;
  }
  // Verify that all the codecs specified are ones that we expect that
  // we can play.
  nsTArray<nsString> codecs;
  if (!ParseCodecsString(aCodecs, codecs)) {
    return false;
  }
  for (const nsString& codec : codecs) {
    if (codec.EqualsLiteral("opus") || codec.EqualsLiteral("vorbis")) {
      continue;
    }
    // Note: Only accept VP8/VP9 in a video content type, not in an audio
    // content type.
    if (isWebMVideo || isMatroskaVideo) {
      UniquePtr<TrackInfo> trackInfo;
      if (IsVP9CodecString(codec))  {
        trackInfo = CreateTrackInfoWithMIMEType(
          NS_LITERAL_CSTRING("video/vp9"));
      } else if (IsVP8CodecString(codec)) {
        trackInfo = CreateTrackInfoWithMIMEType(
          NS_LITERAL_CSTRING("video/vp8"));
      }
      // If it is vp8 or vp9, check the bit depth.
      if (trackInfo) {
        uint8_t profile = 0;
        uint8_t level = 0;
        uint8_t bitDepth = 0;
        if (ExtractVPXCodecDetails(codec, profile, level, bitDepth)) {
          trackInfo->GetAsVideoInfo()->mBitDepth = bitDepth;
        }
        // Verify that we have a PDM that supports this bit depth.
        RefPtr<PDMFactory> platform = new PDMFactory();
        if (!platform->Supports(*trackInfo, nullptr)) {
          return false;
        }
        continue;
      }
    }
#ifdef MOZ_AV1
    if (MediaPrefs::AV1Enabled() && IsAV1CodecString(codec)) {
      continue;
    }
#endif

    if (IsH264CodecString(codec)) {
      continue;
    }

    if (IsAACCodecString(codec)) {
      continue;
    }

    // Some unsupported codec.
    return false;
  }
  return true;
}

/* static */ bool
WebMDecoder::CanHandleMediaType(const nsAString& aContentType)
{
  nsContentTypeParser parser(aContentType);
  nsAutoString mimeType;
  nsresult rv = parser.GetType(mimeType);
  if (NS_FAILED(rv)) {
    return false;
  }
  nsString codecs;
  parser.GetParameter("codecs", codecs);

  return CanHandleMediaType(NS_ConvertUTF16toUTF8(mimeType),
                            codecs);
}

void
WebMDecoder::GetMozDebugReaderData(nsAString& aString)
{
  if (mReader) {
    mReader->GetMozDebugReaderData(aString);
  }
}

} // namespace mozilla

