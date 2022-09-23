/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecoderTraits.h"
#include "MediaContentType.h"
#include "MediaDecoder.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsMimeTypes.h"
#include "mozilla/Preferences.h"

#include "OggDecoder.h"
#include "OggDemuxer.h"

#include "WebMDecoder.h"
#include "WebMDemuxer.h"

#ifdef MOZ_FMP4
#include "MP4Decoder.h"
#include "MP4Demuxer.h"
#endif
#include "MediaFormatReader.h"

#include "MP3Decoder.h"
#include "MP3Demuxer.h"

#include "WaveDecoder.h"
#include "WaveDemuxer.h"

#include "ADTSDecoder.h"
#include "ADTSDemuxer.h"

#include "FlacDecoder.h"
#include "FlacDemuxer.h"

#include "nsPluginHost.h"
#include "MediaPrefs.h"

namespace mozilla
{

template <class String>
static bool
CodecListContains(char const *const * aCodecs, const String& aCodec)
{
  for (int32_t i = 0; aCodecs[i]; ++i) {
    if (aCodec.EqualsASCII(aCodecs[i]))
      return true;
  }
  return false;
}

static bool
IsOggSupportedType(const nsACString& aType,
                   const nsAString& aCodecs = EmptyString())
{
  return OggDecoder::CanHandleMediaType(aType, aCodecs);
}

static bool
IsOggTypeAndEnabled(const nsACString& aType)
{
  return IsOggSupportedType(aType);
}

static bool
IsWebMSupportedType(const nsACString& aType,
                    const nsAString& aCodecs = EmptyString())
{
  return WebMDecoder::CanHandleMediaType(aType, aCodecs);
}

/* static */ bool
DecoderTraits::IsWebMTypeAndEnabled(const nsACString& aType)
{
  return IsWebMSupportedType(aType);
}

/* static */ bool
DecoderTraits::IsWebMAudioType(const nsACString& aType)
{
  return aType.EqualsASCII("audio/webm");
}

#ifdef MOZ_FMP4
static bool
IsMP4SupportedType(const MediaContentType& aParsedType,
                   DecoderDoctorDiagnostics* aDiagnostics)
{
  return MP4Decoder::CanHandleMediaType(aParsedType, aDiagnostics);
}
static bool
IsMP4SupportedType(const nsACString& aType,
                   DecoderDoctorDiagnostics* aDiagnostics)
{
  MediaContentType contentType{aType};
  return IsMP4SupportedType(contentType, aDiagnostics);
}
#endif

/* static */ bool
DecoderTraits::IsMP4TypeAndEnabled(const nsACString& aType,
                                   DecoderDoctorDiagnostics* aDiagnostics)
{
#ifdef MOZ_FMP4
  return IsMP4SupportedType(aType, aDiagnostics);
#else
  return false;
#endif
}

static bool
IsMP3SupportedType(const nsACString& aType,
                   const nsAString& aCodecs = EmptyString())
{
  return MP3Decoder::CanHandleMediaType(aType, aCodecs);
}

static bool
IsAACSupportedType(const nsACString& aType,
                   const nsAString& aCodecs = EmptyString())
{
  return ADTSDecoder::CanHandleMediaType(aType, aCodecs);
}

static bool
IsWaveSupportedType(const nsACString& aType,
                    const nsAString& aCodecs = EmptyString())
{
  return WaveDecoder::CanHandleMediaType(aType, aCodecs);
}

static bool
IsFlacSupportedType(const nsACString& aType,
                   const nsAString& aCodecs = EmptyString())
{
  return FlacDecoder::CanHandleMediaType(aType, aCodecs);
}

static
CanPlayStatus
CanHandleCodecsType(const MediaContentType& aType,
                    DecoderDoctorDiagnostics* aDiagnostics)
{
  MOZ_ASSERT(aType.IsValid());
  // We should have been given a codecs string, though it may be empty.
  MOZ_ASSERT(aType.HaveCodecs());

  char const* const* codecList = nullptr;
  if (IsOggTypeAndEnabled(aType.GetMIMEType())) {
    if (IsOggSupportedType(aType.GetMIMEType(), aType.GetCodecs())) {
      return CANPLAY_YES;
    } else {
      // We can only reach this position if a particular codec was requested,
      // ogg is supported and working: the codec must be invalid.
      return CANPLAY_NO;
    }
  }
  if (IsWaveSupportedType(aType.GetMIMEType())) {
    if (IsWaveSupportedType(aType.GetMIMEType(), aType.GetCodecs())) {
      return CANPLAY_YES;
    } else {
      // We can only reach this position if a particular codec was requested,
      // ogg is supported and working: the codec must be invalid.
      return CANPLAY_NO;
    }
  }
#if !defined(MOZ_OMX_WEBM_DECODER)
  if (DecoderTraits::IsWebMTypeAndEnabled(aType.GetMIMEType())) {
    if (IsWebMSupportedType(aType.GetMIMEType(), aType.GetCodecs())) {
      return CANPLAY_YES;
    } else {
      // We can only reach this position if a particular codec was requested,
      // webm is supported and working: the codec must be invalid.
      return CANPLAY_NO;
    }
  }
#endif
#ifdef MOZ_FMP4
  if (DecoderTraits::IsMP4TypeAndEnabled(aType.GetMIMEType(), aDiagnostics)) {
    if (IsMP4SupportedType(aType, aDiagnostics)) {
      return CANPLAY_YES;
    } else {
      // We can only reach this position if a particular codec was requested,
      // fmp4 is supported and working: the codec must be invalid.
      return CANPLAY_NO;
    }
  }
#endif
  if (IsMP3SupportedType(aType.GetMIMEType(), aType.GetCodecs())) {
    return CANPLAY_YES;
  }
  if (IsAACSupportedType(aType.GetMIMEType(), aType.GetCodecs())) {
    return CANPLAY_YES;
  }
  if (IsFlacSupportedType(aType.GetMIMEType(), aType.GetCodecs())) {
    return CANPLAY_YES;
  }
  if (!codecList) {
    return CANPLAY_MAYBE;
  }

  // See http://www.rfc-editor.org/rfc/rfc4281.txt for the description
  // of the 'codecs' parameter
  nsCharSeparatedTokenizer tokenizer(aType.GetCodecs(), ',');
  bool expectMoreTokens = false;
  while (tokenizer.hasMoreTokens()) {
    const nsSubstring& token = tokenizer.nextToken();

    if (!CodecListContains(codecList, token)) {
      // Totally unsupported codec
      return CANPLAY_NO;
    }
    expectMoreTokens = tokenizer.separatorAfterCurrentToken();
  }
  if (expectMoreTokens) {
    // Last codec name was empty
    return CANPLAY_NO;
  }

  return CANPLAY_YES;
}

static
CanPlayStatus
CanHandleMediaType(const MediaContentType& aType,
                   DecoderDoctorDiagnostics* aDiagnostics)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aType.HaveCodecs()) {
    CanPlayStatus result = CanHandleCodecsType(aType, aDiagnostics);
    if (result == CANPLAY_NO || result == CANPLAY_YES) {
      return result;
    }
  }
  if (IsOggTypeAndEnabled(aType.GetMIMEType())) {
    return CANPLAY_MAYBE;
  }
  if (IsWaveSupportedType(aType.GetMIMEType())) {
    return CANPLAY_MAYBE;
  }
  if (DecoderTraits::IsMP4TypeAndEnabled(aType.GetMIMEType(), aDiagnostics)) {
    return CANPLAY_MAYBE;
  }
