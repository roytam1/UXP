/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_INFALLIBLEVECTOR_H_
#define V8_INFALLIBLEVECTOR_H_

namespace js {
namespace irregexp {

// InfallibleVector is like Vector, but all its methods are infallible (they
// crash on OOM). We use this class instead of Vector to avoid a ton of
// MOZ_MUST_USE warnings in irregexp code (imported from V8).
template<typename T, size_t N>
class InfallibleVector
{
    Vector<T, N, LifoAllocPolicy<Infallible>> vector_;

    InfallibleVector(const InfallibleVector&) = delete;
    void operator=(const InfallibleVector&) = delete;

  public:
    explicit InfallibleVector(const LifoAllocPolicy<Infallible>& alloc) : vector_(alloc) {}

    void append(const T& t) { MOZ_ALWAYS_TRUE(vector_.append(t)); }
    void append(const T* begin, size_t length) { MOZ_ALWAYS_TRUE(vector_.append(begin, length)); }

    // Move a number of elements in a zonelist to another position
    // in the same list. Handles overlapping source and target areas.
    void moveReplace(int from, int to, int count)
    {
        T* array = begin();
        if (from < to) {
            for (int i = count - 1; i >= 0; i--)
                array[to + i] = array[from + i];
        } else {
            for (int i = 0; i < count; i++)
                array[to + i] = array[from + i];
        }
    }

    void clear() { vector_.clear(); }
    void popBack() { vector_.popBack(); }
    void reserve(size_t n) { MOZ_ALWAYS_TRUE(vector_.reserve(n)); }


    size_t length() const { return vector_.length(); }
    T popCopy() { return vector_.popCopy(); }

    T* begin() { return vector_.begin(); }
    const T* begin() const { return vector_.begin(); }

    T* end() { return vector_.end(); }
    const T* end() const { return vector_.end(); }

    T& operator[](size_t index) { return vector_[index]; }
    const T& operator[](size_t index) const { return vector_[index]; }

    InfallibleVector& operator=(InfallibleVector&& rhs) { vector_ = Move(rhs.vector_); return *this; }

    bool equals(const InfallibleVector& other) const {
      if (length() != other.length()) {
        return false;
      }
      return 0 == memcmp(begin(), other.begin(), length() * sizeof(T));
    }
    inline bool operator==(const InfallibleVector& rhs) const {
      return equals(rhs);
    }
};

typedef InfallibleVector<char16_t, 10> CharacterVector;
typedef InfallibleVector<CharacterVector*, 1> CharacterVectorVector;
typedef InfallibleVector<int32_t, 10> IntegerVector;

} }  // namespace js::irregexp

#endif // V8_INFALLIBLEVECTOR_H_