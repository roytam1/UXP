/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_RegExpObject_h
#define vm_RegExpObject_h

#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"

#include "jscntxt.h"

#include "builtin/SelfHostingDefines.h"
#include "gc/Marking.h"
#include "gc/Zone.h"
#include "js/RegExpFlags.h"
#include "proxy/Proxy.h"
#include "vm/ArrayObject.h"
#include "vm/Shape.h"

/*
 * JavaScript Regular Expressions
 *
 * There are several engine concepts associated with a single logical regexp:
 *
 *   RegExpObject - The JS-visible object whose .[[Class]] equals "RegExp"
 *
 *   RegExpShared - The compiled representation of the regexp.
 *
 *   RegExpCompartment - Owns all RegExpShared instances in a compartment.
 *
 * To save memory, a RegExpShared is not created for a RegExpObject until it is
 * needed for execution. When a RegExpShared needs to be created, it is looked
 * up in a per-compartment table to allow reuse between objects. Lastly, on
 * GC, every RegExpShared (that is not active on the callstack) is discarded.
 * Because of the last point, any code using a RegExpShared (viz., by executing
 * a regexp) must indicate the RegExpShared is active via RegExpGuard.
 */
namespace js {

struct MatchPair;
class MatchPairs;
class RegExpShared;
class RegExpStatics;

namespace frontend { class TokenStream; }

enum RegExpRunStatus
{
    RegExpRunStatus_Error,
    RegExpRunStatus_Success,
    RegExpRunStatus_Success_NotFound
};

extern RegExpObject*
RegExpAlloc(ExclusiveContext* cx, HandleObject proto = nullptr);

// |regexp| is under-typed because this function's used in the JIT.
extern JSObject*
CloneRegExpObject(JSContext* cx, JSObject* regexp);

/*
 * A RegExpShared is the compiled representation of a regexp. A RegExpShared is
 * potentially pointed to by multiple RegExpObjects. Additionally, C++ code may
 * have pointers to RegExpShareds on the stack. The RegExpShareds are kept in a
 * table so that they can be reused when compiling the same regex string.
 *
 * During a GC, RegExpShared instances are marked and swept like GC things.
 * Usually, RegExpObjects clear their pointers to their RegExpShareds rather
 * than explicitly tracing them, so that the RegExpShared and any jitcode can
 * be reclaimed quicker. However, the RegExpShareds are traced through by
 * objects when we are preserving jitcode in their zone, to avoid the same
 * recompilation inefficiencies as normal Ion and baseline compilation.
 */
class RegExpShared
{
  public:
    enum ForceByteCodeEnum {
        DontForceByteCode,
        ForceByteCode
    };

    enum class Kind {
        Unparsed,
        Atom,
        RegExp
    };
    
  private:
    friend class RegExpCompartment;
    friend class RegExpStatics;

    typedef frontend::TokenStream TokenStream;

    struct RegExpCompilation
    {
        HeapPtr<jit::JitCode*> jitCode;
        uint8_t* byteCode;

        RegExpCompilation() : byteCode(nullptr) {}
        ~RegExpCompilation() { js_free(byteCode); }

        bool compiled(ForceByteCodeEnum force = DontForceByteCode) const {
            return byteCode || (force == DontForceByteCode && jitCode);
        }
    };

    /* Source to the RegExp, for lazy compilation. */
    HeapPtr<JSAtom*>     source;

    JS::RegExpFlags    flags;
    size_t             pairCount_;

#ifdef JS_NEW_REGEXP
    RegExpShared::Kind kind_ = Kind::Unparsed;
    GCPtrAtom patternAtom_;
#else
    bool canStringMatch = false;
#endif
    bool               marked_;

    RegExpCompilation  compilationArray[2];

    static int CompilationIndex(bool latin1) { return latin1 ? 0 : 1; }

    // Tables referenced by JIT code.
    Vector<uint8_t*, 0, SystemAllocPolicy> tables;

    /* Internal functions. */
    bool compile(JSContext* cx, HandleLinearString input,
                 ForceByteCodeEnum force);
    bool compile(JSContext* cx, HandleAtom pattern, HandleLinearString input,
                 ForceByteCodeEnum force);

    bool compileIfNecessary(JSContext* cx, HandleLinearString input,
                            ForceByteCodeEnum force);

    const RegExpCompilation& compilation(bool latin1) const {
        return compilationArray[CompilationIndex(latin1)];
    }

    RegExpCompilation& compilation(bool latin1) {
        return compilationArray[CompilationIndex(latin1)];
    }

  public:
    RegExpShared(JSAtom* source, JS::RegExpFlags flags);
    ~RegExpShared();

