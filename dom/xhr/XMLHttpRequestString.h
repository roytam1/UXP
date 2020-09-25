/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XMLHttpRequestString_h
#define mozilla_dom_XMLHttpRequestString_h

#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/DOMString.h"
#include "nsString.h"

namespace mozilla {

class Mutex;

namespace dom {

class DOMString;
class XMLHttpRequestStringBuffer;
class XMLHttpRequestStringSnapshot;
class XMLHttpRequestStringWriterHelper;
class XMLHttpRequestStringSnapshotReaderHelper;

// We want to avoid the dup of strings when XHR in workers has access to
// responseText for events dispatched during the loading state. For this reason
// we use this class, able to create snapshots of the current size of itself
// without making extra copies.
class XMLHttpRequestString final
{
  friend class XMLHttpRequestStringWriterHelper;

public:
  XMLHttpRequestString();
  ~XMLHttpRequestString();

  void Truncate();

  uint32_t Length() const;

  void Append(const nsAString& aString);

  // This method should be called only when the string is really needed because
  // it can cause the duplication of the strings in case the loading of the XHR
  // is not completed yet.
  MOZ_MUST_USE bool GetAsString(nsAString& aString) const;

  size_t SizeOfThis(MallocSizeOf aMallocSizeOf) const;

  const char16_t* Data() const;

  bool IsEmpty() const;

  void CreateSnapshot(XMLHttpRequestStringSnapshot& aSnapshot);

private:
  XMLHttpRequestString(const XMLHttpRequestString&) = delete;
  XMLHttpRequestString& operator=(const XMLHttpRequestString&) = delete;
  XMLHttpRequestString& operator=(const XMLHttpRequestString&&) = delete;

  RefPtr<XMLHttpRequestStringBuffer> mBuffer;
};

// This class locks the buffer and allows the callee to write data into it.
class MOZ_STACK_CLASS XMLHttpRequestStringWriterHelper final
{
public:
  explicit XMLHttpRequestStringWriterHelper(XMLHttpRequestString& aString);

  bool
  AddCapacity(int32_t aCapacity);

  char16_t*
  EndOfExistingData();

  void
  AddLength(int32_t aLength);

private:
  XMLHttpRequestStringWriterHelper(const XMLHttpRequestStringWriterHelper&) = delete;
  XMLHttpRequestStringWriterHelper& operator=(const XMLHttpRequestStringWriterHelper&) = delete;
  XMLHttpRequestStringWriterHelper& operator=(const XMLHttpRequestStringWriterHelper&&) = delete;

  RefPtr<XMLHttpRequestStringBuffer> mBuffer;
  MutexAutoLock mLock;
};

// This class is the internal XMLHttpRequestStringBuffer of the
// XMLHttpRequestString plus the current length. GetAsString will return the
// string with that particular length also if the XMLHttpRequestStringBuffer is
// grown in the meantime.
class XMLHttpRequestStringSnapshot final
{
  friend class XMLHttpRequestStringBuffer;
  friend class XMLHttpRequestStringSnapshotReaderHelper;

public:
  XMLHttpRequestStringSnapshot();
  ~XMLHttpRequestStringSnapshot();

  XMLHttpRequestStringSnapshot& operator=(const XMLHttpRequestStringSnapshot&);

  void Reset()
  {
    ResetInternal(false /* isVoid */);
  }

  void SetVoid()
  {
    ResetInternal(true /* isVoid */);
  }

  bool IsVoid() const
  {
    return mVoid;
  }

  bool IsEmpty() const
  {
    return !mLength;
  }

  MOZ_MUST_USE bool GetAsString(DOMString& aString) const;

private:
  XMLHttpRequestStringSnapshot(const XMLHttpRequestStringSnapshot&) = delete;
  XMLHttpRequestStringSnapshot& operator=(const XMLHttpRequestStringSnapshot&&) = delete;

  void Set(XMLHttpRequestStringBuffer* aBuffer, uint32_t aLength);

  void ResetInternal(bool aIsVoid);

  RefPtr<XMLHttpRequestStringBuffer> mBuffer;
  uint32_t mLength;
  bool mVoid;
};

// This class locks the buffer and allows the callee to read data from it.
class MOZ_STACK_CLASS XMLHttpRequestStringSnapshotReaderHelper final
{
public:
  explicit XMLHttpRequestStringSnapshotReaderHelper(XMLHttpRequestStringSnapshot& aSnapshot);

  const char16_t*
  Buffer() const;

  uint32_t
  Length() const;

private:
  XMLHttpRequestStringSnapshotReaderHelper(const XMLHttpRequestStringSnapshotReaderHelper&) = delete;
  XMLHttpRequestStringSnapshotReaderHelper& operator=(const XMLHttpRequestStringSnapshotReaderHelper&) = delete;
  XMLHttpRequestStringSnapshotReaderHelper& operator=(const XMLHttpRequestStringSnapshotReaderHelper&&) = delete;

  RefPtr<XMLHttpRequestStringBuffer> mBuffer;
  MutexAutoLock mLock;
};

class XMLHttpRequestStringBuffer final
{
  friend class XMLHttpRequestStringWriterHelper;
  friend class XMLHttpRequestStringSnapshotReaderHelper;

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(XMLHttpRequestStringBuffer)
  NS_DECL_OWNINGTHREAD

  XMLHttpRequestStringBuffer()
    : mMutex("XMLHttpRequestStringBuffer::mMutex")
  {
  }

  uint32_t
  Length()
  {
    MutexAutoLock lock(mMutex);
    return mData.Length();
  }

  uint32_t
  UnsafeLength() const
  {
    return mData.Length();
  }

  void
  Append(const nsAString& aString)
  {
    NS_ASSERT_OWNINGTHREAD(XMLHttpRequestStringBuffer);

    MutexAutoLock lock(mMutex);
    mData.Append(aString);
  }

  MOZ_MUST_USE bool
  GetAsString(nsAString& aString)
  {
    MutexAutoLock lock(mMutex);
    return aString.Assign(mData, mozilla::fallible);
  }

  size_t
  SizeOfThis(MallocSizeOf aMallocSizeOf) const
  {
    return mData.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

  MOZ_MUST_USE bool
  GetAsString(DOMString& aString, uint32_t aLength)
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(aLength <= mData.Length());
    nsStringBuffer* buf = nsStringBuffer::FromString(mData);
    if (buf) {
      // We have to use SetEphemeralStringBuffer, because once we release our
      // mutex mData can get mutated from some other thread while the DOMString
      // is still alive.
      aString.SetEphemeralStringBuffer(buf, aLength);
      return true;
    }

    // We can get here if mData is empty.  In that case it won't have an
    // nsStringBuffer....
    MOZ_ASSERT(mData.IsEmpty());
    return aString.AsAString().Assign(mData.BeginReading(), aLength,
                                      mozilla::fallible);
  }

  void
  CreateSnapshot(XMLHttpRequestStringSnapshot& aSnapshot)
  {
    MutexAutoLock lock(mMutex);
    aSnapshot.Set(this, mData.Length());
  }

private:
  ~XMLHttpRequestStringBuffer()
  {}

  nsString& UnsafeData()
  {
    return mData;
  }

  Mutex mMutex;

  // The following member variable is protected by mutex.
  nsString mData;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_XMLHttpRequestString_h