#if !defined(MOZ_OMX_WEBM_DECODER)
  if (DecoderTraits::IsWebMTypeAndEnabled(aType.GetMIMEType())) {
    return CANPLAY_MAYBE;
  }
#endif
  if (IsMP3SupportedType(aType.GetMIMEType())) {
    return CANPLAY_MAYBE;
  }
  if (IsAACSupportedType(aType.GetMIMEType())) {
    return CANPLAY_MAYBE;
  }
  if (IsFlacSupportedType(aType.GetMIMEType())) {
    return CANPLAY_MAYBE;
  }
  return CANPLAY_NO;
}

/* static */
CanPlayStatus
DecoderTraits::CanHandleContentType(const MediaContentType& aContentType,
                                    DecoderDoctorDiagnostics* aDiagnostics)
{
  if (!aContentType.IsValid()) {
    return CANPLAY_NO;
  }

  return CanHandleMediaType(aContentType, aDiagnostics);
}

/* static */
bool DecoderTraits::ShouldHandleMediaType(const char* aMIMEType,
                                          DecoderDoctorDiagnostics* aDiagnostics)
{
  // Prior to Issue #1977, we always pop-up the download prompt when
  // wave files are opened directly. This was considered inconsistent
  // since we play wave files when they're inside an HTML5 audio tag
  // anyway. However, there may be users who depended on this old
  // behavior, where they use their helper apps to open WAV audio
  // instead. We should allow this old behavior behind a pref for
  // those who want it.
  if (!Preferences::GetBool("media.wave.play-stand-alone", true) &&
      IsWaveSupportedType(nsDependentCString(aMIMEType))) {
    return false;
  }

  // If an external plugin which can handle quicktime video is available
  // (and not disabled), prefer it over native playback as there are
  // several codecs found in the wild that we do not handle.
  if (nsDependentCString(aMIMEType).EqualsASCII("video/quicktime")) {
    RefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();
    if (pluginHost &&
        pluginHost->HavePluginForType(nsDependentCString(aMIMEType))) {
      return false;
    }
  }

  MediaContentType parsed{nsDependentCString(aMIMEType)};
  return CanHandleMediaType(parsed, aDiagnostics)
         != CANPLAY_NO;
}