    RegExpRunStatus executeAtom(JSContext* cx,
                                HandleLinearString input,
                                size_t start,
                                MatchPairs* matches);

    // Execute this RegExp on input starting from searchIndex, filling in matches.
    RegExpRunStatus execute(JSContext* cx, HandleLinearString input, size_t searchIndex,
                            MatchPairs* matches);

    // Register a table with this RegExpShared, and take ownership.
    bool addTable(uint8_t* table) {
        return tables.append(table);
    }

    /* Accessors */

    size_t pairCount() const {
#ifdef JS_NEW_REGEXP
        MOZ_ASSERT(kind() != Kind::Unparsed);
#else
        MOZ_ASSERT(isCompiled());
#endif
        return pairCount_;
    }

#ifdef JS_NEW_REGEXP
    RegExpShared::Kind kind() const { return kind_; }

    // Use simple string matching for this regexp.
    void useAtomMatch(HandleAtom pattern);
#endif

    JSAtom* getSource() const           { return source; }
#ifdef JS_NEW_REGEXP
    JSAtom* patternAtom() const         { return patternAtom_; }
#else
    JSAtom* patternAtom() const         { return getSource(); }
#endif
    JS::RegExpFlags getFlags() const    { return flags; }

    bool global() const                 { return flags.global(); }
    bool ignoreCase() const             { return flags.ignoreCase(); }
    bool multiline() const              { return flags.multiline(); }
    bool dotall() const                 { return flags.dotAll(); }
    bool unicode() const                { return flags.unicode(); }
    bool sticky() const                 { return flags.sticky(); }

    bool isCompiled(bool latin1,
                    ForceByteCodeEnum force = DontForceByteCode) const {
        return compilation(latin1).compiled(force);
    }
    bool isCompiled() const { return isCompiled(true) || isCompiled(false); }

    void trace(JSTracer* trc);
    bool needsSweep(JSRuntime* rt);
    void discardJitCode();

    bool marked() const { return marked_; }
    void clearMarked() { marked_ = false; }

    bool isMarkedGray() const;
    void unmarkGray();

    static size_t offsetOfSource() {
        return offsetof(RegExpShared, source);
    }

    static size_t offsetOfFlags() {
        return offsetof(RegExpShared, flags);
    }

    static size_t offsetOfPairCount() {
        return offsetof(RegExpShared, pairCount_);
    }

    static size_t offsetOfJitCode(bool latin1) {
        return offsetof(RegExpShared, compilationArray) +
               (CompilationIndex(latin1) * sizeof(RegExpCompilation)) +
               offsetof(RegExpCompilation, jitCode);
    }

    size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

#ifdef DEBUG
    bool dumpBytecode(JSContext* cx, HandleLinearString input);
#endif
};

/*
 * Extend the lifetime of a given RegExpShared to at least the lifetime of
 * the guard object. See Regular Expression comment at the top.
 */
class RegExpGuard : public JS::CustomAutoRooter
{
    RegExpShared* re_;

    RegExpGuard(const RegExpGuard&) = delete;
    void operator=(const RegExpGuard&) = delete;

  public:
    explicit RegExpGuard(ExclusiveContext* cx)
      : CustomAutoRooter(cx), re_(nullptr)
    {}

    RegExpGuard(ExclusiveContext* cx, RegExpShared& re)
      : CustomAutoRooter(cx), re_(nullptr)
    {
        init(re);
    }

    ~RegExpGuard() {
        release();
    }

  public:
    void init(RegExpShared& re) {
        MOZ_ASSERT(!initialized());
        re_ = &re;
    }

    void release() {
        re_ = nullptr;
    }

    virtual void trace(JSTracer* trc) {
        if (re_)
            re_->trace(trc);
    }

    bool initialized() const { return !!re_; }
    RegExpShared* re() const { MOZ_ASSERT(initialized()); return re_; }
    RegExpShared* operator->() { return re(); }
    RegExpShared& operator*() { return *re(); }
};

class RegExpCompartment
{
    struct Key {
        JSAtom* atom = nullptr;
        JS::RegExpFlags flags = JS::RegExpFlag::NoFlags;

        Key() = default;
        Key(JSAtom* atom, JS::RegExpFlags flags)
          : atom(atom), flags(flags)
        { }
        MOZ_IMPLICIT Key(RegExpShared* shared)
          : atom(shared->getSource()), flags(shared->getFlags())
        { }

        typedef Key Lookup;
        static HashNumber hash(const Lookup& l) {
            return DefaultHasher<JSAtom*>::hash(l.atom) ^ (l.flags.value() << 1);
        }
        static bool match(Key l, Key r) {
            return l.atom == r.atom && l.flags == r.flags;
        }
    };

