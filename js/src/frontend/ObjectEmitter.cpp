/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/ObjectEmitter.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT

#include "jsatominlines.h"             // AtomToId
#include "jsgcinlines.h"               // GetGCObjectKind
#include "jsobjinlines.h"              // NewBuiltinClassInstance


#include "frontend/BytecodeEmitter.h"  // BytecodeEmitter
#include "frontend/SharedContext.h"    // SharedContext
#include "frontend/SourceNotes.h"      // SRC_*
#include "gc/Heap.h"                   // AllocKind
#include "js/Id.h"                     // jsid
#include "js/Value.h"                  // UndefinedHandleValue
#include "vm/NativeObject.h"           // NativeDefineDataProperty
#include "vm/ObjectGroup.h"            // TenuredObject
#include "vm/Runtime.h"                // JSAtomState (cx->names())

using namespace js;
using namespace js::frontend;

using mozilla::Maybe;

PropertyEmitter::PropertyEmitter(BytecodeEmitter* bce)
    : bce_(bce), obj_(bce->cx) {}

bool PropertyEmitter::prepareForProtoValue(const Maybe<uint32_t>& keyPos)
{
    MOZ_ASSERT(propertyState_ == PropertyState::Start ||
               propertyState_ == PropertyState::Init);

    //                [stack] CTOR? OBJ CTOR?

    if (keyPos) {
        if (!bce_->updateSourceCoordNotes(*keyPos))
            return false;
    }

#ifdef DEBUG
    propertyState_ = PropertyState::ProtoValue;
#endif
    return true;
}

bool PropertyEmitter::emitMutateProto()
{
    MOZ_ASSERT(propertyState_ == PropertyState::ProtoValue);

    //                [stack] OBJ PROTO

    if (!bce_->emit1(JSOP_MUTATEPROTO)) {
        //            [stack] OBJ
        return false;
    }

    obj_ = nullptr;
#ifdef DEBUG
    propertyState_ = PropertyState::Init;
#endif
    return true;
}

bool PropertyEmitter::prepareForSpreadOperand(const Maybe<uint32_t>& spreadPos)
{
    MOZ_ASSERT(propertyState_ == PropertyState::Start ||
               propertyState_ == PropertyState::Init);

    //                [stack] OBJ

    if (spreadPos) {
        if (!bce_->updateSourceCoordNotes(*spreadPos))
            return false;
    }
    if (!bce_->emit1(JSOP_DUP)) {
        //            [stack] OBJ OBJ
        return false;
    }

#ifdef DEBUG
    propertyState_ = PropertyState::SpreadOperand;
#endif
    return true;
}

bool PropertyEmitter::emitSpread()
{
    MOZ_ASSERT(propertyState_ == PropertyState::SpreadOperand);

    //                [stack] OBJ OBJ VAL

    if (!bce_->emitCopyDataProperties(BytecodeEmitter::CopyOption::Unfiltered)) {
        //            [stack] OBJ
        return false;
    }

    obj_ = nullptr;
#ifdef DEBUG
    propertyState_ = PropertyState::Init;
#endif
    return true;
}

MOZ_ALWAYS_INLINE bool PropertyEmitter::prepareForProp(const Maybe<uint32_t>& keyPos,
                                                       bool isStatic, bool isIndexOrComputed)
{
    isStatic_ = isStatic;
    isIndexOrComputed_ = isIndexOrComputed;

    //                [stack] CTOR? OBJ

    if (keyPos) {
        if (!bce_->updateSourceCoordNotes(*keyPos))
            return false;
    }

    if (isStatic_) {
        if (!bce_->emit1(JSOP_DUP2)) {
            //        [stack] CTOR HOMEOBJ CTOR HOMEOBJ
            return false;
        }
        if (!bce_->emit1(JSOP_POP)) {
            //        [stack] CTOR HOMEOBJ CTOR
            return false;
        }
    }

    return true;
}

bool PropertyEmitter::prepareForPropValue(const Maybe<uint32_t>& keyPos,
                                          Kind kind /* = Kind::Prototype */)
{
    MOZ_ASSERT(propertyState_ == PropertyState::Start ||
               propertyState_ == PropertyState::Init);

    //                [stack] CTOR? OBJ

    if (!prepareForProp(keyPos,
                        /* isStatic_ = */ kind == Kind::Static,
                        /* isIndexOrComputed = */ false)) {
        //            [stack] CTOR? OBJ CTOR?
        return false;
    }

#ifdef DEBUG
    propertyState_ = PropertyState::PropValue;
#endif
    return true;
}

