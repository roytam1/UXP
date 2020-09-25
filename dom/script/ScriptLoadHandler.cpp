/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A class that handles loading and evaluation of <script> elements.
 */

#include "ScriptLoadHandler.h"
#include "ScriptLoader.h"
#include "nsContentUtils.h"

#include "mozilla/dom/EncodingUtils.h"

namespace mozilla {
namespace dom {

ScriptLoadHandler::ScriptLoadHandler(ScriptLoader *aScriptLoader,
                                     ScriptLoadRequest *aRequest,
                                     mozilla::dom::SRICheckDataVerifier *aSRIDataVerifier)
  : mScriptLoader(aScriptLoader),
    mRequest(aRequest),
    mSRIDataVerifier(aSRIDataVerifier),
    mSRIStatus(NS_OK),
    mDecoder(),
    mBuffer()
{}

ScriptLoadHandler::~ScriptLoadHandler()
{}

NS_IMPL_ISUPPORTS(ScriptLoadHandler, nsIIncrementalStreamLoaderObserver)

NS_IMETHODIMP
ScriptLoadHandler::OnIncrementalData(nsIIncrementalStreamLoader* aLoader,
                                     nsISupports* aContext,
                                     uint32_t aDataLength,
                                     const uint8_t* aData,
                                     uint32_t *aConsumedLength)
{
  if (mRequest->IsCanceled()) {
    // If request cancelled, ignore any incoming data.
    *aConsumedLength = aDataLength;
    return NS_OK;
  }

  if (!EnsureDecoder(aLoader, aData, aDataLength,
                     /* aEndOfStream = */ false)) {
    return NS_OK;
  }

  // Below we will/shall consume entire data chunk.
  *aConsumedLength = aDataLength;

  // Decoder has already been initialized. -- trying to decode all loaded bytes.
  nsresult rv = TryDecodeRawData(aData, aDataLength,
                                 /* aEndOfStream = */ false);
  NS_ENSURE_SUCCESS(rv, rv);

  // If SRI is required for this load, appending new bytes to the hash.
  if (mSRIDataVerifier && NS_SUCCEEDED(mSRIStatus)) {
    mSRIStatus = mSRIDataVerifier->Update(aDataLength, aData);
  }

  return rv;
}

nsresult
ScriptLoadHandler::TryDecodeRawData(const uint8_t* aData,
                                    uint32_t aDataLength,
                                    bool aEndOfStream)
{
  int32_t srcLen = aDataLength;
  const char* src = reinterpret_cast<const char *>(aData);
  int32_t dstLen;
  nsresult rv =
    mDecoder->GetMaxLength(src, srcLen, &dstLen);

  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t haveRead = mBuffer.length();

  CheckedInt<uint32_t> capacity = haveRead;
  capacity += dstLen;

  if (!capacity.isValid() || !mBuffer.reserve(capacity.value())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = mDecoder->Convert(src,
                         &srcLen,
                         mBuffer.begin() + haveRead,
                         &dstLen);

  NS_ENSURE_SUCCESS(rv, rv);

  haveRead += dstLen;
  MOZ_ASSERT(haveRead <= capacity.value(), "mDecoder produced more data than expected");
  MOZ_ALWAYS_TRUE(mBuffer.resizeUninitialized(haveRead));

  return NS_OK;
}

bool
ScriptLoadHandler::EnsureDecoder(nsIIncrementalStreamLoader *aLoader,
                                 const uint8_t* aData,
                                 uint32_t aDataLength,
                                 bool aEndOfStream)
{
  // Check if decoder has already been created.
  if (mDecoder) {
    return true;
  }

  nsAutoCString charset;

  // JavaScript modules are always UTF-8.
  if (mRequest->IsModuleRequest()) {
    charset = "UTF-8";
    mDecoder = EncodingUtils::DecoderForEncoding(charset);
    return true;
  }

  // Determine if BOM check should be done.  This occurs either
  // if end-of-stream has been reached, or at least 3 bytes have
  // been read from input.
  if (!aEndOfStream && (aDataLength < 3)) {
    return false;
  }

  // Do BOM detection.
  if (nsContentUtils::CheckForBOM(aData, aDataLength, charset)) {
    mDecoder = EncodingUtils::DecoderForEncoding(charset);
    return true;
  }

  // BOM detection failed, check content stream for charset.
  nsCOMPtr<nsIRequest> req;
  nsresult rv = aLoader->GetRequest(getter_AddRefs(req));
  NS_ASSERTION(req, "StreamLoader's request went away prematurely");
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(req);

  if (channel &&
      NS_SUCCEEDED(channel->GetContentCharset(charset)) &&
      EncodingUtils::FindEncodingForLabel(charset, charset)) {
    mDecoder = EncodingUtils::DecoderForEncoding(charset);
    return true;
  }

  // Check the hint charset from the script element or preload
  // request.
  nsAutoString hintCharset;
  if (!mRequest->IsPreload()) {
    mRequest->mElement->GetScriptCharset(hintCharset);
  } else {
    nsTArray<ScriptLoader::PreloadInfo>::index_type i =
      mScriptLoader->mPreloads.IndexOf(mRequest, 0,
            ScriptLoader::PreloadRequestComparator());

    NS_ASSERTION(i != mScriptLoader->mPreloads.NoIndex,
                 "Incorrect preload bookkeeping");
    hintCharset = mScriptLoader->mPreloads[i].mCharset;
  }

  if (EncodingUtils::FindEncodingForLabel(hintCharset, charset)) {
    mDecoder = EncodingUtils::DecoderForEncoding(charset);
    return true;
  }

  // Get the charset from the charset of the document.
  if (mScriptLoader->mDocument) {
    charset = mScriptLoader->mDocument->GetDocumentCharacterSet();
    mDecoder = EncodingUtils::DecoderForEncoding(charset);
    return true;
  }

  // Curiously, there are various callers that don't pass aDocument. The
  // fallback in the old code was ISO-8859-1, which behaved like
  // windows-1252. Saying windows-1252 for clarity and for compliance
  // with the Encoding Standard.
  charset = "windows-1252";
  mDecoder = EncodingUtils::DecoderForEncoding(charset);
  return true;
}

NS_IMETHODIMP
ScriptLoadHandler::OnStreamComplete(nsIIncrementalStreamLoader* aLoader,
                                    nsISupports* aContext,
                                    nsresult aStatus,
                                    uint32_t aDataLength,
                                    const uint8_t* aData)
{
  if (!mRequest->IsCanceled()) {
    DebugOnly<bool> encoderSet =
      EnsureDecoder(aLoader, aData, aDataLength, /* aEndOfStream = */ true);
    MOZ_ASSERT(encoderSet);
    DebugOnly<nsresult> rv = TryDecodeRawData(aData, aDataLength,
                                              /* aEndOfStream = */ true);

    // If SRI is required for this load, appending new bytes to the hash.
    if (mSRIDataVerifier && NS_SUCCEEDED(mSRIStatus)) {
      mSRIStatus = mSRIDataVerifier->Update(aDataLength, aData);
    }
  }

  // we have to mediate and use mRequest.
  return mScriptLoader->OnStreamComplete(aLoader, mRequest, aStatus, mSRIStatus,
                                         mBuffer, mSRIDataVerifier);
}

} // dom namespace
} // mozilla namespace