    /*
     * The set of all RegExpShareds in the compartment. On every GC, every
     * RegExpShared that was not marked is deleted and removed from the set.
     */
    typedef HashSet<RegExpShared*, Key, RuntimeAllocPolicy> Set;
    Set set_;

    /*
     * This is the template object where the result of re.exec() is based on,
     * if there is a result. This is used in CreateRegExpMatchResult to set
     * the input/index properties faster.
     */
    ReadBarriered<ArrayObject*> matchResultTemplateObject_;

    /*
     * The shape of RegExp.prototype object that satisfies following:
     *   * RegExp.prototype.flags getter is not modified
     *   * RegExp.prototype.global getter is not modified
     *   * RegExp.prototype.ignoreCase getter is not modified
     *   * RegExp.prototype.multiline getter is not modified
     *   * RegExp.prototype.sticky getter is not modified
     *   * RegExp.prototype.unicode getter is not modified
     *   * RegExp.prototype.exec is an own data property
     *   * RegExp.prototype[@@match] is an own data property
     *   * RegExp.prototype[@@search] is an own data property
     */
    ReadBarriered<Shape*> optimizableRegExpPrototypeShape_;

    /*
     * The shape of RegExp instance that satisfies following:
     *   * lastProperty is lastIndex
     *   * prototype is RegExp.prototype
     */
    ReadBarriered<Shape*> optimizableRegExpInstanceShape_;

    ArrayObject* createMatchResultTemplateObject(JSContext* cx);

  public:
    explicit RegExpCompartment(JSRuntime* rt);
    ~RegExpCompartment();

    bool init(JSContext* cx);
    void sweep(JSRuntime* rt);

    bool empty() { return set_.empty(); }

    bool get(JSContext* cx, JSAtom* source, JS::RegExpFlags flags, RegExpGuard* g);

    /* Like 'get', but compile 'maybeOpt' (if non-null). */
    bool get(JSContext* cx, HandleAtom source, JSString* maybeOpt, RegExpGuard* g);

    /* Get or create template object used to base the result of .exec() on. */
    ArrayObject* getOrCreateMatchResultTemplateObject(JSContext* cx) {
        if (matchResultTemplateObject_)
            return matchResultTemplateObject_;
        return createMatchResultTemplateObject(cx);
    }

    Shape* getOptimizableRegExpPrototypeShape() {
        return optimizableRegExpPrototypeShape_;
    }
    void setOptimizableRegExpPrototypeShape(Shape* shape) {
        optimizableRegExpPrototypeShape_ = shape;
    }
    Shape* getOptimizableRegExpInstanceShape() {
        return optimizableRegExpInstanceShape_;
    }
    void setOptimizableRegExpInstanceShape(Shape* shape) {
        optimizableRegExpInstanceShape_ = shape;
    }

    static size_t offsetOfOptimizableRegExpPrototypeShape() {
        return offsetof(RegExpCompartment, optimizableRegExpPrototypeShape_);
    }
    static size_t offsetOfOptimizableRegExpInstanceShape() {
        return offsetof(RegExpCompartment, optimizableRegExpInstanceShape_);
    }

    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf);
};

class RegExpObject : public NativeObject
{
    static const unsigned LAST_INDEX_SLOT          = 0;
    static const unsigned SOURCE_SLOT              = 1;
    static const unsigned FLAGS_SLOT               = 2;

    static_assert(RegExpObject::FLAGS_SLOT == REGEXP_FLAGS_SLOT,
                  "FLAGS_SLOT values should be in sync with self-hosted JS");

  public:
    static const unsigned RESERVED_SLOTS = 3;
    static const unsigned PRIVATE_SLOT = 3;

    static const Class class_;
    static const Class protoClass_;

    // The maximum number of pairs a MatchResult can have, without having to
    // allocate a bigger MatchResult.
    static const size_t MaxPairCount = 14;

    static RegExpObject*
    create(ExclusiveContext* cx, const char16_t* chars, size_t length, JS::RegExpFlags flags,
           frontend::TokenStream* ts, LifoAlloc& alloc);

    static RegExpObject*
    create(ExclusiveContext* cx, HandleAtom atom, JS::RegExpFlags flags,
           frontend::TokenStream* ts, LifoAlloc& alloc);

    /*
     * Compute the initial shape to associate with fresh RegExp objects,
     * encoding their initial properties. Return the shape after
     * changing |obj|'s last property to it.
     */
    static Shape*
    assignInitialShape(ExclusiveContext* cx, Handle<RegExpObject*> obj);

    /* Accessors. */

    static unsigned lastIndexSlot() { return LAST_INDEX_SLOT; }