bool PropertyEmitter::prepareForIndexPropKey(const Maybe<uint32_t>& keyPos,
                                             Kind kind /* = Kind::Prototype */)
{
    MOZ_ASSERT(propertyState_ == PropertyState::Start ||
               propertyState_ == PropertyState::Init);

    //                [stack] CTOR? OBJ

    obj_ = nullptr;

    if (!prepareForProp(keyPos,
                        /* isStatic_ = */ kind == Kind::Static,
                        /* isIndexOrComputed = */ true)) {
        //            [stack] CTOR? OBJ CTOR?
        return false;
    }

#ifdef DEBUG
    propertyState_ = PropertyState::IndexKey;
#endif
    return true;
}

bool PropertyEmitter::prepareForIndexPropValue()
{
    MOZ_ASSERT(propertyState_ == PropertyState::IndexKey);

    //                [stack] CTOR? OBJ CTOR? KEY

#ifdef DEBUG
    propertyState_ = PropertyState::IndexValue;
#endif
    return true;
}

bool PropertyEmitter::prepareForComputedPropKey(const Maybe<uint32_t>& keyPos,
                                                Kind kind /* = Kind::Prototype */)
{
    MOZ_ASSERT(propertyState_ == PropertyState::Start ||
               propertyState_ == PropertyState::Init);

    //                [stack] CTOR? OBJ

    obj_ = nullptr;

    if (!prepareForProp(keyPos,
                        /* isStatic_ = */ kind == Kind::Static,
                        /* isIndexOrComputed = */ true)) {
        //            [stack] CTOR? OBJ CTOR?
        return false;
    }

#ifdef DEBUG
    propertyState_ = PropertyState::ComputedKey;
#endif
    return true;
}

bool PropertyEmitter::prepareForComputedPropValue()
{
    MOZ_ASSERT(propertyState_ == PropertyState::ComputedKey);

    //                [stack] CTOR? OBJ CTOR? KEY

    if (!bce_->emit1(JSOP_TOID)) {
        //            [stack] CTOR? OBJ CTOR? KEY
        return false;
    }

#ifdef DEBUG
    propertyState_ = PropertyState::ComputedValue;
#endif
    return true;
}

bool PropertyEmitter::emitInitHomeObject(FunctionAsyncKind kind /* = FunctionAsyncKind::SyncFunction */)
{
    MOZ_ASSERT(propertyState_ == PropertyState::PropValue ||
               propertyState_ == PropertyState::IndexValue ||
               propertyState_ == PropertyState::ComputedValue);

    //                [stack] CTOR? HOMEOBJ CTOR? KEY? FUN

    bool isAsync = kind == FunctionAsyncKind::AsyncFunction;
    if (isAsync) {
      //              [stack] CTOR? HOMEOBJ CTOR? KEY? UNWRAPPED WRAPPED
      if (!bce_->emit1(JSOP_SWAP)) {
        //            [stack] CTOR? HOMEOBJ CTOR? KEY? WRAPPED UNWRAPPED
        return false;
      }
    }

    if (!bce_->emit2(JSOP_INITHOMEOBJECT, isIndexOrComputed_ + isAsync)) {
        //            [stack] CTOR? HOMEOBJ CTOR? KEY? WRAPPED? FUN
        return false;
    }
    if (isAsync) {
        if (!bce_->emit1(JSOP_POP)) {
            //        [stack] CTOR? HOMEOBJ CTOR? KEY? WRAPPED
            return false;
        }
    }

#ifdef DEBUG
    if (propertyState_ == PropertyState::PropValue) {
        propertyState_ = PropertyState::InitHomeObj;
    } else if (propertyState_ == PropertyState::IndexValue) {
        propertyState_ = PropertyState::InitHomeObjForIndex;
    } else {
        propertyState_ = PropertyState::InitHomeObjForComputed;
    }
#endif
    return true;
}