// Instantiates but does not initialize decoder.
static
already_AddRefed<MediaDecoder>
InstantiateDecoder(const nsACString& aType,
                   MediaDecoderOwner* aOwner,
                   DecoderDoctorDiagnostics* aDiagnostics)
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<MediaDecoder> decoder;

#ifdef MOZ_FMP4
  if (IsMP4SupportedType(aType, aDiagnostics)) {
    decoder = new MP4Decoder(aOwner);
    return decoder.forget();
  }
#endif
  if (IsMP3SupportedType(aType)) {
    decoder = new MP3Decoder(aOwner);
    return decoder.forget();
  }
  if (IsAACSupportedType(aType)) {
    decoder = new ADTSDecoder(aOwner);
    return decoder.forget();
  }
  if (IsOggSupportedType(aType)) {
    decoder = new OggDecoder(aOwner);
    return decoder.forget();
  }
  if (IsWaveSupportedType(aType)) {
    decoder = new WaveDecoder(aOwner);
    return decoder.forget();
  }
  if (IsFlacSupportedType(aType)) {
    decoder = new FlacDecoder(aOwner);
    return decoder.forget();
  }

  if (IsWebMSupportedType(aType)) {
    decoder = new WebMDecoder(aOwner);
    return decoder.forget();
  }

  return nullptr;
}

/* static */
already_AddRefed<MediaDecoder>
DecoderTraits::CreateDecoder(const nsACString& aType,
                             MediaDecoderOwner* aOwner,
                             DecoderDoctorDiagnostics* aDiagnostics)
{
  MOZ_ASSERT(NS_IsMainThread());
  return InstantiateDecoder(aType, aOwner, aDiagnostics);
}

/* static */
MediaDecoderReader* DecoderTraits::CreateReader(const nsACString& aType, AbstractMediaDecoder* aDecoder)
{
  MOZ_ASSERT(NS_IsMainThread());
  MediaDecoderReader* decoderReader = nullptr;

  if (!aDecoder) {
    return decoderReader;
  }
#ifdef MOZ_FMP4
  if (IsMP4SupportedType(aType, /* DecoderDoctorDiagnostics* */ nullptr)) {
    decoderReader = new MediaFormatReader(aDecoder, new MP4Demuxer(aDecoder->GetResource()));
  } else
#endif
  if (IsMP3SupportedType(aType)) {
    decoderReader = new MediaFormatReader(aDecoder, new MP3Demuxer(aDecoder->GetResource()));
  } else
  if (IsAACSupportedType(aType)) {
    decoderReader = new MediaFormatReader(aDecoder, new ADTSDemuxer(aDecoder->GetResource()));
  } else
  if (IsWaveSupportedType(aType)) {
    decoderReader = new MediaFormatReader(aDecoder, new WAVDemuxer(aDecoder->GetResource()));
  } else
  if (IsFlacSupportedType(aType)) {
    decoderReader = new MediaFormatReader(aDecoder, new FlacDemuxer(aDecoder->GetResource()));
  } else
  if (IsOggSupportedType(aType)) {
    decoderReader = new MediaFormatReader(aDecoder, new OggDemuxer(aDecoder->GetResource()));
  } else
  if (IsWebMSupportedType(aType)) {
    decoderReader =
      new MediaFormatReader(aDecoder, new WebMDemuxer(aDecoder->GetResource()));
  }

  return decoderReader;
}

/* static */
bool DecoderTraits::IsSupportedInVideoDocument(const nsACString& aType)
{
  // Forbid playing media in video documents if the user has opted
  // not to, using either the legacy WMF specific pref, or the newer
  // catch-all pref.
  if (!Preferences::GetBool("media.windows-media-foundation.play-stand-alone", true) ||
      !Preferences::GetBool("media.play-stand-alone", true)) {
    return false;
  }

  return
    IsOggSupportedType(aType) ||
    IsWebMSupportedType(aType) ||
#ifdef MOZ_FMP4
    IsMP4SupportedType(aType, /* DecoderDoctorDiagnostics* */ nullptr) ||
#endif
    IsMP3SupportedType(aType) ||
    IsAACSupportedType(aType) ||
    IsWaveSupportedType(aType) ||
    IsFlacSupportedType(aType) ||
    false;
}

} // namespace mozilla
