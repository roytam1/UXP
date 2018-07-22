/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BUFFER_READER_H_
#define BUFFER_READER_H_

#include "mozilla/EndianUtils.h"
#include "mozilla/Vector.h"
#include "nsTArray.h"
#include "MediaData.h"

namespace mp4_demuxer {

class MOZ_RAII BufferReader
{
public:
  BufferReader() : mPtr(nullptr), mRemaining(0) {}
  explicit BufferReader(const mozilla::Vector<uint8_t>& aData)
    : mPtr(aData.begin()), mRemaining(aData.length()), mLength(aData.length())
  {
  }
  BufferReader(const uint8_t* aData, size_t aSize)
    : mPtr(aData), mRemaining(aSize), mLength(aSize)
  {
  }
  template<size_t S>
  explicit BufferReader(const AutoTArray<uint8_t, S>& aData)
    : mPtr(aData.Elements()), mRemaining(aData.Length()), mLength(aData.Length())
  {
  }
  explicit BufferReader(const nsTArray<uint8_t>& aData)
    : mPtr(aData.Elements()), mRemaining(aData.Length()), mLength(aData.Length())
  {
  }
  explicit BufferReader(const mozilla::MediaByteBuffer* aData)
    : mPtr(aData->Elements()), mRemaining(aData->Length()), mLength(aData->Length())
  {
  }

  void SetData(const nsTArray<uint8_t>& aData)
  {
    MOZ_ASSERT(!mPtr && !mRemaining);
    mPtr = aData.Elements();
    mRemaining = aData.Length();
    mLength = mRemaining;
  }

  ~BufferReader()
  {
  }

  size_t Offset() const
  {
    return mLength - mRemaining;
  }

  size_t Remaining() const { return mRemaining; }

  bool CanRead8() const { return mRemaining >= 1; }

  bool ReadU8(uint8_t& aU8)
  {
    const uint8_t* ptr;
    if (!Read(1, &ptr)) {
      NS_WARNING("Failed to read data");
      return false;
    }
    aU8 = *ptr;
    return true;
  }

  bool CanRead16() { return mRemaining >= 2; }

  bool ReadU16(uint16_t& u16)
  {
    const uint8_t* ptr;
    if (!Read(2, &ptr)) {
      NS_WARNING("Failed to read data");
      return false;
    }
    u16 = mozilla::BigEndian::readUint16(ptr);
    return true;
  }

  bool CanRead32() { return mRemaining >= 4; }

  bool ReadU32(uint32_t& u32)
  {
    const uint8_t* ptr;
    if (!Read(4, &ptr)) {
      NS_WARNING("Failed to read data");
      return false;
    }
    u32 = mozilla::BigEndian::readUint32(ptr);
    return true;
  }

  bool Read32(int32_t& i32)
  {
    const uint8_t* ptr;
    if (!Read(4, &ptr)) {
      NS_WARNING("Failed to read data");
      return 0;
    }
    i32 = mozilla::BigEndian::readInt32(ptr);
    return true;
  }

  bool ReadU64(uint64_t& u64)
  {
    const uint8_t* ptr;
    if (!Read(8, &ptr)) {
      NS_WARNING("Failed to read data");
      return false;
    }
    u64 = mozilla::BigEndian::readUint64(ptr);
    return true;
  }

  bool Read64(int64_t& i64)
  {
    const uint8_t* ptr;
    if (!Read(8, &ptr)) {
      NS_WARNING("Failed to read data");
      return false;
    }
    i64 = mozilla::BigEndian::readInt64(ptr);
    return true;
  }

  bool ReadU24(uint32_t& u32)
  {
    const uint8_t* ptr;
    if (!Read(3, &ptr)) {
      NS_WARNING("Failed to read data");
      return false;
    }
    u32 = ptr[0] << 16 | ptr[1] << 8 | ptr[2];
    return true;
  }

  bool Skip(size_t aCount)
  {
    const uint8_t* ptr;
    if (!Read(aCount, &ptr)) {
      NS_WARNING("Failed to skip data");
      return false;
    }
    return true;
  }

  bool Read(size_t aCount, const uint8_t** aPtr)
  {
    if (aCount > mRemaining) {
      mRemaining = 0;
      return false;
    }
    mRemaining -= aCount;

    *aPtr = mPtr;
    mPtr += aCount;

    return true;
  }

  uint32_t Align() const
  {
    return 4 - ((intptr_t)mPtr & 3);
  }

  template <typename T> bool CanReadType() const { return mRemaining >= sizeof(T); }

  template <typename T> T ReadType()
  {
    const uint8_t* ptr;
    if (!Read(sizeof(T), &ptr)) {
      NS_WARNING("ReadType failed");
      return 0;
    }
    return *reinterpret_cast<const T*>(ptr);
  }

  template <typename T>
  MOZ_MUST_USE bool ReadArray(nsTArray<T>& aDest, size_t aLength)
  {
    const uint8_t* ptr;
    if (!Read(aLength * sizeof(T), &ptr)) {
      NS_WARNING("ReadArray failed");
      return false;
    }

    aDest.Clear();
    aDest.AppendElements(reinterpret_cast<const T*>(ptr), aLength);
    return true;
  }

  template <typename T>
  MOZ_MUST_USE bool ReadArray(FallibleTArray<T>& aDest, size_t aLength)
  {
    const uint8_t* ptr;
    if (!Read(aLength * sizeof(T), &ptr)) {
      NS_WARNING("ReadArray failed");
      return false;
    }

    aDest.Clear();
    if (!aDest.SetCapacity(aLength, mozilla::fallible)) {
      return false;
    }
    MOZ_ALWAYS_TRUE(aDest.AppendElements(reinterpret_cast<const T*>(ptr),
                                         aLength,
                                         mozilla::fallible));
    return true;
  }

private:
  const uint8_t* mPtr;
  size_t mRemaining;
  size_t mLength;
};

} // namespace mp4_demuxer

#endif