bool PropertyEmitter::emitInitProp(JS::Handle<JSAtom*> key,
                                   bool isPropertyAnonFunctionOrClass /* = false */,
                                   JS::Handle<JSFunction*> anonFunction /* = nullptr */)
{
    return emitInit(isClass_ ? JSOP_INITHIDDENPROP : JSOP_INITPROP, key,
                    isPropertyAnonFunctionOrClass, anonFunction);
}

bool PropertyEmitter::emitInitGetter(JS::Handle<JSAtom*> key)
{
    obj_ = nullptr;
    return emitInit(isClass_ ? JSOP_INITHIDDENPROP_GETTER : JSOP_INITPROP_GETTER,
                    key, false, nullptr);
}

bool PropertyEmitter::emitInitSetter(JS::Handle<JSAtom*> key)
{
    obj_ = nullptr;
    return emitInit(isClass_ ? JSOP_INITHIDDENPROP_SETTER : JSOP_INITPROP_SETTER,
                    key, false, nullptr);
}

bool PropertyEmitter::emitInitIndexProp(bool isPropertyAnonFunctionOrClass /* = false */)
{
    return emitInitIndexOrComputed(isClass_ ? JSOP_INITHIDDENELEM : JSOP_INITELEM,
                                   FunctionPrefixKind::None,
                                   isPropertyAnonFunctionOrClass);
}

bool PropertyEmitter::emitInitIndexGetter()
{
    obj_ = nullptr;
    return emitInitIndexOrComputed(
        isClass_ ? JSOP_INITHIDDENELEM_GETTER : JSOP_INITELEM_GETTER,
        FunctionPrefixKind::Get, false);
}

bool PropertyEmitter::emitInitIndexSetter()
{
    obj_ = nullptr;
    return emitInitIndexOrComputed(
        isClass_ ? JSOP_INITHIDDENELEM_SETTER : JSOP_INITELEM_SETTER,
        FunctionPrefixKind::Set, false);
}

bool PropertyEmitter::emitInitComputedProp(bool isPropertyAnonFunctionOrClass /* = false */)
{
    return emitInitIndexOrComputed(isClass_ ? JSOP_INITHIDDENELEM : JSOP_INITELEM,
                                   FunctionPrefixKind::None,
                                   isPropertyAnonFunctionOrClass);
}

bool PropertyEmitter::emitInitComputedGetter()
{
    obj_ = nullptr;
    return emitInitIndexOrComputed(isClass_ ? JSOP_INITHIDDENELEM_GETTER : JSOP_INITELEM_GETTER,
                                   FunctionPrefixKind::Get, true);
}

bool PropertyEmitter::emitInitComputedSetter()
{
    obj_ = nullptr;
    return emitInitIndexOrComputed(isClass_ ? JSOP_INITHIDDENELEM_SETTER : JSOP_INITELEM_SETTER,
                                   FunctionPrefixKind::Set, true);
}

bool PropertyEmitter::emitInit(JSOp op, JS::Handle<JSAtom*> key,
                               bool isPropertyAnonFunctionOrClass,
                               JS::Handle<JSFunction*> anonFunction)
{
    MOZ_ASSERT(propertyState_ == PropertyState::PropValue ||
               propertyState_ == PropertyState::InitHomeObj);

    MOZ_ASSERT(op == JSOP_INITPROP || op == JSOP_INITHIDDENPROP ||
               op == JSOP_INITPROP_GETTER || op == JSOP_INITHIDDENPROP_GETTER ||
               op == JSOP_INITPROP_SETTER || op == JSOP_INITHIDDENPROP_SETTER);

    //                [stack] CTOR? OBJ CTOR? VAL

    uint32_t index;
    if (!bce_->makeAtomIndex(key, &index))
        return false;

    if (obj_) {
        MOZ_ASSERT(!IsHiddenInitOp(op));
        MOZ_ASSERT(!obj_->inDictionaryMode());
        JS::RootedId id(bce_->cx, AtomToId(key));
        if (!NativeDefineProperty(bce_->cx, obj_, id, UndefinedHandleValue, nullptr, nullptr,
                                  JSPROP_ENUMERATE))
            return false;
        if (obj_->inDictionaryMode())
            obj_ = nullptr;
    }

    if (isPropertyAnonFunctionOrClass) {
        MOZ_ASSERT(op == JSOP_INITPROP || op == JSOP_INITHIDDENPROP);

        if (anonFunction) {
            if (!bce_->setFunName(anonFunction, key))
                return false;
        } else {
            // NOTE: This is setting the constructor's name of the class which is
            //       the property value.  Not of the enclosing class.
            if (!bce_->emitSetClassConstructorName(key)) {
                //          [stack] CTOR? OBJ CTOR? FUN
                return false;
            }
        }
    }

    if (!bce_->emitIndex32(op, index)) {
        //              [stack] CTOR? OBJ CTOR?
        return false;
    }

    if (!emitPopClassConstructor())
      return false;

#ifdef DEBUG
    propertyState_ = PropertyState::Init;
#endif
    return true;
}