    static bool isInitialShape(RegExpObject* rx) {
        Shape* shape = rx->lastProperty();
        if (!shape->hasSlot())
            return false;
        if (shape->maybeSlot() != LAST_INDEX_SLOT)
            return false;
        return true;
    }

    const Value& getLastIndex() const { return getSlot(LAST_INDEX_SLOT); }

    void setLastIndex(double d) {
        setSlot(LAST_INDEX_SLOT, NumberValue(d));
    }

    void zeroLastIndex(ExclusiveContext* cx) {
        MOZ_ASSERT(lookupPure(cx->names().lastIndex)->writable(),
                   "can't infallibly zero a non-writable lastIndex on a "
                   "RegExp that's been exposed to script");
        setSlot(LAST_INDEX_SLOT, Int32Value(0));
    }

    JSFlatString* toString(JSContext* cx) const;

    JSAtom* getSource() const { return &getSlot(SOURCE_SLOT).toString()->asAtom(); }

    void setSource(JSAtom* source) {
        setSlot(SOURCE_SLOT, StringValue(source));
    }

    /* Flags. */

    static unsigned flagsSlot() { return FLAGS_SLOT; }

    JS::RegExpFlags getFlags() const {
        return JS::RegExpFlags(getFixedSlot(FLAGS_SLOT).toInt32());
    }
    void setFlags(JS::RegExpFlags flags) {
        setFixedSlot(FLAGS_SLOT, Int32Value(flags.value()));
    }

    bool global() const     { return getFlags().global(); }
    bool ignoreCase() const { return getFlags().ignoreCase(); }
    bool multiline() const  { return getFlags().multiline(); }
    bool dotall() const     { return getFlags().dotAll(); }
    bool unicode() const    { return getFlags().unicode(); }
    bool sticky() const     { return getFlags().sticky(); }

    static bool isOriginalFlagGetter(JSNative native, JS::RegExpFlags* mask);

    static MOZ_MUST_USE bool getShared(JSContext* cx, Handle<RegExpObject*> regexp,
                                       RegExpGuard* g);

    void setShared(RegExpShared& shared) {
        MOZ_ASSERT(!maybeShared());
        NativeObject::setPrivate(&shared);
    }

    static void trace(JSTracer* trc, JSObject* obj);

    void initIgnoringLastIndex(HandleAtom source, JS::RegExpFlags flags);

    // NOTE: This method is *only* safe to call on RegExps that haven't been
    //       exposed to script, because it requires that the "lastIndex"
    //       property be writable.
    void initAndZeroLastIndex(HandleAtom source, JS::RegExpFlags flags, ExclusiveContext* cx);

#ifdef DEBUG
    static MOZ_MUST_USE bool dumpBytecode(JSContext* cx, Handle<RegExpObject*> regexp,
                                          bool match_only, HandleLinearString input);
#endif

  private:
    /*
     * Precondition: the syntax for |source| has already been validated.
     * Side effect: sets the private field.
     */
    static MOZ_MUST_USE bool createShared(JSContext* cx, Handle<RegExpObject*> regexp,
                                          RegExpGuard* g);
    RegExpShared* maybeShared() const {
        return static_cast<RegExpShared*>(NativeObject::getPrivate(PRIVATE_SLOT));
    }

    /* Call setShared in preference to setPrivate. */
    void setPrivate(void* priv) = delete;
};

/*
 * Parse regexp flags. Report an error and return false if an invalid
 * sequence of flags is encountered (repeat/invalid flag).
 *
 * N.B. flagStr must be rooted.
 */
bool
ParseRegExpFlags(JSContext* cx, JSString* flagStr, JS::RegExpFlags* flagsOut);

/* Assuming GetBuiltinClass(obj) is ESClass::RegExp, return a RegExpShared for obj. */
inline bool
RegExpToShared(JSContext* cx, HandleObject obj, RegExpGuard* g)
{
    if (obj->is<RegExpObject>())
        return RegExpObject::getShared(cx, obj.as<RegExpObject>(), g);

    return Proxy::regexp_toShared(cx, obj, g);
}

template<XDRMode mode>
bool
XDRScriptRegExpObject(XDRState<mode>* xdr, MutableHandle<RegExpObject*> objp);

extern JSObject*
CloneScriptRegExpObject(JSContext* cx, RegExpObject& re);

/* Escape all slashes and newlines in the given string. */
extern JSAtom*
EscapeRegExpPattern(JSContext* cx, HandleAtom src);

template <typename CharT>
extern bool
HasRegExpMetaChars(const CharT* chars, size_t length);

extern bool
StringHasRegExpMetaChars(JSLinearString* str);

} /* namespace js */

#endif /* vm_RegExpObject_h */
