/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcom-private.h"

//-----------------------------------------------------------------------------
// Windows
//-----------------------------------------------------------------------------
#if defined(XP_WIN)

#include <windows.h>
#include "nsString.h"
#include "nsAString.h"
#include "nsReadableUtils.h"

using namespace mozilla;

nsresult
NS_CopyNativeToUnicode(const nsACString& aInput, nsAString& aOutput)
{
  uint32_t inputLen = aInput.Length();

  nsACString::const_iterator iter;
  aInput.BeginReading(iter);

  const char* buf = iter.get();

  // determine length of result
  uint32_t resultLen = 0;
  int n = ::MultiByteToWideChar(CP_ACP, 0, buf, inputLen, nullptr, 0);
  if (n > 0) {
    resultLen += n;
  }

  // allocate sufficient space
  if (!aOutput.SetLength(resultLen, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (resultLen > 0) {
    nsAString::iterator out_iter;
    aOutput.BeginWriting(out_iter);

    char16_t* result = out_iter.get();

    ::MultiByteToWideChar(CP_ACP, 0, buf, inputLen, wwc(result), resultLen);
  }
  return NS_OK;
}

nsresult
NS_CopyUnicodeToNative(const nsAString&  aInput, nsACString& aOutput)
{
  uint32_t inputLen = aInput.Length();

  nsAString::const_iterator iter;
  aInput.BeginReading(iter);

  char16ptr_t buf = iter.get();

  // determine length of result
  uint32_t resultLen = 0;

  int n = ::WideCharToMultiByte(CP_ACP, 0, buf, inputLen, nullptr, 0,
                                nullptr, nullptr);
  if (n > 0) {
    resultLen += n;
  }

  // allocate sufficient space
  if (!aOutput.SetLength(resultLen, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (resultLen > 0) {
    nsACString::iterator out_iter;
    aOutput.BeginWriting(out_iter);

    // default "defaultChar" is '?', which is an illegal character on windows
    // file system.  That will cause file uncreatable. Change it to '_'
    const char defaultChar = '_';

    char* result = out_iter.get();

    ::WideCharToMultiByte(CP_ACP, 0, buf, inputLen, result, resultLen,
                          &defaultChar, nullptr);
  }
  return NS_OK;
}

// moved from widget/windows/nsToolkit.cpp
int32_t
NS_ConvertAtoW(const char* aStrInA, int aBufferSize, char16_t* aStrOutW)
{
  return MultiByteToWideChar(CP_ACP, 0, aStrInA, -1, wwc(aStrOutW), aBufferSize);
}

int32_t
NS_ConvertWtoA(const char16_t* aStrInW, int aBufferSizeOut,
               char* aStrOutA, const char* aDefault)
{
  if ((!aStrInW) || (!aStrOutA) || (aBufferSizeOut <= 0)) {
    return 0;
  }

  int numCharsConverted = WideCharToMultiByte(CP_ACP, 0, char16ptr_t(aStrInW), -1,
                                              aStrOutA, aBufferSizeOut,
                                              aDefault, nullptr);

  if (!numCharsConverted) {
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
      // Overflow, add missing null termination but return 0
      aStrOutA[aBufferSizeOut - 1] = '\0';
    } else {
      // Other error, clear string and return 0
      aStrOutA[0] = '\0';
    }
  } else if (numCharsConverted < aBufferSizeOut) {
    // Add 2nd null (really necessary?)
    aStrOutA[numCharsConverted] = '\0';
  }

  return numCharsConverted;
}

#else

// Non-windows will always use UTF-8 conversion.

#include "nsReadableUtils.h"

nsresult
NS_CopyNativeToUnicode(const nsACString& aInput, nsAString& aOutput)
{
  CopyUTF8toUTF16(aInput, aOutput);
  return NS_OK;
}

nsresult
NS_CopyUnicodeToNative(const nsAString& aInput, nsACString& aOutput)
{
  CopyUTF16toUTF8(aInput, aOutput);
  return NS_OK;
}

#endif