bool PropertyEmitter::emitInitIndexOrComputed(JSOp op, FunctionPrefixKind prefixKind,
                                              bool isPropertyAnonFunctionOrClass)
{
    MOZ_ASSERT(propertyState_ == PropertyState::IndexValue ||
               propertyState_ == PropertyState::InitHomeObjForIndex ||
               propertyState_ == PropertyState::ComputedValue ||
               propertyState_ == PropertyState::InitHomeObjForComputed);

    MOZ_ASSERT(op == JSOP_INITELEM || op == JSOP_INITHIDDENELEM ||
               op == JSOP_INITELEM_GETTER || op == JSOP_INITHIDDENELEM_GETTER ||
               op == JSOP_INITELEM_SETTER || op == JSOP_INITHIDDENELEM_SETTER);

    //                [stack] CTOR? OBJ CTOR? KEY VAL

    if (isPropertyAnonFunctionOrClass) {
        if (!bce_->emitDupAt(1)) {
            //            [stack] CTOR? OBJ CTOR? KEY FUN FUN
            return false;
        }
        if (!bce_->emit2(JSOP_SETFUNNAME, uint8_t(prefixKind))) {
            //            [stack] CTOR? OBJ CTOR? KEY FUN
            return false;
        }
    }

    if (!bce_->emit1(op)) {
        //              [stack] CTOR? OBJ CTOR?
        return false;
    }

    if (!emitPopClassConstructor())
        return false;

#ifdef DEBUG
    propertyState_ = PropertyState::Init;
#endif
    return true;
}

bool PropertyEmitter::emitPopClassConstructor()
{
    if (isStatic_) {
      //              [stack] CTOR HOMEOBJ CTOR

        if (!bce_->emit1(JSOP_POP)) {
            //        [stack] CTOR HOMEOBJ
            return false;
        }
    }

  return true;
}

ObjectEmitter::ObjectEmitter(BytecodeEmitter* bce) : PropertyEmitter(bce) {}

bool ObjectEmitter::emitObject(size_t propertyCount)
{
    MOZ_ASSERT(propertyState_ == PropertyState::Start);
    MOZ_ASSERT(objectState_ == ObjectState::Start);

    //                [stack]

    // Emit code for {p:a, '%q':b, 2:c} that is equivalent to constructing
    // a new object and defining (in source order) each property on the object
    // (or mutating the object's [[Prototype]], in the case of __proto__).
    top_ = bce_->offset();
    if (!bce_->emitNewInit(JSProto_Object)) {
        //              [stack] OBJ
        return false;
    }

    // Try to construct the shape of the object as we go, so we can emit a
    // JSOP_NEWOBJECT with the final shape instead.
    // In the case of computed property names and indices, we cannot fix the
    // shape at bytecode compile time. When the shape cannot be determined,
    // |obj| is nulled out.

    // No need to do any guessing for the object kind, since we know the upper
    // bound of how many properties we plan to have.
    gc::AllocKind kind = gc::GetGCObjectKind(propertyCount);
    obj_ = NewBuiltinClassInstance<PlainObject>(bce_->cx, kind, TenuredObject);
    if (!obj_)
        return false;

#ifdef DEBUG
    objectState_ = ObjectState::Object;
#endif
    return true;
}

