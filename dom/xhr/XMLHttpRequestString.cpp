/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XMLHttpRequestString.h"
#include "mozilla/Mutex.h"
#include "nsISupportsImpl.h"
#include "mozilla/dom/DOMString.h"

namespace mozilla {
namespace dom {


// ---------------------------------------------------------------------------
// XMLHttpRequestString

XMLHttpRequestString::XMLHttpRequestString()
  : mBuffer(new XMLHttpRequestStringBuffer())
{
}

XMLHttpRequestString::~XMLHttpRequestString()
{
}

void
XMLHttpRequestString::Truncate()
{
  mBuffer = new XMLHttpRequestStringBuffer();
}

uint32_t
XMLHttpRequestString::Length() const
{
  return mBuffer->Length();
}

void
XMLHttpRequestString::Append(const nsAString& aString)
{
  mBuffer->Append(aString);
}

bool
XMLHttpRequestString::GetAsString(nsAString& aString) const
{
  return mBuffer->GetAsString(aString);
}

size_t
XMLHttpRequestString::SizeOfThis(MallocSizeOf aMallocSizeOf) const
{
  return mBuffer->SizeOfThis(aMallocSizeOf);
}

bool
XMLHttpRequestString::IsEmpty() const
{
  return !mBuffer->Length();
}

void
XMLHttpRequestString::CreateSnapshot(XMLHttpRequestStringSnapshot& aSnapshot)
{
  mBuffer->CreateSnapshot(aSnapshot);
}

// ---------------------------------------------------------------------------
// XMLHttpRequestStringSnapshot

XMLHttpRequestStringSnapshot::XMLHttpRequestStringSnapshot()
  : mLength(0)
  , mVoid(false)
{
}

XMLHttpRequestStringSnapshot::~XMLHttpRequestStringSnapshot()
{
}

XMLHttpRequestStringSnapshot&
XMLHttpRequestStringSnapshot::operator=(const XMLHttpRequestStringSnapshot& aOther)
{
  mBuffer = aOther.mBuffer;
  mLength = aOther.mLength;
  mVoid = aOther.mVoid;
  return *this;
}

void
XMLHttpRequestStringSnapshot::ResetInternal(bool aIsVoid)
{
  mBuffer = nullptr;
  mLength = 0;
  mVoid = aIsVoid;
}

void
XMLHttpRequestStringSnapshot::Set(XMLHttpRequestStringBuffer* aBuffer,
                                  uint32_t aLength)
{
  MOZ_ASSERT(aBuffer);
  MOZ_ASSERT(aLength <= aBuffer->UnsafeLength());

  mBuffer = aBuffer;
  mLength = aLength;
  mVoid = false;
}

bool
XMLHttpRequestStringSnapshot::GetAsString(DOMString& aString) const
{
  if (mBuffer) {
    MOZ_ASSERT(!mVoid);
    return mBuffer->GetAsString(aString, mLength);
  }

  if (mVoid) {
    aString.SetNull();
  }

  return true;
}

// ---------------------------------------------------------------------------
// XMLHttpRequestStringWriterHelper

XMLHttpRequestStringWriterHelper::XMLHttpRequestStringWriterHelper(XMLHttpRequestString& aString)
  : mBuffer(aString.mBuffer)
  , mLock(aString.mBuffer->mMutex)
{
}

bool
XMLHttpRequestStringWriterHelper::AddCapacity(int32_t aCapacity)
{
  return mBuffer->UnsafeData().SetCapacity(mBuffer->UnsafeLength() + aCapacity, fallible);
}

char16_t*
XMLHttpRequestStringWriterHelper::EndOfExistingData()
{
  return mBuffer->UnsafeData().BeginWriting() + mBuffer->UnsafeLength();
}

void
XMLHttpRequestStringWriterHelper::AddLength(int32_t aLength)
{
  mBuffer->UnsafeData().SetLength(mBuffer->UnsafeLength() + aLength);
}

// ---------------------------------------------------------------------------
// XMLHttpRequestStringReaderHelper

XMLHttpRequestStringSnapshotReaderHelper::XMLHttpRequestStringSnapshotReaderHelper(XMLHttpRequestStringSnapshot& aSnapshot)
  : mBuffer(aSnapshot.mBuffer)
  , mLock(aSnapshot.mBuffer->mMutex)
{
}

const char16_t*
XMLHttpRequestStringSnapshotReaderHelper::Buffer() const
{
  return mBuffer->UnsafeData().BeginReading();
}

uint32_t
XMLHttpRequestStringSnapshotReaderHelper::Length() const
{
  return mBuffer->UnsafeLength();
}

} // dom namespace
} // mozilla namespace
