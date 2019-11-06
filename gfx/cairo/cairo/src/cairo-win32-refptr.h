/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_WIN32_REFPTR_H
#define CAIRO_WIN32_REFPTR_H

template<typename T> class TemporaryRef;

/**
 * RefPtr points to a refcounted thing that has AddRef and Release
 * methods to increase/decrease the refcount, respectively.  After a
 * RefPtr<T> is assigned a T*, the T* can be used through the RefPtr
 * as if it were a T*.
 *
 * A RefPtr can forget its underlying T*, which results in the T*
 * being wrapped in a temporary object until the T* is either
 * re-adopted from or released by the temporary.
 */
template<typename T>
class RefPtr
{
    // To allow them to use unref()
    friend class TemporaryRef<T>;

    struct dontRef {};

public:
    RefPtr() : ptr(0) { }
    RefPtr(const RefPtr& o) : ptr(ref(o.ptr)) {}
    RefPtr(const TemporaryRef<T>& o) : ptr(o.drop()) {}
    RefPtr(T* t) : ptr(ref(t)) {}

    template<typename U>
    RefPtr(const RefPtr<U>& o) : ptr(ref(o.get())) {}

    ~RefPtr() { unref(ptr); }

    RefPtr& operator=(const RefPtr& o) {
        assign(ref(o.ptr));
        return *this;
    }
    RefPtr& operator=(const TemporaryRef<T>& o) {
        assign(o.drop());
        return *this;
    }
    RefPtr& operator=(T* t) {
        assign(ref(t));
        return *this;
    }

    template<typename U>
    RefPtr& operator=(const RefPtr<U>& o) {
        assign(ref(o.get()));
        return *this;
    }

    TemporaryRef<T> forget() {
        T* tmp = ptr;
        ptr = 0;
        return TemporaryRef<T>(tmp, dontRef());
    }

    T* get() const { return ptr; }
    operator T*() const { return ptr; }
    T* operator->() const { return ptr; }
    T& operator*() const { return *ptr; }
    template<typename U>
    operator TemporaryRef<U>() { return TemporaryRef<U>(ptr); }

    /** 
     * WARNING for ease of use, passing a reference will release/clear out ptr!
     * We null out the ptr before returning its address so people passing byref
     * as input will most likely get functions returning errors rather than accessing
     * freed memory. Further more accessing it after this point if it hasn't
     * been set will produce a null pointer dereference.
     */
    T** operator&()
    {
       if (ptr) {
           ptr->Release();
           ptr = NULL;
       }
       return &ptr;
    }

private:
    void assign(T* t) {
        unref(ptr);
        ptr = t;
    }

    T* ptr;

    static inline T* ref(T* t) {
        if (t) {
            t->AddRef();
        }
        return t;
    }

    static inline void unref(T* t) {
        if (t) {
            t->Release();
        }
    }
};

/**
 * TemporaryRef<T> represents an object that holds a temporary
 * reference to a T.  TemporaryRef objects can't be manually ref'd or
 * unref'd (being temporaries, not lvalues), so can only relinquish
 * references to other objects, or unref on destruction.
 */
template<typename T>
class TemporaryRef
{
    // To allow it to construct TemporaryRef from a bare T*
    friend class RefPtr<T>;

    typedef typename RefPtr<T>::dontRef dontRef;

public:
    TemporaryRef(T* t) : ptr(RefPtr<T>::ref(t)) {}
    TemporaryRef(const TemporaryRef& o) : ptr(o.drop()) {}

    template<typename U>
    TemporaryRef(const TemporaryRef<U>& o) : ptr(o.drop()) {}

    ~TemporaryRef() { RefPtr<T>::unref(ptr); }

    T* drop() const {
        T* tmp = ptr;
        ptr = 0;
        return tmp;
    }

private:
    TemporaryRef(T* t, const dontRef&) : ptr(t) {}

    mutable T* ptr;

    TemporaryRef();
    TemporaryRef& operator=(const TemporaryRef&);
};

#endif