bool ClassEmitter::prepareForFieldInitializers(size_t numFields, bool isStatic)
{
    MOZ_ASSERT_IF(!isStatic, classState_ == ClassState::Class);
    MOZ_ASSERT_IF(isStatic, classState_ == ClassState::InitConstructor);
    MOZ_ASSERT(fieldState_ == FieldState::Start);

    // .initializers is a variable that stores an array of lambdas containing
    // code (the initializer) for each field. Upon an object's construction,
    // these lambdas will be called, defining the values.
    HandlePropertyName initializers = isStatic ? bce_->cx->names().dotStaticInitializers
                                               : bce_->cx->names().dotInitializers;
    initializersAssignment_.emplace(bce_, initializers,
                                    NameOpEmitter::Kind::Initialize);
    if (!initializersAssignment_->prepareForRhs()) {
        return false;
    }

    if (!bce_->emitUint32Operand(JSOP_NEWARRAY, numFields)) {
        //              [stack] ARRAY
        return false;
    }

    fieldIndex_ = 0;
#ifdef DEBUG
    if (isStatic) {
        classState_ = ClassState::StaticFieldInitializers;
    } else {
        classState_ = ClassState::InstanceFieldInitializers;
    }
    numFields_ = numFields;
#endif
    return true;
}

bool ClassEmitter::prepareForFieldInitializer()
{
    MOZ_ASSERT(classState_ == ClassState::InstanceFieldInitializers ||
               classState_ == ClassState::StaticFieldInitializers);
    MOZ_ASSERT(fieldState_ == FieldState::Start);

#ifdef DEBUG
    fieldState_ = FieldState::Initializer;
#endif
    return true;
}

bool ClassEmitter::emitFieldInitializerHomeObject(bool isStatic)
{
    MOZ_ASSERT(fieldState_ == FieldState::Initializer);
    //                [stack] OBJ HERITAGE? ARRAY METHOD
    // or:
    //                [stack] CTOR HOMEOBJ ARRAY METHOD
    uint8_t ofs = isStatic ? 2
    //                [stack] CTOR HOMEOBJ ARRAY METHOD CTOR
                           : isDerived_ ? 2 : 1;
    //                [stack] OBJ HERITAGE? ARRAY METHOD OBJ
    if (!bce_->emit2(JSOP_INITHOMEOBJECT, ofs)) {
        //            [stack] OBJ HERITAGE? ARRAY METHOD
        // or:
        //            [stack] CTOR HOMEOBJ ARRAY METHOD
        return false;
    }

#ifdef DEBUG
    fieldState_ = FieldState::InitializerWithHomeObject;
#endif
    return true;
}

bool ClassEmitter::emitStoreFieldInitializer()
{
    MOZ_ASSERT(fieldState_ == FieldState::Initializer ||
               fieldState_ == FieldState::InitializerWithHomeObject);
    MOZ_ASSERT(fieldIndex_ < numFields_);
    //          [stack] HOMEOBJ HERITAGE? ARRAY METHOD

    if (!bce_->emitUint32Operand(JSOP_INITELEM_ARRAY, fieldIndex_)) {
        //          [stack] HOMEOBJ HERITAGE? ARRAY
        return false;
    }

    fieldIndex_++;
#ifdef DEBUG
    fieldState_ = FieldState::Start;
#endif
    return true;
}

bool ClassEmitter::emitFieldInitializersEnd()
{
    MOZ_ASSERT(propertyState_ == PropertyState::Start ||
               propertyState_ == PropertyState::Init);
    MOZ_ASSERT(classState_ == ClassState::InstanceFieldInitializers ||
               classState_ == ClassState::StaticFieldInitializers);
    MOZ_ASSERT(fieldState_ == FieldState::Start);
    MOZ_ASSERT(fieldIndex_ == numFields_);

    if (!initializersAssignment_->emitAssignment()) {
      //              [stack] HOMEOBJ HERITAGE? ARRAY
      return false;
    }
    initializersAssignment_.reset();

    if (!bce_->emit1(JSOP_POP)) {
      //              [stack] HOMEOBJ HERITAGE?
      return false;
    }

#ifdef DEBUG
    if (classState_ == ClassState::InstanceFieldInitializers) {
        classState_ = ClassState::InstanceFieldInitializersEnd;
    } else {
        classState_ = ClassState::StaticFieldInitializersEnd;
    }
#endif
    return true;
}

bool ObjectEmitter::emitEnd()
{
    MOZ_ASSERT(propertyState_ == PropertyState::Start ||
               propertyState_ == PropertyState::Init);
    MOZ_ASSERT(objectState_ == ObjectState::Object);

    //                [stack] OBJ

    if (obj_) {
        // The object survived and has a predictable shape: update the original
        // bytecode.
        if (!bce_->replaceNewInitWithNewObject(obj_, top_)) {
            //            [stack] OBJ
            return false;
        }
    }

#ifdef DEBUG
    objectState_ = ObjectState::End;
#endif
    return true;
}

AutoSaveLocalStrictMode::AutoSaveLocalStrictMode(SharedContext* sc) : sc_(sc)
{
    savedStrictness_ = sc_->setLocalStrictMode(true);
}

AutoSaveLocalStrictMode::~AutoSaveLocalStrictMode()
{
    if (sc_) {
        restore();
    }
}

void AutoSaveLocalStrictMode::restore()
{
    MOZ_ALWAYS_TRUE(sc_->setLocalStrictMode(savedStrictness_));
    sc_ = nullptr;
}

ClassEmitter::ClassEmitter(BytecodeEmitter* bce)
    : PropertyEmitter(bce), strictMode_(bce->sc), name_(bce->cx)
{
    isClass_ = true;
}

bool ClassEmitter::emitScope(JS::Handle<LexicalScope::Data*> scopeBindings)
{
    MOZ_ASSERT(propertyState_ == PropertyState::Start);
    MOZ_ASSERT(classState_ == ClassState::Start);

    tdzCache_.emplace(bce_);

    innerScope_.emplace(bce_);
    if (!innerScope_->enterLexical(bce_, ScopeKind::Lexical, scopeBindings))
        return false;

#ifdef DEBUG
    classState_ = ClassState::Scope;
#endif
    return true;
}

bool ClassEmitter::emitClass(JS::Handle<JSAtom*> name)
{
    MOZ_ASSERT(propertyState_ == PropertyState::Start);
    MOZ_ASSERT(classState_ == ClassState::Start ||
               classState_ == ClassState::Scope);

    //                [stack]

    setName(name);
    isDerived_ = false;

    if (!bce_->emitNewInit(JSProto_Object)) {
        //            [stack] HOMEOBJ
        return false;
    }

#ifdef DEBUG
    classState_ = ClassState::Class;
#endif
    return true;
}

bool ClassEmitter::emitDerivedClass(JS::Handle<JSAtom*> name)
{
    MOZ_ASSERT(propertyState_ == PropertyState::Start);
    MOZ_ASSERT(classState_ == ClassState::Start ||
               classState_ == ClassState::Scope);

    //                [stack] HERITAGE

    setName(name);
    isDerived_ = true;

    if (!bce_->emit1(JSOP_CLASSHERITAGE)) {
        //              [stack] funcProto objProto
        return false;
    }
    if (!bce_->emit1(JSOP_OBJWITHPROTO)) {
        //              [stack] funcProto HOMEOBJ
        return false;
    }

    // JSOP_CLASSHERITAGE leaves both protos on the stack. After
    // creating the prototype, swap it to the bottom to make the
    // constructor.
    if (!bce_->emit1(JSOP_SWAP)) {
        //              [stack] HOMEOBJ funcProto
        return false;
    }

#ifdef DEBUG
    classState_ = ClassState::Class;
#endif
    return true;
}

void ClassEmitter::setName(JS::Handle<JSAtom*> name)
{
    name_ = name;
    if (!name_)
        name_ = bce_->cx->names().empty;
}

bool ClassEmitter::emitInitConstructor(bool needsHomeObject)
{
    MOZ_ASSERT(propertyState_ == PropertyState::Start);
    MOZ_ASSERT(classState_ == ClassState::Class ||
               classState_ == ClassState::InstanceFieldInitializersEnd);

    //                [stack] HOMEOBJ CTOR

    if (needsHomeObject) {
        if (!bce_->emit2(JSOP_INITHOMEOBJECT, 0)) {
            //            [stack] HOMEOBJ CTOR
            return false;
        }
    }

    if (!initProtoAndCtor()) {
        //              [stack] CTOR HOMEOBJ
        return false;
    }

#ifdef DEBUG
    classState_ = ClassState::InitConstructor;
#endif
    return true;
}

bool ClassEmitter::emitInitDefaultConstructor(const Maybe<uint32_t>& classStart,
                                              const Maybe<uint32_t>& classEnd)
{
    MOZ_ASSERT(propertyState_ == PropertyState::Start);
    MOZ_ASSERT(classState_ == ClassState::Class);

    if (classStart && classEnd) {
        // In the case of default class constructors, emit the start and end
        // offsets in the source buffer as source notes so that when we
        // actually make the constructor during execution, we can give it the
        // correct toString output.
        if (!bce_->newSrcNote3(SRC_CLASS_SPAN, ptrdiff_t(*classStart),
                               ptrdiff_t(*classEnd))) {
            return false;
        }
    }

    if (isDerived_) {
        //              [stack] HERITAGE PROTO
        if (!bce_->emitAtomOp(name_, JSOP_DERIVEDCONSTRUCTOR)) {
            //          [stack] HOMEOBJ CTOR
            return false;
        }
    } else {
        //              [stack] HOMEOBJ
        if (!bce_->emitAtomOp(name_, JSOP_CLASSCONSTRUCTOR)) {
            //          [stack] HOMEOBJ CTOR
            return false;
        }
    }

    if (!initProtoAndCtor()) {
        //              [stack] CTOR HOMEOBJ
        return false;
    }

#ifdef DEBUG
    classState_ = ClassState::InitConstructor;
#endif
    return true;
}

bool ClassEmitter::initProtoAndCtor()
{
    //                [stack] HOMEOBJ CTOR

    if (!bce_->emit1(JSOP_SWAP)) {
        //              [stack] CTOR HOMEOBJ
        return false;
    }
    if (!bce_->emit1(JSOP_DUP2)) {
        //              [stack] CTOR HOMEOBJ CTOR HOMEOBJ
        return false;
    }
    if (!bce_->emitAtomOp(bce_->cx->names().prototype, JSOP_INITLOCKEDPROP)) {
        //              [stack] CTOR HOMEOBJ CTOR
        return false;
    }
    if (!bce_->emitAtomOp(bce_->cx->names().constructor, JSOP_INITHIDDENPROP)) {
        //              [stack] CTOR HOMEOBJ
        return false;
    }

    return true;
}

bool ClassEmitter::emitBinding()
{
    MOZ_ASSERT(propertyState_ == PropertyState::Start ||
               propertyState_ == PropertyState::Init);
    MOZ_ASSERT(classState_ == ClassState::InitConstructor ||
               classState_ == ClassState::InstanceFieldInitializersEnd ||
               classState_ == ClassState::StaticFieldInitializersEnd);

    //                [stack] CTOR HOMEOBJ

    if (!bce_->emit1(JSOP_POP)) {
        //              [stack] CTOR
        return false;
    }

    if (name_ != bce_->cx->names().empty) {
        MOZ_ASSERT(innerScope_.isSome());

        if (!bce_->emitLexicalInitialization(name_)) {
            //            [stack] CTOR
            return false;
        }
    }

    //                [stack] CTOR

#ifdef DEBUG
    classState_ = ClassState::BoundName;
#endif
    return true;
}

bool ClassEmitter::emitEnd(Kind kind)
{
    MOZ_ASSERT(classState_ == ClassState::BoundName);
    //                [stack] CTOR

    if (innerScope_.isSome()) {
        MOZ_ASSERT(tdzCache_.isSome());

        if (!innerScope_->leave(bce_))
            return false;
        innerScope_.reset();
        tdzCache_.reset();
    } else {
        MOZ_ASSERT(kind == Kind::Expression);
        MOZ_ASSERT(tdzCache_.isNothing());
    }

    if (kind == Kind::Declaration) {
        MOZ_ASSERT(name_);

        if (!bce_->emitLexicalInitialization(name_)) {
            //            [stack] CTOR
            return false;
        }
        // Only class statements make outer bindings, and they do not leave
        // themselves on the stack.
        if (!bce_->emit1(JSOP_POP)) {
            //            [stack]
            return false;
        }
    }

    //                [stack] # class declaration
    //                [stack]
    //                [stack] # class expression
    //                [stack] CTOR

    strictMode_.restore();

#ifdef DEBUG
    classState_ = ClassState::End;
#endif
    return true;
}
