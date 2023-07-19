/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS parser.
 *
 * This is a recursive-descent parser for the JavaScript language specified by
 * "The ECMAScript Language Specification" (Standard ECMA-262).  It uses
 * lexical and semantic feedback to disambiguate non-LL(1) structures.  It
 * generates trees of nodes induced by the recursive parsing (not precise
 * syntax trees, see Parser.h).  After tree construction, it rewrites trees to
 * fold constants and evaluate compile-time expressions.
 *
 * This parser attempts no error recovery.
 */

#include "frontend/Parser.h"

#include "mozilla/ArrayUtils.h"  // mozilla::ArrayLength
#include "mozilla/Sprintf.h"

#include <new>

#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jsopcode.h"
#include "jsscript.h"
#include "jstypes.h"

#include "builtin/ModuleObject.h"
#include "builtin/SelfHostingDefines.h"
#include "frontend/BytecodeCompiler.h"
#include "frontend/FoldConstants.h"
#include "frontend/TokenStream.h"
#include "wasm/AsmJS.h"

#include "jsatominlines.h"
#include "jsscriptinlines.h"

#include "frontend/ParseNode-inl.h"
#include "vm/EnvironmentObject-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::ArrayLength;
using mozilla::Maybe;
using mozilla::Move;
using mozilla::Nothing;
using mozilla::PodCopy;
using mozilla::PodZero;
using mozilla::Some;

using JS::AutoGCRooter;

namespace js {
namespace frontend {

using DeclaredNamePtr = ParseContext::Scope::DeclaredNamePtr;
using AddDeclaredNamePtr = ParseContext::Scope::AddDeclaredNamePtr;
using BindingIter = ParseContext::Scope::BindingIter;
using UsedNamePtr = UsedNameTracker::UsedNameMap::Ptr;

template <class T, class U>
static inline void
PropagateTransitiveParseFlags(const T* inner, U* outer)
{
    if (inner->bindingsAccessedDynamically())
        outer->setBindingsAccessedDynamically();
    if (inner->hasDebuggerStatement())
        outer->setHasDebuggerStatement();
    if (inner->hasDirectEval())
        outer->setHasDirectEval();
}

static const char*
DeclarationKindString(DeclarationKind kind)
{
    switch (kind) {
      case DeclarationKind::PositionalFormalParameter:
      case DeclarationKind::FormalParameter:
        return "formal parameter";
      case DeclarationKind::CoverArrowParameter:
        return "cover arrow parameter";
      case DeclarationKind::Var:
        return "var";
      case DeclarationKind::Let:
        return "let";
      case DeclarationKind::Const:
        return "const";
      case DeclarationKind::Import:
        return "import";
      case DeclarationKind::BodyLevelFunction:
      case DeclarationKind::ModuleBodyLevelFunction:
      case DeclarationKind::LexicalFunction:
        return "function";
      case DeclarationKind::VarForAnnexBLexicalFunction:
        return "annex b var";
      case DeclarationKind::ForOfVar:
        return "var in for-of";
      case DeclarationKind::SimpleCatchParameter:
      case DeclarationKind::CatchParameter:
        return "catch parameter";
    }

    MOZ_CRASH("Bad DeclarationKind");
}

static bool
StatementKindIsBraced(StatementKind kind)
{
    return kind == StatementKind::Block ||
           kind == StatementKind::Switch ||
           kind == StatementKind::Try ||
           kind == StatementKind::Catch ||
           kind == StatementKind::Finally ||
           kind == StatementKind::Class;
}

void
ParseContext::Scope::dump(ParseContext* pc)
{
    ExclusiveContext* cx = pc->sc()->context;

    fprintf(stdout, "ParseScope %p", this);

    fprintf(stdout, "\n  decls:\n");
    for (DeclaredNameMap::Range r = declared_->all(); !r.empty(); r.popFront()) {
        JSAutoByteString bytes;
        if (!AtomToPrintableString(cx, r.front().key(), &bytes))
            return;
        DeclaredNameInfo& info = r.front().value().wrapped;
        fprintf(stdout, "    %s %s%s\n",
                DeclarationKindString(info.kind()),
                bytes.ptr(),
                info.closedOver() ? " (closed over)" : "");
    }

    fprintf(stdout, "\n");
}

/* static */ void
ParseContext::Scope::removeVarForAnnexBLexicalFunction(ParseContext* pc, JSAtom* name)
{
    // Local strict mode is allowed, e.g., a class binding removing a
    // synthesized Annex B binding.
    MOZ_ASSERT(!pc->sc()->strictScript);

    for (ParseContext::Scope* scope = pc->innermostScope();
         scope != pc->varScope().enclosing();
         scope = scope->enclosing())
    {
        if (DeclaredNamePtr p = scope->declared_->lookup(name)) {
            if (p->value()->kind() == DeclarationKind::VarForAnnexBLexicalFunction)
                scope->declared_->remove(p);
        }
    }

    // Annex B semantics no longer applies to any functions with this name, as
    // an early error would have occurred.
    pc->removeInnerFunctionBoxesForAnnexB(name);
}

static bool
DeclarationKindIsCatchParameter(DeclarationKind kind)
{
    return kind == DeclarationKind::SimpleCatchParameter ||
           kind == DeclarationKind::CatchParameter;
}

bool
ParseContext::Scope::addCatchParameters(ParseContext* pc, Scope& catchParamScope)
{
    if (pc->useAsmOrInsideUseAsm())
        return true;

    for (DeclaredNameMap::Range r = catchParamScope.declared_->all(); !r.empty(); r.popFront()) {
        DeclarationKind kind = r.front().value()->kind();
        uint32_t pos = r.front().value()->pos();
        MOZ_ASSERT(DeclarationKindIsCatchParameter(kind));
        JSAtom* name = r.front().key();
        AddDeclaredNamePtr p = lookupDeclaredNameForAdd(name);
        MOZ_ASSERT(!p);
        if (!addDeclaredName(pc, p, name, kind, pos))
            return false;
    }

    return true;
}

void
ParseContext::Scope::removeCatchParameters(ParseContext* pc, Scope& catchParamScope)
{
    if (pc->useAsmOrInsideUseAsm())
        return;

    for (DeclaredNameMap::Range r = catchParamScope.declared_->all(); !r.empty(); r.popFront()) {
        DeclaredNamePtr p = declared_->lookup(r.front().key());
        MOZ_ASSERT(p);

        // This check is needed because the catch body could have declared
        // vars, which would have been added to catchParamScope.
        if (DeclarationKindIsCatchParameter(r.front().value()->kind()))
            declared_->remove(p);
    }
}

void
SharedContext::computeAllowSyntax(Scope* scope)
{
    for (ScopeIter si(scope); si; si++) {
        if (si.kind() == ScopeKind::Function) {
            FunctionScope* funScope = &si.scope()->as<FunctionScope>();
            JSFunction* fun = funScope->canonicalFunction();
            if (fun->isArrow())
                continue;
            allowNewTarget_ = true;
            allowSuperProperty_ = fun->allowSuperProperty();
            allowSuperCall_ = fun->isDerivedClassConstructor();
            if (fun->isFieldInitializer()) {
                allowSuperCall_ = false;
                allowArguments_ = false;
            }
            return;
        }
    }
}

void
SharedContext::computeThisBinding(Scope* scope)
{
    for (ScopeIter si(scope); si; si++) {
        if (si.kind() == ScopeKind::Module) {
            thisBinding_ = ThisBinding::Module;
            return;
        }

        if (si.kind() == ScopeKind::Function) {
            JSFunction* fun = si.scope()->as<FunctionScope>().canonicalFunction();

            // Arrow functions and generator expression lambdas don't have
            // their own `this` binding.
            if (fun->isArrow() || fun->nonLazyScript()->isGeneratorExp())
                continue;

            // Derived class constructors (including nested arrow functions and
            // eval) need TDZ checks when accessing |this|.
            if (fun->isDerivedClassConstructor())
                needsThisTDZChecks_ = true;

            thisBinding_ = ThisBinding::Function;
            return;
        }
    }

    thisBinding_ = ThisBinding::Global;
}

void
SharedContext::computeInWith(Scope* scope)
{
    for (ScopeIter si(scope); si; si++) {
        if (si.kind() == ScopeKind::With) {
            inWith_ = true;
            break;
        }
    }
}

EvalSharedContext::EvalSharedContext(ExclusiveContext* cx, JSObject* enclosingEnv,
                                     Scope* enclosingScope, Directives directives,
                                     bool extraWarnings)
  : SharedContext(cx, Kind::Eval, directives, extraWarnings),
    enclosingScope_(cx, enclosingScope),
    bindings(cx)
{
    computeAllowSyntax(enclosingScope);
    computeInWith(enclosingScope);
    computeThisBinding(enclosingScope);

    // Like all things Debugger, Debugger.Frame.eval needs special
    // handling. Since the environment chain of such evals are non-syntactic
    // (DebuggerEnvironmentProxy is not an EnvironmentObject), computing the
    // this binding with respect to enclosingScope is incorrect if the
    // Debugger.Frame is a function frame. Recompute the this binding if we
    // are such an eval.
    if (enclosingEnv && enclosingScope->hasOnChain(ScopeKind::NonSyntactic)) {
        // For Debugger.Frame.eval with bindings, the environment chain may
        // have more than the DebugEnvironmentProxy.
        JSObject* env = enclosingEnv;
        while (env) {
            if (env->is<DebugEnvironmentProxy>())
                env = &env->as<DebugEnvironmentProxy>().environment();

            if (env->is<CallObject>()) {
                computeThisBinding(env->as<CallObject>().callee().nonLazyScript()->bodyScope());
                break;
            }

            env = env->enclosingEnvironment();
        }
    }
}

bool
ParseContext::init()
{
    if (scriptId_ == UINT32_MAX) {
        tokenStream_.reportError(JSMSG_NEED_DIET, js_script_str);
        return false;
    }

    ExclusiveContext* cx = sc()->context;

    if (isFunctionBox()) {
        // Named lambdas always need a binding for their own name. If this
        // binding is closed over when we finish parsing the function in
        // finishExtraFunctionScopes, the function box needs to be marked as
        // needing a dynamic DeclEnv object.
        RootedFunction fun(cx, functionBox()->function());
        if (fun->isNamedLambda()) {
            if (!namedLambdaScope_->init(this))
                return false;
            AddDeclaredNamePtr p =
                namedLambdaScope_->lookupDeclaredNameForAdd(fun->explicitName());
            MOZ_ASSERT(!p);
            if (!namedLambdaScope_->addDeclaredName(this, p, fun->explicitName(),
                                                    DeclarationKind::Const,
                                                    DeclaredNameInfo::npos))
            {
                return false;
            }
        }

        if (!functionScope_->init(this))
            return false;

        if (!positionalFormalParameterNames_.acquire(cx))
            return false;
    }

    if (!closedOverBindingsForLazy_.acquire(cx))
        return false;

    if (!sc()->strict()) {
        if (!innerFunctionBoxesForAnnexB_.acquire(cx))
            return false;
    }

    return true;
}

bool
ParseContext::addInnerFunctionBoxForAnnexB(FunctionBox* funbox)
{
    for (uint32_t i = 0; i < innerFunctionBoxesForAnnexB_->length(); i++) {
        if (!innerFunctionBoxesForAnnexB_[i]) {
            innerFunctionBoxesForAnnexB_[i] = funbox;
            return true;
        }
    }
    return innerFunctionBoxesForAnnexB_->append(funbox);
}

void
ParseContext::removeInnerFunctionBoxesForAnnexB(JSAtom* name)
{
    for (uint32_t i = 0; i < innerFunctionBoxesForAnnexB_->length(); i++) {
        if (FunctionBox* funbox = innerFunctionBoxesForAnnexB_[i]) {
            if (funbox->function()->explicitName() == name)
                innerFunctionBoxesForAnnexB_[i] = nullptr;
        }
    }
}

void
ParseContext::finishInnerFunctionBoxesForAnnexB()
{
    // Strict mode doesn't have wack Annex B function semantics. Or we
    // could've failed to initialize ParseContext.
    if (sc()->strict() || !innerFunctionBoxesForAnnexB_)
        return;

    for (uint32_t i = 0; i < innerFunctionBoxesForAnnexB_->length(); i++) {
        if (FunctionBox* funbox = innerFunctionBoxesForAnnexB_[i])
            funbox->isAnnexB = true;
    }
}

ParseContext::~ParseContext()
{
    // Any funboxes still in the list at the end of parsing means no early
    // error would have occurred for declaring a binding in the nearest var
    // scope. Mark them as needing extra assignments to this var binding.
    finishInnerFunctionBoxesForAnnexB();
}

bool
UsedNameTracker::noteUse(ExclusiveContext* cx, JSAtom* name, uint32_t scriptId, uint32_t scopeId)
{
    if (UsedNameMap::AddPtr p = map_.lookupForAdd(name)) {
        if (!p->value().noteUsedInScope(scriptId, scopeId))
            return false;
    } else {
        UsedNameInfo info(cx);
        if (!info.noteUsedInScope(scriptId, scopeId))
            return false;
        if (!map_.add(p, name, Move(info)))
            return false;
    }

    return true;
}

void
UsedNameTracker::UsedNameInfo::resetToScope(uint32_t scriptId, uint32_t scopeId)
{
    while (!uses_.empty()) {
        Use& innermost = uses_.back();
        if (innermost.scopeId < scopeId)
            break;
        MOZ_ASSERT(innermost.scriptId >= scriptId);
        uses_.popBack();
    }
}

void
UsedNameTracker::rewind(RewindToken token)
{
    scriptCounter_ = token.scriptId;
    scopeCounter_ = token.scopeId;

    for (UsedNameMap::Range r = map_.all(); !r.empty(); r.popFront())
        r.front().value().resetToScope(token.scriptId, token.scopeId);
}

FunctionBox::FunctionBox(ExclusiveContext* cx, LifoAlloc& alloc, TraceListNode* traceListHead,
                         JSFunction* fun, uint32_t toStringStart,
                         Directives directives, bool extraWarnings,
                         GeneratorKind generatorKind, FunctionAsyncKind asyncKind)
  : ObjectBox(fun, traceListHead),
    SharedContext(cx, Kind::ObjectBox, directives, extraWarnings),
    enclosingScope_(nullptr),
    namedLambdaBindings_(nullptr),
    functionScopeBindings_(nullptr),
    extraVarScopeBindings_(nullptr),
    functionNode(nullptr),
    bufStart(0),
    bufEnd(0),
    startLine(1),
    startColumn(0),
    toStringStart(toStringStart),
    toStringEnd(0),
    length(0),
    generatorKindBits_(GeneratorKindAsBits(generatorKind)),
    asyncKindBits_(AsyncKindAsBits(asyncKind)),
    isGenexpLambda(false),
    hasDestructuringArgs(false),
    hasParameterExprs(false),
    hasDirectEvalInParameterExpr(false),
    hasDuplicateParameters(false),
    allowReturn_(true),
    useAsm(false),
    insideUseAsm(false),
    isAnnexB(false),
    wasEmitted(false),
    declaredArguments(false),
    usesArguments(false),
    usesApply(false),
    usesThis(false),
    usesReturn(false),
    hasRest_(false),
    isExprBody_(false),
    funCxFlags()
{
    // Functions created at parse time may be set singleton after parsing and
    // baked into JIT code, so they must be allocated tenured. They are held by
    // the JSScript so cannot be collected during a minor GC anyway.
    MOZ_ASSERT(fun->isTenured());
}

void
FunctionBox::initFromLazyFunction()
{
    JSFunction* fun = function();
    if (fun->lazyScript()->isDerivedClassConstructor())
        setDerivedClassConstructor();
    if (fun->lazyScript()->needsHomeObject())
        setNeedsHomeObject();
    if (fun->lazyScript()->hasModuleGoal()) {
      setHasModuleGoal();
    }
    enclosingScope_ = fun->lazyScript()->enclosingScope();
    initWithEnclosingScope(enclosingScope_);
}

void
FunctionBox::initStandaloneFunction(Scope* enclosingScope)
{
    // Standalone functions are Function or Generator constructors and are
    // always scoped to the global.
    MOZ_ASSERT(enclosingScope->is<GlobalScope>());
    enclosingScope_ = enclosingScope;
    allowNewTarget_ = true;
    thisBinding_ = ThisBinding::Function;
}

void
FunctionBox::initWithEnclosingParseContext(ParseContext* enclosing, FunctionSyntaxKind kind)
{
    SharedContext* sc = enclosing->sc();
    useAsm = sc->isFunctionBox() && sc->asFunctionBox()->useAsmOrInsideUseAsm();

    JSFunction* fun = function();

    // Arrow functions and generator expression lambdas don't have
    // their own `this` binding.
    if (fun->isArrow()) {
        allowNewTarget_ = sc->allowNewTarget();
        allowSuperProperty_ = sc->allowSuperProperty();
        allowSuperCall_ = sc->allowSuperCall();
        allowArguments_ = sc->allowArguments();
        needsThisTDZChecks_ = sc->needsThisTDZChecks();
        thisBinding_ = sc->thisBinding();
    } else {
        allowNewTarget_ = true;
        allowSuperProperty_ = fun->allowSuperProperty();

        if (isGenexpLambda)
            thisBinding_ = sc->thisBinding();
        else
            thisBinding_ = ThisBinding::Function;

        if (IsConstructorKind(kind)) {
            auto stmt = enclosing->findInnermostStatement<ParseContext::ClassStatement>();
            MOZ_ASSERT(stmt);
            stmt->constructorBox = this;
        }

        if (kind == FunctionSyntaxKind::DerivedClassConstructor) {
            setDerivedClassConstructor();
            allowSuperCall_ = true;
            needsThisTDZChecks_ = true;
        }

        if (kind == FunctionSyntaxKind::FieldInitializer ||
            kind == FunctionSyntaxKind::StaticClassBlock) {
            allowArguments_ = false;
            if (kind == FunctionSyntaxKind::StaticClassBlock) {
                allowSuperCall_ = false;
                allowReturn_ = false;
            } else {
                MOZ_ASSERT(kind == FunctionSyntaxKind::FieldInitializer);
                setFieldInitializer();
            }
        }
    }

    // We inherit the parse goal from our top-level.
    hasModuleGoal_ = sc->hasModuleGoal();

    if (sc->inWith()) {
        inWith_ = true;
    } else {
        auto isWith = [](ParseContext::Statement* stmt) {
            return stmt->kind() == StatementKind::With;
        };

        inWith_ = enclosing->findInnermostStatement(isWith);
    }
}

void
FunctionBox::initWithEnclosingScope(Scope* enclosingScope)
{
    if (!function()->isArrow()) {
        allowNewTarget_ = true;
        allowSuperProperty_ = function()->allowSuperProperty();

        if (isDerivedClassConstructor()) {
            setDerivedClassConstructor();
            allowSuperCall_ = true;
            needsThisTDZChecks_ = true;
        }

        thisBinding_ = ThisBinding::Function;
    } else {
        computeAllowSyntax(enclosingScope);
        computeThisBinding(enclosingScope);
    }

    computeInWith(enclosingScope);
}

template <typename ParseHandler>
template <typename ConditionT, typename ErrorReportT>
bool
Parser<ParseHandler>::mustMatchTokenInternal(ConditionT condition, Modifier modifier,
                                             ErrorReportT errorReport)
{
    TokenKind actual;
    if (!tokenStream.getToken(&actual, modifier)) {
        return false;
    }
    if (!condition(actual)) {
        errorReport(actual);
        return false;
    }
    return true;
}

void
ParserBase::error(unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);
#ifdef DEBUG
    bool result =
#endif
        tokenStream.reportCompileErrorNumberVA(nullptr, pos().begin, JSREPORT_ERROR,
                                               errorNumber, args);
    MOZ_ASSERT(!result, "reporting an error returned true?");
    va_end(args);
}

void
ParserBase::errorWithNotes(UniquePtr<JSErrorNotes> notes, unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);
#ifdef DEBUG
    bool result =
#endif
        tokenStream.reportCompileErrorNumberVA(Move(notes), pos().begin, JSREPORT_ERROR,
                                               errorNumber, args);
    MOZ_ASSERT(!result, "reporting an error returned true?");
    va_end(args);
}

void
ParserBase::errorAt(uint32_t offset, unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);
#ifdef DEBUG
    bool result =
#endif
        tokenStream.reportCompileErrorNumberVA(nullptr, offset, JSREPORT_ERROR, errorNumber, args);
    MOZ_ASSERT(!result, "reporting an error returned true?");
    va_end(args);
}

void
ParserBase::errorWithNotesAt(UniquePtr<JSErrorNotes> notes, uint32_t offset,
                             unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);
#ifdef DEBUG
    bool result =
#endif
        tokenStream.reportCompileErrorNumberVA(Move(notes), offset, JSREPORT_ERROR,
                                               errorNumber, args);
    MOZ_ASSERT(!result, "reporting an error returned true?");
    va_end(args);
}

bool
ParserBase::warning(unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);
    bool result =
        tokenStream.reportCompileErrorNumberVA(nullptr, pos().begin, JSREPORT_WARNING,
                                               errorNumber, args);
    va_end(args);
    return result;
}

bool
ParserBase::warningAt(uint32_t offset, unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);
    bool result =
        tokenStream.reportCompileErrorNumberVA(nullptr, offset, JSREPORT_WARNING,
                                               errorNumber, args);
    va_end(args);
    return result;
}

bool
ParserBase::extraWarning(unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);
    bool result = tokenStream.reportExtraWarningErrorNumberVA(nullptr, pos().begin,
                                                              errorNumber, args);
    va_end(args);
    return result;
}

bool
ParserBase::extraWarningAt(uint32_t offset, unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);

    bool result =
        tokenStream.reportExtraWarningErrorNumberVA(nullptr, offset, errorNumber, args);

    va_end(args);
    return result;
}

bool
ParserBase::strictModeError(unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);
    bool res =
        tokenStream.reportStrictModeErrorNumberVA(nullptr, pos().begin, pc->sc()->strict(),
                                                  errorNumber, args);
    va_end(args);
    return res;
}

bool
ParserBase::strictModeErrorAt(uint32_t offset, unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);
    bool res =
        tokenStream.reportStrictModeErrorNumberVA(nullptr, offset, pc->sc()->strict(),
                                                  errorNumber, args);
    va_end(args);
    return res;
}

bool
ParserBase::reportNoOffset(ParseReportKind kind, bool strict, unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);
    bool result = false;
    uint32_t offset = TokenStream::NoOffset;
    switch (kind) {
      case ParseError:
        result = tokenStream.reportCompileErrorNumberVA(nullptr, offset, JSREPORT_ERROR,
                                                        errorNumber, args);
        break;
      case ParseWarning:
        result =
            tokenStream.reportCompileErrorNumberVA(nullptr, offset, JSREPORT_WARNING,
                                                   errorNumber, args);
        break;
      case ParseExtraWarning:
        result = tokenStream.reportExtraWarningErrorNumberVA(nullptr, offset,
                                                             errorNumber, args);
        break;
      case ParseStrictError:
        result = tokenStream.reportStrictModeErrorNumberVA(nullptr, offset, strict,
                                                           errorNumber, args);
        break;
    }
    va_end(args);
    return result;
}

template <>
inline bool
Parser<FullParseHandler>::abortIfSyntaxParser()
{
    handler.disableSyntaxParser();
    return true;
}

template <>
inline bool
Parser<SyntaxParseHandler>::abortIfSyntaxParser()
{
    abortedSyntaxParse = true;
    return false;
}

ParserBase::ParserBase(ExclusiveContext* cx, LifoAlloc& alloc,
                       const ReadOnlyCompileOptions& options,
                       const char16_t* chars, size_t length,
                       bool foldConstants,
                       UsedNameTracker& usedNames,
                       Parser<SyntaxParseHandler>* syntaxParser,
                       LazyScript* lazyOuterFunction)
  : context(cx),
    alloc(alloc),
    tokenStream(cx, options, chars, length, thisForCtor()),
    traceListHead(nullptr),
    pc(nullptr),
    usedNames(usedNames),
    sct(nullptr),
    ss(nullptr),
    keepAtoms(cx->perThreadData),
    foldConstants(foldConstants),
#ifdef DEBUG
    checkOptionsCalled(false),
#endif
    abortedSyntaxParse(false),
    isUnexpectedEOF_(false),
    awaitHandling_(AwaitIsName)
{
    cx->perThreadData->frontendCollectionPool.addActiveCompilation();
    tempPoolMark = alloc.mark();
}

ParserBase::~ParserBase()
{
    alloc.release(tempPoolMark);

    /*
     * The parser can allocate enormous amounts of memory for large functions.
     * Eagerly free the memory now (which otherwise won't be freed until the
     * next GC) to avoid unnecessary OOMs.
     */
    alloc.freeAllIfHugeAndUnused();

    context->perThreadData->frontendCollectionPool.removeActiveCompilation();
}

template <typename ParseHandler>
Parser<ParseHandler>::Parser(ExclusiveContext* cx, LifoAlloc& alloc,
                             const ReadOnlyCompileOptions& options,
                             const char16_t* chars, size_t length,
                             bool foldConstants,
                             UsedNameTracker& usedNames,
                             Parser<SyntaxParseHandler>* syntaxParser,
                             LazyScript* lazyOuterFunction)
  : ParserBase(cx, alloc, options, chars, length, foldConstants, usedNames, syntaxParser,
              lazyOuterFunction),
    AutoGCRooter(cx, PARSER),
    handler(cx, alloc, tokenStream, syntaxParser, lazyOuterFunction)
{
    // The Mozilla specific JSOPTION_EXTRA_WARNINGS option adds extra warnings
    // which are not generated if functions are parsed lazily. Note that the
    // standard "use strict" does not inhibit lazy parsing.
    if (options.extraWarningsOption)
        handler.disableSyntaxParser();
}

template<typename ParseHandler>
bool
Parser<ParseHandler>::checkOptions()
{
#ifdef DEBUG
    checkOptionsCalled = true;
#endif

    return tokenStream.checkOptions();
}

template <typename ParseHandler>
Parser<ParseHandler>::~Parser()
{
    MOZ_ASSERT(checkOptionsCalled);
}

template <>
void
Parser<SyntaxParseHandler>::setAwaitHandling(AwaitHandling awaitHandling)
{
    awaitHandling_ = awaitHandling;
}

template <>
void
Parser<FullParseHandler>::setAwaitHandling(AwaitHandling awaitHandling)
{
    awaitHandling_ = awaitHandling;
    if (Parser<SyntaxParseHandler>* parser = handler.syntaxParser)
        parser->setAwaitHandling(awaitHandling);
}

template <typename BoxT, typename ArgT>
BoxT*
ParserBase::newTraceListNode(ArgT* arg)
{
    MOZ_ASSERT(arg);

    /*
     * We use JSContext.tempLifoAlloc to allocate parsed objects and place them
     * on a list in this Parser to ensure GC safety. Thus the tempLifoAlloc
     * arenas containing the entries must be alive until we are done with
     * scanning, parsing and code generation for the whole script or top-level
     * function.
     */

    BoxT* box = alloc.template new_<BoxT>(arg, traceListHead);
    if (!box) {
        ReportOutOfMemory(context);
        return nullptr;
    }

    traceListHead = box;

    return box;
}

ObjectBox*
ParserBase::newObjectBox(JSObject* obj)
{
    return newTraceListNode<ObjectBox, JSObject>(obj);
}

BigIntBox*
ParserBase::newBigIntBox(BigInt* val)
{
    return newTraceListNode<BigIntBox, BigInt>(val);
}

template <typename ParseHandler>
FunctionBox*
Parser<ParseHandler>::newFunctionBox(FunctionNodeType funNode, JSFunction* fun, uint32_t toStringStart,
                                     Directives inheritedDirectives,
                                     GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
                                     bool tryAnnexB)
{
    MOZ_ASSERT(fun);
    MOZ_ASSERT_IF(tryAnnexB, !pc->sc()->strict());

    /*
     * We use JSContext.tempLifoAlloc to allocate parsed objects and place them
     * on a list in this Parser to ensure GC safety. Thus the tempLifoAlloc
     * arenas containing the entries must be alive until we are done with
     * scanning, parsing and code generation for the whole script or top-level
     * function.
     */
    FunctionBox* funbox =
        alloc.new_<FunctionBox>(context, alloc, traceListHead, fun, toStringStart,
                                inheritedDirectives, options().extraWarningsOption,
                                generatorKind, asyncKind);
    if (!funbox) {
        ReportOutOfMemory(context);
        return nullptr;
    }

    traceListHead = funbox;
    if (funNode)
        handler.setFunctionBox(funNode, funbox);

    if (tryAnnexB && !pc->addInnerFunctionBoxForAnnexB(funbox))
        return nullptr;

    return funbox;
}

ModuleSharedContext::ModuleSharedContext(ExclusiveContext* cx, ModuleObject* module,
                                         Scope* enclosingScope, ModuleBuilder& builder)
  : SharedContext(cx, Kind::Module, Directives(true), false),
    module_(cx, module),
    enclosingScope_(cx, enclosingScope),
    bindings(cx),
    builder(builder)
{
    thisBinding_ = ThisBinding::Module;
    hasModuleGoal_ = true;
}

template <typename ParseHandler>
void
Parser<ParseHandler>::trace(JSTracer* trc)
{
    ObjectBox::TraceList(trc, traceListHead);
}

void
MarkParser(JSTracer* trc, AutoGCRooter* parser)
{
    static_cast<Parser<FullParseHandler>*>(parser)->trace(trc);
}

/*
 * Parse a top-level JS script.
 */
template <typename ParseHandler>
typename ParseHandler::ListNodeType
Parser<ParseHandler>::parse()
{
    MOZ_ASSERT(checkOptionsCalled);

    Directives directives(options().strictOption);
    GlobalSharedContext globalsc(context, ScopeKind::Global,
                                 directives, options().extraWarningsOption);
    ParseContext globalpc(this, &globalsc, /* newDirectives = */ nullptr);
    if (!globalpc.init())
        return null();

    ParseContext::VarScope varScope(this);
    if (!varScope.init(pc))
        return null();

    ListNodeType stmtList = statementList(YieldIsName);
    if (!stmtList)
        return null();

    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();
    if (tt != TOK_EOF) {
        error(JSMSG_GARBAGE_AFTER_INPUT, "script", TokenKindToDesc(tt));
        return null();
    }
    if (foldConstants) {
        Node node = stmtList;
        if (!FoldConstants(context, &node, this)) {
            return null();
        }
        stmtList = handler.asList(node);
    }

    return stmtList;
}

/*
 * Strict mode forbids introducing new definitions for 'eval', 'arguments', or
 * for any strict mode reserved word.
 */
bool
ParserBase::isValidStrictBinding(PropertyName* name)
{
    TokenKind tt = ReservedWordTokenKind(name);
    if (tt == TOK_NAME) {
        return name != context->names().eval &&
               name != context->names().arguments;
    }
    return tt != TOK_LET &&
           tt != TOK_STATIC &&
           tt != TOK_YIELD &&
           !TokenKindIsStrictReservedWord(tt);
}

/*
 * Returns true if all parameter names are valid strict mode binding names and
 * no duplicate parameter names are present.
 */
template <typename ParseHandler>
bool
Parser<ParseHandler>::hasValidSimpleStrictParameterNames()
{
    MOZ_ASSERT(pc->isFunctionBox() && pc->functionBox()->hasSimpleParameterList());

    if (pc->functionBox()->hasDuplicateParameters)
        return false;

    for (size_t i = 0; i < pc->positionalFormalParameterNames().length(); i++) {
        JSAtom* name = pc->positionalFormalParameterNames()[i];
        MOZ_ASSERT(name);
        if (!isValidStrictBinding(name->asPropertyName()))
            return false;
    }
    return true;
}

template <typename ParseHandler>
void
Parser<ParseHandler>::reportMissingClosing(unsigned errorNumber, unsigned noteNumber,
                                           uint32_t openedPos)
{
    auto notes = MakeUnique<JSErrorNotes>();
    if (!notes)
        return;

    uint32_t line, column;
    tokenStream.srcCoords.lineNumAndColumnIndex(openedPos, &line, &column);

    const size_t MaxWidth = sizeof("4294967295");
    char columnNumber[MaxWidth];
    SprintfLiteral(columnNumber, "%" PRIu32, column);
    char lineNumber[MaxWidth];
    SprintfLiteral(lineNumber, "%" PRIu32, line);

    if (!notes->addNoteASCII(pc->sc()->context,
                             getFilename(), line, column,
                             GetErrorMessage, nullptr,
                             noteNumber, lineNumber, columnNumber))
    {
        return;
    }

    errorWithNotes(Move(notes), errorNumber);
}

template <typename ParseHandler>
void
Parser<ParseHandler>::reportRedeclaration(HandlePropertyName name, DeclarationKind prevKind,
                                          TokenPos pos, uint32_t prevPos)
{
    JSAutoByteString bytes;
    if (!AtomToPrintableString(context, name, &bytes))
        return;

    if (prevPos == DeclaredNameInfo::npos) {
        errorAt(pos.begin, JSMSG_REDECLARED_VAR, DeclarationKindString(prevKind), bytes.ptr());
        return;
    }

    auto notes = MakeUnique<JSErrorNotes>();
    if (!notes)
        return;

    uint32_t line, column;
    tokenStream.srcCoords.lineNumAndColumnIndex(prevPos, &line, &column);

    const size_t MaxWidth = sizeof("4294967295");
    char columnNumber[MaxWidth];
    SprintfLiteral(columnNumber, "%" PRIu32, column);
    char lineNumber[MaxWidth];
    SprintfLiteral(lineNumber, "%" PRIu32, line);

    if (!notes->addNoteASCII(pc->sc()->context,
                             getFilename(), line, column,
                             GetErrorMessage, nullptr,
                             JSMSG_REDECLARED_PREV,
                             lineNumber, columnNumber))
    {
        return;
    }

    errorWithNotesAt(Move(notes), pos.begin, JSMSG_REDECLARED_VAR,
                     DeclarationKindString(prevKind), bytes.ptr());
}

// notePositionalFormalParameter is called for both the arguments of a regular
// function definition and the arguments specified by the Function
// constructor.
//
// The 'disallowDuplicateParams' bool indicates whether the use of another
// feature (destructuring or default arguments) disables duplicate arguments.
// (ECMA-262 requires us to support duplicate parameter names, but, for newer
// features, we consider the code to have "opted in" to higher standards and
// forbid duplicates.)
template <typename ParseHandler>
bool
Parser<ParseHandler>::notePositionalFormalParameter(FunctionNodeType funNode, HandlePropertyName name,
                                                    uint32_t beginPos,
                                                    bool disallowDuplicateParams,
                                                    bool* duplicatedParam)
{
    if (AddDeclaredNamePtr p = pc->functionScope().lookupDeclaredNameForAdd(name)) {
        if (disallowDuplicateParams) {
            error(JSMSG_BAD_DUP_ARGS);
            return false;
        }

        // Strict-mode disallows duplicate args. We may not know whether we are
        // in strict mode or not (since the function body hasn't been parsed).
        // In such cases, report will queue up the potential error and return
        // 'true'.
        if (pc->sc()->needStrictChecks()) {
            JSAutoByteString bytes;
            if (!AtomToPrintableString(context, name, &bytes))
                return false;
            if (!strictModeError(JSMSG_DUPLICATE_FORMAL, bytes.ptr()))
                return false;
        }

        *duplicatedParam = true;
    } else {
        DeclarationKind kind = DeclarationKind::PositionalFormalParameter;
        if (!pc->functionScope().addDeclaredName(pc, p, name, kind, beginPos))
            return false;
    }

    if (!pc->positionalFormalParameterNames().append(name)) {
        ReportOutOfMemory(context);
        return false;
    }

    NameNodeType paramNode = newName(name);
    if (!paramNode)
        return false;

    handler.addFunctionFormalParameter(funNode, paramNode);
    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::noteDestructuredPositionalFormalParameter(FunctionNodeType funNode, Node destruct)
{
    // Append an empty name to the positional formals vector to keep track of
    // argument slots when making FunctionScope::Data.
    if (!pc->positionalFormalParameterNames().append(nullptr)) {
        ReportOutOfMemory(context);
        return false;
    }

    handler.addFunctionFormalParameter(funNode, destruct);
    return true;
}

static bool
DeclarationKindIsVar(DeclarationKind kind)
{
    return kind == DeclarationKind::Var ||
           kind == DeclarationKind::BodyLevelFunction ||
           kind == DeclarationKind::VarForAnnexBLexicalFunction ||
           kind == DeclarationKind::ForOfVar;
}

template <typename ParseHandler>
Maybe<DeclarationKind>
Parser<ParseHandler>::isVarRedeclaredInEval(HandlePropertyName name, DeclarationKind kind)
{
    MOZ_ASSERT(DeclarationKindIsVar(kind));
    MOZ_ASSERT(pc->sc()->isEvalContext());

    // In the case of eval, we also need to check enclosing VM scopes to see
    // if the var declaration is allowed in the context.
    //
    // This check is necessary in addition to
    // js::CheckEvalDeclarationConflicts because we only know during parsing
    // if a var is bound by for-of.
    Scope* enclosingScope = pc->sc()->compilationEnclosingScope();
    Scope* varScope = EvalScope::nearestVarScopeForDirectEval(enclosingScope);
    MOZ_ASSERT(varScope);
    for (ScopeIter si(enclosingScope); si; si++) {
        for (js::BindingIter bi(si.scope()); bi; bi++) {
            if (bi.name() != name)
                continue;

            switch (bi.kind()) {
              case BindingKind::Let: {
                  // Annex B.3.5 allows redeclaring simple (non-destructured)
                  // catch parameters with var declarations, except when it
                  // appears in a for-of.
                  bool annexB35Allowance = si.kind() == ScopeKind::SimpleCatch &&
                                           kind != DeclarationKind::ForOfVar;
                  if (!annexB35Allowance) {
                      return Some(ScopeKindIsCatch(si.kind())
                                  ? DeclarationKind::CatchParameter
                                  : DeclarationKind::Let);
                  }
                  break;
              }

              case BindingKind::Const:
                return Some(DeclarationKind::Const);

              case BindingKind::Import:
              case BindingKind::FormalParameter:
              case BindingKind::Var:
              case BindingKind::NamedLambdaCallee:
                break;
            }
        }

        if (si.scope() == varScope)
            break;
    }

    return Nothing();
}

static bool
DeclarationKindIsParameter(DeclarationKind kind)
{
    return kind == DeclarationKind::PositionalFormalParameter ||
           kind == DeclarationKind::FormalParameter;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::tryDeclareVar(HandlePropertyName name, DeclarationKind kind, uint32_t beginPos,
                                    Maybe<DeclarationKind>* redeclaredKind, uint32_t* prevPos)
{
    MOZ_ASSERT(DeclarationKindIsVar(kind));

    // It is an early error if a 'var' declaration appears inside a
    // scope contour that has a lexical declaration of the same name. For
    // example, the following are early errors:
    //
    //   { let x; var x; }
    //   { { var x; } let x; }
    //
    // And the following are not:
    //
    //   { var x; var x; }
    //   { { let x; } var x; }

    for (ParseContext::Scope* scope = pc->innermostScope();
         scope != pc->varScope().enclosing();
         scope = scope->enclosing())
    {
        if (AddDeclaredNamePtr p = scope->lookupDeclaredNameForAdd(name)) {
            DeclarationKind declaredKind = p->value()->kind();
            if (DeclarationKindIsVar(declaredKind)) {
                // Any vars that are redeclared as body-level functions must
                // be recorded as body-level functions.
                //
                // In the case of global and eval scripts, GlobalDeclaration-
                // Instantiation [1] and EvalDeclarationInstantiation [2]
                // check for the declarability of global var and function
                // bindings via CanDeclareVar [3] and CanDeclareGlobal-
                // Function [4]. CanDeclareGlobalFunction is strictly more
                // restrictive than CanDeclareGlobalVar, so record the more
                // restrictive kind. These semantics are implemented in
                // CheckCanDeclareGlobalBinding.
                //
                // For a var previously declared as ForOfVar, this previous
                // DeclarationKind is used only to check for if the
                // 'arguments' binding should be declared. Since body-level
                // functions shadow 'arguments' [5], it is correct to alter
                // the kind to BodyLevelFunction. See
                // declareFunctionArgumentsObject.
                //
                // For a var previously declared as
                // VarForAnnexBLexicalFunction, this previous DeclarationKind
                // is used so that vars synthesized solely for Annex B.3.3 may
                // be removed if an early error would occur. If a synthesized
                // Annex B.3.3 var has the same name as a body-level function,
                // this is not a redeclaration, and indeed, because the
                // body-level function binds the name, this name should not be
                // removed should a redeclaration occur in the future. Thus it
                // is also correct to alter the kind to BodyLevelFunction.
                //
                // [1] ES 15.1.11
                // [2] ES 18.2.1.3
                // [3] ES 8.1.1.4.15
                // [4] ES 8.1.1.4.16
                // [5] ES 9.2.12
                if (kind == DeclarationKind::BodyLevelFunction)
                    p->value()->alterKind(kind);
            } else if (!DeclarationKindIsParameter(declaredKind)) {
                // Annex B.3.5 allows redeclaring simple (non-destructured)
                // catch parameters with var declarations, except when it
                // appears in a for-of.
                bool annexB35Allowance = declaredKind == DeclarationKind::SimpleCatchParameter &&
                                         kind != DeclarationKind::ForOfVar;

                // Annex B.3.3 allows redeclaring functions in the same block.
                bool annexB33Allowance = declaredKind == DeclarationKind::LexicalFunction &&
                                         kind == DeclarationKind::VarForAnnexBLexicalFunction &&
                                         scope == pc->innermostScope();

                if (!annexB35Allowance && !annexB33Allowance) {
                    *redeclaredKind = Some(declaredKind);
                    *prevPos = p->value()->pos();
                    return true;
                }
            } else if (kind == DeclarationKind::VarForAnnexBLexicalFunction) {
                MOZ_ASSERT(DeclarationKindIsParameter(declaredKind));

                // Annex B.3.3.1 disallows redeclaring parameter names.
                // We don't need to set *prevPos here since this case is not
                // an error.
                *redeclaredKind = Some(declaredKind);
                return true;
            }
        } else {
            if (!scope->addDeclaredName(pc, p, name, kind, beginPos))
                return false;
        }
    }

    if (!pc->sc()->strict() && pc->sc()->isEvalContext()) {
        *redeclaredKind = isVarRedeclaredInEval(name, kind);
        // We don't have position information at runtime.
        *prevPos = DeclaredNameInfo::npos;
    }

    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::tryDeclareVarForAnnexBLexicalFunction(HandlePropertyName name,
                                                            uint32_t beginPos, bool* tryAnnexB)
{
    Maybe<DeclarationKind> redeclaredKind;
    uint32_t unused;
    if (!tryDeclareVar(name, DeclarationKind::VarForAnnexBLexicalFunction, beginPos,
                       &redeclaredKind, &unused))
    {
        return false;
    }

    if (!redeclaredKind && pc->isFunctionBox()) {
        ParseContext::Scope& funScope = pc->functionScope();
        ParseContext::Scope& varScope = pc->varScope();
        if (&funScope != &varScope) {
            // Annex B.3.3.1 disallows redeclaring parameter names. In the
            // presence of parameter expressions, parameter names are on the
            // function scope, which encloses the var scope. This means
            // tryDeclareVar call above would not catch this case, so test it
            // manually.
            if (AddDeclaredNamePtr p = funScope.lookupDeclaredNameForAdd(name)) {
                DeclarationKind declaredKind = p->value()->kind();
                if (DeclarationKindIsParameter(declaredKind))
                    redeclaredKind = Some(declaredKind);
                else
                    MOZ_ASSERT(FunctionScope::isSpecialName(context, name));
            }
        }
    }

    if (redeclaredKind) {
        // If an early error would have occurred, undo all the
        // VarForAnnexBLexicalFunction declarations.
        *tryAnnexB = false;
        ParseContext::Scope::removeVarForAnnexBLexicalFunction(pc, name);
    } else {
        *tryAnnexB = true;
    }

    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::checkLexicalDeclarationDirectlyWithinBlock(ParseContext::Statement& stmt,
                                                                 DeclarationKind kind,
                                                                 TokenPos pos)
{
    MOZ_ASSERT(DeclarationKindIsLexical(kind));

    // It is an early error to declare a lexical binding not directly
    // within a block.
    if (!StatementKindIsBraced(stmt.kind()) &&
        stmt.kind() != StatementKind::ForLoopLexicalHead)
    {
        errorAt(pos.begin,
                stmt.kind() == StatementKind::Label
                ? JSMSG_LEXICAL_DECL_LABEL
                : JSMSG_LEXICAL_DECL_NOT_IN_BLOCK,
                DeclarationKindString(kind));
        return false;
    }

    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::noteDeclaredName(HandlePropertyName name, DeclarationKind kind,
                                       TokenPos pos)
{
    // The asm.js validator does all its own symbol-table management so, as an
    // optimization, avoid doing any work here.
    if (pc->useAsmOrInsideUseAsm())
        return true;

    switch (kind) {
      case DeclarationKind::Var:
      case DeclarationKind::BodyLevelFunction:
      case DeclarationKind::ForOfVar: {
        Maybe<DeclarationKind> redeclaredKind;
        uint32_t prevPos;
        if (!tryDeclareVar(name, kind, pos.begin, &redeclaredKind, &prevPos))
            return false;

        if (redeclaredKind) {
            reportRedeclaration(name, *redeclaredKind, pos, prevPos);
            return false;
        }

        break;
      }

      case DeclarationKind::ModuleBodyLevelFunction: {
          MOZ_ASSERT(pc->atModuleLevel());

          AddDeclaredNamePtr p = pc->varScope().lookupDeclaredNameForAdd(name);
          if (p) {
              reportRedeclaration(name, p->value()->kind(), pos, p->value()->pos());
              return false;
          }

          if (!pc->varScope().addDeclaredName(pc, p, name, kind, pos.begin))
              return false;

          // Body-level functions in modules are always closed over.
          pc->varScope().lookupDeclaredName(name)->value()->setClosedOver();

          break;
      }

      case DeclarationKind::FormalParameter: {
        // It is an early error if any non-positional formal parameter name
        // (e.g., destructuring formal parameter) is duplicated.

        AddDeclaredNamePtr p = pc->functionScope().lookupDeclaredNameForAdd(name);
        if (p) {
            error(JSMSG_BAD_DUP_ARGS);
            return false;
        }

        if (!pc->functionScope().addDeclaredName(pc, p, name, kind, pos.begin))
            return false;

        break;
      }

      case DeclarationKind::LexicalFunction: {
        // Functions in block have complex allowances in sloppy mode for being
        // labelled that other lexical declarations do not have. Those checks
        // are more complex than calling checkLexicalDeclarationDirectlyWithin-
        // Block and are done inline in callers.

        ParseContext::Scope* scope = pc->innermostScope();
        if (AddDeclaredNamePtr p = scope->lookupDeclaredNameForAdd(name)) {
            // It is usually an early error if there is another declaration
            // with the same name in the same scope.
            //
            // In sloppy mode, lexical functions may redeclare other lexical
            // functions for web compatibility reasons.
            if (pc->sc()->strict() ||
                (p->value()->kind() != DeclarationKind::LexicalFunction &&
                 p->value()->kind() != DeclarationKind::VarForAnnexBLexicalFunction))
            {
                reportRedeclaration(name, p->value()->kind(), pos, p->value()->pos());
                return false;
            }

            // Update the DeclarationKind to make a LexicalFunction
            // declaration that shadows the VarForAnnexBLexicalFunction.
            p->value()->alterKind(kind);
        } else {
            if (!scope->addDeclaredName(pc, p, name, kind, pos.begin))
                return false;
        }

        break;
      }

      case DeclarationKind::Let:
      case DeclarationKind::Const:
        // The BoundNames of LexicalDeclaration and ForDeclaration must not
        // contain 'let'. (CatchParameter is the only lexical binding form
        // without this restriction.)
        if (name == context->names().let) {
            errorAt(pos.begin, JSMSG_LEXICAL_DECL_DEFINES_LET);
            return false;
        }

        MOZ_FALLTHROUGH;

      case DeclarationKind::Import:
        // Module code is always strict, so 'let' is always a keyword and never a name.
        MOZ_ASSERT(name != context->names().let);
        MOZ_FALLTHROUGH;

      case DeclarationKind::SimpleCatchParameter:
      case DeclarationKind::CatchParameter: {
        if (ParseContext::Statement* stmt = pc->innermostStatement()) {
            if (!checkLexicalDeclarationDirectlyWithinBlock(*stmt, kind, pos))
                return false;
        }

        ParseContext::Scope* scope = pc->innermostScope();

        // For body-level lexically declared names in a function, it is an
        // early error if there is a formal parameter of the same name. This
        // needs a special check if there is an extra var scope due to
        // parameter expressions.
        if (pc->isFunctionExtraBodyVarScopeInnermost()) {
            DeclaredNamePtr p = pc->functionScope().lookupDeclaredName(name);
            if (p && DeclarationKindIsParameter(p->value()->kind())) {
                reportRedeclaration(name, p->value()->kind(), pos, p->value()->pos());
                return false;
            }
        }

        // It is an early error if there is another declaration with the same
        // name in the same scope.
        AddDeclaredNamePtr p = scope->lookupDeclaredNameForAdd(name);
        if (p) {
            // If the early error would have occurred due to Annex B.3.3
            // semantics, remove the synthesized Annex B var declaration, do
            // not report the redeclaration, and declare the lexical name.
            if (p->value()->kind() == DeclarationKind::VarForAnnexBLexicalFunction) {
                ParseContext::Scope::removeVarForAnnexBLexicalFunction(pc, name);
                p = scope->lookupDeclaredNameForAdd(name);
                MOZ_ASSERT(!p);
            } else {
                reportRedeclaration(name, p->value()->kind(), pos, p->value()->pos());
                return false;
            }
        }

        if (!p && !scope->addDeclaredName(pc, p, name, kind, pos.begin))
            return false;

        break;
      }

      case DeclarationKind::CoverArrowParameter:
        // CoverArrowParameter is only used as a placeholder declaration kind.
        break;

      case DeclarationKind::PositionalFormalParameter:
        MOZ_CRASH("Positional formal parameter names should use "
                  "notePositionalFormalParameter");
        break;

      case DeclarationKind::VarForAnnexBLexicalFunction:
        MOZ_CRASH("Synthesized Annex B vars should go through "
                  "tryDeclareVarForAnnexBLexicalFunction");
        break;
    }

    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::noteUsedName(HandlePropertyName name)
{
    // If the we are delazifying, the LazyScript already has all the
    // closed-over info for bindings and there's no need to track used names.
    if (handler.canSkipLazyClosedOverBindings())
        return true;

    // The asm.js validator does all its own symbol-table management so, as an
    // optimization, avoid doing any work here.
    if (pc->useAsmOrInsideUseAsm())
        return true;

    // Global bindings are properties and not actual bindings; we don't need
    // to know if they are closed over. So no need to track used name at the
    // global scope. It is not incorrect to track them, this is an
    // optimization.
    ParseContext::Scope* scope = pc->innermostScope();
    if (pc->sc()->isGlobalContext() && scope == &pc->varScope())
        return true;

    return usedNames.noteUse(context, name, pc->scriptId(), scope->id());
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::hasUsedName(HandlePropertyName name)
{
    if (UsedNamePtr p = usedNames.lookup(name))
        return p->value().isUsedInScript(pc->scriptId());
    return false;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::propagateFreeNamesAndMarkClosedOverBindings(ParseContext::Scope& scope)
{
    if (handler.canSkipLazyClosedOverBindings()) {
        // Scopes are nullptr-delimited in the LazyScript closed over bindings
        // array.
        while (JSAtom* name = handler.nextLazyClosedOverBinding())
            scope.lookupDeclaredName(name)->value()->setClosedOver();
        return true;
    }

    bool isSyntaxParser = mozilla::IsSame<ParseHandler, SyntaxParseHandler>::value;
    uint32_t scriptId = pc->scriptId();
    uint32_t scopeId = scope.id();
    for (BindingIter bi = scope.bindings(pc); bi; bi++) {
        if (UsedNamePtr p = usedNames.lookup(bi.name())) {
            bool closedOver;
            p->value().noteBoundInScope(scriptId, scopeId, &closedOver);
            if (closedOver) {
                bi.setClosedOver();

                if (isSyntaxParser && !pc->closedOverBindingsForLazy().append(bi.name())) {
                    ReportOutOfMemory(context);
                    return false;
                }
            }
        }
    }

    // Append a nullptr to denote end-of-scope.
    if (isSyntaxParser && !pc->closedOverBindingsForLazy().append(nullptr)) {
        ReportOutOfMemory(context);
        return false;
    }

    return true;
}

template <>
bool
Parser<FullParseHandler>::checkStatementsEOF()
{
    // This is designed to be paired with parsing a statement list at the top
    // level.
    //
    // The statementList() call breaks on TOK_RC, so make sure we've
    // reached EOF here.
    TokenKind tt;
    if (!tokenStream.peekToken(&tt, TokenStream::Operand))
        return false;
    if (tt != TOK_EOF) {
        error(JSMSG_UNEXPECTED_TOKEN, "expression", TokenKindToDesc(tt));
        return false;
    }
    return true;
}

template <typename Scope>
static typename Scope::Data*
NewEmptyBindingData(ExclusiveContext* cx, LifoAlloc& alloc, uint32_t numBindings)
{
    using Data = typename Scope::Data;
    size_t allocSize = Scope::sizeOfData(numBindings);
    auto* bindings = alloc.allocInSize<Data>(allocSize, numBindings);
    if (!bindings)
        ReportOutOfMemory(cx);
    return bindings;
}

/**
 * Copy-construct |BindingName|s from |bindings| into |cursor|, then return
 * the location one past the newly-constructed |BindingName|s.
 */
static MOZ_MUST_USE BindingName*
FreshlyInitializeBindings(BindingName* cursor, const Vector<BindingName>& bindings)
{
    for (const BindingName& binding : bindings)
        new (cursor++) BindingName(binding);
    return cursor;
}

template <>
Maybe<GlobalScope::Data*>
Parser<FullParseHandler>::newGlobalScopeData(ParseContext::Scope& scope)
{
    Vector<BindingName> funs(context);
    Vector<BindingName> vars(context);
    Vector<BindingName> lets(context);
    Vector<BindingName> consts(context);

    bool allBindingsClosedOver = pc->sc()->allBindingsClosedOver();
    for (BindingIter bi = scope.bindings(pc); bi; bi++) {
        BindingName binding(bi.name(), allBindingsClosedOver || bi.closedOver());
        switch (bi.kind()) {
          case BindingKind::Var:
            if (bi.declarationKind() == DeclarationKind::BodyLevelFunction) {
                if (!funs.append(binding))
                    return Nothing();
            } else {
                if (!vars.append(binding))
                    return Nothing();
            }
            break;
          case BindingKind::Let:
            if (!lets.append(binding))
                return Nothing();
            break;
          case BindingKind::Const:
            if (!consts.append(binding))
                return Nothing();
            break;
          default:
            MOZ_CRASH("Bad global scope BindingKind");
        }
    }

    GlobalScope::Data* bindings = nullptr;
    uint32_t numBindings = funs.length() + vars.length() + lets.length() + consts.length();

    if (numBindings > 0) {
        bindings = NewEmptyBindingData<GlobalScope>(context, alloc, numBindings);
        if (!bindings)
            return Nothing();

        // The ordering here is important. See comments in GlobalScope.
        BindingName* start = bindings->trailingNames.start();
        BindingName* cursor = start;

        cursor = FreshlyInitializeBindings(cursor, funs);

        bindings->varStart = cursor - start;
        cursor = FreshlyInitializeBindings(cursor, vars);

        bindings->letStart = cursor - start;
        cursor = FreshlyInitializeBindings(cursor, lets);

        bindings->constStart = cursor - start;
        cursor = FreshlyInitializeBindings(cursor, consts);

        bindings->length = numBindings;
    }

    return Some(bindings);
}

template <>
Maybe<ModuleScope::Data*>
Parser<FullParseHandler>::newModuleScopeData(ParseContext::Scope& scope)
{
    Vector<BindingName> imports(context);
    Vector<BindingName> vars(context);
    Vector<BindingName> lets(context);
    Vector<BindingName> consts(context);

    bool allBindingsClosedOver = pc->sc()->allBindingsClosedOver();
    for (BindingIter bi = scope.bindings(pc); bi; bi++) {
        // Imports are indirect bindings and must not be given known slots.
        BindingName binding(bi.name(), (allBindingsClosedOver || bi.closedOver()) &&
                                       bi.kind() != BindingKind::Import);
        switch (bi.kind()) {
          case BindingKind::Import:
            if (!imports.append(binding))
                return Nothing();
            break;
          case BindingKind::Var:
            if (!vars.append(binding))
                return Nothing();
            break;
          case BindingKind::Let:
            if (!lets.append(binding))
                return Nothing();
            break;
          case BindingKind::Const:
            if (!consts.append(binding))
                return Nothing();
            break;
          default:
            MOZ_CRASH("Bad module scope BindingKind");
        }
    }

    ModuleScope::Data* bindings = nullptr;
    uint32_t numBindings = imports.length() + vars.length() + lets.length() + consts.length();

    if (numBindings > 0) {
        bindings = NewEmptyBindingData<ModuleScope>(context, alloc, numBindings);
        if (!bindings)
            return Nothing();

        // The ordering here is important. See comments in ModuleScope.
        BindingName* start = bindings->trailingNames.start();
        BindingName* cursor = start;

        cursor = FreshlyInitializeBindings(cursor, imports);

        bindings->varStart = cursor - start;
        cursor = FreshlyInitializeBindings(cursor, vars);

        bindings->letStart = cursor - start;
        cursor = FreshlyInitializeBindings(cursor, lets);

        bindings->constStart = cursor - start;
        cursor = FreshlyInitializeBindings(cursor, consts);

        bindings->length = numBindings;
    }

    return Some(bindings);
}

template <>
Maybe<EvalScope::Data*>
Parser<FullParseHandler>::newEvalScopeData(ParseContext::Scope& scope)
{
    Vector<BindingName> funs(context);
    Vector<BindingName> vars(context);

    for (BindingIter bi = scope.bindings(pc); bi; bi++) {
        // Eval scopes only contain 'var' bindings. Make all bindings aliased
        // for now.
        MOZ_ASSERT(bi.kind() == BindingKind::Var);
        BindingName binding(bi.name(), true);
        if (bi.declarationKind() == DeclarationKind::BodyLevelFunction) {
            if (!funs.append(binding))
                return Nothing();
        } else {
            if (!vars.append(binding))
                return Nothing();
        }
    }

    EvalScope::Data* bindings = nullptr;
    uint32_t numBindings = funs.length() + vars.length();

    if (numBindings > 0) {
        bindings = NewEmptyBindingData<EvalScope>(context, alloc, numBindings);
        if (!bindings)
            return Nothing();

        BindingName* start = bindings->trailingNames.start();
        BindingName* cursor = start;

        // Keep track of what vars are functions. This is only used in BCE to omit
        // superfluous DEFVARs.
        cursor = FreshlyInitializeBindings(cursor, funs);

        bindings->varStart = cursor - start;
        cursor = FreshlyInitializeBindings(cursor, vars);

        bindings->length = numBindings;
    }

    return Some(bindings);
}

template <>
Maybe<FunctionScope::Data*>
Parser<FullParseHandler>::newFunctionScopeData(ParseContext::Scope& scope, bool hasParameterExprs)
{
    Vector<BindingName> positionalFormals(context);
    Vector<BindingName> formals(context);
    Vector<BindingName> vars(context);

    bool allBindingsClosedOver = pc->sc()->allBindingsClosedOver();
    bool hasDuplicateParams = pc->functionBox()->hasDuplicateParameters;

    // Positional parameter names must be added in order of appearance as they are
    // referenced using argument slots.
    for (size_t i = 0; i < pc->positionalFormalParameterNames().length(); i++) {
        JSAtom* name = pc->positionalFormalParameterNames()[i];

        BindingName bindName;
        if (name) {
            DeclaredNamePtr p = scope.lookupDeclaredName(name);

            // Do not consider any positional formal parameters closed over if
            // there are parameter defaults. It is the binding in the defaults
            // scope that is closed over instead.
            bool closedOver = allBindingsClosedOver ||
                              (p && p->value()->closedOver());

            // If the parameter name has duplicates, only the final parameter
            // name should be on the environment, as otherwise the environment
            // object would have multiple, same-named properties.
            if (hasDuplicateParams) {
                for (size_t j = pc->positionalFormalParameterNames().length() - 1; j > i; j--) {
                    if (pc->positionalFormalParameterNames()[j] == name) {
                        closedOver = false;
                        break;
                    }
                }
            }

            bindName = BindingName(name, closedOver);
        }

        if (!positionalFormals.append(bindName))
            return Nothing();
    }

    for (BindingIter bi = scope.bindings(pc); bi; bi++) {
        BindingName binding(bi.name(), allBindingsClosedOver || bi.closedOver());
        switch (bi.kind()) {
          case BindingKind::FormalParameter:
            // Positional parameter names are already handled above.
            if (bi.declarationKind() == DeclarationKind::FormalParameter) {
                if (!formals.append(binding))
                    return Nothing();
            }
            break;
          case BindingKind::Var:
            // The only vars in the function scope when there are parameter
            // exprs, which induces a separate var environment, should be the
            // special bindings.
            MOZ_ASSERT_IF(hasParameterExprs, FunctionScope::isSpecialName(context, bi.name()));
            if (!vars.append(binding))
                return Nothing();
            break;
          default:
            break;
        }
    }

    FunctionScope::Data* bindings = nullptr;
    uint32_t numBindings = positionalFormals.length() + formals.length() + vars.length();

    if (numBindings > 0) {
        bindings = NewEmptyBindingData<FunctionScope>(context, alloc, numBindings);
        if (!bindings)
            return Nothing();

        // The ordering here is important. See comments in FunctionScope.
        BindingName* start = bindings->trailingNames.start();
        BindingName* cursor = start;

        cursor = FreshlyInitializeBindings(cursor, positionalFormals);

        bindings->nonPositionalFormalStart = cursor - start;
        cursor = FreshlyInitializeBindings(cursor, formals);

        bindings->varStart = cursor - start;
        cursor = FreshlyInitializeBindings(cursor, vars);

        bindings->length = numBindings;
    }

    return Some(bindings);
}

template <>
Maybe<VarScope::Data*>
Parser<FullParseHandler>::newVarScopeData(ParseContext::Scope& scope)
{
    Vector<BindingName> vars(context);

    bool allBindingsClosedOver = pc->sc()->allBindingsClosedOver();

    for (BindingIter bi = scope.bindings(pc); bi; bi++) {
        BindingName binding(bi.name(), allBindingsClosedOver || bi.closedOver());
        if (!vars.append(binding))
            return Nothing();
    }

    VarScope::Data* bindings = nullptr;
    uint32_t numBindings = vars.length();

    if (numBindings > 0) {
        bindings = NewEmptyBindingData<VarScope>(context, alloc, numBindings);
        if (!bindings)
            return Nothing();

        // The ordering here is important. See comments in FunctionScope.
        BindingName* start = bindings->trailingNames.start();
        BindingName* cursor = start;

        cursor = FreshlyInitializeBindings(cursor, vars);

        bindings->length = numBindings;
    }

    return Some(bindings);
}

template <>
Maybe<LexicalScope::Data*>
Parser<FullParseHandler>::newLexicalScopeData(ParseContext::Scope& scope)
{
    Vector<BindingName> lets(context);
    Vector<BindingName> consts(context);

    // Unlike other scopes with bindings which are body-level, it is unknown
    // if pc->sc()->allBindingsClosedOver() is correct at the time of
    // finishing parsing a lexical scope.
    //
    // Instead, pc->sc()->allBindingsClosedOver() is checked in
    // EmitterScope::enterLexical. Also see comment there.
    for (BindingIter bi = scope.bindings(pc); bi; bi++) {
        BindingName binding(bi.name(), bi.closedOver());
        switch (bi.kind()) {
          case BindingKind::Let:
            if (!lets.append(binding))
                return Nothing();
            break;
          case BindingKind::Const:
            if (!consts.append(binding))
                return Nothing();
            break;
          default:
            break;
        }
    }

    LexicalScope::Data* bindings = nullptr;
    uint32_t numBindings = lets.length() + consts.length();

    if (numBindings > 0) {
        bindings = NewEmptyBindingData<LexicalScope>(context, alloc, numBindings);
        if (!bindings)
            return Nothing();

        // The ordering here is important. See comments in LexicalScope.
        BindingName* cursor = bindings->trailingNames.start();
        BindingName* start = cursor;

        cursor = FreshlyInitializeBindings(cursor, lets);

        bindings->constStart = cursor - start;
        cursor = FreshlyInitializeBindings(cursor, consts);

        bindings->length = numBindings;
    }

    return Some(bindings);
}

template <>
SyntaxParseHandler::LexicalScopeNodeType
Parser<SyntaxParseHandler>::finishLexicalScope(ParseContext::Scope& scope, Node body)
{
    if (!propagateFreeNamesAndMarkClosedOverBindings(scope))
        return null();
    return handler.newLexicalScope(body);
}

template <>
LexicalScopeNode*
Parser<FullParseHandler>::finishLexicalScope(ParseContext::Scope& scope, ParseNode* body)
{
    if (!propagateFreeNamesAndMarkClosedOverBindings(scope))
        return nullptr;
    Maybe<LexicalScope::Data*> bindings = newLexicalScopeData(scope);
    if (!bindings)
        return nullptr;
    return handler.newLexicalScope(*bindings, body);
}

static bool
IsArgumentsUsedInLegacyGenerator(ExclusiveContext* cx, Scope* scope)
{
    JSAtom* argumentsName = cx->names().arguments;
    for (ScopeIter si(scope); si; si++) {
        if (si.scope()->is<LexicalScope>()) {
            // Using a shadowed lexical 'arguments' is okay.
            for (::BindingIter bi(si.scope()); bi; bi++) {
                if (bi.name() == argumentsName)
                    return false;
            }
        } else if (si.scope()->is<FunctionScope>()) {
            // It's an error to use 'arguments' in a legacy generator expression.
            JSScript* script = si.scope()->as<FunctionScope>().script();
            return script->isGeneratorExp() && script->isLegacyGenerator();
        }
    }

    return false;
}

template <>
LexicalScopeNode*
Parser<FullParseHandler>::evalBody(EvalSharedContext* evalsc)
{
    ParseContext evalpc(this, evalsc, /* newDirectives = */ nullptr);
    if (!evalpc.init())
        return nullptr;

    ParseContext::VarScope varScope(this);
    if (!varScope.init(pc))
        return nullptr;

    LexicalScopeNode* body;
    {
        // All evals have an implicit non-extensible lexical scope.
        ParseContext::Scope lexicalScope(this);
        if (!lexicalScope.init(pc))
            return nullptr;

        ParseNode* list = statementList(YieldIsName);
        if (!list)
            return nullptr;

        if (!checkStatementsEOF())
            return nullptr;

        body = finishLexicalScope(lexicalScope, list);
        if (!body)
            return nullptr;
    }

    // It's an error to use 'arguments' in a legacy generator expression.
    //
    // If 'arguments' appears free (i.e. not a declared name) or if the
    // declaration does not shadow the enclosing script's 'arguments'
    // binding (i.e. not a lexical declaration), check the enclosing
    // script.
    if (hasUsedName(context->names().arguments)) {
        if (IsArgumentsUsedInLegacyGenerator(context, pc->sc()->compilationEnclosingScope())) {
            error(JSMSG_BAD_GENEXP_BODY, js_arguments_str);
            return nullptr;
        }
    }

#ifdef DEBUG
    if (evalpc.superScopeNeedsHomeObject() && evalsc->compilationEnclosingScope()) {
        // If superScopeNeedsHomeObject_ is set and we are an entry-point
        // ParseContext, then we must be emitting an eval script, and the
        // outer function must already be marked as needing a home object
        // since it contains an eval.
        ScopeIter si(evalsc->compilationEnclosingScope());
        for (; si; si++) {
            if (si.kind() == ScopeKind::Function) {
                JSFunction* fun = si.scope()->as<FunctionScope>().canonicalFunction();
                if (fun->isArrow())
                    continue;
                MOZ_ASSERT(fun->allowSuperProperty());
                MOZ_ASSERT(fun->nonLazyScript()->needsHomeObject());
                break;
            }
        }
        MOZ_ASSERT(!si.done(),
                   "Eval must have found an enclosing function box scope that allows super.property");
    }
#endif

    ParseNode* node = body;
    if (!FoldConstants(context, &node, this))
        return nullptr;
    body = handler.asLexicalScope(node);

    Maybe<EvalScope::Data*> bindings = newEvalScopeData(pc->varScope());
    if (!bindings)
        return nullptr;
    evalsc->bindings = *bindings;

    return body;
}

template <>
ListNode*
Parser<FullParseHandler>::globalBody(GlobalSharedContext* globalsc)
{
    ParseContext globalpc(this, globalsc, /* newDirectives = */ nullptr);
    if (!globalpc.init())
        return nullptr;

    ParseContext::VarScope varScope(this);
    if (!varScope.init(pc))
        return nullptr;

    ListNode* body = statementList(YieldIsName);
    if (!body)
        return nullptr;

    if (!checkStatementsEOF())
        return nullptr;

    ParseNode* node = body;
    if (!FoldConstants(context, &node, this)) {
        return null();
    }
    body = &node->as<ListNode>();

    Maybe<GlobalScope::Data*> bindings = newGlobalScopeData(pc->varScope());
    if (!bindings)
        return nullptr;
    globalsc->bindings = *bindings;

    return body;
}

template <>
ModuleNode*
Parser<FullParseHandler>::moduleBody(ModuleSharedContext* modulesc)
{
    MOZ_ASSERT(checkOptionsCalled);

    ParseContext modulepc(this, modulesc, nullptr);
    if (!modulepc.init())
        return null();

    ParseContext::VarScope varScope(this);
    if (!varScope.init(pc))
        return nullptr;

    ModuleNodeType moduleNode = handler.newModule();
    if (!moduleNode)
        return null();

    AutoAwaitIsKeyword<FullParseHandler> awaitIsKeyword(this, AwaitIsModuleKeyword);
    ListNode* stmtList = statementList(YieldIsName);
    if (!stmtList) {
        return null();
    }

    MOZ_ASSERT(stmtList->isKind(PNK_STATEMENTLIST));
    moduleNode->setBody(&stmtList->as<ListNode>());

    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();
    if (tt != TOK_EOF) {
        error(JSMSG_GARBAGE_AFTER_INPUT, "module", TokenKindToDesc(tt));
        return null();
    }

    if (!modulesc->builder.buildTables())
        return null();

    // Check exported local bindings exist and mark them as closed over.
    for (auto entry : modulesc->builder.localExportEntries()) {
        JSAtom* name = entry->localName();
        MOZ_ASSERT(name);

        DeclaredNamePtr p = modulepc.varScope().lookupDeclaredName(name);
        if (!p) {
            JSAutoByteString str;
            if (!str.encodeLatin1(context, name))
                return null();

            JS_ReportErrorNumberLatin1(context->asJSContext(), GetErrorMessage, nullptr,
                                       JSMSG_MISSING_EXPORT, str.ptr());
            return null();
        }

        p->value()->setClosedOver();
    }

    ParseNode* node = stmtList;
    if (!FoldConstants(context, &node, this)) {
        return null();
    }
    stmtList = &node->as<ListNode>();

    if (!propagateFreeNamesAndMarkClosedOverBindings(modulepc.varScope()))
        return null();

    Maybe<ModuleScope::Data*> bindings = newModuleScopeData(modulepc.varScope());
    if (!bindings)
        return nullptr;

    modulesc->bindings = *bindings;
    return moduleNode;
}

template <>
SyntaxParseHandler::ModuleNodeType
Parser<SyntaxParseHandler>::moduleBody(ModuleSharedContext* modulesc)
{
    MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::hasUsedFunctionSpecialName(HandlePropertyName name)
{
    MOZ_ASSERT(name == context->names().arguments || name == context->names().dotThis);
    return hasUsedName(name) || pc->functionBox()->bindingsAccessedDynamically();
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::declareFunctionThis(bool canSkipLazyClosedOverBindings)
{
    // The asm.js validator does all its own symbol-table management so, as an
    // optimization, avoid doing any work here.
    if (pc->useAsmOrInsideUseAsm())
        return true;

    // Derived class constructors emit JSOP_CHECKRETURN, which requires
    // '.this' to be bound.
    FunctionBox* funbox = pc->functionBox();
    HandlePropertyName dotThis = context->names().dotThis;

    bool declareThis;
    if (canSkipLazyClosedOverBindings)
        declareThis = funbox->function()->lazyScript()->hasThisBinding();
    else
        declareThis = hasUsedFunctionSpecialName(dotThis) ||
                      funbox->function()->kind() == JSFunction::FunctionKind::ClassConstructor;

    if (declareThis) {
        ParseContext::Scope& funScope = pc->functionScope();
        AddDeclaredNamePtr p = funScope.lookupDeclaredNameForAdd(dotThis);
        MOZ_ASSERT(!p);
        if (!funScope.addDeclaredName(pc, p, dotThis, DeclarationKind::Var,
                                      DeclaredNameInfo::npos))
        {
            return false;
        }
        funbox->setHasThisBinding();
    }

    return true;
}

template <typename ParseHandler>
typename ParseHandler::NameNodeType
Parser<ParseHandler>::newInternalDotName(HandlePropertyName name)
{
    NameNodeType nameNode = newName(name);
    if (!nameNode)
        return null();
    if (!noteUsedName(name))
        return null();
    return nameNode;
}

template <typename ParseHandler>
typename ParseHandler::NameNodeType
Parser<ParseHandler>::newThisName()
{
    return newInternalDotName(context->names().dotThis);
}

template <typename ParseHandler>
typename ParseHandler::NameNodeType
Parser<ParseHandler>::newDotGeneratorName()
{
    return newInternalDotName(context->names().dotGenerator);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::declareDotGeneratorName()
{
    // The special '.generator' binding must be on the function scope, as
    // generators expect to find it on the CallObject.
    ParseContext::Scope& funScope = pc->functionScope();
    HandlePropertyName dotGenerator = context->names().dotGenerator;
    AddDeclaredNamePtr p = funScope.lookupDeclaredNameForAdd(dotGenerator);
    if (!p && !funScope.addDeclaredName(pc, p, dotGenerator, DeclarationKind::Var,
                                        DeclaredNameInfo::npos))
    {
        return false;
    }
    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::finishFunctionScopes(bool isStandaloneFunction)
{
    FunctionBox* funbox = pc->functionBox();

    if (funbox->hasParameterExprs) {
        if (!propagateFreeNamesAndMarkClosedOverBindings(pc->functionScope()))
            return false;
    }

    if (funbox->function()->isNamedLambda() && !isStandaloneFunction) {
        if (!propagateFreeNamesAndMarkClosedOverBindings(pc->namedLambdaScope()))
            return false;
    }

    return true;
}

template <>
bool
Parser<FullParseHandler>::finishFunction(bool isStandaloneFunction /* = false */)
{
    if (!finishFunctionScopes(isStandaloneFunction))
        return false;

    FunctionBox* funbox = pc->functionBox();
    bool hasParameterExprs = funbox->hasParameterExprs;

    if (hasParameterExprs) {
        Maybe<VarScope::Data*> bindings = newVarScopeData(pc->varScope());
        if (!bindings)
            return false;
        funbox->extraVarScopeBindings().set(*bindings);
    }

    {
        Maybe<FunctionScope::Data*> bindings = newFunctionScopeData(pc->functionScope(),
                                                                    hasParameterExprs);
        if (!bindings)
            return false;
        funbox->functionScopeBindings().set(*bindings);
    }

    if (funbox->function()->isNamedLambda() && !isStandaloneFunction) {
        Maybe<LexicalScope::Data*> bindings = newLexicalScopeData(pc->namedLambdaScope());
        if (!bindings)
            return false;
        funbox->namedLambdaBindings().set(*bindings);
    }

    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::finishFunction(bool isStandaloneFunction /* = false */)
{
    // The LazyScript for a lazily parsed function needs to know its set of
    // free variables and inner functions so that when it is fully parsed, we
    // can skip over any already syntax parsed inner functions and still
    // retain correct scope information.

    if (!finishFunctionScopes(isStandaloneFunction))
        return false;

    // There are too many bindings or inner functions to be saved into the
    // LazyScript. Do a full parse.
    if (pc->closedOverBindingsForLazy().length() >= LazyScript::NumClosedOverBindingsLimit ||
        pc->innerFunctionsForLazy.length() >= LazyScript::NumInnerFunctionsLimit)
    {
        MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
        return false;
    }

    FunctionBox* funbox = pc->functionBox();
    RootedFunction fun(context, funbox->function());
    LazyScript* lazy = LazyScript::Create(context, fun, pc->closedOverBindingsForLazy(),
                                          pc->innerFunctionsForLazy, versionNumber(),
                                          funbox->bufStart, funbox->bufEnd,
                                          funbox->toStringStart,
                                          funbox->startLine, funbox->startColumn);
    if (!lazy)
        return false;

    // Flags that need to be copied into the JSScript when we do the full
    // parse.
    if (pc->sc()->strict())
        lazy->setStrict();
    lazy->setGeneratorKind(funbox->generatorKind());
    lazy->setAsyncKind(funbox->asyncKind());
    if (funbox->hasRest())
        lazy->setHasRest();
    if (funbox->isExprBody())
        lazy->setIsExprBody();
    if (funbox->isLikelyConstructorWrapper())
        lazy->setLikelyConstructorWrapper();
    if (funbox->isDerivedClassConstructor())
        lazy->setIsDerivedClassConstructor();
    if (funbox->needsHomeObject())
        lazy->setNeedsHomeObject();
    if (funbox->declaredArguments)
        lazy->setShouldDeclareArguments();
    if (funbox->hasThisBinding())
        lazy->setHasThisBinding();
    if (funbox->hasModuleGoal())
        lazy->setHasModuleGoal();

    // Flags that need to copied back into the parser when we do the full
    // parse.
    PropagateTransitiveParseFlags(funbox, lazy);

    fun->initLazyScript(lazy);
    return true;
}

static YieldHandling
GetYieldHandling(GeneratorKind generatorKind)
{
    if (generatorKind == NotGenerator)
        return YieldIsName;
    return YieldIsKeyword;
}

static AwaitHandling
GetAwaitHandling(FunctionAsyncKind asyncKind)
{
    if (asyncKind == SyncFunction)
        return AwaitIsName;
    return AwaitIsKeyword;
}

template <>
FunctionNode*
Parser<FullParseHandler>::standaloneFunction(HandleFunction fun,
                                             HandleScope enclosingScope,
                                             Maybe<uint32_t> parameterListEnd,
                                             GeneratorKind generatorKind,
                                             FunctionAsyncKind asyncKind,
                                             Directives inheritedDirectives,
                                             Directives* newDirectives)
{
    MOZ_ASSERT(checkOptionsCalled);

    // Skip prelude.
    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return null();
    if (asyncKind == AsyncFunction) {
        if (!tokenStream.getToken(&tt))
            return null();
    }
    MOZ_ASSERT(tt == TOK_FUNCTION);

    if (!tokenStream.getToken(&tt))
        return null();
    if (generatorKind == StarGenerator && asyncKind == SyncFunction) {
        MOZ_ASSERT(tt == TOK_MUL);
        if (!tokenStream.getToken(&tt))
            return null();
    }

    // Skip function name, if present.
    if (tt == TOK_NAME || tt == TOK_YIELD) {
        MOZ_ASSERT(tokenStream.currentName() == fun->explicitName());
    } else {
        MOZ_ASSERT(fun->explicitName() == nullptr);
        tokenStream.ungetToken();
    }

    FunctionNodeType funNode = handler.newFunction(FunctionSyntaxKind::Statement, pos());
    if (!funNode)
        return null();

    ListNodeType argsbody = handler.newList(PNK_PARAMSBODY);
    if (!argsbody)
        return null();
    funNode->setBody(argsbody);

    FunctionBox* funbox = newFunctionBox(funNode, fun, /* toStringStart = */ 0, inheritedDirectives,
                                         generatorKind, asyncKind, /* tryAnnexB = */ false);
    if (!funbox)
        return null();
    funbox->initStandaloneFunction(enclosingScope);

    ParseContext funpc(this, funbox, newDirectives);
    if (!funpc.init())
        return null();
    funpc.setIsStandaloneFunctionBody();

    YieldHandling yieldHandling = GetYieldHandling(generatorKind);
    AwaitHandling awaitHandling = GetAwaitHandling(asyncKind);
    AutoAwaitIsKeyword<FullParseHandler> awaitIsKeyword(this, awaitHandling);
    if (!functionFormalParametersAndBody(InAllowed, yieldHandling, funNode, FunctionSyntaxKind::Statement,
                                         parameterListEnd, /* isStandaloneFunction = */ true))
    {
        return null();
    }

    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();
    if (tt != TOK_EOF) {
        error(JSMSG_GARBAGE_AFTER_INPUT, "function body", TokenKindToDesc(tt));
        return null();
    }

    ParseNode* node = funNode;
    if (!FoldConstants(context, &node, this))
        return null();
    funNode = &node->as<FunctionNode>();

    return funNode;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::declareFunctionArgumentsObject(bool canSkipLazyClosedOverBindings)
{
    FunctionBox* funbox = pc->functionBox();
    ParseContext::Scope& funScope = pc->functionScope();
    ParseContext::Scope& varScope = pc->varScope();

    bool hasExtraBodyVarScope = &funScope != &varScope;

    // Time to implement the odd semantics of 'arguments'.
    HandlePropertyName argumentsName = context->names().arguments;

    bool tryDeclareArguments;
    if (canSkipLazyClosedOverBindings)
        tryDeclareArguments = funbox->function()->lazyScript()->shouldDeclareArguments();
    else
        tryDeclareArguments = hasUsedFunctionSpecialName(argumentsName);

    // ES 9.2.12 steps 19 and 20 say formal parameters, lexical bindings,
    // and body-level functions named 'arguments' shadow the arguments
    // object.
    //
    // So even if there wasn't a free use of 'arguments' but there is a var
    // binding of 'arguments', we still might need the arguments object.
    //
    // If we have an extra var scope due to parameter expressions and the body
    // declared 'var arguments', we still need to declare 'arguments' in the
    // function scope.
    DeclaredNamePtr p = varScope.lookupDeclaredName(argumentsName);
    if (p && (p->value()->kind() == DeclarationKind::Var ||
              p->value()->kind() == DeclarationKind::ForOfVar))
    {
        if (hasExtraBodyVarScope)
            tryDeclareArguments = true;
        else
            funbox->usesArguments = true;
    }

    if (tryDeclareArguments) {
        AddDeclaredNamePtr p = funScope.lookupDeclaredNameForAdd(argumentsName);
        if (!p) {
            if (!funScope.addDeclaredName(pc, p, argumentsName, DeclarationKind::Var,
                                          DeclaredNameInfo::npos))
            {
                return false;
            }
            funbox->declaredArguments = true;
            funbox->usesArguments = true;
        } else if (hasExtraBodyVarScope) {
            // Formal parameters shadow the arguments object.
            return true;
        }
    }

    // Compute if we need an arguments object.
    if (funbox->usesArguments) {
        // There is an 'arguments' binding. Is the arguments object definitely
        // needed?
        //
        // Also see the flags' comments in ContextFlags.
        funbox->setArgumentsHasLocalBinding();

        // Dynamic scope access destroys all hope of optimization.
        if (pc->sc()->bindingsAccessedDynamically())
            funbox->setDefinitelyNeedsArgsObj();

        // If a script contains the debugger statement either directly or
        // within an inner function, the arguments object should be created
        // eagerly so the Debugger API may observe bindings.
        if (pc->sc()->hasDebuggerStatement())
            funbox->setDefinitelyNeedsArgsObj();
    }

    return true;
}

template <typename ParseHandler>
typename ParseHandler::LexicalScopeNodeType
Parser<ParseHandler>::functionBody(InHandling inHandling, YieldHandling yieldHandling,
                                   FunctionSyntaxKind kind, FunctionBodyType type)
{
    MOZ_ASSERT(pc->isFunctionBox());
    MOZ_ASSERT(!pc->funHasReturnExpr && !pc->funHasReturnVoid);

#ifdef DEBUG
    uint32_t startYieldOffset = pc->lastYieldOffset;
#endif

    // One might expect noteUsedName(".initializers") here when parsing a
    // constructor. See Parser<ParseHandler>::classDefinition on why
    // it's not here.

    Node body;
    if (type == StatementListBody) {
        bool inheritedStrict = pc->sc()->strict();
        body = statementList(yieldHandling);
        if (!body)
            return null();

        // When we transitioned from non-strict to strict mode, we need to
        // validate that all parameter names are valid strict mode names.
        if (!inheritedStrict && pc->sc()->strict()) {
            MOZ_ASSERT(pc->sc()->hasExplicitUseStrict(),
                       "strict mode should only change when a 'use strict' directive is present");
            if (!hasValidSimpleStrictParameterNames()) {
                // Request that this function be reparsed as strict to report
                // the invalid parameter name at the correct source location.
                pc->newDirectives->setStrict();
                return null();
            }
        }
    } else {
        MOZ_ASSERT(type == ExpressionBody);

        // Async functions are implemented as star generators, and star
        // generators are assumed to be statement lists, to prepend initial
        // `yield`.
        ListNodeType stmtList = null();
        if (pc->isAsync()) {
            stmtList = handler.newStatementList(pos());
            if (!stmtList)
                return null();
        }

        Node kid = assignExpr(inHandling, yieldHandling, TripledotProhibited);
        if (!kid)
            return null();

        body = handler.newReturnStatement(kid, handler.getPosition(kid));
        if (!body)
            return null();

        if (pc->isAsync()) {
            handler.addStatementToList(stmtList, body);
            body = stmtList;
        }
    }

    switch (pc->generatorKind()) {
      case NotGenerator:
        MOZ_ASSERT_IF(!pc->isAsync(), pc->lastYieldOffset == startYieldOffset);
        break;

      case LegacyGenerator:
        MOZ_ASSERT(pc->lastYieldOffset != startYieldOffset);

        // These should throw while parsing the yield expression.
        MOZ_ASSERT(kind != FunctionSyntaxKind::Arrow);
        MOZ_ASSERT(!IsGetterKind(kind));
        MOZ_ASSERT(!IsSetterKind(kind));
        MOZ_ASSERT(!IsConstructorKind(kind));
        MOZ_ASSERT(kind != FunctionSyntaxKind::Method);
        MOZ_ASSERT(type != ExpressionBody);
        break;

      case StarGenerator:
        MOZ_ASSERT(kind != FunctionSyntaxKind::Arrow);
        MOZ_ASSERT(type == StatementListBody);
        break;
    }

    if (pc->needsDotGeneratorName()) {
        MOZ_ASSERT_IF(!pc->isAsync(), type == StatementListBody);
        if (!declareDotGeneratorName())
            return null();
        NameNodeType generator = newDotGeneratorName();
        if (!generator)
            return null();
        if (!handler.prependInitialYield(handler.asList(body), generator))
            return null();
    }

    // Declare the 'arguments' and 'this' bindings if necessary before
    // finishing up the scope so these special bindings get marked as closed
    // over if necessary. Arrow functions don't have these bindings.
    if (kind != FunctionSyntaxKind::Arrow) {
        bool canSkipLazyClosedOverBindings = handler.canSkipLazyClosedOverBindings();
        if (!declareFunctionArgumentsObject(canSkipLazyClosedOverBindings))
            return null();
        if (!declareFunctionThis(canSkipLazyClosedOverBindings))
            return null();
    }

    return finishLexicalScope(pc->varScope(), body);
}

template <typename ParseHandler>
JSFunction*
Parser<ParseHandler>::newFunction(HandleAtom atom, FunctionSyntaxKind kind,
                                  GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
                                  HandleObject proto /* = nullptr */)
{
    MOZ_ASSERT_IF(kind == FunctionSyntaxKind::Statement, atom != nullptr);

    RootedFunction fun(context);

    gc::AllocKind allocKind = gc::AllocKind::FUNCTION;
    JSFunction::Flags flags;
    switch (kind) {
      case FunctionSyntaxKind::Expression:
        flags = (generatorKind == NotGenerator && asyncKind == SyncFunction
                 ? JSFunction::INTERPRETED_LAMBDA
                 : JSFunction::INTERPRETED_LAMBDA_GENERATOR_OR_ASYNC);
        break;
      case FunctionSyntaxKind::Arrow:
        flags = JSFunction::INTERPRETED_LAMBDA_ARROW;
        allocKind = gc::AllocKind::FUNCTION_EXTENDED;
        break;
      case FunctionSyntaxKind::Method:
      case FunctionSyntaxKind::FieldInitializer:
      case FunctionSyntaxKind::StaticClassBlock:
        MOZ_ASSERT(generatorKind == NotGenerator || generatorKind == StarGenerator);
        flags = (generatorKind == NotGenerator && asyncKind == SyncFunction
                 ? JSFunction::INTERPRETED_METHOD
                 : JSFunction::INTERPRETED_METHOD_GENERATOR_OR_ASYNC);
        allocKind = gc::AllocKind::FUNCTION_EXTENDED;
        break;
      case FunctionSyntaxKind::ClassConstructor:
      case FunctionSyntaxKind::DerivedClassConstructor:
        flags = JSFunction::INTERPRETED_CLASS_CONSTRUCTOR;
        allocKind = gc::AllocKind::FUNCTION_EXTENDED;
        break;
      case FunctionSyntaxKind::Getter:
      case FunctionSyntaxKind::GetterNoExpressionClosure:
        flags = JSFunction::INTERPRETED_GETTER;
        allocKind = gc::AllocKind::FUNCTION_EXTENDED;
        break;
      case FunctionSyntaxKind::Setter:
      case FunctionSyntaxKind::SetterNoExpressionClosure:
        flags = JSFunction::INTERPRETED_SETTER;
        allocKind = gc::AllocKind::FUNCTION_EXTENDED;
        break;
      default:
        MOZ_ASSERT(kind == FunctionSyntaxKind::Statement);
        if (options().selfHostingMode && !pc->isFunctionBox()) {
            allocKind = gc::AllocKind::FUNCTION_EXTENDED;
        }
        flags = (generatorKind == NotGenerator && asyncKind == SyncFunction
                 ? JSFunction::INTERPRETED_NORMAL
                 : JSFunction::INTERPRETED_GENERATOR_OR_ASYNC);
    }

    // We store the async wrapper in a slot for later access.
    if (asyncKind == AsyncFunction)
        allocKind = gc::AllocKind::FUNCTION_EXTENDED;

    fun = NewFunctionWithProto(context, nullptr, 0, flags, nullptr, atom, proto,
                               allocKind, TenuredObject);
    if (!fun)
        return nullptr;
    if (options().selfHostingMode) {
        fun->setIsSelfHostedBuiltin();
    }
    return fun;
}

/*
 * WARNING: Do not call this function directly.
 * Call either matchOrInsertSemicolonAfterExpression or
 * matchOrInsertSemicolonAfterNonExpression instead, depending on context.
 */
template <typename ParseHandler>
bool
Parser<ParseHandler>::matchOrInsertSemicolonHelper(TokenStream::Modifier modifier)
{
    TokenKind tt = TOK_EOF;
    if (!tokenStream.peekTokenSameLine(&tt, modifier))
        return false;
    if (tt != TOK_EOF && tt != TOK_EOL && tt != TOK_SEMI && tt != TOK_RC) {
        /*
         * When current token is `await` and it's outside of async function,
         * it's possibly intended to be an await expression.
         *
         *   await f();
         *        ^
         *        |
         *        tried to insert semicolon here
         *
         * Detect this situation and throw an understandable error.  Otherwise
         * we'd throw a confusing "missing ; before statement" error.
         */
        if (!pc->isAsync() && tokenStream.currentToken().type == TOK_AWAIT) {
            error(JSMSG_AWAIT_OUTSIDE_ASYNC);
            return false;
        }

        /* Advance the scanner for proper error location reporting. */
        tokenStream.consumeKnownToken(tt, modifier);
        error(JSMSG_SEMI_BEFORE_STMNT);
        return false;
    }
    bool matched;
    if (!tokenStream.matchToken(&matched, TOK_SEMI, modifier))
        return false;
    if (!matched && modifier == TokenStream::None)
        tokenStream.addModifierException(TokenStream::OperandIsNone);
    return true;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::matchOrInsertSemicolonAfterExpression()
{
    return matchOrInsertSemicolonHelper(TokenStream::None);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::matchOrInsertSemicolonAfterNonExpression()
{
    return matchOrInsertSemicolonHelper(TokenStream::Operand);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::leaveInnerFunction(ParseContext* outerpc)
{
    MOZ_ASSERT(pc != outerpc);

    // If the current function allows super.property but cannot have a home
    // object, i.e., it is an arrow function, we need to propagate the flag to
    // the outer ParseContext.
    if (pc->superScopeNeedsHomeObject()) {
        if (!pc->isArrowFunction())
            MOZ_ASSERT(pc->functionBox()->needsHomeObject());
        else
            outerpc->setSuperScopeNeedsHomeObject();
    }

    // Lazy functions inner to another lazy function need to be remembered by
    // the inner function so that if the outer function is eventually parsed
    // we do not need any further parsing or processing of the inner function.
    //
    // Append the inner function here unconditionally; the vector is only used
    // if the Parser using outerpc is a syntax parsing. See
    // Parser<SyntaxParseHandler>::finishFunction.
    if (!outerpc->innerFunctionsForLazy.append(pc->functionBox()->function()))
        return false;

    PropagateTransitiveParseFlags(pc->functionBox(), outerpc->sc());

    return true;
}

template <typename ParseHandler>
JSAtom*
Parser<ParseHandler>::prefixAccessorName(PropertyType propType, HandleAtom propAtom)
{
    RootedAtom prefix(context);
    if (propType == PropertyType::Setter || propType == PropertyType::SetterNoExpressionClosure) {
        prefix = context->names().setPrefix;
    } else {
        MOZ_ASSERT(propType == PropertyType::Getter || propType == PropertyType::GetterNoExpressionClosure);
        prefix = context->names().getPrefix;
    }

    RootedString str(context, ConcatStrings<CanGC>(context, prefix, propAtom));
    if (!str)
        return nullptr;

    return AtomizeString(context, str);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::functionArguments(YieldHandling yieldHandling, FunctionSyntaxKind kind,
                                        FunctionNodeType funNode)
{
    FunctionBox* funbox = pc->functionBox();

    bool parenFreeArrow = false;
    // Modifier for the following tokens.
    // TokenStream::None for the following cases:
    //   async a => 1
    //         ^
    //
    //   (a) => 1
    //   ^
    //
    //   async (a) => 1
    //         ^
    //
    //   function f(a) {}
    //             ^
    //
    // TokenStream::Operand for the following case:
    //   a => 1
    //   ^
    TokenStream::Modifier firstTokenModifier = TokenStream::None;

    // Modifier for the the first token in each argument.
    // can be changed to TokenStream::None for the following case:
    //   async a => 1
    //         ^
    TokenStream::Modifier argModifier = TokenStream::Operand;
    if (kind == FunctionSyntaxKind::Arrow) {
        TokenKind tt;
        // In async function, the first token after `async` is already gotten
        // with TokenStream::None.
        // In sync function, the first token is already gotten with
        // TokenStream::Operand.
        firstTokenModifier = funbox->isAsync() ? TokenStream::None : TokenStream::Operand;
        if (!tokenStream.peekToken(&tt, firstTokenModifier))
            return false;
        if (TokenKindIsPossibleIdentifier(tt)) {
            parenFreeArrow = true;
            argModifier = firstTokenModifier;
        }
    }
    if (!parenFreeArrow) {
        TokenKind tt;
        if (!tokenStream.getToken(&tt, firstTokenModifier))
            return false;
        if (tt != TOK_LP) {
            error(kind == FunctionSyntaxKind::Arrow ? JSMSG_BAD_ARROW_ARGS : JSMSG_PAREN_BEFORE_FORMAL);
            return false;
        }

        // Record the start of function source (for FunctionToString). If we
        // are parenFreeArrow, we will set this below, after consuming the NAME.
        funbox->setStart(tokenStream);
    }

    ListNodeType argsbody = handler.newList(PNK_PARAMSBODY);
    if (!argsbody)
        return false;
    handler.setFunctionFormalParametersAndBody(funNode, argsbody);

    bool hasArguments = false;
    if (parenFreeArrow) {
        hasArguments = true;
    } else {
        bool matched;
        if (!tokenStream.matchToken(&matched, TOK_RP, TokenStream::Operand))
            return false;
        if (!matched)
            hasArguments = true;
    }
    if (hasArguments) {
        bool hasRest = false;
        bool hasDefault = false;
        bool duplicatedParam = false;
        bool disallowDuplicateParams = kind == FunctionSyntaxKind::Arrow ||
                                       kind == FunctionSyntaxKind::Method ||
                                       kind == FunctionSyntaxKind::FieldInitializer ||
                                       kind == FunctionSyntaxKind::ClassConstructor;
        AtomVector& positionalFormals = pc->positionalFormalParameterNames();

        if (IsGetterKind(kind)) {
            error(JSMSG_ACCESSOR_WRONG_ARGS, "getter", "no", "s");
            return false;
        }

        while (true) {
            if (hasRest) {
                error(JSMSG_PARAMETER_AFTER_REST);
                return false;
            }

            TokenKind tt;
            if (!tokenStream.getToken(&tt, argModifier))
                return false;
            argModifier = TokenStream::Operand;
            MOZ_ASSERT_IF(parenFreeArrow, TokenKindIsPossibleIdentifier(tt));

            if (tt == TOK_TRIPLEDOT) {
                if (IsSetterKind(kind)) {
                    error(JSMSG_ACCESSOR_WRONG_ARGS, "setter", "one", "");
                    return false;
                }

                disallowDuplicateParams = true;
                if (duplicatedParam) {
                    // Has duplicated args before the rest parameter.
                    error(JSMSG_BAD_DUP_ARGS);
                    return false;
                }

                hasRest = true;
                funbox->setHasRest();

                if (!tokenStream.getToken(&tt))
                    return false;

                if (!TokenKindIsPossibleIdentifier(tt) && tt != TOK_LB && tt != TOK_LC) {
                    error(JSMSG_NO_REST_NAME);
                    return false;
                }
            }

            switch (tt) {
              case TOK_LB:
              case TOK_LC: {
                disallowDuplicateParams = true;
                if (duplicatedParam) {
                    // Has duplicated args before the destructuring parameter.
                    error(JSMSG_BAD_DUP_ARGS);
                    return false;
                }

                funbox->hasDestructuringArgs = true;

                Node destruct = destructuringDeclarationWithoutYieldOrAwait(
                    DeclarationKind::FormalParameter,
                    yieldHandling, tt);
                if (!destruct)
                    return false;

                if (!noteDestructuredPositionalFormalParameter(funNode, destruct))
                    return false;

                break;
              }

              default: {
                if (!TokenKindIsPossibleIdentifier(tt)) {
                    error(JSMSG_MISSING_FORMAL);
                    return false;
                }

                if (parenFreeArrow)
                    funbox->setStart(tokenStream);

                RootedPropertyName name(context, bindingIdentifier(yieldHandling));
                if (!name)
                    return false;

                if (!notePositionalFormalParameter(funNode, name, pos().begin,
                                                   disallowDuplicateParams, &duplicatedParam))
                {
                    return false;
                }
                if (duplicatedParam)
                    funbox->hasDuplicateParameters = true;

                break;
              }
            }

            if (positionalFormals.length() >= ARGNO_LIMIT) {
                error(JSMSG_TOO_MANY_FUN_ARGS);
                return false;
            }

            bool matched;
            if (!tokenStream.matchToken(&matched, TOK_ASSIGN))
                return false;
            if (matched) {
                // A default argument without parentheses would look like:
                // a = expr => body, but both operators are right-associative, so
                // that would have been parsed as a = (expr => body) instead.
                // Therefore it's impossible to get here with parenFreeArrow.
                MOZ_ASSERT(!parenFreeArrow);

                if (hasRest) {
                    error(JSMSG_REST_WITH_DEFAULT);
                    return false;
                }
                disallowDuplicateParams = true;
                if (duplicatedParam) {
                    error(JSMSG_BAD_DUP_ARGS);
                    return false;
                }

                if (!hasDefault) {
                    hasDefault = true;

                    // The Function.length property is the number of formals
                    // before the first default argument.
                    funbox->length = positionalFormals.length() - 1;
                }
                funbox->hasParameterExprs = true;

                Node def_expr = assignExprWithoutYieldOrAwait(yieldHandling);
                if (!def_expr)
                    return false;
                if (!handler.setLastFunctionFormalParameterDefault(funNode, def_expr))
                    return false;
            }

            if (parenFreeArrow || IsSetterKind(kind))
                break;

            if (!tokenStream.matchToken(&matched, TOK_COMMA))
                return false;
            if (!matched)
                break;

            if (!hasRest) {
                if (!tokenStream.peekToken(&tt, TokenStream::Operand))
                    return false;
                if (tt == TOK_RP) {
                    tokenStream.addModifierException(TokenStream::NoneIsOperand);
                    break;
                }
            }
        }

        if (!parenFreeArrow) {
            TokenKind tt;
            if (!tokenStream.getToken(&tt))
                return false;
            if (tt != TOK_RP) {
                if (IsSetterKind(kind)) {
                    error(JSMSG_ACCESSOR_WRONG_ARGS, "setter", "one", "");
                    return false;
                }

                error(JSMSG_PAREN_AFTER_FORMAL);
                return false;
            }
        }

        if (!hasDefault)
            funbox->length = positionalFormals.length() - hasRest;

        if (funbox->hasParameterExprs && funbox->hasDirectEval())
            funbox->hasDirectEvalInParameterExpr = true;

        funbox->function()->setArgCount(positionalFormals.length());
    } else if (IsSetterKind(kind)) {
        error(JSMSG_ACCESSOR_WRONG_ARGS, "setter", "one", "");
        return false;
    }

    return true;
}

template <>
bool
Parser<FullParseHandler>::skipLazyInnerFunction(FunctionNode* funNode, uint32_t toStringStart,
                                                FunctionSyntaxKind kind, bool tryAnnexB)
{
    // When a lazily-parsed function is called, we only fully parse (and emit)
    // that function, not any of its nested children. The initial syntax-only
    // parse recorded the free variables of nested functions and their extents,
    // so we can skip over them after accounting for their free variables.

    RootedFunction fun(context, handler.nextLazyInnerFunction());
    MOZ_ASSERT(!fun->isLegacyGenerator());
    FunctionBox* funbox = newFunctionBox(funNode, fun, toStringStart, Directives(/* strict = */ false),
                                         fun->generatorKind(), fun->asyncKind(), tryAnnexB);
    if (!funbox)
        return false;

    LazyScript* lazy = fun->lazyScript();
    if (lazy->needsHomeObject())
        funbox->setNeedsHomeObject();
    if (lazy->isExprBody())
        funbox->setIsExprBody();

    PropagateTransitiveParseFlags(lazy, pc->sc());

    // The position passed to tokenStream.advance() is an offset of the sort
    // returned by userbuf.offset() and expected by userbuf.rawCharPtrAt(),
    // while LazyScript::{begin,end} offsets are relative to the outermost
    // script source.
    Rooted<LazyScript*> lazyOuter(context, handler.lazyOuterFunction());
    uint32_t userbufBase = lazyOuter->begin() - lazyOuter->column();
    if (!tokenStream.advance(fun->lazyScript()->end() - userbufBase))
        return false;

#if JS_HAS_EXPR_CLOSURES
    // Only expression closure can be Statement kind.
    // If we remove expression closure, we can remove isExprBody flag from
    // LazyScript and JSScript.
    if (kind == FunctionSyntaxKind::Statement && funbox->isExprBody()) {
        if (!matchOrInsertSemicolonAfterExpression())
            return false;
    }
#endif

    return true;
}

template <>
bool
Parser<SyntaxParseHandler>::skipLazyInnerFunction(FunctionNodeType funNode, uint32_t toStringStart,
                                                  FunctionSyntaxKind kind, bool tryAnnexB)
{
    MOZ_CRASH("Cannot skip lazy inner functions when syntax parsing");
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::addExprAndGetNextTemplStrToken(YieldHandling yieldHandling, ListNodeType nodeList,
                                                     TokenKind* ttp)
{
    Node pn = expr(InAllowed, yieldHandling, TripledotProhibited);
    if (!pn)
        return false;
    handler.addList(nodeList, pn);

    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return false;
    if (tt != TOK_RC) {
        error(JSMSG_TEMPLSTR_UNTERM_EXPR);
        return false;
    }

    return tokenStream.getToken(ttp, TokenStream::TemplateTail);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::taggedTemplate(YieldHandling yieldHandling, ListNodeType tagArgsList, TokenKind tt)
{
    CallSiteNodeType callSiteObjNode = handler.newCallSiteObject(pos().begin);
    if (!callSiteObjNode)
        return false;
    handler.addList(tagArgsList, callSiteObjNode);

    while (true) {
        if (!appendToCallSiteObj(callSiteObjNode))
            return false;
        if (tt != TOK_TEMPLATE_HEAD)
            break;

        if (!addExprAndGetNextTemplStrToken(yieldHandling, tagArgsList, &tt))
            return false;
    }
    handler.setEndPosition(tagArgsList, callSiteObjNode);
    return true;
}

template <typename ParseHandler>
typename ParseHandler::ListNodeType
Parser<ParseHandler>::templateLiteral(YieldHandling yieldHandling)
{
    NameNodeType literal = noSubstitutionUntaggedTemplate();
    if (!literal)
        return null();

    ListNodeType nodeList = handler.newList(PNK_TEMPLATE_STRING_LIST, literal);
    if (!nodeList)
        return null();

    TokenKind tt;
    do {
        if (!addExprAndGetNextTemplStrToken(yieldHandling, nodeList, &tt))
            return null();

        literal = noSubstitutionUntaggedTemplate();
        if (!literal)
            return null();

        handler.addList(nodeList, literal);
    } while (tt == TOK_TEMPLATE_HEAD);
    return nodeList;
}

template <typename ParseHandler>
typename ParseHandler::FunctionNodeType
Parser<ParseHandler>::functionDefinition(uint32_t toStringStart, FunctionNodeType funNode, InHandling inHandling,
                                         YieldHandling yieldHandling,
                                         HandleAtom funName, FunctionSyntaxKind kind,
                                         GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
                                         bool tryAnnexB /* = false */)
{
    MOZ_ASSERT_IF(kind == FunctionSyntaxKind::Statement, funName);

    // When fully parsing a LazyScript, we do not fully reparse its inner
    // functions, which are also lazy. Instead, their free variables and
    // source extents are recorded and may be skipped.
    if (handler.canSkipLazyInnerFunctions()) {
        if (!skipLazyInnerFunction(funNode, toStringStart, kind, tryAnnexB))
            return null();

        return funNode;
    }

    RootedObject proto(context);
    if (generatorKind == StarGenerator || asyncKind == AsyncFunction) {
        // If we are off the main thread, the generator meta-objects have
        // already been created by js::StartOffThreadParseScript, so cx will not
        // be necessary.
        JSContext* cx = context->maybeJSContext();
        proto = GlobalObject::getOrCreateStarGeneratorFunctionPrototype(cx, context->global());
        if (!proto)
            return null();
    }
    RootedFunction fun(context, newFunction(funName, kind, generatorKind, asyncKind, proto));
    if (!fun)
        return null();

    // Speculatively parse using the directives of the parent parsing context.
    // If a directive is encountered (e.g., "use strict") that changes how the
    // function should have been parsed, we backup and reparse with the new set
    // of directives.
    Directives directives(pc);
    Directives newDirectives = directives;

    TokenStream::Position start(keepAtoms);
    tokenStream.tell(&start);

    // Parse the inner function. The following is a loop as we may attempt to
    // reparse a function due to failed syntax parsing and encountering new
    // "use foo" directives.
    while (true) {
        if (trySyntaxParseInnerFunction(funNode, fun, toStringStart, inHandling, yieldHandling,
                                        kind, generatorKind, asyncKind, tryAnnexB, directives,
                                        &newDirectives))
        {
            break;
        }

        // Return on error.
        if (tokenStream.hadError() || directives == newDirectives)
            return null();

        // Assignment must be monotonic to prevent infinitely attempting to
        // reparse.
        MOZ_ASSERT_IF(directives.strict(), newDirectives.strict());
        MOZ_ASSERT_IF(directives.asmJS(), newDirectives.asmJS());
        directives = newDirectives;

        tokenStream.seek(start);

        // functionFormalParametersAndBody may have already set body before failing.
        handler.setFunctionFormalParametersAndBody(funNode, null());
    }

    return funNode;
}

template <>
bool
Parser<FullParseHandler>::trySyntaxParseInnerFunction(FunctionNode* funNode, HandleFunction fun,
                                                      uint32_t toStringStart,
                                                      InHandling inHandling,
                                                      YieldHandling yieldHandling,
                                                      FunctionSyntaxKind kind,
                                                      GeneratorKind generatorKind,
                                                      FunctionAsyncKind asyncKind,
                                                      bool tryAnnexB,
                                                      Directives inheritedDirectives,
                                                      Directives* newDirectives)
{
    // Try a syntax parse for this inner function.
    do {
        // If we're assuming this function is an IIFE, always perform a full
        // parse to avoid the overhead of a lazy syntax-only parse. Although
        // the prediction may be incorrect, IIFEs are common enough that it
        // pays off for lots of code.
        if (funNode->isLikelyIIFE() && generatorKind == NotGenerator && asyncKind == SyncFunction)
            break;

        Parser<SyntaxParseHandler>* parser = handler.syntaxParser;
        if (!parser)
            break;

        UsedNameTracker::RewindToken token = usedNames.getRewindToken();

        // Move the syntax parser to the current position in the stream.
        TokenStream::Position position(keepAtoms);
        tokenStream.tell(&position);
        if (!parser->tokenStream.seek(position, tokenStream))
            return false;

        // Make a FunctionBox before we enter the syntax parser, because |funNode|
        // still expects a FunctionBox to be attached to it during BCE, and
        // the syntax parser cannot attach one to it.
        FunctionBox* funbox = newFunctionBox(funNode, fun, toStringStart, inheritedDirectives,
                                             generatorKind, asyncKind, tryAnnexB);
        if (!funbox)
            return false;
        funbox->initWithEnclosingParseContext(pc, kind);

        if (!parser->innerFunction(SyntaxParseHandler::NodeGeneric, pc, funbox, toStringStart,
                                   inHandling, yieldHandling, kind,
                                   inheritedDirectives, newDirectives))
        {
            if (parser->hadAbortedSyntaxParse()) {
                // Try again with a full parse. UsedNameTracker needs to be
                // rewound to just before we tried the syntax parse for
                // correctness.
                parser->clearAbortedSyntaxParse();
                usedNames.rewind(token);
                MOZ_ASSERT_IF(parser->context->isJSContext(),
                              !parser->context->asJSContext()->isExceptionPending());
                break;
            }
            return false;
        }

        // Advance this parser over tokens processed by the syntax parser.
        parser->tokenStream.tell(&position);
        if (!tokenStream.seek(position, parser->tokenStream))
            return false;

        // Update the end position of the parse node.
        funNode->pn_pos.end = tokenStream.currentToken().pos.end;
        return true;
    } while (false);

    // We failed to do a syntax parse above, so do the full parse.
    return innerFunction(funNode, pc, fun, toStringStart, inHandling, yieldHandling, kind,
                         generatorKind, asyncKind, tryAnnexB, inheritedDirectives, newDirectives);
}

template <>
bool
Parser<SyntaxParseHandler>::trySyntaxParseInnerFunction(FunctionNodeType funNode, HandleFunction fun,
                                                        uint32_t toStringStart,
                                                        InHandling inHandling,
                                                        YieldHandling yieldHandling,
                                                        FunctionSyntaxKind kind,
                                                        GeneratorKind generatorKind,
                                                        FunctionAsyncKind asyncKind,
                                                        bool tryAnnexB,
                                                        Directives inheritedDirectives,
                                                        Directives* newDirectives)
{
    // This is already a syntax parser, so just parse the inner function.
    return innerFunction(funNode, pc, fun, toStringStart, inHandling, yieldHandling, kind,
                          generatorKind, asyncKind, tryAnnexB, inheritedDirectives, newDirectives);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::innerFunction(FunctionNodeType funNode, ParseContext* outerpc, FunctionBox* funbox,
                                    uint32_t toStringStart,
                                    InHandling inHandling, YieldHandling yieldHandling,
                                    FunctionSyntaxKind kind, Directives inheritedDirectives,
                                    Directives* newDirectives)
{
    // Note that it is possible for outerpc != this->pc, as we may be
    // attempting to syntax parse an inner function from an outer full
    // parser. In that case, outerpc is a ParseContext from the full parser
    // instead of the current top of the stack of the syntax parser.

    // Push a new ParseContext.
    ParseContext funpc(this, funbox, newDirectives);
    if (!funpc.init())
        return false;

    if (!functionFormalParametersAndBody(inHandling, yieldHandling, funNode, kind))
        return false;

    return leaveInnerFunction(outerpc);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::innerFunction(FunctionNodeType funNode, ParseContext* outerpc, HandleFunction fun,
                                    uint32_t toStringStart,
                                    InHandling inHandling, YieldHandling yieldHandling,
                                    FunctionSyntaxKind kind,
                                    GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
                                    bool tryAnnexB,
                                    Directives inheritedDirectives, Directives* newDirectives)
{
    // Note that it is possible for outerpc != this->pc, as we may be
    // attempting to syntax parse an inner function from an outer full
    // parser. In that case, outerpc is a ParseContext from the full parser
    // instead of the current top of the stack of the syntax parser.

    FunctionBox* funbox = newFunctionBox(funNode, fun, toStringStart, inheritedDirectives,
                                         generatorKind, asyncKind, tryAnnexB);
    if (!funbox)
        return false;
    funbox->initWithEnclosingParseContext(outerpc, kind);

    return innerFunction(funNode, outerpc, funbox, toStringStart, inHandling, yieldHandling, kind,
                         inheritedDirectives, newDirectives);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::appendToCallSiteObj(CallSiteNodeType callSiteObj)
{
    Node cookedNode = noSubstitutionTaggedTemplate();
    if (!cookedNode)
        return false;

    JSAtom* atom = tokenStream.getRawTemplateStringAtom();
    if (!atom)
        return false;
    NameNodeType rawNode = handler.newTemplateStringLiteral(atom, pos());
    if (!rawNode)
        return false;

    handler.addToCallSiteObject(callSiteObj, rawNode, cookedNode);
    return true;
}

template <>
FunctionNode*
Parser<FullParseHandler>::standaloneLazyFunction(HandleFunction fun, bool strict,
                                                 GeneratorKind generatorKind,
                                                 FunctionAsyncKind asyncKind)
{
    MOZ_ASSERT(checkOptionsCalled);

    FunctionSyntaxKind syntaxKind = FunctionSyntaxKind::Statement;
    if (fun->isClassConstructor()) {
        if (fun->isDerivedClassConstructor()) {
            syntaxKind = FunctionSyntaxKind::DerivedClassConstructor;
        } else {
            syntaxKind = FunctionSyntaxKind::ClassConstructor;
        }
    } else if (fun->isMethod()) {
        if (fun->isFieldInitializer()) {
            syntaxKind = FunctionSyntaxKind::FieldInitializer;
        } else {
            syntaxKind = FunctionSyntaxKind::Method;
        }
    } else if (fun->isGetter()) {
        syntaxKind = FunctionSyntaxKind::Getter;
    } else if (fun->isSetter()) {
        syntaxKind = FunctionSyntaxKind::Setter;
    } else if (fun->isArrow()) {
        syntaxKind = FunctionSyntaxKind::Arrow;
    }

    FunctionNodeType funNode = handler.newFunction(syntaxKind, pos());
    if (!funNode)
        return null();

    Directives directives(strict);
    FunctionBox* funbox = newFunctionBox(funNode, fun, /* toStringStart = */ 0, directives,
                                         generatorKind, asyncKind, /* tryAnnexB = */ false);
    if (!funbox)
        return null();
    funbox->initFromLazyFunction();

    Directives newDirectives = directives;
    ParseContext funpc(this, funbox, &newDirectives);
    if (!funpc.init())
        return null();

    // Our tokenStream has no current token, so funNode's position is garbage.
    // Substitute the position of the first token in our source. If the function
    // is a not-async arrow, use TokenStream::Operand to keep
    // verifyConsistentModifier from complaining (we will use
    // TokenStream::Operand in functionArguments).
    TokenStream::Modifier modifier = (fun->isArrow() && asyncKind == SyncFunction)
                                     ? TokenStream::Operand : TokenStream::None;
    if (!tokenStream.peekTokenPos(&funNode->pn_pos, modifier))
        return null();

    YieldHandling yieldHandling = GetYieldHandling(generatorKind);

    if (!functionFormalParametersAndBody(InAllowed, yieldHandling, funNode, syntaxKind)) {
        MOZ_ASSERT(directives == newDirectives);
        return null();
    }

    ParseNode* node = funNode;
    if (!FoldConstants(context, &node, this))
        return null();
    funNode = &node->as<FunctionNode>();

    return funNode;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::functionFormalParametersAndBody(InHandling inHandling,
                                                      YieldHandling yieldHandling,
                                                      FunctionNodeType funNode, FunctionSyntaxKind kind,
                                                      Maybe<uint32_t> parameterListEnd /* = Nothing() */,
                                                      bool isStandaloneFunction /* = false */)
{
    // Given a properly initialized parse context, try to parse an actual
    // function without concern for conversion to strict mode, use of lazy
    // parsing and such.

    FunctionBox* funbox = pc->functionBox();
    RootedFunction fun(context, funbox->function());

    if (kind == FunctionSyntaxKind::ClassConstructor ||
        kind == FunctionSyntaxKind::DerivedClassConstructor) {
        if (!noteUsedName(context->names().dotInitializers))
            return false;
    }

    // See below for an explanation why arrow function parameters and arrow
    // function bodies are parsed with different yield/await settings.
    {
        AwaitHandling awaitHandling = kind == FunctionSyntaxKind::StaticClassBlock
                                         ? AwaitIsDisallowed
                                         : (funbox->isAsync() ||
                                            (kind == FunctionSyntaxKind::Arrow && awaitIsKeyword()))
                                            ? AwaitIsKeyword
                                            : AwaitIsName;
        AutoAwaitIsKeyword<ParseHandler> awaitIsKeyword(this, awaitHandling);
        if (!functionArguments(yieldHandling, kind, funNode))
            return false;
    }

    Maybe<ParseContext::VarScope> varScope;
    if (funbox->hasParameterExprs) {
        varScope.emplace(this);
        if (!varScope->init(pc))
            return false;
    } else {
        pc->functionScope().useAsVarScope(pc);
    }

    if (kind == FunctionSyntaxKind::Arrow) {
        bool matched;
        if (!tokenStream.matchToken(&matched, TOK_ARROW))
            return false;
        if (!matched) {
            error(JSMSG_BAD_ARROW_ARGS);
            return false;
        }
    }

    // When parsing something for new Function() we have to make sure to
    // only treat a certain part of the source as a parameter list.
    if (parameterListEnd.isSome() && parameterListEnd.value() != pos().begin) {
        error(JSMSG_UNEXPECTED_PARAMLIST_END);
        return false;
    }

    // Parse the function body.
    FunctionBodyType bodyType = StatementListBody;
    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return false;
    uint32_t openedPos = 0;
    if (tt != TOK_LC) {
        if (funbox->isStarGenerator() || kind == FunctionSyntaxKind::Method ||
            kind == FunctionSyntaxKind::GetterNoExpressionClosure ||
            kind == FunctionSyntaxKind::SetterNoExpressionClosure ||
            IsConstructorKind(kind)) {
            error(JSMSG_CURLY_BEFORE_BODY);
            return false;
        }

        if (kind != FunctionSyntaxKind::Arrow) {
#if JS_HAS_EXPR_CLOSURES
            if (!warnOnceAboutExprClosure())
                return false;
#else
            error(JSMSG_CURLY_BEFORE_BODY);
            return false;
#endif
        }

        tokenStream.ungetToken();
        bodyType = ExpressionBody;
        funbox->setIsExprBody();
    } else {
        openedPos = pos().begin;
    }

    // Arrow function parameters inherit yieldHandling from the enclosing
    // context, but the arrow body doesn't. E.g. in |(a = yield) => yield|,
    // |yield| in the parameters is either a name or keyword, depending on
    // whether the arrow function is enclosed in a generator function or not.
    // Whereas the |yield| in the function body is always parsed as a name.
    // The same goes when parsing |await| in arrow functions.
    YieldHandling bodyYieldHandling = GetYieldHandling(pc->generatorKind());
    AwaitHandling bodyAwaitHandling = GetAwaitHandling(pc->asyncKind());
    bool inheritedStrict = pc->sc()->strict();
    LexicalScopeNodeType body;
    {
        AutoAwaitIsKeyword<ParseHandler> awaitIsKeyword(this, bodyAwaitHandling);
        body = functionBody(inHandling, bodyYieldHandling, kind, bodyType);
        if (!body)
            return false;
    }

    // Revalidate the function name when we transitioned to strict mode.
    if ((kind == FunctionSyntaxKind::Statement ||
         kind == FunctionSyntaxKind::Expression) && fun->explicitName()
        && !inheritedStrict && pc->sc()->strict())
    {
        MOZ_ASSERT(pc->sc()->hasExplicitUseStrict(),
                   "strict mode should only change when a 'use strict' directive is present");

        PropertyName* propertyName = fun->explicitName()->asPropertyName();
        YieldHandling nameYieldHandling;
        if (kind == FunctionSyntaxKind::Expression) {
            // Named lambda has binding inside it.
            nameYieldHandling = bodyYieldHandling;
        } else {
            // Otherwise YieldHandling cannot be checked at this point
            // because of different context.
            // It should already be checked before this point.
            nameYieldHandling = YieldIsName;
        }

        // We already use the correct await-handling at this point, therefore
        // we don't need call AutoAwaitIsKeyword here.

        if (!checkBindingIdentifier(propertyName, handler.getPosition(funNode).begin,
                                    nameYieldHandling))
        {
            return false;
        }
    }

    if (bodyType == StatementListBody) {
        if (!mustMatchToken(TOK_RC, TokenStream::Operand,
                            [this, openedPos](TokenKind actual) {
                                this->reportMissingClosing(JSMSG_CURLY_AFTER_BODY,
                                                           JSMSG_CURLY_OPENED, openedPos);
                            }))
        {
            return false;
        }
        funbox->setEnd(pos().end);
    } else {
#if !JS_HAS_EXPR_CLOSURES
        MOZ_ASSERT(kind == FunctionSyntaxKind::Arrow);
#endif
        if (tokenStream.hadError())
            return false;
        funbox->setEnd(pos().end);
        if (kind == FunctionSyntaxKind::Statement && !matchOrInsertSemicolonAfterExpression())
            return false;
    }

    if (IsMethodDefinitionKind(kind) && pc->superScopeNeedsHomeObject())
        funbox->setNeedsHomeObject();

    if (!finishFunction(isStandaloneFunction))
        return false;

    handler.setEndPosition(body, pos().begin);
    handler.setEndPosition(funNode, pos().end);
    handler.setFunctionBody(funNode, body);

    return true;
}

template <typename ParseHandler>
typename ParseHandler::FunctionNodeType
Parser<ParseHandler>::functionStmt(uint32_t toStringStart, YieldHandling yieldHandling,
                                   DefaultHandling defaultHandling, FunctionAsyncKind asyncKind)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FUNCTION));

    // In sloppy mode, Annex B.3.2 allows labelled function declarations.
    // Otherwise it's a parse error.
    ParseContext::Statement* declaredInStmt = pc->innermostStatement();
    if (declaredInStmt && declaredInStmt->kind() == StatementKind::Label) {
        MOZ_ASSERT(!pc->sc()->strict(),
                   "labeled functions shouldn't be parsed in strict mode");

        // Find the innermost non-label statement. Report an error if it's
        // unbraced: functions can't appear in it. Otherwise the statement
        // (or its absence) determines the scope the function's bound in.
        while (declaredInStmt && declaredInStmt->kind() == StatementKind::Label)
            declaredInStmt = declaredInStmt->enclosing();

        if (declaredInStmt && !StatementKindIsBraced(declaredInStmt->kind())) {
            error(JSMSG_SLOPPY_FUNCTION_LABEL);
            return null();
        }
    }

    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return null();

    GeneratorKind generatorKind = NotGenerator;
    if (tt == TOK_MUL) {
        generatorKind = StarGenerator;
        if (!tokenStream.getToken(&tt))
            return null();
    }

    RootedPropertyName name(context);
    if (TokenKindIsPossibleIdentifier(tt)) {
        name = bindingIdentifier(yieldHandling);
        if (!name)
            return null();
    } else if (defaultHandling == AllowDefaultName) {
        name = context->names().starDefaultStar;
        tokenStream.ungetToken();
    } else {
        /* Unnamed function expressions are forbidden in statement context. */
        error(JSMSG_UNNAMED_FUNCTION_STMT);
        return null();
    }

    // Note the declared name and check for early errors.
    bool tryAnnexB = false;
    if (declaredInStmt) {
        MOZ_ASSERT(declaredInStmt->kind() != StatementKind::Label);
        MOZ_ASSERT(StatementKindIsBraced(declaredInStmt->kind()));

        if (!pc->sc()->strict() && generatorKind == NotGenerator && asyncKind == SyncFunction) {
            // In sloppy mode, try Annex B.3.3 semantics. If making an
            // additional 'var' binding of the same name does not throw an
            // early error, do so. This 'var' binding would be assigned
            // the function object when its declaration is reached, not at
            // the start of the block.
            if (!tryDeclareVarForAnnexBLexicalFunction(name, pos().begin, &tryAnnexB))
                return null();
        }

        if (!noteDeclaredName(name, DeclarationKind::LexicalFunction, pos()))
            return null();
    } else {
        DeclarationKind kind = pc->atModuleLevel()
                               ? DeclarationKind::ModuleBodyLevelFunction
                               : DeclarationKind::BodyLevelFunction;
        if (!noteDeclaredName(name, kind, pos()))
            return null();
    }

    FunctionNodeType funNode = handler.newFunction(FunctionSyntaxKind::Statement, pos());
    if (!funNode)
        return null();

    YieldHandling newYieldHandling = GetYieldHandling(generatorKind);
    return functionDefinition(toStringStart, funNode, InAllowed, newYieldHandling,
                              name, FunctionSyntaxKind::Statement, generatorKind,
                              asyncKind, tryAnnexB);
}

template <typename ParseHandler>
typename ParseHandler::FunctionNodeType
Parser<ParseHandler>::functionExpr(uint32_t toStringStart, InvokedPrediction invoked,
                                   FunctionAsyncKind asyncKind)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FUNCTION));

    AutoAwaitIsKeyword<ParseHandler> awaitIsKeyword(this, GetAwaitHandling(asyncKind));
    GeneratorKind generatorKind = NotGenerator;
    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return null();

    if (tt == TOK_MUL) {
        generatorKind = StarGenerator;
        if (!tokenStream.getToken(&tt))
            return null();
    }

    YieldHandling yieldHandling = GetYieldHandling(generatorKind);

    RootedPropertyName name(context);
    if (TokenKindIsPossibleIdentifier(tt)) {
        name = bindingIdentifier(yieldHandling);
        if (!name)
            return null();
    } else {
        tokenStream.ungetToken();
    }

    FunctionNodeType funNode = handler.newFunction(FunctionSyntaxKind::Expression, pos());
    if (!funNode)
        return null();

    if (invoked)
        funNode = handler.setLikelyIIFE(funNode);

    return functionDefinition(toStringStart, funNode, InAllowed, yieldHandling, name,
                              FunctionSyntaxKind::Expression, generatorKind, asyncKind);
}

/*
 * Return true if this node, known to be an unparenthesized string literal,
 * could be the string of a directive in a Directive Prologue. Directive
 * strings never contain escape sequences or line continuations.
 * isEscapeFreeStringLiteral, below, checks whether the node itself could be
 * a directive.
 */
static inline bool
IsEscapeFreeStringLiteral(const TokenPos& pos, JSAtom* str)
{
    /*
     * If the string's length in the source code is its length as a value,
     * accounting for the quotes, then it must not contain any escape
     * sequences or line continuations.
     */
    return pos.begin + str->length() + 2 == pos.end;
}

template <>
bool
Parser<SyntaxParseHandler>::asmJS(ListNodeType list)
{
    // While asm.js could technically be validated and compiled during syntax
    // parsing, we have no guarantee that some later JS wouldn't abort the
    // syntax parse and cause us to re-parse (and re-compile) the asm.js module.
    // For simplicity, unconditionally abort the syntax parse when "use asm" is
    // encountered so that asm.js is always validated/compiled exactly once
    // during a full parse.
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return false;
}

template <>
bool
Parser<FullParseHandler>::asmJS(ListNodeType list)
{
    // Disable syntax parsing in anything nested inside the asm.js module.
    handler.disableSyntaxParser();

    // We should be encountering the "use asm" directive for the first time; if
    // the directive is already, we must have failed asm.js validation and we're
    // reparsing. In that case, don't try to validate again. A non-null
    // newDirectives means we're not in a normal function.
    if (!pc->newDirectives || pc->newDirectives->asmJS())
        return true;

    // If there is no ScriptSource, then we are doing a non-compiling parse and
    // so we shouldn't (and can't, without a ScriptSource) compile.
    if (ss == nullptr)
        return true;

    pc->functionBox()->useAsm = true;

    // Attempt to validate and compile this asm.js module. On success, the
    // tokenStream has been advanced to the closing }. On failure, the
    // tokenStream is in an indeterminate state and we must reparse the
    // function from the beginning. Reparsing is triggered by marking that a
    // new directive has been encountered and returning 'false'.
    bool validated;
    if (!CompileAsmJS(context, *this, list, &validated))
        return false;
    if (!validated) {
        pc->newDirectives->setAsmJS();
        return false;
    }

    return true;
}

/*
 * Recognize Directive Prologue members and directives. Assuming |pn| is a
 * candidate for membership in a directive prologue, recognize directives and
 * set |pc|'s flags accordingly. If |pn| is indeed part of a prologue, set its
 * |prologue| flag.
 *
 * Note that the following is a strict mode function:
 *
 * function foo() {
 *   "blah" // inserted semi colon
 *        "blurgh"
 *   "use\x20loose"
 *   "use strict"
 * }
 *
 * That is, even though "use\x20loose" can never be a directive, now or in the
 * future (because of the hex escape), the Directive Prologue extends through it
 * to the "use strict" statement, which is indeed a directive.
 */
template <typename ParseHandler>
bool
Parser<ParseHandler>::maybeParseDirective(ListNodeType list, Node possibleDirective, bool* cont)
{
    TokenPos directivePos;
    JSAtom* directive = handler.isStringExprStatement(possibleDirective, &directivePos);

    *cont = !!directive;
    if (!*cont)
        return true;

    if (IsEscapeFreeStringLiteral(directivePos, directive)) {
        // Mark this statement as being a possibly legitimate part of a
        // directive prologue, so the bytecode emitter won't warn about it being
        // useless code. (We mustn't just omit the statement entirely yet, as it
        // could be producing the value of an eval or JSScript execution.)
        //
        // Note that even if the string isn't one we recognize as a directive,
        // the emitter still shouldn't flag it as useless, as it could become a
        // directive in the future. We don't want to interfere with people
        // taking advantage of directive-prologue-enabled features that appear
        // in other browsers first.
        handler.setInDirectivePrologue(handler.asUnary(possibleDirective));

        if (directive == context->names().useStrict) {
            // Functions with non-simple parameter lists (destructuring,
            // default or rest parameters) must not contain a "use strict"
            // directive.
            if (pc->isFunctionBox()) {
                FunctionBox* funbox = pc->functionBox();
                if (!funbox->hasSimpleParameterList()) {
                    const char* parameterKind = funbox->hasDestructuringArgs
                                                ? "destructuring"
                                                : funbox->hasParameterExprs
                                                ? "default"
                                                : "rest";
                    errorAt(directivePos.begin, JSMSG_STRICT_NON_SIMPLE_PARAMS, parameterKind);
                    return false;
                }
            }

            // We're going to be in strict mode. Note that this scope explicitly
            // had "use strict";
            pc->sc()->setExplicitUseStrict();
            if (!pc->sc()->strict()) {
                // We keep track of the one possible strict violation that could
                // occur in the directive prologue -- octal escapes -- and
                // complain now.
                if (tokenStream.sawOctalEscape()) {
                    error(JSMSG_DEPRECATED_OCTAL);
                    return false;
                }
                pc->sc()->strictScript = true;
            }
        } else if (directive == context->names().useAsm) {
            if (pc->isFunctionBox())
                return asmJS(list);
            return warningAt(directivePos.begin, JSMSG_USE_ASM_DIRECTIVE_FAIL);
        }
    }
    return true;
}

template <typename ParseHandler>
typename ParseHandler::ListNodeType
Parser<ParseHandler>::statementList(YieldHandling yieldHandling)
{
    JS_CHECK_RECURSION(context, return null());

    ListNodeType stmtList = handler.newStatementList(pos());
    if (!stmtList)
        return null();

    bool canHaveDirectives = pc->atBodyLevel();
    if (canHaveDirectives)
        tokenStream.clearSawOctalEscape();
    bool afterReturn = false;
    bool warnedAboutStatementsAfterReturn = false;
    uint32_t statementBegin = 0;
    for (;;) {
        TokenKind tt = TOK_EOF;
        if (!tokenStream.peekToken(&tt, TokenStream::Operand)) {
            if (tokenStream.isEOF())
                isUnexpectedEOF_ = true;
            return null();
        }
        if (tt == TOK_EOF || tt == TOK_RC)
            break;
        if (afterReturn) {
            if (!tokenStream.peekOffset(&statementBegin, TokenStream::Operand))
                return null();
        }
        Node next = statementListItem(yieldHandling, canHaveDirectives);
        if (!next) {
            if (tokenStream.isEOF())
                isUnexpectedEOF_ = true;
            return null();
        }
        if (!warnedAboutStatementsAfterReturn) {
            if (afterReturn) {
                if (!handler.isStatementPermittedAfterReturnStatement(next)) {
                    if (!warningAt(statementBegin, JSMSG_STMT_AFTER_RETURN))
                        return null();

                    warnedAboutStatementsAfterReturn = true;
                }
            } else if (handler.isReturnStatement(next)) {
                afterReturn = true;
            }
        }

        if (canHaveDirectives) {
            if (!maybeParseDirective(stmtList, next, &canHaveDirectives))
                return null();
        }

        handler.addStatementToList(stmtList, next);
    }

    return stmtList;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::condition(InHandling inHandling, YieldHandling yieldHandling)
{
    if (!mustMatchToken(TOK_LP, JSMSG_PAREN_BEFORE_COND)) {
        return null();
    }

    Node pn = exprInParens(inHandling, yieldHandling, TripledotProhibited);
    if (!pn)
        return null();
    if (!mustMatchToken(TOK_RP, JSMSG_PAREN_AFTER_COND)) {
        return null();
    }

    /* Check for (a = b) and warn about possible (a == b) mistype. */
    if (handler.isUnparenthesizedAssignment(pn)) {
        if (!extraWarning(JSMSG_EQUAL_AS_ASSIGN))
            return null();
    }
    return pn;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::matchLabel(YieldHandling yieldHandling, MutableHandle<PropertyName*> label)
{
    TokenKind tt = TOK_EOF;
    if (!tokenStream.peekTokenSameLine(&tt, TokenStream::Operand))
        return false;

    if (TokenKindIsPossibleIdentifier(tt)) {
        tokenStream.consumeKnownToken(tt, TokenStream::Operand);

        label.set(labelIdentifier(yieldHandling));
        if (!label)
            return false;
    } else {
        label.set(nullptr);
    }
    return true;
}

template <typename ParseHandler>
Parser<ParseHandler>::PossibleError::PossibleError(Parser<ParseHandler>& parser)
  : parser_(parser)
{}

template <typename ParseHandler>
typename Parser<ParseHandler>::PossibleError::Error&
Parser<ParseHandler>::PossibleError::error(ErrorKind kind)
{
    if (kind == ErrorKind::Expression)
        return exprError_;
    if (kind == ErrorKind::Destructuring)
        return destructuringError_;
    MOZ_ASSERT(kind == ErrorKind::DestructuringWarning);
    return destructuringWarning_;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::PossibleError::hasPendingDestructuringError()
{
    return hasError(ErrorKind::Destructuring);
}

template <typename ParseHandler>
void
Parser<ParseHandler>::PossibleError::setResolved(ErrorKind kind)
{
    error(kind).state_ = ErrorState::None;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::PossibleError::hasError(ErrorKind kind)
{
    return error(kind).state_ == ErrorState::Pending;
}

template <typename ParseHandler>
void
Parser<ParseHandler>::PossibleError::setPending(ErrorKind kind, const TokenPos& pos,
                                                unsigned errorNumber)
{
    // Don't overwrite a previously recorded error.
    if (hasError(kind))
        return;

    // If we report an error later, we'll do it from the position where we set
    // the state to pending.
    Error& err = error(kind);
    err.offset_ = pos.begin;
    err.errorNumber_ = errorNumber;
    err.state_ = ErrorState::Pending;
}

template <typename ParseHandler>
void
Parser<ParseHandler>::PossibleError::setPendingDestructuringErrorAt(const TokenPos& pos,
                                                                    unsigned errorNumber)
{
    setPending(ErrorKind::Destructuring, pos, errorNumber);
}

template <typename ParseHandler>
void
Parser<ParseHandler>::PossibleError::setPendingDestructuringWarningAt(const TokenPos& pos,
                                                                      unsigned errorNumber)
{
    setPending(ErrorKind::DestructuringWarning, pos, errorNumber);
}

template <typename ParseHandler>
void
Parser<ParseHandler>::PossibleError::setPendingExpressionErrorAt(const TokenPos& pos,
                                                                 unsigned errorNumber)
{
    setPending(ErrorKind::Expression, pos, errorNumber);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::PossibleError::checkForError(ErrorKind kind)
{
    if (!hasError(kind))
        return true;

    Error& err = error(kind);
    parser_.errorAt(err.offset_, err.errorNumber_);
    return false;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::PossibleError::checkForWarning(ErrorKind kind)
{
    if (!hasError(kind))
        return true;

    Error& err = error(kind);
    return parser_.extraWarningAt(err.offset_, err.errorNumber_);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::PossibleError::checkForDestructuringErrorOrWarning()
{
    // Clear pending expression error, because we're definitely not in an
    // expression context.
    setResolved(ErrorKind::Expression);

    // Report any pending destructuring error or warning.
    return checkForError(ErrorKind::Destructuring) &&
           checkForWarning(ErrorKind::DestructuringWarning);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::PossibleError::checkForExpressionError()
{
    // Clear pending destructuring error or warning, because we're definitely
    // not in a destructuring context.
    setResolved(ErrorKind::Destructuring);
    setResolved(ErrorKind::DestructuringWarning);

    // Report any pending expression error.
    return checkForError(ErrorKind::Expression);
}

template <typename ParseHandler>
void
Parser<ParseHandler>::PossibleError::transferErrorTo(ErrorKind kind, PossibleError* other)
{
    if (hasError(kind) && !other->hasError(kind)) {
        Error& err = error(kind);
        Error& otherErr = other->error(kind);
        otherErr.offset_ = err.offset_;
        otherErr.errorNumber_ = err.errorNumber_;
        otherErr.state_ = err.state_;
    }
}

template <typename ParseHandler>
void
Parser<ParseHandler>::PossibleError::transferErrorsTo(PossibleError* other)
{
    MOZ_ASSERT(other);
    MOZ_ASSERT(this != other);
    MOZ_ASSERT(&parser_ == &other->parser_,
               "Can't transfer fields to an instance which belongs to a different parser");

    transferErrorTo(ErrorKind::Destructuring, other);
    transferErrorTo(ErrorKind::Expression, other);
}

template <typename ParseHandler>
typename ParseHandler::BinaryNodeType
Parser<ParseHandler>::bindingInitializer(Node lhs, DeclarationKind kind,
                                         YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_ASSIGN));

    if (kind == DeclarationKind::FormalParameter)
        pc->functionBox()->hasParameterExprs = true;

    Node rhs = assignExpr(InAllowed, yieldHandling, TripledotProhibited);
    if (!rhs)
        return null();

    handler.checkAndSetIsDirectRHSAnonFunction(rhs);

    BinaryNodeType assign = handler.newAssignment(PNK_ASSIGN, lhs, rhs, JSOP_NOP);
    if (!assign)
        return null();

    if (foldConstants) {
        Node node = assign;
        if (!FoldConstants(context, &node, this)) {
            return null();
        }
        assign = handler.asBinary(node);
    }

    return assign;
}

template <typename ParseHandler>
typename ParseHandler::NameNodeType
Parser<ParseHandler>::bindingIdentifier(DeclarationKind kind, YieldHandling yieldHandling)
{
    RootedPropertyName name(context, bindingIdentifier(yieldHandling));
    if (!name)
        return null();

    NameNodeType binding = newName(name);
    if (!binding || !noteDeclaredName(name, kind, pos()))
        return null();

    return binding;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::bindingIdentifierOrPattern(DeclarationKind kind, YieldHandling yieldHandling,
                                                 TokenKind tt)
{
    if (tt == TOK_LB)
        return arrayBindingPattern(kind, yieldHandling);

    if (tt == TOK_LC)
        return objectBindingPattern(kind, yieldHandling);

    if (!TokenKindIsPossibleIdentifierName(tt)) {
        error(JSMSG_NO_VARIABLE_NAME);
        return null();
    }

    return bindingIdentifier(kind, yieldHandling);
}

template <typename ParseHandler>
typename ParseHandler::ListNodeType
Parser<ParseHandler>::objectBindingPattern(DeclarationKind kind, YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LC));

    JS_CHECK_RECURSION(context, return null());

    uint32_t begin = pos().begin;
    ListNodeType literal = handler.newObjectLiteral(begin);
    if (!literal)
        return null();

    Maybe<DeclarationKind> declKind = Some(kind);
    RootedAtom propAtom(context);
    for (;;) {
        TokenKind tt;
        if (!tokenStream.peekToken(&tt))
            return null();
        if (tt == TOK_RC)
            break;

        if (tt == TOK_TRIPLEDOT) {
            // rest-binding property
            tokenStream.consumeKnownToken(TOK_TRIPLEDOT);
            uint32_t begin = pos().begin;

            TokenKind tt;
            if (!tokenStream.getToken(&tt))
                return null();

            Node inner = bindingIdentifierOrPattern(kind, yieldHandling, tt);
            if (!inner)
                return null();

            if (!handler.addSpreadProperty(literal, begin, inner))
                return null();
        } else {
            TokenPos namePos = tokenStream.nextToken().pos;

            PropertyType propType;
            Node propName = propertyOrMethodName(yieldHandling, PropertyNameInPattern, declKind,
                                                 literal, &propType, &propAtom);
            if (!propName)
                return null();
            if (propType == PropertyType::Normal) {
                // Handle e.g., |var {p: x} = o| and |var {p: x=0} = o|.

                if (!tokenStream.getToken(&tt, TokenStream::Operand))
                    return null();

                Node binding = bindingIdentifierOrPattern(kind, yieldHandling, tt);
                if (!binding)
                    return null();

                bool hasInitializer;
                if (!tokenStream.matchToken(&hasInitializer, TOK_ASSIGN))
                    return null();

                Node bindingExpr = hasInitializer
                                   ? bindingInitializer(binding, kind, yieldHandling)
                                   : binding;
                if (!bindingExpr)
                    return null();

                if (!handler.addPropertyDefinition(literal, propName, bindingExpr))
                    return null();
            } else if (propType == PropertyType::Shorthand) {
                // Handle e.g., |var {x, y} = o| as destructuring shorthand
                // for |var {x: x, y: y} = o|.
                MOZ_ASSERT(TokenKindIsPossibleIdentifierName(tt));

                NameNodeType binding = bindingIdentifier(kind, yieldHandling);
                if (!binding)
                    return null();

                if (!handler.addShorthand(literal, handler.asName(propName), binding))
                    return null();
            } else if (propType == PropertyType::CoverInitializedName) {
                // Handle e.g., |var {x=1, y=2} = o| as destructuring
                // shorthand with default values.
                MOZ_ASSERT(TokenKindIsPossibleIdentifierName(tt));

                NameNodeType binding = bindingIdentifier(kind, yieldHandling);
                if (!binding)
                    return null();

                tokenStream.consumeKnownToken(TOK_ASSIGN);

                BinaryNodeType bindingExpr = bindingInitializer(binding, kind, yieldHandling);
                if (!bindingExpr)
                    return null();

                if (!handler.addPropertyDefinition(literal, propName, bindingExpr))
                    return null();
            } else {
                errorAt(namePos.begin, JSMSG_NO_VARIABLE_NAME);
                return null();

            }
        }

        bool matched;
        if (!tokenStream.matchToken(&matched, TOK_COMMA))
            return null();
        if (!matched)
            break;
        if (tt == TOK_TRIPLEDOT) {
            error(JSMSG_REST_WITH_COMMA);
            return null();
        }
    }

    if (!mustMatchToken(TOK_RC, TokenStream::None,
                        [this, begin](TokenKind actual) {
                            this->reportMissingClosing(JSMSG_CURLY_AFTER_LIST,
                                                       JSMSG_CURLY_OPENED, begin);
                        }))
    {
        return null();
    }

    handler.setEndPosition(literal, pos().end);
    return literal;
}

template <typename ParseHandler>
typename ParseHandler::ListNodeType
Parser<ParseHandler>::arrayBindingPattern(DeclarationKind kind, YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LB));

    JS_CHECK_RECURSION(context, return null());

    uint32_t begin = pos().begin;
    ListNodeType literal = handler.newArrayLiteral(begin);
    if (!literal)
        return null();

     uint32_t index = 0;
     TokenStream::Modifier modifier = TokenStream::Operand;
     for (; ; index++) {
         if (index >= NativeObject::MAX_DENSE_ELEMENTS_COUNT) {
             error(JSMSG_ARRAY_INIT_TOO_BIG);
             return null();
         }

         TokenKind tt;
         if (!tokenStream.getToken(&tt, TokenStream::Operand))
             return null();

         if (tt == TOK_RB) {
             tokenStream.ungetToken();
             break;
         }

         if (tt == TOK_COMMA) {
             if (!handler.addElision(literal, pos()))
                 return null();
         } else if (tt == TOK_TRIPLEDOT) {
             uint32_t begin = pos().begin;

             TokenKind tt;
             if (!tokenStream.getToken(&tt, TokenStream::Operand))
                 return null();

             Node inner = bindingIdentifierOrPattern(kind, yieldHandling, tt);
             if (!inner)
                 return null();

             if (!handler.addSpreadElement(literal, begin, inner))
                 return null();
         } else {
             Node binding = bindingIdentifierOrPattern(kind, yieldHandling, tt);
             if (!binding)
                 return null();

             bool hasInitializer;
             if (!tokenStream.matchToken(&hasInitializer, TOK_ASSIGN))
                 return null();

             Node element = hasInitializer
                            ? bindingInitializer(binding, kind, yieldHandling)
                            : binding;
             if (!element)
                 return null();

             handler.addArrayElement(literal, element);
         }

         if (tt != TOK_COMMA) {
             // If we didn't already match TOK_COMMA in above case.
             bool matched;
             if (!tokenStream.matchToken(&matched, TOK_COMMA))
                 return null();
             if (!matched) {
                 modifier = TokenStream::None;
                 break;
             }
             if (tt == TOK_TRIPLEDOT) {
                 error(JSMSG_REST_WITH_COMMA);
                 return null();
             }
         }
     }

     if (!mustMatchToken(TOK_RB, modifier,
                         [this, begin](TokenKind actual) {
                             this->reportMissingClosing(JSMSG_BRACKET_AFTER_LIST,
                                                        JSMSG_BRACKET_OPENED, begin);
                         }))
     {
         return null();
     }

    handler.setEndPosition(literal, pos().end);
    return literal;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::destructuringDeclaration(DeclarationKind kind, YieldHandling yieldHandling,
                                               TokenKind tt)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(tt));
    MOZ_ASSERT(tt == TOK_LB || tt == TOK_LC);

    return tt == TOK_LB
           ? arrayBindingPattern(kind, yieldHandling)
           : objectBindingPattern(kind, yieldHandling);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::destructuringDeclarationWithoutYieldOrAwait(DeclarationKind kind,
                                                                  YieldHandling yieldHandling,
                                                                  TokenKind tt)
{
    uint32_t startYieldOffset = pc->lastYieldOffset;
    uint32_t startAwaitOffset = pc->lastAwaitOffset;
    Node res = destructuringDeclaration(kind, yieldHandling, tt);
    if (res) {
        if (pc->lastYieldOffset != startYieldOffset) {
            errorAt(pc->lastYieldOffset, JSMSG_YIELD_IN_DEFAULT);
            return null();
        }
        if (pc->lastAwaitOffset != startAwaitOffset) {
            errorAt(pc->lastAwaitOffset, JSMSG_AWAIT_IN_DEFAULT);
            return null();
        }
    }
    return res;
}

template <typename ParseHandler>
typename ParseHandler::LexicalScopeNodeType
Parser<ParseHandler>::blockStatement(YieldHandling yieldHandling, unsigned errorNumber)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LC));
    uint32_t openedPos = pos().begin;

    ParseContext::Statement stmt(pc, StatementKind::Block);
    ParseContext::Scope scope(this);
    if (!scope.init(pc))
        return null();

    ListNodeType list = statementList(yieldHandling);
    if (!list)
        return null();

    if (!mustMatchToken(TOK_RC, TokenStream::Operand,
                        [this, errorNumber, openedPos](TokenKind actual) {
                            this->reportMissingClosing(errorNumber, JSMSG_CURLY_OPENED, openedPos);
                        }))
    {
        return null();
    }

    return finishLexicalScope(scope, list);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::expressionAfterForInOrOf(ParseNodeKind forHeadKind,
                                               YieldHandling yieldHandling)
{
    MOZ_ASSERT(forHeadKind == PNK_FORIN || forHeadKind == PNK_FOROF);
    Node pn = forHeadKind == PNK_FOROF
           ? assignExpr(InAllowed, yieldHandling, TripledotProhibited)
           : expr(InAllowed, yieldHandling, TripledotProhibited);
    return pn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::declarationPattern(Node decl, DeclarationKind declKind, TokenKind tt,
                                         bool initialDeclaration, YieldHandling yieldHandling,
                                         ParseNodeKind* forHeadKind, Node* forInOrOfExpression)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LB) ||
               tokenStream.isCurrentTokenType(TOK_LC));

    Node pattern = destructuringDeclaration(declKind, yieldHandling, tt);
    if (!pattern)
        return null();

    if (initialDeclaration && forHeadKind) {
        bool isForIn, isForOf;
        if (!matchInOrOf(&isForIn, &isForOf))
            return null();

        if (isForIn) {
            *forHeadKind = PNK_FORIN;
        } else if (isForOf) {
            *forHeadKind = PNK_FOROF;

            // Annex B.3.5 has different early errors for vars in for-of loops.
            if (declKind == DeclarationKind::Var)
                declKind = DeclarationKind::ForOfVar;
        } else {
            *forHeadKind = PNK_FORHEAD;
        }

        if (*forHeadKind != PNK_FORHEAD) {
            *forInOrOfExpression = expressionAfterForInOrOf(*forHeadKind, yieldHandling);
            if (!*forInOrOfExpression)
                return null();

            return pattern;
        }
    }

    if (!mustMatchToken(TOK_ASSIGN, JSMSG_BAD_DESTRUCT_DECL)) {
        return null();
    }

    Node init = assignExpr(forHeadKind ? InProhibited : InAllowed,
                           yieldHandling, TripledotProhibited);
    if (!init)
        return null();

    handler.checkAndSetIsDirectRHSAnonFunction(init);

    if (forHeadKind) {
        // For for(;;) declarations, consistency with |for (;| parsing requires
        // that the ';' first be examined as Operand, even though absence of a
        // binary operator (examined with modifier None) terminated |init|.
        // For all other declarations, through ASI's infinite majesty, a next
        // token on a new line would begin an expression.
        // Similar to the case in initializerInNameDeclaration(), we need to
        // peek at the next token when assignExpr() is a lazily parsed arrow
        // function.
        TokenKind ignored;
        if (!tokenStream.peekToken(&ignored))
            return null();
        tokenStream.addModifierException(TokenStream::OperandIsNone);
    }

    return handler.newBinary(PNK_ASSIGN, pattern, init);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::initializerInNameDeclaration(Node decl, NameNodeType binding,
                                                   Handle<PropertyName*> name,
                                                   DeclarationKind declKind,
                                                   bool initialDeclaration,
                                                   YieldHandling yieldHandling,
                                                   ParseNodeKind* forHeadKind,
                                                   Node* forInOrOfExpression)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_ASSIGN));

    uint32_t initializerOffset;
    if (!tokenStream.peekOffset(&initializerOffset, TokenStream::Operand))
        return false;

    Node initializer = assignExpr(forHeadKind ? InProhibited : InAllowed,
                                  yieldHandling, TripledotProhibited);
    if (!initializer)
        return false;

    handler.checkAndSetIsDirectRHSAnonFunction(initializer);

    if (forHeadKind) {
        if (initialDeclaration) {
            bool isForIn, isForOf;
            if (!matchInOrOf(&isForIn, &isForOf))
                return false;

            // An initialized declaration can't appear in a for-of:
            //
            //   for (var/let/const x = ... of ...); // BAD
            if (isForOf) {
                errorAt(initializerOffset, JSMSG_OF_AFTER_FOR_LOOP_DECL);
                return false;
            }

            if (isForIn) {
                // Lexical declarations in for-in loops can't be initialized:
                //
                //   for (let/const x = ... in ...); // BAD
                if (DeclarationKindIsLexical(declKind)) {
                    errorAt(initializerOffset, JSMSG_IN_AFTER_LEXICAL_FOR_DECL);
                    return false;
                }

                // This leaves only initialized for-in |var| declarations.  ES6
                // forbids these; later ES un-forbids in non-strict mode code.
                *forHeadKind = PNK_FORIN;
                if (!strictModeErrorAt(initializerOffset, JSMSG_INVALID_FOR_IN_DECL_WITH_INIT))
                    return false;

                *forInOrOfExpression = expressionAfterForInOrOf(PNK_FORIN, yieldHandling);
                if (!*forInOrOfExpression)
                    return false;
            } else {
                *forHeadKind = PNK_FORHEAD;
            }
        } else {
            MOZ_ASSERT(*forHeadKind == PNK_FORHEAD);

            // In the very rare case of Parser::assignExpr consuming an
            // ArrowFunction with block body, when full-parsing with the arrow
            // function being a skipped lazy inner function, we don't have
            // lookahead for the next token.  Do a one-off peek here to be
            // consistent with what Parser::matchForInOrOf does in the other
            // arm of this |if|.
            //
            // If you think this all sounds pretty code-smelly, you're almost
            // certainly correct.
            TokenKind ignored;
            if (!tokenStream.peekToken(&ignored))
                return false;
        }

        if (*forHeadKind == PNK_FORHEAD) {
            // Per Parser::forHeadStart, the semicolon in |for (;| is
            // ultimately gotten as Operand.  But initializer expressions
            // terminate with the absence of an operator gotten as None,
            // so we need an exception.
            tokenStream.addModifierException(TokenStream::OperandIsNone);
        }
    }

    return handler.finishInitializerAssignment(binding, initializer);
}

template <typename ParseHandler>
typename ParseHandler::NameNodeType
Parser<ParseHandler>::declarationName(Node decl, DeclarationKind declKind, TokenKind tt,
                                      bool initialDeclaration, YieldHandling yieldHandling,
                                      ParseNodeKind* forHeadKind, Node* forInOrOfExpression)
{
    // Anything other than TOK_YIELD or TOK_NAME is an error.
    // Anything other than possible identifier is an error.
    if (!TokenKindIsPossibleIdentifier(tt)) {
        error(JSMSG_NO_VARIABLE_NAME);
        return null();
    }

    RootedPropertyName name(context, bindingIdentifier(yieldHandling));
    if (!name)
        return null();

    NameNodeType binding = newName(name);
    if (!binding)
        return null();

    TokenPos namePos = pos();

    // The '=' context after a variable name in a declaration is an opportunity
    // for ASI, and thus for the next token to start an ExpressionStatement:
    //
    //  var foo   // VariableDeclaration
    //  /bar/g;   // ExpressionStatement
    //
    // Therefore get the token here as Operand.
    bool matched;
    if (!tokenStream.matchToken(&matched, TOK_ASSIGN, TokenStream::Operand))
        return null();

    if (matched) {
        if (!initializerInNameDeclaration(decl, binding, name, declKind, initialDeclaration,
                                          yieldHandling, forHeadKind, forInOrOfExpression))
        {
            return null();
        }
    } else {
        tokenStream.addModifierException(TokenStream::NoneIsOperand);

        if (initialDeclaration && forHeadKind) {
            bool isForIn, isForOf;
            if (!matchInOrOf(&isForIn, &isForOf))
                return null();

            if (isForIn) {
                *forHeadKind = PNK_FORIN;
            } else if (isForOf) {
                *forHeadKind = PNK_FOROF;

                // Annex B.3.5 has different early errors for vars in for-of loops.
                if (declKind == DeclarationKind::Var)
                    declKind = DeclarationKind::ForOfVar;
            } else {
                *forHeadKind = PNK_FORHEAD;
            }
        }

        if (forHeadKind && *forHeadKind != PNK_FORHEAD) {
            *forInOrOfExpression = expressionAfterForInOrOf(*forHeadKind, yieldHandling);
            if (!*forInOrOfExpression)
                return null();
        } else {
            // Normal const declarations, and const declarations in for(;;)
            // heads, must be initialized.
            if (declKind == DeclarationKind::Const) {
                errorAt(namePos.begin, JSMSG_BAD_CONST_DECL);
                return null();
            }
        }
    }

    // Note the declared name after knowing whether or not we are in a for-of
    // loop, due to special early error semantics in Annex B.3.5.
    if (!noteDeclaredName(name, declKind, namePos))
        return null();

    return binding;
}

template <typename ParseHandler>
typename ParseHandler::ListNodeType
Parser<ParseHandler>::declarationList(YieldHandling yieldHandling,
                                      ParseNodeKind kind,
                                      ParseNodeKind* forHeadKind /* = nullptr */,
                                      Node* forInOrOfExpression /* = nullptr */)
{
    MOZ_ASSERT(kind == PNK_VAR || kind == PNK_LET || kind == PNK_CONST);

    JSOp op;
    DeclarationKind declKind;
    switch (kind) {
      case PNK_VAR:
        op = JSOP_DEFVAR;
        declKind = DeclarationKind::Var;
        break;
      case PNK_CONST:
        op = JSOP_DEFCONST;
        declKind = DeclarationKind::Const;
        break;
      case PNK_LET:
        op = JSOP_DEFLET;
        declKind = DeclarationKind::Let;
        break;
      default:
        MOZ_CRASH("Unknown declaration kind");
    }

    ListNodeType decl = handler.newDeclarationList(kind, op);
    if (!decl)
        return null();

    bool matched;
    bool initialDeclaration = true;
    do {
        MOZ_ASSERT_IF(!initialDeclaration && forHeadKind,
                      *forHeadKind == PNK_FORHEAD);

        TokenKind tt;
        if (!tokenStream.getToken(&tt))
            return null();

        Node binding = (tt == TOK_LB || tt == TOK_LC)
                       ? declarationPattern(decl, declKind, tt, initialDeclaration, yieldHandling,
                                            forHeadKind, forInOrOfExpression)
                       : declarationName(decl, declKind, tt, initialDeclaration, yieldHandling,
                                         forHeadKind, forInOrOfExpression);
        if (!binding)
            return null();

        handler.addList(decl, binding);

        if (forHeadKind && *forHeadKind != PNK_FORHEAD)
            break;

        initialDeclaration = false;

        if (!tokenStream.matchToken(&matched, TOK_COMMA))
            return null();
    } while (matched);

    return decl;
}

template <typename ParseHandler>
typename ParseHandler::ListNodeType
Parser<ParseHandler>::lexicalDeclaration(YieldHandling yieldHandling, DeclarationKind kind)
{
    MOZ_ASSERT(kind == DeclarationKind::Const || kind == DeclarationKind::Let);

    /*
     * Parse body-level lets without a new block object. ES6 specs
     * that an execution environment's initial lexical environment
     * is the VariableEnvironment, i.e., body-level lets are in
     * the same environment record as vars.
     *
     * However, they cannot be parsed exactly as vars, as ES6
     * requires that uninitialized lets throw ReferenceError on use.
     *
     * See 8.1.1.1.6 and the note in 13.2.1.
     */
    ListNodeType decl = declarationList(yieldHandling,
                                        kind == DeclarationKind::Const ? PNK_CONST : PNK_LET);
    if (!decl || !matchOrInsertSemicolonAfterExpression())
        return null();

    return decl;
}

template <>
bool
Parser<FullParseHandler>::namedImportsOrNamespaceImport(TokenKind tt, ListNodeType importSpecSet)
{
    if (tt == TOK_LC) {
        while (true) {
            // Handle the forms |import {} from 'a'| and
            // |import { ..., } from 'a'| (where ... is non empty), by
            // escaping the loop early if the next token is }.
            if (!tokenStream.getToken(&tt))
                return false;

            if (tt == TOK_RC)
                break;

            if (!TokenKindIsPossibleIdentifierName(tt)) {
                error(JSMSG_NO_IMPORT_NAME);
                return false;
            }

            Rooted<PropertyName*> importName(context, tokenStream.currentName());
            TokenPos importNamePos = pos();

            bool matched;
            if (!tokenStream.matchToken(&matched, TOK_AS))
                return false;

            if (matched) {
                TokenKind afterAs;
                if (!tokenStream.getToken(&afterAs))
                    return false;

                if (!TokenKindIsPossibleIdentifierName(afterAs)) {
                    error(JSMSG_NO_BINDING_NAME);
                    return false;
                }
            } else {
                // Keywords cannot be bound to themselves, so an import name
                // that is a keyword is a syntax error if it is not followed
                // by the keyword 'as'.
                // See the ImportSpecifier production in ES6 section 15.2.2.
                if (IsKeyword(importName)) {
                    error(JSMSG_AS_AFTER_RESERVED_WORD, ReservedWordToCharZ(importName));
                    return false;
                }
            }

            RootedPropertyName bindingAtom(context, importedBinding());
            if (!bindingAtom)
                return false;

            NameNodeType bindingName = newName(bindingAtom);
            if (!bindingName)
                return false;
            if (!noteDeclaredName(bindingAtom, DeclarationKind::Import, pos()))
                return false;

            NameNodeType importNameNode = newName(importName, importNamePos);
            if (!importNameNode)
                return false;

            BinaryNodeType importSpec = handler.newBinary(PNK_IMPORT_SPEC, importNameNode, bindingName);
            if (!importSpec)
                return false;

            handler.addList(importSpecSet, importSpec);

            TokenKind next;
            if (!tokenStream.getToken(&next))
                return false;

            if (next == TOK_RC)
                break;

            if (next != TOK_COMMA) {
                error(JSMSG_RC_AFTER_IMPORT_SPEC_LIST);
                return false;
            }
        }
    } else {
        MOZ_ASSERT(tt == TOK_MUL);

        if (!mustMatchToken(TOK_AS, JSMSG_AS_AFTER_IMPORT_STAR)) {
            return false;
        }

        if (!mustMatchToken(TokenKindIsPossibleIdentifierName, JSMSG_NO_BINDING_NAME)) {
            return false;
        }

        NameNodeType importName = newName(context->names().star);
        if (!importName)
            return false;

        // Namespace imports are are not indirect bindings but lexical
        // definitions that hold a module namespace object. They are treated
        // as const variables which are initialized during the
        // ModuleInstantiate step.
        RootedPropertyName bindingName(context, importedBinding());
        if (!bindingName)
            return false;
        NameNodeType bindingNameNode = newName(bindingName);
        if (!bindingNameNode)
            return false;
        if (!noteDeclaredName(bindingName, DeclarationKind::Const, pos()))
            return false;

        // The namespace import name is currently required to live on the
        // environment.
        pc->varScope().lookupDeclaredName(bindingName)->value()->setClosedOver();

        BinaryNodeType importSpec = handler.newBinary(PNK_IMPORT_SPEC, importName, bindingNameNode);
        if (!importSpec)
            return false;

        handler.addList(importSpecSet, importSpec);
    }

    return true;
}

template<>
BinaryNode*
Parser<FullParseHandler>::importDeclaration()
{
    MOZ_ASSERT(tokenStream.currentToken().type == TOK_IMPORT);

    if (!pc->atModuleLevel()) {
        error(JSMSG_IMPORT_DECL_AT_TOP_LEVEL);
        return null();
    }

    uint32_t begin = pos().begin;
    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return null();

    ListNodeType importSpecSet = handler.newList(PNK_IMPORT_SPEC_LIST);
    if (!importSpecSet)
        return null();

    if (tt == TOK_STRING) {
        // Handle the form |import 'a'| by leaving the list empty. This is
        // equivalent to |import {} from 'a'|.
        importSpecSet->pn_pos.end = importSpecSet->pn_pos.begin;
    } else {
        if (tt == TOK_LC || tt == TOK_MUL) {
            if (!namedImportsOrNamespaceImport(tt, importSpecSet))
                return null();
        } else if (TokenKindIsPossibleIdentifierName(tt)) {
            // Handle the form |import a from 'b'|, by adding a single import
            // specifier to the list, with 'default' as the import name and
            // 'a' as the binding name. This is equivalent to
            // |import { default as a } from 'b'|.
            Node importName = newName(context->names().default_);
            if (!importName)
                return null();

            RootedPropertyName bindingAtom(context, importedBinding());
            if (!bindingAtom)
                return null();

            Node bindingName = newName(bindingAtom);
            if (!bindingName)
                return null();

            if (!noteDeclaredName(bindingAtom, DeclarationKind::Import, pos()))
                return null();

            BinaryNodeType importSpec = handler.newBinary(PNK_IMPORT_SPEC, importName, bindingName);
            if (!importSpec)
                return null();

            handler.addList(importSpecSet, importSpec);

            if (!tokenStream.peekToken(&tt))
                return null();

            if (tt == TOK_COMMA) {
                tokenStream.consumeKnownToken(tt);
                if (!tokenStream.getToken(&tt))
                    return null();

                if (tt != TOK_LC && tt != TOK_MUL) {
                    error(JSMSG_NAMED_IMPORTS_OR_NAMESPACE_IMPORT);
                    return null();
                }

                if (!namedImportsOrNamespaceImport(tt, importSpecSet))
                    return null();
            }
        } else {
            error(JSMSG_DECLARATION_AFTER_IMPORT);
            return null();
        }

        if (!mustMatchToken(TOK_FROM, JSMSG_FROM_AFTER_IMPORT_CLAUSE)) {
            return null();
        }

        if (!mustMatchToken(TOK_STRING, JSMSG_MODULE_SPEC_AFTER_FROM)) {
            return null();
        }
    }

    NameNodeType moduleSpec = stringLiteral();
    if (!moduleSpec)
        return null();

    if (!matchOrInsertSemicolonAfterNonExpression())
        return null();

    BinaryNode* node =
        handler.newImportDeclaration(importSpecSet, moduleSpec, TokenPos(begin, pos().end));
    if (!node || !pc->sc()->asModuleContext()->builder.processImport(node))
        return null();

    return node;
}

template<>
SyntaxParseHandler::BinaryNodeType
Parser<SyntaxParseHandler>::importDeclaration()
{
    JS_ALWAYS_FALSE(abortIfSyntaxParser());
    return SyntaxParseHandler::NodeFailure;
}

template <class ParseHandler>
inline typename ParseHandler::Node
Parser<ParseHandler>::importDeclarationOrImportExpr(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_IMPORT));

    TokenKind tt;
    if (!tokenStream.peekToken(&tt))
        return null();

    if (tt == TOK_DOT || tt == TOK_LP)
        return expressionStatement(yieldHandling);

    return importDeclaration();
}

template<>
bool
Parser<FullParseHandler>::checkExportedName(JSAtom* exportName)
{
    if (!pc->sc()->asModuleContext()->builder.hasExportedName(exportName))
        return true;

    JSAutoByteString str;
    if (!AtomToPrintableString(context, exportName, &str))
        return false;

    error(JSMSG_DUPLICATE_EXPORT_NAME, str.ptr());
    return false;
}

template<>
bool
Parser<SyntaxParseHandler>::checkExportedName(JSAtom* exportName)
{
    MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
    return false;
}

// Because all these template methods call each other, let's declare them first before
// defining them to prevent a GCC compiler error in specialization of member function
// template after instantiation.
template<>
bool
Parser<FullParseHandler>::checkExportedNamesForDeclaration(ParseNode* node);

template<>
bool
Parser<SyntaxParseHandler>::checkExportedNamesForDeclaration(Node node);

template<>
bool
Parser<FullParseHandler>::checkExportedNamesForArrayBinding(ListNode* array);

template<>
inline bool
Parser<SyntaxParseHandler>::checkExportedNamesForArrayBinding(ListNodeType array);

template<>
bool
Parser<FullParseHandler>::checkExportedNamesForObjectBinding(ListNode* obj);

template<>
inline bool
Parser<SyntaxParseHandler>::checkExportedNamesForObjectBinding(ListNodeType obj);

template<>
bool
Parser<FullParseHandler>::checkExportedNamesForArrayBinding(ListNode* array)
{
    MOZ_ASSERT(array->isKind(PNK_ARRAY));

    for (ParseNode* node : array->contents()) {
        if (node->isKind(PNK_ELISION))
            continue;

        ParseNode* binding;
        if (node->isKind(PNK_SPREAD))
            binding = node->as<UnaryNode>().kid();
        else if (node->isKind(PNK_ASSIGN))
            binding = node->as<AssignmentNode>().left();
        else
            binding = node;

        if (!checkExportedNamesForDeclaration(binding))
            return false;
    }

    return true;
}

template<>
inline bool
Parser<SyntaxParseHandler>::checkExportedNamesForArrayBinding(ListNodeType array)
{
    MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
    return false;
}

template<>
bool
Parser<FullParseHandler>::checkExportedNamesForObjectBinding(ListNode* obj)
{
    MOZ_ASSERT(obj->isKind(PNK_OBJECT));

    for (ParseNode* node : obj->contents()) {
        MOZ_ASSERT(node->isKind(PNK_MUTATEPROTO) ||
                   node->isKind(PNK_COLON) ||
                   node->isKind(PNK_SHORTHAND) ||
                   node->isKind(PNK_SPREAD));

        ParseNode* target;
        if (node->isKind(PNK_SPREAD)) {
            target = node->as<UnaryNode>().kid();
        } else {
            if (node->isKind(PNK_MUTATEPROTO))
                target = node->as<UnaryNode>().kid();
            else
                target = node->as<BinaryNode>().right();

            if (target->isKind(PNK_ASSIGN))
                target = target->as<AssignmentNode>().left();
        }

        if (!checkExportedNamesForDeclaration(target))
            return false;
    }

    return true;
}

template<>
inline bool
Parser<SyntaxParseHandler>::checkExportedNamesForObjectBinding(ListNodeType obj)
{
    MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
    return false;
}

template<>
bool
Parser<FullParseHandler>::checkExportedNamesForDeclaration(ParseNode* node)
{
    if (node->isKind(PNK_NAME)) {
        if (!checkExportedName(node->as<NameNode>().atom()))
            return false;
    } else if (node->isKind(PNK_ARRAY)) {
        if (!checkExportedNamesForArrayBinding(&node->as<ListNode>()))
            return false;
    } else {
        MOZ_ASSERT(node->isKind(PNK_OBJECT));
        if (!checkExportedNamesForObjectBinding(&node->as<ListNode>()))
            return false;
    }

    return true;
}

template<>
bool
Parser<SyntaxParseHandler>::checkExportedNamesForDeclaration(Node node)
{
    MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
    return false;
}

template<>
bool
Parser<FullParseHandler>::checkExportedNamesForDeclarationList(ListNode* node)
{
    for (ParseNode* binding : node->contents()) {
        if (binding->isKind(PNK_ASSIGN))
            binding = binding->as<AssignmentNode>().left();
        else
            MOZ_ASSERT(binding->isKind(PNK_NAME));

        if (!checkExportedNamesForDeclaration(binding))
            return false;
    }

    return true;
}

template<>
bool
Parser<SyntaxParseHandler>::checkExportedNamesForDeclarationList(ListNodeType node)
{
    MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
    return false;
}

template<>
bool
Parser<FullParseHandler>::checkExportedNameForClause(NameNodeType nameNode)
{
    return checkExportedName(nameNode->atom());
}

template<>
bool
Parser<SyntaxParseHandler>::checkExportedNameForClause(NameNodeType nameNode)
{
    MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
    return false;
}

template<>
bool
Parser<FullParseHandler>::checkExportedNameForFunction(FunctionNodeType funNode)
{
    return checkExportedName(funNode->funbox()->function()->explicitName());
}

template<>
bool
Parser<SyntaxParseHandler>::checkExportedNameForFunction(FunctionNodeType funNode)
{
    MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
    return false;
}

template<>
bool
Parser<FullParseHandler>::checkExportedNameForClass(ClassNode* classNode)
{
    MOZ_ASSERT(classNode->names());
    return checkExportedName(classNode->names()->innerBinding()->atom());
}

template<>
bool
Parser<SyntaxParseHandler>::checkExportedNameForClass(ClassNodeType classNode)
{
    MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
    return false;
}

template<>
bool
Parser<FullParseHandler>::processExport(ParseNode* node)
{
    return pc->sc()->asModuleContext()->builder.processExport(node);
}

template<>
bool
Parser<SyntaxParseHandler>::processExport(Node node)
{
    MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
    return false;
}

template<>
bool
Parser<FullParseHandler>::processExportFrom(BinaryNodeType node)
{
    return pc->sc()->asModuleContext()->builder.processExportFrom(node);
}

template<>
bool
Parser<SyntaxParseHandler>::processExportFrom(BinaryNodeType node)
{
    MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
    return false;
}

template <typename ParseHandler>
typename ParseHandler::BinaryNodeType
Parser<ParseHandler>::exportFrom(uint32_t begin, Node specList)
{
    if (!abortIfSyntaxParser())
        return null();

    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FROM));

    if (!abortIfSyntaxParser())
        return null();

    if (!mustMatchToken(TOK_STRING, JSMSG_MODULE_SPEC_AFTER_FROM)) {
        return null();
    }

    NameNodeType moduleSpec = stringLiteral();
    if (!moduleSpec)
        return null();

    if (!matchOrInsertSemicolonAfterNonExpression())
        return null();

    BinaryNodeType node = handler.newExportFromDeclaration(begin, specList, moduleSpec);
    if (!node)
        return null();

    if (!processExportFrom(node))
        return null();

    return node;
}

template <typename ParseHandler>
typename ParseHandler::BinaryNodeType
Parser<ParseHandler>::exportBatch(uint32_t begin)
{
    if (!abortIfSyntaxParser())
        return null();

    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_MUL));

    ListNodeType kid = handler.newList(PNK_EXPORT_SPEC_LIST);
    if (!kid)
        return null();

    // Handle the form |export *| by adding a special export batch
    // specifier to the list.
    NullaryNodeType exportSpec = handler.newNullary(PNK_EXPORT_BATCH_SPEC, JSOP_NOP, pos());
    if (!exportSpec)
        return null();

    handler.addList(kid, exportSpec);

    if (!mustMatchToken(TOK_FROM, JSMSG_FROM_AFTER_EXPORT_STAR)) {
        return null();
    }

    return exportFrom(begin, kid);
}

template<>
bool
Parser<FullParseHandler>::checkLocalExportNames(ListNode* node)
{
    // ES 2017 draft 15.2.3.1.
    for (ParseNode* next : node->contents()) {
        ParseNode* name = next->as<BinaryNode>().left();
        MOZ_ASSERT(name->isKind(PNK_NAME));

        RootedPropertyName ident(context, name->as<NameNode>().atom()->asPropertyName());
        if (!checkLocalExportName(ident, name->pn_pos.begin))
            return false;
    }

    return true;
}

template<>
bool
Parser<SyntaxParseHandler>::checkLocalExportNames(ListNodeType node)
{
    MOZ_ALWAYS_FALSE(abortIfSyntaxParser());
    return false;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::exportClause(uint32_t begin)
{
    if (!abortIfSyntaxParser())
        return null();

    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LC));

    ListNodeType kid = handler.newList(PNK_EXPORT_SPEC_LIST);
    if (!kid)
        return null();

    TokenKind tt;
    while (true) {
        // Handle the forms |export {}| and |export { ..., }| (where ... is non
        // empty), by escaping the loop early if the next token is }.
        if (!tokenStream.getToken(&tt))
            return null();

        if (tt == TOK_RC)
            break;

        if (!TokenKindIsPossibleIdentifierName(tt)) {
            error(JSMSG_NO_BINDING_NAME);
            return null();
        }

        NameNodeType bindingName = newName(tokenStream.currentName());
        if (!bindingName)
            return null();

        bool foundAs;
        if (!tokenStream.matchToken(&foundAs, TOK_AS))
            return null();
        if (foundAs) {
            if (!mustMatchToken(TokenKindIsPossibleIdentifierName, JSMSG_NO_EXPORT_NAME)) {
                return null();
            }
        }

        NameNodeType exportName = newName(tokenStream.currentName());
        if (!exportName)
            return null();

        if (!checkExportedNameForClause(exportName))
            return null();

        BinaryNodeType exportSpec = handler.newBinary(PNK_EXPORT_SPEC, bindingName, exportName);
        if (!exportSpec)
            return null();

        handler.addList(kid, exportSpec);

        TokenKind next;
        if (!tokenStream.getToken(&next))
            return null();

        if (next == TOK_RC)
            break;

        if (next != TOK_COMMA) {
            error(JSMSG_RC_AFTER_EXPORT_SPEC_LIST);
            return null();
        }
    }

    // Careful!  If |from| follows, even on a new line, it must start a
    // FromClause:
    //
    //   export { x }
    //   from "foo"; // a single ExportDeclaration
    //
    // But if it doesn't, we might have an ASI opportunity in Operand context:
    //
    //   export { x }   // ExportDeclaration, terminated by ASI
    //   fro\u006D      // ExpressionStatement, the name "from"
    //
    // In that case let matchOrInsertSemicolonAfterNonExpression sort out ASI
    // or any necessary error.
    bool matched;
    if (!tokenStream.matchToken(&matched, TOK_FROM, TokenStream::Operand))
        return null();

    if (matched)
        return exportFrom(begin, kid);

    if (!matchOrInsertSemicolonAfterNonExpression())
        return null();

    if (!checkLocalExportNames(kid))
        return null();

    UnaryNodeType node = handler.newExportDeclaration(kid, TokenPos(begin, pos().end));
    if (!node)
        return null();

    if (!processExport(node))
        return null();

    return node;
}

template <typename ParseHandler>
typename ParseHandler::UnaryNodeType
Parser<ParseHandler>::exportVariableStatement(uint32_t begin)
{
    if (!abortIfSyntaxParser())
        return null();

    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_VAR));

    ListNodeType kid = declarationList(YieldIsName, PNK_VAR);
    if (!kid)
        return null();
    if (!matchOrInsertSemicolonAfterExpression())
        return null();
    if (!checkExportedNamesForDeclarationList(kid))
        return null();

    UnaryNodeType node = handler.newExportDeclaration(kid, TokenPos(begin, pos().end));
    if (!node)
        return null();

    if (!processExport(node))
        return null();

    return node;
}

template <typename ParseHandler>
typename ParseHandler::UnaryNodeType
Parser<ParseHandler>::exportFunctionDeclaration(uint32_t begin,
                                                FunctionAsyncKind asyncKind /* = SyncFunction */)
{
    if (!abortIfSyntaxParser())
        return null();

    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FUNCTION));

    Node kid = functionStmt(pos().begin, YieldIsKeyword, NameRequired, asyncKind);
    if (!kid)
        return null();

    if (!checkExportedNameForFunction(handler.asFunction(kid)))
        return null();

    UnaryNodeType node = handler.newExportDeclaration(kid, TokenPos(begin, pos().end));
    if (!node)
        return null();

    if (!processExport(node))
        return null();

    return node;
}

template <typename ParseHandler>
typename ParseHandler::UnaryNodeType
Parser<ParseHandler>::exportClassDeclaration(uint32_t begin)
{
    if (!abortIfSyntaxParser())
        return null();

    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_CLASS));

    ClassNodeType kid = classDefinition(YieldIsName, ClassStatement, NameRequired);
    if (!kid)
        return null();

    if (!checkExportedNameForClass(kid))
        return null();

    UnaryNodeType node = handler.newExportDeclaration(kid, TokenPos(begin, pos().end));
    if (!node)
        return null();

    if (!processExport(node))
        return null();

    return node;
}

template <typename ParseHandler>
typename ParseHandler::UnaryNodeType
Parser<ParseHandler>::exportLexicalDeclaration(uint32_t begin, DeclarationKind kind)
{
    if (!abortIfSyntaxParser())
        return null();

    MOZ_ASSERT(kind == DeclarationKind::Const || kind == DeclarationKind::Let);
    MOZ_ASSERT_IF(kind == DeclarationKind::Const, tokenStream.isCurrentTokenType(TOK_CONST));
    MOZ_ASSERT_IF(kind == DeclarationKind::Let, tokenStream.isCurrentTokenType(TOK_LET));

    ListNodeType kid = lexicalDeclaration(YieldIsName, kind);
    if (!kid)
        return null();
    if (!checkExportedNamesForDeclarationList(kid))
        return null();

    UnaryNodeType node = handler.newExportDeclaration(kid, TokenPos(begin, pos().end));
    if (!node)
        return null();

    if (!processExport(node))
        return null();

    return node;
}

template <typename ParseHandler>
typename ParseHandler::BinaryNodeType
Parser<ParseHandler>::exportDefaultFunctionDeclaration(uint32_t begin,
                                                           FunctionAsyncKind asyncKind
                                                           /* = SyncFunction */)
{
    if (!abortIfSyntaxParser())
        return null();

    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FUNCTION));

    Node kid = functionStmt(pos().begin, YieldIsKeyword, AllowDefaultName, asyncKind);
    if (!kid)
        return null();

    BinaryNodeType node = handler.newExportDefaultDeclaration(kid, null(), TokenPos(begin, pos().end));
    if (!node)
        return null();

    if (!processExport(node))
        return null();

    return node;
}

template <typename ParseHandler>
typename ParseHandler::BinaryNodeType
Parser<ParseHandler>::exportDefaultClassDeclaration(uint32_t begin)
{
    if (!abortIfSyntaxParser())
        return null();

    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_CLASS));

    ClassNodeType kid = classDefinition(YieldIsName, ClassStatement, AllowDefaultName);
    if (!kid)
        return null();

    BinaryNodeType node = handler.newExportDefaultDeclaration(kid, null(), TokenPos(begin, pos().end));
    if (!node)
        return null();

    if (!processExport(node))
        return null();

    return node;
}

template <typename ParseHandler>
typename ParseHandler::BinaryNodeType
Parser<ParseHandler>::exportDefaultAssignExpr(uint32_t begin)
{
    if (!abortIfSyntaxParser())
        return null();

    RootedPropertyName name(context, context->names().starDefaultStar);
    NameNodeType nameNode = newName(name);
    if (!nameNode)
        return null();
    if (!noteDeclaredName(name, DeclarationKind::Const, pos()))
        return null();

    Node kid = assignExpr(InAllowed, YieldIsKeyword, TripledotProhibited);
    if (!kid)
        return null();
    if (!matchOrInsertSemicolonAfterExpression())
        return null();

    BinaryNodeType node = handler.newExportDefaultDeclaration(kid, nameNode, TokenPos(begin, pos().end));
    if (!node)
        return null();

    if (!processExport(node))
        return null();

    return node;
}

template <typename ParseHandler>
typename ParseHandler::BinaryNodeType
Parser<ParseHandler>::exportDefault(uint32_t begin)
{
    if (!abortIfSyntaxParser())
        return null();

    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_DEFAULT));

    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();

    if (!checkExportedName(context->names().default_))
        return null();

    switch (tt) {
      case TOK_FUNCTION:
        return exportDefaultFunctionDeclaration(begin);

      case TOK_ASYNC: {
        TokenKind nextSameLine = TOK_EOF;
        if (!tokenStream.peekTokenSameLine(&nextSameLine))
            return null();

        if (nextSameLine == TOK_FUNCTION) {
            tokenStream.consumeKnownToken(TOK_FUNCTION);
            return exportDefaultFunctionDeclaration(begin, AsyncFunction);
        }

        tokenStream.ungetToken();
        return exportDefaultAssignExpr(begin);
      }

      case TOK_CLASS:
        return exportDefaultClassDeclaration(begin);

      default:
        tokenStream.ungetToken();
        return exportDefaultAssignExpr(begin);
    }
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::exportDeclaration()
{
    if (!abortIfSyntaxParser())
        return null();

    MOZ_ASSERT(tokenStream.currentToken().type == TOK_EXPORT);

    if (!pc->atModuleLevel()) {
        error(JSMSG_EXPORT_DECL_AT_TOP_LEVEL);
        return null();
    }

    uint32_t begin = pos().begin;

    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return null();
    switch (tt) {
      case TOK_MUL:
        return exportBatch(begin);

      case TOK_LC:
        return exportClause(begin);

      case TOK_VAR:
        return exportVariableStatement(begin);

      case TOK_FUNCTION:
        return exportFunctionDeclaration(begin);

      case TOK_ASYNC: {
        TokenKind nextSameLine = TOK_EOF;
        if (!tokenStream.peekTokenSameLine(&nextSameLine))
            return null();

        if (nextSameLine == TOK_FUNCTION) {
            tokenStream.consumeKnownToken(TOK_FUNCTION);
            return exportFunctionDeclaration(begin, AsyncFunction);
        }

        error(JSMSG_DECLARATION_AFTER_EXPORT);
        return null();
      }

      case TOK_CLASS:
        return exportClassDeclaration(begin);

      case TOK_CONST:
        return exportLexicalDeclaration(begin, DeclarationKind::Const);

      case TOK_LET:
        return exportLexicalDeclaration(begin, DeclarationKind::Let);

      case TOK_DEFAULT:
        return exportDefault(begin);

      default:
        error(JSMSG_DECLARATION_AFTER_EXPORT);
        return null();
    }
}

template <typename ParseHandler>
typename ParseHandler::UnaryNodeType
Parser<ParseHandler>::expressionStatement(YieldHandling yieldHandling, InvokedPrediction invoked)
{
    tokenStream.ungetToken();
    Node pnexpr = expr(InAllowed, yieldHandling, TripledotProhibited,
                       /* possibleError = */ nullptr, invoked);
    if (!pnexpr)
        return null();
    if (!matchOrInsertSemicolonAfterExpression())
        return null();
    return handler.newExprStatement(pnexpr, pos().end);
}

template <class ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::consequentOrAlternative(YieldHandling yieldHandling)
{
    TokenKind next;
    if (!tokenStream.peekToken(&next, TokenStream::Operand))
        return null();

    // Annex B.3.4 says that unbraced FunctionDeclarations under if/else in
    // non-strict code act as if they were braced: |if (x) function f() {}|
    // parses as |if (x) { function f() {} }|.
    //
    // Careful!  FunctionDeclaration doesn't include generators or async
    // functions.
    if (next == TOK_ASYNC) {
        tokenStream.consumeKnownToken(next, TokenStream::Operand);

        // Peek only on the same line: ExpressionStatement's lookahead
        // restriction is phrased as
        //
        //   [lookahead ∉ { {, function, async [no LineTerminator here] function, class, let [ }]
        //
        // meaning that code like this is valid:
        //
        //   if (true)
        //     async       // ASI opportunity
        //   function clownshoes() {}
        TokenKind maybeFunction;
        if (!tokenStream.peekTokenSameLine(&maybeFunction))
            return null();

        if (maybeFunction == TOK_FUNCTION) {
            error(JSMSG_FORBIDDEN_AS_STATEMENT, "async function declarations");
            return null();
        }

        // Otherwise this |async| begins an ExpressionStatement.
        tokenStream.ungetToken();
    } else if (next == TOK_FUNCTION) {
        tokenStream.consumeKnownToken(next, TokenStream::Operand);

        // Parser::statement would handle this, but as this function handles
        // every other error case, it seems best to handle this.
        if (pc->sc()->strict()) {
            error(JSMSG_FORBIDDEN_AS_STATEMENT, "function declarations");
            return null();
        }

        TokenKind maybeStar;
        if (!tokenStream.peekToken(&maybeStar))
            return null();

        if (maybeStar == TOK_MUL) {
            error(JSMSG_FORBIDDEN_AS_STATEMENT, "generator declarations");
            return null();
        }

        ParseContext::Statement stmt(pc, StatementKind::Block);
        ParseContext::Scope scope(this);
        if (!scope.init(pc))
            return null();

        TokenPos funcPos = pos();
        Node fun = functionStmt(pos().begin, yieldHandling, NameRequired);
        if (!fun)
            return null();

        ListNodeType block = handler.newStatementList(funcPos);
        if (!block)
            return null();

        handler.addStatementToList(block, fun);
        return finishLexicalScope(scope, block);
    }

    return statement(yieldHandling);
}

template <typename ParseHandler>
typename ParseHandler::TernaryNodeType
Parser<ParseHandler>::ifStatement(YieldHandling yieldHandling)
{
    Vector<Node, 4> condList(context), thenList(context);
    Vector<uint32_t, 4> posList(context);
    Node elseBranch;

    ParseContext::Statement stmt(pc, StatementKind::If);

    while (true) {
        uint32_t begin = pos().begin;

        /* An IF node has three kids: condition, then, and optional else. */
        Node cond = condition(InAllowed, yieldHandling);
        if (!cond)
            return null();

        TokenKind tt;
        if (!tokenStream.peekToken(&tt, TokenStream::Operand))
            return null();
        if (tt == TOK_SEMI) {
            if (!extraWarning(JSMSG_EMPTY_CONSEQUENT))
                return null();
        }

        Node thenBranch = consequentOrAlternative(yieldHandling);
        if (!thenBranch)
            return null();

        if (!condList.append(cond) || !thenList.append(thenBranch) || !posList.append(begin))
            return null();

        bool matched;
        if (!tokenStream.matchToken(&matched, TOK_ELSE, TokenStream::Operand))
            return null();
        if (matched) {
            if (!tokenStream.matchToken(&matched, TOK_IF, TokenStream::Operand))
                return null();
            if (matched)
                continue;
            elseBranch = consequentOrAlternative(yieldHandling);
            if (!elseBranch)
                return null();
        } else {
            elseBranch = null();
        }
        break;
    }

    TernaryNodeType ifNode;
    for (int i = condList.length() - 1; i >= 0; i--) {
        ifNode = handler.newIfStatement(posList[i], condList[i], thenList[i], elseBranch);
        if (!ifNode) {
            return null();
        }
        elseBranch = ifNode;
    }

    return ifNode;
}

template <typename ParseHandler>
typename ParseHandler::BinaryNodeType
Parser<ParseHandler>::doWhileStatement(YieldHandling yieldHandling)
{
    uint32_t begin = pos().begin;
    ParseContext::Statement stmt(pc, StatementKind::DoLoop);
    Node body = statement(yieldHandling);
    if (!body)
        return null();
    if (!mustMatchToken(TOK_WHILE, TokenStream::Operand, JSMSG_WHILE_AFTER_DO)) {
        return null();
    }
    Node cond = condition(InAllowed, yieldHandling);
    if (!cond)
        return null();

    // The semicolon after do-while is even more optional than most
    // semicolons in JS.  Web compat required this by 2004:
    //   http://bugzilla.mozilla.org/show_bug.cgi?id=238945
    // ES3 and ES5 disagreed, but ES6 conforms to Web reality:
    //   https://bugs.ecmascript.org/show_bug.cgi?id=157
    // To parse |do {} while (true) false| correctly, use Operand.
    bool ignored;
    if (!tokenStream.matchToken(&ignored, TOK_SEMI, TokenStream::Operand))
        return null();
    return handler.newDoWhileStatement(body, cond, TokenPos(begin, pos().end));
}

template <typename ParseHandler>
typename ParseHandler::BinaryNodeType
Parser<ParseHandler>::whileStatement(YieldHandling yieldHandling)
{
    uint32_t begin = pos().begin;
    ParseContext::Statement stmt(pc, StatementKind::WhileLoop);
    Node cond = condition(InAllowed, yieldHandling);
    if (!cond)
        return null();
    Node body = statement(yieldHandling);
    if (!body)
        return null();
    return handler.newWhileStatement(begin, cond, body);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::matchInOrOf(bool* isForInp, bool* isForOfp)
{
    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return false;

    *isForInp = tt == TOK_IN;
    *isForOfp = tt == TOK_OF;
    if (!*isForInp && !*isForOfp)
        tokenStream.ungetToken();

    MOZ_ASSERT_IF(*isForInp || *isForOfp, *isForInp != *isForOfp);
    return true;
}

template <class ParseHandler>
bool
Parser<ParseHandler>::forHeadStart(YieldHandling yieldHandling,
                                   IteratorKind iterKind,
                                   ParseNodeKind* forHeadKind,
                                   Node* forInitialPart,
                                   Maybe<ParseContext::Scope>& forLoopLexicalScope,
                                   Node* forInOrOfExpression)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LP));

    TokenKind tt;
    if (!tokenStream.peekToken(&tt, TokenStream::Operand))
        return false;

    // Super-duper easy case: |for (;| is a C-style for-loop with no init
    // component.
    if (tt == TOK_SEMI) {
        *forInitialPart = null();
        *forHeadKind = PNK_FORHEAD;
        return true;
    }

    // Parsing after |for (var| is also relatively simple (from this method's
    // point of view).  No block-related work complicates matters, so delegate
    // to Parser::declaration.
    if (tt == TOK_VAR) {
        tokenStream.consumeKnownToken(tt, TokenStream::Operand);

        // Pass null for block object because |var| declarations don't use one.
        *forInitialPart = declarationList(yieldHandling, PNK_VAR, forHeadKind,
                                          forInOrOfExpression);
        return *forInitialPart != null();
    }

    // Otherwise we have a lexical declaration or an expression.

    // For-in loop backwards compatibility requires that |let| starting a
    // for-loop that's not a (new to ES6) for-of loop, in non-strict mode code,
    // parse as an identifier.  (|let| in for-of is always a declaration.)
    bool parsingLexicalDeclaration = false;
    bool letIsIdentifier = false;
    if (tt == TOK_CONST) {
        parsingLexicalDeclaration = true;
        tokenStream.consumeKnownToken(tt, TokenStream::Operand);
    } else if (tt == TOK_LET) {
        // We could have a {For,Lexical}Declaration, or we could have a
        // LeftHandSideExpression with lookahead restrictions so it's not
        // ambiguous with the former.  Check for a continuation of the former
        // to decide which we have.
        tokenStream.consumeKnownToken(TOK_LET, TokenStream::Operand);

        TokenKind next;
        if (!tokenStream.peekToken(&next))
            return false;

        parsingLexicalDeclaration = nextTokenContinuesLetDeclaration(next, yieldHandling);
        if (!parsingLexicalDeclaration) {
            tokenStream.ungetToken();
            letIsIdentifier = true;
        }
    }

    if (parsingLexicalDeclaration) {
        forLoopLexicalScope.emplace(this);
        if (!forLoopLexicalScope->init(pc))
            return false;

        // Push a temporary ForLoopLexicalHead Statement that allows for
        // lexical declarations, as they are usually allowed only in braced
        // statements.
        ParseContext::Statement forHeadStmt(pc, StatementKind::ForLoopLexicalHead);

        *forInitialPart = declarationList(yieldHandling, tt == TOK_CONST ? PNK_CONST : PNK_LET,
                                          forHeadKind, forInOrOfExpression);
        return *forInitialPart != null();
    }

    uint32_t exprOffset;
    if (!tokenStream.peekOffset(&exprOffset, TokenStream::Operand))
        return false;

    // Finally, handle for-loops that start with expressions.  Pass
    // |InProhibited| so that |in| isn't parsed in a RelationalExpression as a
    // binary operator.  |in| makes it a for-in loop, *not* an |in| expression.
    PossibleError possibleError(*this);
    *forInitialPart = expr(InProhibited, yieldHandling, TripledotProhibited, &possibleError);
    if (!*forInitialPart)
        return false;

    bool isForIn, isForOf;
    if (!matchInOrOf(&isForIn, &isForOf))
        return false;

    if (iterKind == IteratorKind::Async && !isForOf) {
        error(JSMSG_FOR_AWAIT_NOT_OF);
        return false;
    }

    // If we don't encounter 'in'/'of', we have a for(;;) loop.  We've handled
    // the init expression; the caller handles the rest.  Allow the Operand
    // modifier when regetting: Operand must be used to examine the ';' in
    // |for (;|, and our caller handles this case and that.
    if (!isForIn && !isForOf) {
        if (!possibleError.checkForExpressionError())
            return false;
        *forHeadKind = PNK_FORHEAD;
        tokenStream.addModifierException(TokenStream::OperandIsNone);
        return true;
    }

    MOZ_ASSERT(isForIn != isForOf);

    // In a for-of loop, 'let' that starts the loop head is a |let| keyword,
    // per the [lookahead ≠ let] restriction on the LeftHandSideExpression
    // variant of such loops.  Expressions that start with |let| can't be used
    // here.
    //
    //   var let = {};
    //   for (let.prop of [1]) // BAD
    //     break;
    //
    // See ES6 13.7.
    if (isForOf && letIsIdentifier) {
        errorAt(exprOffset, JSMSG_LET_STARTING_FOROF_LHS);
        return false;
    }

    *forHeadKind = isForIn ? PNK_FORIN : PNK_FOROF;

    // Verify the left-hand side expression doesn't have a forbidden form.
    if (handler.isUnparenthesizedDestructuringPattern(*forInitialPart)) {
        if (!possibleError.checkForDestructuringErrorOrWarning())
            return false;
    } else if (handler.isNameAnyParentheses(*forInitialPart)) {
        const char* chars = handler.nameIsArgumentsEvalAnyParentheses(*forInitialPart, context);
        if (chars) {
            // |chars| is "arguments" or "eval" here.
            if (!strictModeErrorAt(exprOffset, JSMSG_BAD_STRICT_ASSIGN, chars))
                return false;
        }

        handler.adjustGetToSet(*forInitialPart);
    } else if (handler.isPropertyAccess(*forInitialPart)) {
        // Permitted: no additional testing/fixup needed.
    } else if (handler.isFunctionCall(*forInitialPart)) {
        if (!strictModeErrorAt(exprOffset, JSMSG_BAD_FOR_LEFTSIDE))
            return false;
    } else {
        errorAt(exprOffset, JSMSG_BAD_FOR_LEFTSIDE);
        return false;
    }

    if (!possibleError.checkForExpressionError())
        return false;

    // Finally, parse the iterated expression, making the for-loop's closing
    // ')' the next token.
    *forInOrOfExpression = expressionAfterForInOrOf(*forHeadKind, yieldHandling);
    return *forInOrOfExpression != null();
}

template <class ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::forStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));
    uint32_t begin = pos().begin;

    ParseContext::Statement stmt(pc, StatementKind::ForLoop);

    bool isForEach = false;
    IteratorKind iterKind = IteratorKind::Sync;
    unsigned iflags = 0;

    if (allowsForEachIn()) {
        bool matched;
        if (!tokenStream.matchToken(&matched, TOK_EACH))
            return null();
        if (matched) {
            iflags = JSITER_FOREACH;
            isForEach = true;
            if (!warnOnceAboutForEach())
                return null();
        }
    }

    if (pc->isAsync()) {
        bool matched;
        if (!tokenStream.matchToken(&matched, TOK_AWAIT))
            return null();

        if (matched) {
            iflags |= JSITER_FORAWAITOF;
            iterKind = IteratorKind::Async;
        }
    }

    if (!mustMatchToken(TOK_LP, JSMSG_PAREN_AFTER_FOR)) {
        return null();
    }

    // PNK_FORHEAD, PNK_FORIN, or PNK_FOROF depending on the loop type.
    ParseNodeKind headKind;

    // |x| in either |for (x; ...; ...)| or |for (x in/of ...)|.
    Node startNode;

    // The next two variables are used to implement `for (let/const ...)`.
    //
    // We generate an implicit block, wrapping the whole loop, to store loop
    // variables declared this way. Note that if the loop uses `for (var...)`
    // instead, those variables go on some existing enclosing scope, so no
    // implicit block scope is created.
    //
    // Both variables remain null/none if the loop is any other form.

    // The static block scope for the implicit block scope.
    Maybe<ParseContext::Scope> forLoopLexicalScope;

    // The expression being iterated over, for for-in/of loops only.  Unused
    // for for(;;) loops.
    Node iteratedExpr;

    // Parse the entirety of the loop-head for a for-in/of loop (so the next
    // token is the closing ')'):
    //
    //   for (... in/of ...) ...
    //                     ^next token
    //
    // ...OR, parse up to the first ';' in a C-style for-loop:
    //
    //   for (...; ...; ...) ...
    //           ^next token
    //
    // In either case the subsequent token can be consistently accessed using
    // TokenStream::None semantics.
    if (!forHeadStart(yieldHandling, iterKind, &headKind, &startNode, forLoopLexicalScope,
                      &iteratedExpr))
    {
        return null();
    }

    MOZ_ASSERT(headKind == PNK_FORIN || headKind == PNK_FOROF || headKind == PNK_FORHEAD);

    TernaryNodeType forHead;
    if (headKind == PNK_FORHEAD) {
        Node init = startNode;

        if (isForEach) {
            errorAt(begin, JSMSG_BAD_FOR_EACH_LOOP);
            return null();
        }

        // Look for an operand: |for (;| means we might have already examined
        // this semicolon with that modifier.
        if (!mustMatchToken(TOK_SEMI, TokenStream::Operand, JSMSG_SEMI_AFTER_FOR_INIT)) {
            return null();
        }

        TokenKind tt;
        if (!tokenStream.peekToken(&tt, TokenStream::Operand))
            return null();

        Node test;
        TokenStream::Modifier mod;
        if (tt == TOK_SEMI) {
            test = null();
            mod = TokenStream::Operand;
        } else {
            test = expr(InAllowed, yieldHandling, TripledotProhibited);
            if (!test)
                return null();
            mod = TokenStream::None;
        }

        if (!mustMatchToken(TOK_SEMI, mod, JSMSG_SEMI_AFTER_FOR_COND)) {
            return null();
        }

        if (!tokenStream.peekToken(&tt, TokenStream::Operand))
            return null();

        Node update;
        if (tt == TOK_RP) {
            update = null();
            mod = TokenStream::Operand;
        } else {
            update = expr(InAllowed, yieldHandling, TripledotProhibited);
            if (!update)
                return null();
            mod = TokenStream::None;
        }

        if (!mustMatchToken(TOK_RP, mod, JSMSG_PAREN_AFTER_FOR_CTRL)) {
            return null();
        }

        TokenPos headPos(begin, pos().end);
        forHead = handler.newForHead(init, test, update, headPos);
        if (!forHead)
            return null();
    } else {
        MOZ_ASSERT(headKind == PNK_FORIN || headKind == PNK_FOROF);

        // |target| is the LeftHandSideExpression or declaration to which the
        // per-iteration value (an arbitrary value exposed by the iteration
        // protocol, or a string naming a property) is assigned.
        Node target = startNode;

        // Parse the rest of the for-in/of head.
        if (headKind == PNK_FORIN) {
            stmt.refineForKind(StatementKind::ForInLoop);
            iflags |= JSITER_ENUMERATE;
        } else {
            if (isForEach) {
                errorAt(begin, JSMSG_BAD_FOR_EACH_LOOP);
                return null();
            }

            stmt.refineForKind(StatementKind::ForOfLoop);
        }

        // Parser::declaration consumed everything up to the closing ')'.  That
        // token follows an {Assignment,}Expression, so the next token must be
        // consumed as if an operator continued the expression, i.e. as None.
        if (!mustMatchToken(TOK_RP, TokenStream::None, JSMSG_PAREN_AFTER_FOR_CTRL)) {
            return null();
        }

        TokenPos headPos(begin, pos().end);
        forHead = handler.newForInOrOfHead(headKind, target, iteratedExpr, headPos);
        if (!forHead)
            return null();
    }

    Node body = statement(yieldHandling);
    if (!body)
        return null();

    BinaryNodeType forLoop = handler.newForStatement(begin, forHead, body, iflags);
    if (!forLoop)
        return null();

    if (forLoopLexicalScope)
        return finishLexicalScope(*forLoopLexicalScope, forLoop);

    return forLoop;
}

template <typename ParseHandler>
typename ParseHandler::SwitchStatementType
Parser<ParseHandler>::switchStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_SWITCH));
    uint32_t begin = pos().begin;

    if (!mustMatchToken(TOK_LP, JSMSG_PAREN_BEFORE_SWITCH)) {
        return null();
    }

    Node discriminant = exprInParens(InAllowed, yieldHandling, TripledotProhibited);
    if (!discriminant)
        return null();

    if (!mustMatchToken(TOK_RP, JSMSG_PAREN_AFTER_SWITCH)) {
        return null();
    }
    if (!mustMatchToken(TOK_LC, JSMSG_CURLY_BEFORE_SWITCH)) {
        return null();
    }

    ParseContext::Statement stmt(pc, StatementKind::Switch);
    ParseContext::Scope scope(this);
    if (!scope.init(pc))
        return null();

    ListNodeType caseList = handler.newStatementList(pos());
    if (!caseList)
        return null();

    bool seenDefault = false;
    TokenKind tt;
    while (true) {
        if (!tokenStream.getToken(&tt, TokenStream::Operand))
            return null();
        if (tt == TOK_RC)
            break;
        uint32_t caseBegin = pos().begin;

        Node caseExpr;
        switch (tt) {
          case TOK_DEFAULT:
            if (seenDefault) {
                error(JSMSG_TOO_MANY_DEFAULTS);
                return null();
            }
            seenDefault = true;
            caseExpr = null();  // The default case has pn_left == nullptr.
            break;

          case TOK_CASE:
            caseExpr = expr(InAllowed, yieldHandling, TripledotProhibited);
            if (!caseExpr)
                return null();
            break;

          default:
            error(JSMSG_BAD_SWITCH);
            return null();
        }

        if (!mustMatchToken(TOK_COLON, JSMSG_COLON_AFTER_CASE)) {
            return null();
        }

        ListNodeType body = handler.newStatementList(pos());
        if (!body)
            return null();

        bool afterReturn = false;
        bool warnedAboutStatementsAfterReturn = false;
        uint32_t statementBegin = 0;
        while (true) {
            if (!tokenStream.peekToken(&tt, TokenStream::Operand))
                return null();
            if (tt == TOK_RC || tt == TOK_CASE || tt == TOK_DEFAULT)
                break;
            if (afterReturn) {
                if (!tokenStream.peekOffset(&statementBegin, TokenStream::Operand))
                    return null();
            }
            Node stmt = statementListItem(yieldHandling);
            if (!stmt)
                return null();
            if (!warnedAboutStatementsAfterReturn) {
                if (afterReturn) {
                    if (!handler.isStatementPermittedAfterReturnStatement(stmt)) {
                        if (!warningAt(statementBegin, JSMSG_STMT_AFTER_RETURN))
                            return null();

                        warnedAboutStatementsAfterReturn = true;
                    }
                } else if (handler.isReturnStatement(stmt)) {
                    afterReturn = true;
                }
            }
            handler.addStatementToList(body, stmt);
        }

        CaseClauseType caseClause = handler.newCaseOrDefault(caseBegin, caseExpr, body);
        if (!caseClause)
            return null();
        handler.addCaseStatementToList(caseList, caseClause);
    }

    LexicalScopeNodeType lexicalForCaseList = finishLexicalScope(scope, caseList);
    if (!lexicalForCaseList)
        return null();

    handler.setEndPosition(lexicalForCaseList, pos().end);

    return handler.newSwitchStatement(begin, discriminant, lexicalForCaseList, seenDefault);
}

template <typename ParseHandler>
typename ParseHandler::ContinueStatementType
Parser<ParseHandler>::continueStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_CONTINUE));
    uint32_t begin = pos().begin;

    RootedPropertyName label(context);
    if (!matchLabel(yieldHandling, &label))
        return null();

    // Labeled 'continue' statements target the nearest labeled loop
    // statements with the same label. Unlabeled 'continue' statements target
    // the innermost loop statement.
    auto isLoop = [](ParseContext::Statement* stmt) {
        return StatementKindIsLoop(stmt->kind());
    };

    if (label) {
        ParseContext::Statement* stmt = pc->innermostStatement();
        bool foundLoop = false;

        for (;;) {
            stmt = ParseContext::Statement::findNearest(stmt, isLoop);
            if (!stmt) {
                if (foundLoop)
                    error(JSMSG_LABEL_NOT_FOUND);
                else
                    errorAt(begin, JSMSG_BAD_CONTINUE);
                return null();
            }

            foundLoop = true;

            // Is it labeled by our label?
            bool foundTarget = false;
            stmt = stmt->enclosing();
            while (stmt && stmt->is<ParseContext::LabelStatement>()) {
                if (stmt->as<ParseContext::LabelStatement>().label() == label) {
                    foundTarget = true;
                    break;
                }
                stmt = stmt->enclosing();
            }
            if (foundTarget)
                break;
        }
    } else if (!pc->findInnermostStatement(isLoop)) {
        error(JSMSG_BAD_CONTINUE);
        return null();
    }

    if (!matchOrInsertSemicolonAfterNonExpression())
        return null();

    return handler.newContinueStatement(label, TokenPos(begin, pos().end));
}

template <typename ParseHandler>
typename ParseHandler::BreakStatementType
Parser<ParseHandler>::breakStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_BREAK));
    uint32_t begin = pos().begin;

    RootedPropertyName label(context);
    if (!matchLabel(yieldHandling, &label))
        return null();

    // Labeled 'break' statements target the nearest labeled statements (could
    // be any kind) with the same label. Unlabeled 'break' statements target
    // the innermost loop or switch statement.
    if (label) {
        auto hasSameLabel = [&label](ParseContext::LabelStatement* stmt) {
            return stmt->label() == label;
        };

        if (!pc->findInnermostStatement<ParseContext::LabelStatement>(hasSameLabel)) {
            error(JSMSG_LABEL_NOT_FOUND);
            return null();
        }
    } else {
        auto isBreakTarget = [](ParseContext::Statement* stmt) {
            return StatementKindIsUnlabeledBreakTarget(stmt->kind());
        };

        if (!pc->findInnermostStatement(isBreakTarget)) {
            errorAt(begin, JSMSG_TOUGH_BREAK);
            return null();
        }
    }

    if (!matchOrInsertSemicolonAfterNonExpression())
        return null();

    return handler.newBreakStatement(label, TokenPos(begin, pos().end));
}

template <typename ParseHandler>
typename ParseHandler::UnaryNodeType
Parser<ParseHandler>::returnStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_RETURN));
    uint32_t begin = pos().begin;

    MOZ_ASSERT(pc->isFunctionBox());
    pc->functionBox()->usesReturn = true;

    // Parse an optional operand.
    //
    // This is ugly, but we don't want to require a semicolon.
    Node exprNode;
    TokenKind tt = TOK_EOF;
    if (!tokenStream.peekTokenSameLine(&tt, TokenStream::Operand))
        return null();
    switch (tt) {
      case TOK_EOL:
      case TOK_EOF:
      case TOK_SEMI:
      case TOK_RC:
        exprNode = null();
        pc->funHasReturnVoid = true;
        break;
      default: {
        exprNode = expr(InAllowed, yieldHandling, TripledotProhibited);
        if (!exprNode)
            return null();
        pc->funHasReturnExpr = true;
      }
    }

    if (exprNode) {
        if (!matchOrInsertSemicolonAfterExpression())
            return null();
    } else {
        if (!matchOrInsertSemicolonAfterNonExpression())
            return null();
    }

    UnaryNodeType pn = handler.newReturnStatement(exprNode, TokenPos(begin, pos().end));
    if (!pn)
        return null();

    /* Disallow "return v;" in legacy generators. */
    if (pc->isLegacyGenerator() && exprNode) {
        errorAt(begin, JSMSG_BAD_GENERATOR_RETURN);
        return null();
    }

    return pn;
}

template <typename ParseHandler>
typename ParseHandler::UnaryNodeType
Parser<ParseHandler>::yieldExpression(InHandling inHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_YIELD));
    uint32_t begin = pos().begin;

    switch (pc->generatorKind()) {
      case StarGenerator:
      {
        MOZ_ASSERT(pc->isFunctionBox());

        pc->lastYieldOffset = begin;

        Node exprNode;
        ParseNodeKind kind = PNK_YIELD;
        TokenKind tt = TOK_EOF;
        if (!tokenStream.peekTokenSameLine(&tt, TokenStream::Operand))
            return null();
        switch (tt) {
          // TOK_EOL is special; it implements the [no LineTerminator here]
          // quirk in the grammar.
          case TOK_EOL:
          // The rest of these make up the complete set of tokens that can
          // appear after any of the places where AssignmentExpression is used
          // throughout the grammar.  Conveniently, none of them can also be the
          // start an expression.
          case TOK_EOF:
          case TOK_SEMI:
          case TOK_RC:
          case TOK_RB:
          case TOK_RP:
          case TOK_COLON:
          case TOK_COMMA:
          case TOK_IN:
            // No value.
            exprNode = null();
            tokenStream.addModifierException(TokenStream::NoneIsOperand);
            break;
          case TOK_MUL:
            kind = PNK_YIELD_STAR;
            tokenStream.consumeKnownToken(TOK_MUL, TokenStream::Operand);
            MOZ_FALLTHROUGH;
          default:
            exprNode = assignExpr(inHandling, YieldIsKeyword, TripledotProhibited);
            if (!exprNode)
                return null();
        }
        if (kind == PNK_YIELD_STAR)
            return handler.newYieldStarExpression(begin, exprNode);
        return handler.newYieldExpression(begin, exprNode);
      }

      case NotGenerator:
        // We are in code that has not seen a yield, but we are in JS 1.7 or
        // later.  Try to transition to being a legacy generator.
        MOZ_ASSERT(tokenStream.versionNumber() >= JSVERSION_1_7);
        MOZ_ASSERT(pc->lastYieldOffset == ParseContext::NoYieldOffset);

        if (!abortIfSyntaxParser())
            return null();

        if (!pc->isFunctionBox()) {
            error(JSMSG_BAD_RETURN_OR_YIELD, js_yield_str);
            return null();
        }

        if (pc->functionBox()->isArrow()) {
            errorAt(begin, JSMSG_YIELD_IN_ARROW, js_yield_str);
            return null();
        }

        if (pc->functionBox()->function()->isMethod() ||
            pc->functionBox()->function()->isGetter() ||
            pc->functionBox()->function()->isSetter())
        {
            errorAt(begin, JSMSG_YIELD_IN_METHOD, js_yield_str);
            return null();
        }

        if (pc->funHasReturnExpr
#if JS_HAS_EXPR_CLOSURES
            || pc->functionBox()->isExprBody()
#endif
            )
        {
            /* As in Python (see PEP-255), disallow return v; in generators. */
            errorAt(begin, JSMSG_BAD_GENERATOR_RETURN);
            return null();
        }

        pc->functionBox()->setGeneratorKind(LegacyGenerator);

        MOZ_FALLTHROUGH;

      case LegacyGenerator:
      {
        // We are in a legacy generator: a function that has already seen a
        // yield.
        MOZ_ASSERT(pc->isFunctionBox());

        pc->lastYieldOffset = begin;

        // Legacy generators do not require a value.
        Node exprNode;
        TokenKind tt = TOK_EOF;
        if (!tokenStream.peekTokenSameLine(&tt, TokenStream::Operand))
            return null();
        switch (tt) {
          case TOK_EOF:
          case TOK_EOL:
          case TOK_SEMI:
          case TOK_RC:
          case TOK_RB:
          case TOK_RP:
          case TOK_COLON:
          case TOK_COMMA:
            // No value.
            exprNode = null();
            tokenStream.addModifierException(TokenStream::NoneIsOperand);
            break;
          default:
            exprNode = assignExpr(inHandling, YieldIsKeyword, TripledotProhibited);
            if (!exprNode)
                return null();
        }

        return handler.newYieldExpression(begin, exprNode);
      }
    }

    MOZ_CRASH("yieldExpr");
}

template <typename ParseHandler>
typename ParseHandler::BinaryNodeType
Parser<ParseHandler>::withStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_WITH));
    uint32_t begin = pos().begin;

    // Usually we want the constructs forbidden in strict mode code to be a
    // subset of those that ContextOptions::extraWarnings() warns about, and we
    // use strictModeError directly.  But while 'with' is forbidden in strict
    // mode code, it doesn't even merit a warning in non-strict code.  See
    // https://bugzilla.mozilla.org/show_bug.cgi?id=514576#c1.
    if (pc->sc()->strict()) {
        if (!strictModeError(JSMSG_STRICT_CODE_WITH))
            return null();
    }

    if (!mustMatchToken(TOK_LP, JSMSG_PAREN_BEFORE_WITH)) {
        return null();
    }
    Node objectExpr = exprInParens(InAllowed, yieldHandling, TripledotProhibited);
    if (!objectExpr)
        return null();
    if (!mustMatchToken(TOK_RP, JSMSG_PAREN_AFTER_WITH)) {
        return null();
    }

    Node innerBlock;
    {
        ParseContext::Statement stmt(pc, StatementKind::With);
        innerBlock = statement(yieldHandling);
        if (!innerBlock)
            return null();
    }

    pc->sc()->setBindingsAccessedDynamically();

    return handler.newWithStatement(begin, objectExpr, innerBlock);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::labeledItem(YieldHandling yieldHandling)
{
    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();

    if (tt == TOK_FUNCTION) {
        TokenKind next;
        if (!tokenStream.peekToken(&next))
            return null();

        // GeneratorDeclaration is only matched by HoistableDeclaration in
        // StatementListItem, so generators can't be inside labels.
        if (next == TOK_MUL) {
            error(JSMSG_GENERATOR_LABEL);
            return null();
        }

        // Per 13.13.1 it's a syntax error if LabelledItem: FunctionDeclaration
        // is ever matched.  Per Annex B.3.2 that modifies this text, this
        // applies only to strict mode code.
        if (pc->sc()->strict()) {
            error(JSMSG_FUNCTION_LABEL);
            return null();
        }

        return functionStmt(pos().begin, yieldHandling, NameRequired);
    }

    tokenStream.ungetToken();
    return statement(yieldHandling);
}

template <typename ParseHandler>
typename ParseHandler::LabeledStatementType
Parser<ParseHandler>::labeledStatement(YieldHandling yieldHandling)
{
    RootedPropertyName label(context, labelIdentifier(yieldHandling));
    if (!label)
        return null();

    auto hasSameLabel = [&label](ParseContext::LabelStatement* stmt) {
        return stmt->label() == label;
    };

    uint32_t begin = pos().begin;

    if (pc->findInnermostStatement<ParseContext::LabelStatement>(hasSameLabel)) {
        errorAt(begin, JSMSG_DUPLICATE_LABEL);
        return null();
    }

    tokenStream.consumeKnownToken(TOK_COLON);

    /* Push a label struct and parse the statement. */
    ParseContext::LabelStatement stmt(pc, label);
    Node pn = labeledItem(yieldHandling);
    if (!pn)
        return null();

    return handler.newLabeledStatement(label, pn, begin);
}

template <typename ParseHandler>
typename ParseHandler::UnaryNodeType
Parser<ParseHandler>::throwStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_THROW));
    uint32_t begin = pos().begin;

    /* ECMA-262 Edition 3 says 'throw [no LineTerminator here] Expr'. */
    TokenKind tt = TOK_EOF;
    if (!tokenStream.peekTokenSameLine(&tt, TokenStream::Operand))
        return null();
    if (tt == TOK_EOF || tt == TOK_SEMI || tt == TOK_RC) {
        error(JSMSG_MISSING_EXPR_AFTER_THROW);
        return null();
    }
    if (tt == TOK_EOL) {
        error(JSMSG_LINE_BREAK_AFTER_THROW);
        return null();
    }

    Node throwExpr = expr(InAllowed, yieldHandling, TripledotProhibited);
    if (!throwExpr)
        return null();

    if (!matchOrInsertSemicolonAfterExpression())
        return null();

    return handler.newThrowStatement(throwExpr, TokenPos(begin, pos().end));
}

template <typename ParseHandler>
typename ParseHandler::TernaryNodeType
Parser<ParseHandler>::tryStatement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_TRY));
    uint32_t begin = pos().begin;

    /*
     * try nodes are ternary.
     * kid1 is the try statement
     * kid2 is the catch node list or null
     * kid3 is the finally statement
     *
     * catch nodes are ternary.
     * kid1 is the lvalue (possible identifier, TOK_LB, or TOK_LC)
     * kid2 is the catch guard or null if no guard
     * kid3 is the catch block
     *
     * catch lvalue nodes are either:
     *   a single identifier
     *   TOK_RB or TOK_RC for a destructuring left-hand side
     *
     * finally nodes are TOK_LC statement lists.
     */

    LexicalScopeNodeType innerBlock;
    {
        if (!mustMatchToken(TOK_LC, JSMSG_CURLY_BEFORE_TRY)) {
            return null();
        }

        uint32_t openedPos = pos().begin;

        ParseContext::Statement stmt(pc, StatementKind::Try);
        ParseContext::Scope scope(this);
        if (!scope.init(pc))
            return null();

        ListNodeType list = statementList(yieldHandling);
        if (!list)
            return null();

        innerBlock = finishLexicalScope(scope, list);
        if (!innerBlock)
            return null();

        if (!mustMatchToken(TOK_RC, TokenStream::Operand,
                            [this, openedPos](TokenKind actual) {
                                this->reportMissingClosing(JSMSG_CURLY_AFTER_TRY, 
                                                           JSMSG_CURLY_OPENED, openedPos);
                            }))
        {
            return null();
        }
    }

    bool hasUnconditionalCatch = false;
    ListNodeType catchList = null();
    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return null();
    if (tt == TOK_CATCH) {
        catchList = handler.newCatchList();
        if (!catchList)
            return null();

        do {
            /* Check for another catch after unconditional catch. */
            if (hasUnconditionalCatch) {
                error(JSMSG_CATCH_AFTER_GENERAL);
                return null();
            }

            /*
             * Create a lexical scope node around the whole catch clause,
             * including the head.
             */
            ParseContext::Statement stmt(pc, StatementKind::Catch);
            ParseContext::Scope scope(this);
            if (!scope.init(pc))
                return null();

            /*
             * Legal catch forms are:
             *   catch (lhs)
             *   catch (lhs if <boolean_expression>)  ** non-standard **
             *   catch                                ** ES2019 **
             * where lhs is a name or a destructuring left-hand side.
             * The second is legal only #ifdef JS_HAS_CATCH_GUARD
             */
            bool omittedBinding;
            if (!tokenStream.matchToken(&omittedBinding, TOK_LC))
                return null();

            Node catchName;
            Node catchGuard = null();

            if (omittedBinding) {
                catchName = null();
            } else {
                if (!mustMatchToken(TOK_LP, JSMSG_PAREN_BEFORE_CATCH)) {
                    return null();
                }

                if (!tokenStream.getToken(&tt))
                    return null();
                switch (tt) {
                  case TOK_LB:
                  case TOK_LC:
                    catchName = destructuringDeclaration(DeclarationKind::CatchParameter,
                                                     yieldHandling, tt);
                    if (!catchName)
                        return null();
                    break;

                  default: {
                    if (!TokenKindIsPossibleIdentifierName(tt)) {
                        error(JSMSG_CATCH_IDENTIFIER);
                        return null();
                    }

                    catchName = bindingIdentifier(DeclarationKind::SimpleCatchParameter,
                                                  yieldHandling);
                    if (!catchName)
                        return null();
                    break;
                  }
                }

#if JS_HAS_CATCH_GUARD
                /*
                 * We use 'catch (x if x === 5)' (not 'catch (x : x === 5)')
                 * to avoid conflicting with the JS2/ECMAv4 type annotation
                 * catchguard syntax.
                 */
                bool matched;
                if (!tokenStream.matchToken(&matched, TOK_IF))
                    return null();
                if (matched) {
                    catchGuard = expr(InAllowed, yieldHandling, TripledotProhibited);
                    if (!catchGuard)
                        return null();
                }
#endif
                if (!mustMatchToken(TOK_RP, JSMSG_PAREN_AFTER_CATCH)) {
                    return null();
                }

                if (!mustMatchToken(TOK_LC, JSMSG_CURLY_BEFORE_CATCH)) {
                    return null();
                }
            }

            LexicalScopeNodeType catchBody = catchBlockStatement(yieldHandling, scope);
            if (!catchBody)
                return null();

            if (!catchGuard)
                hasUnconditionalCatch = true;

            LexicalScopeNodeType catchScope = finishLexicalScope(scope, catchBody);
            if (!catchScope)
                return null();

            if (!handler.addCatchBlock(catchList, catchScope, catchName, catchGuard, catchBody))
                return null();
            handler.setEndPosition(catchList, pos().end);
            handler.setEndPosition(catchScope, pos().end);

            if (!tokenStream.getToken(&tt, TokenStream::Operand))
                return null();
        } while (tt == TOK_CATCH);
    }

    LexicalScopeNodeType finallyBlock = null();

    if (tt == TOK_FINALLY) {
        if (!mustMatchToken(TOK_LC, JSMSG_CURLY_BEFORE_FINALLY)) {
            return null();
        }

        uint32_t openedPos = pos().begin;

        ParseContext::Statement stmt(pc, StatementKind::Finally);
        ParseContext::Scope scope(this);
        if (!scope.init(pc))
            return null();

        ListNodeType list = statementList(yieldHandling);
        if (!list)
            return null();

        finallyBlock = finishLexicalScope(scope, list);
        if (!finallyBlock)
            return null();

        if (!mustMatchToken(TOK_RC, TokenStream::Operand,
                            [this, openedPos](TokenKind actual) {
                                this->reportMissingClosing(JSMSG_CURLY_AFTER_FINALLY,
                                                           JSMSG_CURLY_OPENED, openedPos);
                            }))
        {
            return null();
        }
                                                            
    } else {
        tokenStream.ungetToken();
    }
    if (!catchList && !finallyBlock) {
        error(JSMSG_CATCH_OR_FINALLY);
        return null();
    }

    return handler.newTryStatement(begin, innerBlock, catchList, finallyBlock);
}

template <typename ParseHandler>
typename ParseHandler::LexicalScopeNodeType
Parser<ParseHandler>::catchBlockStatement(YieldHandling yieldHandling,
                                          ParseContext::Scope& catchParamScope)
{
    uint32_t openedPos = pos().begin;

    ParseContext::Statement stmt(pc, StatementKind::Block);

    // ES 13.15.7 CatchClauseEvaluation
    //
    // Step 8 means that the body of a catch block always has an additional
    // lexical scope.
    ParseContext::Scope scope(this);
    if (!scope.init(pc))
        return null();

    // The catch parameter names cannot be redeclared inside the catch
    // block, so declare the name in the inner scope.
    if (!scope.addCatchParameters(pc, catchParamScope))
        return null();

    ListNodeType list = statementList(yieldHandling);
    if (!list)
        return null();

    if (!mustMatchToken(TOK_RC, TokenStream::Operand,
                        [this, openedPos](TokenKind actual) {
                            this->reportMissingClosing(JSMSG_CURLY_AFTER_CATCH,
                                                       JSMSG_CURLY_OPENED, openedPos);
                        }))
    {
        return null();
    }

    // The catch parameter names are not bound in the body scope, so remove
    // them before generating bindings.
    scope.removeCatchParameters(pc, catchParamScope);
    return finishLexicalScope(scope, list);
}

template <typename ParseHandler>
typename ParseHandler::DebuggerStatementType
Parser<ParseHandler>::debuggerStatement()
{
    TokenPos p;
    p.begin = pos().begin;
    if (!matchOrInsertSemicolonAfterNonExpression())
        return null();
    p.end = pos().end;

    pc->sc()->setBindingsAccessedDynamically();
    pc->sc()->setHasDebuggerStatement();

    return handler.newDebuggerStatement(p);
}

static JSOp
JSOpFromPropertyType(PropertyType propType)
{
    switch (propType) {
      case PropertyType::Getter:
      case PropertyType::GetterNoExpressionClosure:
        return JSOP_INITPROP_GETTER;
      case PropertyType::Setter:
      case PropertyType::SetterNoExpressionClosure:
        return JSOP_INITPROP_SETTER;
      case PropertyType::Normal:
      case PropertyType::Method:
      case PropertyType::GeneratorMethod:
      case PropertyType::AsyncMethod:
      case PropertyType::AsyncGeneratorMethod:
      case PropertyType::Constructor:
      case PropertyType::DerivedConstructor:
        return JSOP_INITPROP;
      default:
        MOZ_CRASH("unexpected property type");
    }
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::classMember(YieldHandling yieldHandling,
                                  const ParseContext::ClassStatement& classStmt,
                                  HandlePropertyName className,
                                  uint32_t classStartOffset, bool hasHeritage,
                                  ClassFields& classFields,
                                  ListNodeType& classMembers, bool* done)
{
    *done = false;

    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return false;

    if (tt == TOK_RC) {
        *done = true;
        return true;
    }

    if (tt == TOK_SEMI)
        return true;

    bool isStatic = false;
    if (tt == TOK_STATIC) {
        if (!tokenStream.peekToken(&tt))
            return false;

        if (tt == TOK_RC) {
            tokenStream.consumeKnownToken(tt);
            error(JSMSG_UNEXPECTED_TOKEN, "property name", TokenKindToDesc(tt));
            return false;
        }

        if (tt == TOK_LC) {
            /* Parsing static class block: static { ... } */
            FunctionNodeType staticBlockBody = staticClassBlock(classFields);
            if (!staticBlockBody)
                return false;

            StaticClassBlockType classBlock = handler.newStaticClassBlock(staticBlockBody);
            if (!classBlock)
                return false;

            return handler.addClassMemberDefinition(classMembers, classBlock);
        }

        if (tt != TOK_LP) {
            isStatic = true;
        } else {
            tokenStream.ungetToken();
        }
    } else {
        tokenStream.ungetToken();
    }

    uint32_t propNameOffset;
    if (!tokenStream.peekOffset(&propNameOffset))
        return false;
        
    RootedAtom propAtom(context);
    PropertyType propType;
    Node propName = propertyOrMethodName(yieldHandling, PropertyNameInClass,
                                         /* maybeDecl = */ Nothing(),
                                         classMembers, &propType, &propAtom);
    if (!propName)
        return false;

    if (propType == PropertyType::Field) {
        if (isStatic) {
            if (propAtom == context->names().prototype) {
                errorAt(propNameOffset, JSMSG_BAD_METHOD_DEF);
                return false;
            }
        }

        if (propAtom == context->names().constructor) {
          errorAt(propNameOffset, JSMSG_BAD_METHOD_DEF);
          return false;
        }

        if (!abortIfSyntaxParser())
            return false;

        if (isStatic) {
            classFields.staticFields++;
        } else {
            classFields.instanceFields++;
        }

        FunctionNodeType initializer = fieldInitializerOpt(propAtom, classFields, isStatic);
        if (!initializer)
            return false;

        if (!matchOrInsertSemicolonAfterExpression()) {
            return false;
        }

        ClassFieldType field = handler.newClassFieldDefinition(propName, initializer, isStatic);
        if (!field)
            return false;

        return handler.addClassMemberDefinition(classMembers, field);
    }

    if (propType != PropertyType::Getter && propType != PropertyType::Setter &&
        propType != PropertyType::Method && propType != PropertyType::GeneratorMethod &&
        propType != PropertyType::AsyncMethod && propType != PropertyType::AsyncGeneratorMethod &&
        propType != PropertyType::Constructor && propType != PropertyType::DerivedConstructor)
    {
        errorAt(propNameOffset, JSMSG_BAD_METHOD_DEF);
        return false;
    }

    if (propType == PropertyType::Getter)
        propType = PropertyType::GetterNoExpressionClosure;
    if (propType == PropertyType::Setter)
        propType = PropertyType::SetterNoExpressionClosure;

    bool isConstructor = !isStatic && propAtom == context->names().constructor;
    if (isConstructor) {
        if (propType != PropertyType::Method) {
            errorAt(propNameOffset, JSMSG_BAD_METHOD_DEF);
            return false;
        }
        if (classStmt.constructorBox) {
            errorAt(propNameOffset, JSMSG_DUPLICATE_PROPERTY, "constructor");
            return false;
        }
        propType = hasHeritage ? PropertyType::DerivedConstructor
                               : PropertyType::Constructor;
    } else if (isStatic && propAtom == context->names().prototype) {
        errorAt(propNameOffset, JSMSG_BAD_METHOD_DEF);
        return false;
    }

    RootedAtom funName(context);
    switch (propType) {
      case PropertyType::GetterNoExpressionClosure:
      case PropertyType::SetterNoExpressionClosure:
        if (!tokenStream.isCurrentTokenType(TOK_RB)) {
            funName = prefixAccessorName(propType, propAtom);
            if (!funName)
                return false;
        }
        break;
      case PropertyType::Constructor:
      case PropertyType::DerivedConstructor:
        funName = className;
        break;
      default:
        if (!tokenStream.isCurrentTokenType(TOK_RB))
            funName = propAtom;
    }

    // When |super()| is invoked, we search for the nearest scope containing
    // |.initializers| to initialize the class fields. This set-up precludes
    // declaring |.initializers| in the class scope, because in some syntactic
    // contexts |super()| can appear nested in a class, while actually belonging
    // to an outer class definition.
    //
    // Example:
    // class Outer extends Base {
    //   field = 1;
    //   constructor() {
    //     class Inner {
    //       field = 2;
    //
    //       // The super() call in the computed property name mustn't access
    //       // Inner's |.initializers| array, but instead Outer's.
    //       [super()]() {}
    //     }
    //   }
    // }
    Maybe<ParseContext::Scope> dotInitializersScope;
    if (isConstructor && !options().selfHostingMode) {
        dotInitializersScope.emplace(this);
        if (!dotInitializersScope->init(pc))
            return false;

        if (!noteDeclaredName(context->names().dotInitializers, DeclarationKind::Let, pos()))
            return false;
    }


    // Calling toString on constructors need to return the source text for
    // the entire class. The end offset is unknown at this point in
    // parsing and will be amended when class parsing finishes below.
    FunctionNodeType funNode = methodDefinition(isConstructor ? classStartOffset : propNameOffset,
                                                propType, funName);
    if (!funNode)
        return false;

    handler.checkAndSetIsDirectRHSAnonFunction(funNode);

    JSOp op = JSOpFromPropertyType(propType);
    Node method = handler.newClassMethodDefinition(propName, funNode, op, isStatic);
    if (!method)
        return false;

    if (dotInitializersScope.isSome()) {
        method = finishLexicalScope(*dotInitializersScope, method);
        if (!method)
            return false;
        dotInitializersScope.reset();
    }

    return handler.addClassMemberDefinition(classMembers, method);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::finishClassConstructor(const ParseContext::ClassStatement& classStmt,
                                             HandlePropertyName className, bool hasHeritage,
                                             uint32_t classStartOffset, uint32_t classEndOffset,
                                             const ClassFields& classFields, ListNodeType& classMembers)
{
    // Fields cannot re-use the constructor obtained via JSOP_CLASSCONSTRUCTOR or
    // JSOP_DERIVEDCONSTRUCTOR due to needing to emit calls to the field
    // initializers in the constructor. So, synthesize a new one.
    size_t numFields = classFields.instanceFields;
    if (classStmt.constructorBox == nullptr && numFields > 0) {
        MOZ_ASSERT(!options().selfHostingMode);
        // Unconditionally create the scope here, because it's always the
        // constructor.
        ParseContext::Scope dotInitializersScope(this);
        if (!dotInitializersScope.init(pc))
            return false;

        if (!noteDeclaredName(context->names().dotInitializers, DeclarationKind::Let, pos()))
            return false;

        // synthesizeConstructor assigns to classStmt.constructorBox
        FunctionNodeType synthesizedCtor = synthesizeConstructor(className, classStartOffset, hasHeritage);
        if (!synthesizedCtor) {
            return false;
        }

        MOZ_ASSERT(classStmt.constructorBox != nullptr);

        // Note: the *function* has the name of the class, but the *property*
        // containing the function has the name "constructor"
        Node constructorNameNode = handler.newObjectLiteralPropertyName(context->names().constructor, pos());
        if (!constructorNameNode) {
            return false;
        }

        ClassMethodType method = handler.newClassMethodDefinition(constructorNameNode, synthesizedCtor,
                                                                  JSOP_INITPROP, /* isStatic = */ false);
        if (!method)
            return false;

        LexicalScopeNodeType scope = finishLexicalScope(dotInitializersScope, method);

        if (!handler.addClassMemberDefinition(classMembers, scope)) {
            return false;
        }
    }

    if (FunctionBox* ctorbox = classStmt.constructorBox) {
        // Amend the toStringEnd offset for the constructor now that we've
        // finished parsing the class.
        ctorbox->toStringEnd = classEndOffset;

        if (numFields > 0) {
            // Field initialization need access to `this`.
            ctorbox->setHasThisBinding();
        }

        // Set the same information, but on the lazyScript.
        if (ctorbox->function()->isInterpretedLazy()) {
            ctorbox->function()->lazyScript()->setToStringEnd(classEndOffset);

            if (numFields > 0) {
                ctorbox->function()->lazyScript()->setHasThisBinding();
            }
        }
    }

    return true;
}

template <typename ParseHandler>
typename ParseHandler::ClassNodeType
Parser<ParseHandler>::classDefinition(YieldHandling yieldHandling,
                                      ClassContext classContext,
                                      DefaultHandling defaultHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_CLASS));

    uint32_t classStartOffset = pos().begin;
    bool savedStrictness = setLocalStrictMode(true);

    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return null();

    RootedPropertyName className(context);
    if (TokenKindIsPossibleIdentifier(tt)) {
        className = bindingIdentifier(yieldHandling);
        if (!className)
            return null();
    } else if (classContext == ClassStatement) {
        if (defaultHandling == AllowDefaultName) {
            className = context->names().starDefaultStar;
            tokenStream.ungetToken();
        } else {
            // Class statements must have a bound name
            error(JSMSG_UNNAMED_CLASS_STMT);
            return null();
        }
    } else {
        // Make sure to put it back, whatever it was
        tokenStream.ungetToken();
    }

    // Because the binding definitions keep track of their blockId, we need to
    // create at least the inner binding later. Keep track of the name's position
    // in order to provide it for the nodes created later.
    TokenPos namePos = pos();

    // Push a ParseContext::ClassStatement to keep track of the constructor
    // funbox.
    ParseContext::ClassStatement classStmt(pc);

    NameNodeType innerName;
    Node nameNode = null();
    Node classHeritage = null();
    LexicalScopeNodeType classBlock = null();
    uint32_t classEndOffset;
    {
        // A named class creates a new lexical scope with a const binding of the
        // class name for the "inner name".
        ParseContext::Statement innerScopeStmt(pc, StatementKind::Block);
        ParseContext::Scope innerScope(this);
        if (!innerScope.init(pc))
            return null();

        bool hasHeritage;
        if (!tokenStream.matchToken(&hasHeritage, TOK_EXTENDS))
            return null();
        if (hasHeritage) {
            if (!tokenStream.getToken(&tt))
                return null();
            classHeritage = optionalExpr(yieldHandling, TripledotProhibited, tt);
            if (!classHeritage)
                return null();
        }

        if (!mustMatchToken(TOK_LC, JSMSG_CURLY_BEFORE_CLASS)) {
            return null();
        }

        ListNodeType classMembers = handler.newClassMemberList(pos().begin);
        if (!classMembers)
            return null();

        ClassFields classFields{};
        for (;;) {
            bool done;
            if (!classMember(yieldHandling, classStmt, className, classStartOffset, hasHeritage,
                             classFields, classMembers, &done))
                return null();
            if (done)
                break;
        }

        if (classFields.instanceFieldKeys > 0) {
            if (!noteDeclaredName(context->names().dotFieldKeys, DeclarationKind::Let, namePos))
                return null();
        }

        if (classFields.staticFields > 0) {
            if (!noteDeclaredName(context->names().dotStaticInitializers,
                                  DeclarationKind::Let, namePos))
                return null();
        }

        if (classFields.staticFieldKeys > 0) {
            if (!noteDeclaredName(context->names().dotStaticFieldKeys,
                                  DeclarationKind::Let, namePos))
                return null();
        }

        classEndOffset = pos().end;
        if (!finishClassConstructor(classStmt, className, hasHeritage,
                                    classStartOffset, classEndOffset, classFields, classMembers))
            return null();

        if (className) {
            // The inner name is immutable.
            if (!noteDeclaredName(className, DeclarationKind::Const, namePos))
                return null();

            innerName = newName(className, namePos);
            if (!innerName)
                return null();
        }

        classBlock = finishLexicalScope(innerScope, classMembers);
        if (!classBlock)
            return null();

        // Pop the inner scope.
    }

    if (className) {
        NameNodeType outerName = null();
        if (classContext == ClassStatement) {
            // The outer name is mutable.
            if (!noteDeclaredName(className, DeclarationKind::Let, namePos))
                return null();

            outerName = newName(className, namePos);
            if (!outerName)
                return null();
        }

        nameNode = handler.newClassNames(outerName, innerName, namePos);
        if (!nameNode)
            return null();
    }

    MOZ_ALWAYS_TRUE(setLocalStrictMode(savedStrictness));

    return handler.newClass(nameNode, classHeritage, classBlock,
                            TokenPos(classStartOffset, classEndOffset));
}

template <class ParseHandler>
typename ParseHandler::FunctionNodeType
Parser<ParseHandler>::synthesizeConstructor(HandleAtom className, uint32_t classNameOffset, bool hasHeritage)
{
    FunctionSyntaxKind functionSyntaxKind = hasHeritage ? FunctionSyntaxKind::DerivedClassConstructor
                                                        : FunctionSyntaxKind::ClassConstructor;

    // Create the function object.
    RootedFunction fun(context, newFunction(className, functionSyntaxKind,
                                            GeneratorKind::NotGenerator,
                                            FunctionAsyncKind::SyncFunction));
    if (!fun)
        return null();

    // Create the top-level field initializer node.
    FunctionNodeType funNode = handler.newFunction(functionSyntaxKind, pos());
    if (!funNode)
        return null();

    // Create the FunctionBox and link it to the function object.
    Directives directives(true);
    FunctionBox* funbox = newFunctionBox(funNode, fun, classNameOffset,
                                         directives, GeneratorKind::NotGenerator,
                                         FunctionAsyncKind::SyncFunction, false);
    if (!funbox)
        return null();
    funbox->initWithEnclosingParseContext(pc, functionSyntaxKind);
    funbox->setEnd(pos().end);

    // Push a ParseContext on to the stack.
    ParseContext funpc(this, funbox, /* newDirectives = */ nullptr);
    if (!funpc.init())
        return null();

    TokenPos synthesizedBodyPos = TokenPos(classNameOffset, classNameOffset + 1);
    // Create a ListNode for the parameters + body (there are no parameters).
    ListNodeType argsbody = handler.newList(PNK_PARAMSBODY, synthesizedBodyPos);
    if (!argsbody)
        return null();
    handler.setFunctionFormalParametersAndBody(funNode, argsbody);
    funbox->setStart(tokenStream);

    if (hasHeritage) {
        // Synthesize the equivalent to `function f(...args)`
        funbox->setHasRest();
        if (!notePositionalFormalParameter(funNode, context->names().args,
                                           synthesizedBodyPos.begin,
                                           /* disallowDuplicateParams = */ false,
                                           /* duplicatedParam = */ nullptr)) {
          return null();
        }
        funbox->function()->setArgCount(1);
    } else {
        funbox->function()->setArgCount(0);
    }

    pc->functionScope().useAsVarScope(pc);

    auto stmtList = handler.newStatementList(synthesizedBodyPos);
    if (!stmtList)
        return null();

    if (!noteUsedName(context->names().dotThis))
        return null();

    if (!noteUsedName(context->names().dotInitializers))
        return null();

    bool canSkipLazyClosedOverBindings = handler.canSkipLazyClosedOverBindings();
    if (!declareFunctionThis(canSkipLazyClosedOverBindings))
        return null();

    if (hasHeritage) {
        // {Goanna} Need a different this-NameNode for SuperBase and SetThis or the recycling
        //          by ParseNodeAllocator runs into all sorts of problems because the
        //          same ParseNode gets cleaned up twice.
        //          Parser<ParseHandler>::memberExpr does the same.
        NameNodeType thisNameBase = newThisName();
        if (!thisNameBase)
            return null();

        UnaryNodeType superBase = handler.newSuperBase(thisNameBase, synthesizedBodyPos);
        if (!superBase)
            return null();

        ListNodeType arguments = handler.newArguments(synthesizedBodyPos);
        if (!arguments)
            return null();

        NameNodeType argsNameNode = newName(context->names().args, synthesizedBodyPos);
        if (!argsNameNode)
            return null();
        if (!noteUsedName(context->names().args))
            return null();

        UnaryNodeType spreadArgs = handler.newSpread(synthesizedBodyPos.begin, argsNameNode);
        if (!spreadArgs)
            return null();
        handler.addList(arguments, spreadArgs);

        BinaryNodeType superCall = handler.newSuperCall(superBase, arguments, /* isSpread = */ true);
        if (!superCall)
            return null();

        NameNodeType thisName = newThisName();
        if (!thisName)
            return null();

        BinaryNodeType setThis = handler.newSetThis(thisName, superCall);
        if (!setThis)
            return null();

        UnaryNodeType exprStatement = handler.newExprStatement(setThis, synthesizedBodyPos.end);
        if (!exprStatement)
            return null();

        handler.addStatementToList(stmtList, exprStatement);
    }

    auto initializerBody = finishLexicalScope(pc->varScope(), stmtList);
    if (!initializerBody)
        return null();
    handler.setBeginPosition(initializerBody, stmtList);
    handler.setEndPosition(initializerBody, stmtList);

    handler.setFunctionBody(funNode, initializerBody);

    if (!finishFunction())
        return null();

    // This function is asserted to set classStmt->constructorBox - however, it's
    // not directly set in this function, but rather in
    // initWithEnclosingParseContext.

    return funNode;
}

template <class ParseHandler>
typename ParseHandler::FunctionNodeType
Parser<ParseHandler>::staticClassBlock(ClassFields& classFields)
{
    // Both for getting-this-done, and because this will invariably be executed,
    // syntax parsing should be aborted.
    if (!abortIfSyntaxParser())
        return null();

    TokenPos firstTokenPos(pos());

    // Create the anonymous function object.
    FunctionSyntaxKind syntaxKind = FunctionSyntaxKind::StaticClassBlock;
    AutoAwaitIsKeyword<ParseHandler> awaitIsKeyword(this, AwaitIsDisallowed);

    RootedFunction fun(context,
                       newFunction(nullptr, syntaxKind,
                                   GeneratorKind::NotGenerator,
                                   FunctionAsyncKind::SyncFunction));
    if (!fun)
        return null();

    // Create the function node for the static class body.
    FunctionNodeType funNode = handler.newFunction(syntaxKind, firstTokenPos);
    if (!funNode)
        return null();

    // Create the FunctionBox and link it to the function object.
    Directives directives(true);
    FunctionBox* funbox = newFunctionBox(funNode, fun, firstTokenPos.begin, directives,
                                         GeneratorKind::NotGenerator,
                                         FunctionAsyncKind::SyncFunction, false);
    if (!funbox)
        return null();
    funbox->initWithEnclosingParseContext(pc, syntaxKind);
    MOZ_ASSERT(!funbox->allowSuperCall());
    MOZ_ASSERT(!funbox->allowArguments());
    MOZ_ASSERT(!funbox->allowReturn());

    // Set start at `static` token.
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_STATIC));
    funbox->setStart(tokenStream, firstTokenPos);

    // Push a SourceParseContext on to the stack.
    ParseContext* outerpc = pc;
    ParseContext funpc(this, funbox, /* newDirectives = */ nullptr);
    if (!funpc.init())
        return null();

    pc->functionScope().useAsVarScope(pc);

    uint32_t start = firstTokenPos.begin;

    tokenStream.consumeKnownToken(TOK_LC);

    // Static class blocks are code-generated as if they were static field
    // initializers, so we bump the staticFields count here, which ensures
    // .staticInitializers is noted as used.
    classFields.staticFields++;

    LexicalScopeNodeType body = functionBody(InAllowed, YieldIsKeyword, syntaxKind,
                                             StatementListBody);
    if (!body)
        return null();

    if (tokenStream.isEOF()) {
        error(JSMSG_UNTERMINATED_STATIC_CLASS_BLOCK);
        return null();
    }

    tokenStream.consumeKnownToken(TOK_RC, TokenStream::Operand);

    TokenPos wholeBodyPos(start, pos().end);

    handler.setEndPosition(funNode, wholeBodyPos.end);
    funbox->setEnd(pos().end);

    // Create a ListNode for the parameters + body (there are no parameters).
    ListNodeType argsbody = handler.newList(PNK_PARAMSBODY, wholeBodyPos);
    if (!argsbody)
        return null();

    handler.setFunctionFormalParametersAndBody(funNode, argsbody);
    funbox->function()->setArgCount(0);

    if (pc->superScopeNeedsHomeObject()) {
        funbox->setNeedsHomeObject();
    }

    handler.setEndPosition(body, pos().begin);
    handler.setEndPosition(funNode, pos().end);
    handler.setFunctionBody(funNode, body);

    if (!finishFunction())
        return null();

    if (!leaveInnerFunction(outerpc))
        return null();

    return funNode;
}

template <class ParseHandler>
typename ParseHandler::FunctionNodeType
Parser<ParseHandler>::fieldInitializerOpt(HandleAtom propAtom, ClassFields& classFields, bool isStatic)
{
    bool hasInitializer = false;
    if (!tokenStream.matchToken(&hasInitializer, TOK_ASSIGN))
      return null();

    TokenPos firstTokenPos;
    if (hasInitializer) {
        firstTokenPos = pos();
    } else {
        // the location of the "initializer" should be a zero-width span:
        // class C {
        //   x /* here */ ;
        // }
        uint32_t endPos = pos().end;
        firstTokenPos = TokenPos(endPos, endPos);
    }

    // Create the anonymous function object.
    FunctionSyntaxKind syntaxKind = FunctionSyntaxKind::FieldInitializer;
    RootedFunction fun(context,
                       newFunction(nullptr, syntaxKind,
                                   GeneratorKind::NotGenerator,
                                   FunctionAsyncKind::SyncFunction));
    if (!fun)
        return null();

    // Create the top-level field initializer node.
    FunctionNodeType funNode = handler.newFunction(syntaxKind, firstTokenPos);
    if (!funNode)
        return null();

    // Create the FunctionBox and link it to the function object.
    Directives directives(true);
    FunctionBox* funbox = newFunctionBox(funNode, fun, firstTokenPos.begin, directives,
                                         GeneratorKind::NotGenerator,
                                         FunctionAsyncKind::SyncFunction, false);
    if (!funbox)
        return null();
    funbox->initWithEnclosingParseContext(pc, syntaxKind);
    MOZ_ASSERT(funbox->isFieldInitializer());
    funbox->setStart(tokenStream, firstTokenPos);

    // Push a SourceParseContext on to the stack.
    ParseContext* outerpc = pc;
    ParseContext funpc(this, funbox, /* newDirectives = */ nullptr);
    if (!funpc.init())
        return null();

    pc->functionScope().useAsVarScope(pc);

    Node initializerExpr;
    TokenPos wholeInitializerPos;
    if (hasInitializer) {
        // Parse the expression for the field initializer.
        {
            AutoAwaitIsKeyword<ParseHandler> awaitIsKeyword(this, AwaitIsName);
            initializerExpr = assignExpr(InAllowed, YieldIsName, TripledotProhibited);
            if (!initializerExpr)
                return null();
        }

        // In `class { x = function() {} }`, the anon function can get a name.
        handler.checkAndSetIsDirectRHSAnonFunction(initializerExpr);

        wholeInitializerPos = pos();
        wholeInitializerPos.begin = firstTokenPos.begin;
    } else {
        initializerExpr = handler.newRawUndefinedLiteral(firstTokenPos);
        if (!initializerExpr)
            return null();
        wholeInitializerPos = firstTokenPos;
    }

    // Update the end position of the parse node.
    handler.setEndPosition(funNode, wholeInitializerPos.end);
    funbox->setEnd(pos().end);

    // Create a ListNode for the parameters + body (there are no parameters)
    ListNodeType argsbody = handler.newList(PNK_PARAMSBODY, wholeInitializerPos);
    if (!argsbody)
        return null();
    handler.setFunctionFormalParametersAndBody(funNode, argsbody);
    funbox->function()->setArgCount(0);

    funbox->usesThis = true;
    NameNodeType thisName = newThisName();
    if (!thisName)
        return null();

    // Build `this.field` expression.
    ThisLiteralType propAssignThis = handler.newThisLiteral(wholeInitializerPos, thisName);
    if (!propAssignThis)
        return null();

    Node propAssignFieldAccess;
    uint32_t indexValue;
    if (!propAtom) {
        // See BytecodeEmitter::emitCreateFieldKeys for an explanation of what
        // .fieldKeys means and its purpose.
        NameNodeType fieldKeysName = newInternalDotName(isStatic ? context->names().dotStaticFieldKeys
                                                                 : context->names().dotFieldKeys);
        if (!fieldKeysName)
            return null();

        double fieldKeyIndex = isStatic ? classFields.staticFieldKeys++
                                        : classFields.instanceFieldKeys++;
        Node fieldKeyIndexNode = handler.newNumber(fieldKeyIndex, DecimalPoint::NoDecimal, wholeInitializerPos);
        if (!fieldKeyIndexNode)
            return null();

        Node fieldKeyValue = handler.newPropertyByValue(fieldKeysName, fieldKeyIndexNode, wholeInitializerPos.end);
        if (!fieldKeyValue)
            return null();

        propAssignFieldAccess = handler.newPropertyByValue(propAssignThis, fieldKeyValue, wholeInitializerPos.end);
        if (!propAssignFieldAccess)
            return null();
    } else if (propAtom->isIndex(&indexValue)) {
        // {Goanna} Can't reuse propName here, see comment in synthesizeConstructor
        Node indexNode = handler.newNumber(indexValue, DecimalPoint::NoDecimal, wholeInitializerPos);
        if (!indexNode)
            return null();

        propAssignFieldAccess = handler.newPropertyByValue(propAssignThis, indexNode, wholeInitializerPos.end);
        if (!propAssignFieldAccess)
            return null();
    } else {
        NameNodeType propAssignName = handler.newPropertyName(propAtom->asPropertyName(), wholeInitializerPos);
        if (!propAssignName)
            return null();

        propAssignFieldAccess = handler.newPropertyAccess(propAssignThis, propAssignName);
        if (!propAssignFieldAccess)
            return null();
    }

    // Synthesize a property init.
    AssignmentNodeType initializerPropInit = handler.newAssignment(PNK_INITPROP,
                                                                   propAssignFieldAccess, initializerExpr,
                                                                   JSOP_NOP);
    if (!initializerPropInit)
        return null();

    bool canSkipLazyClosedOverBindings = handler.canSkipLazyClosedOverBindings();
    if (!declareFunctionThis(canSkipLazyClosedOverBindings))
        return null();

    UnaryNodeType exprStatement = handler.newExprStatement(initializerPropInit, wholeInitializerPos.end);
    if (!exprStatement)
      return null();

    ListNodeType statementList = handler.newStatementList(wholeInitializerPos);
    if (!statementList)
      return null();
    handler.addStatementToList(statementList, exprStatement);

    // Set the function's body to the field assignment.
    LexicalScopeNodeType initializerBody = finishLexicalScope(pc->varScope(), statementList);
    if (!initializerBody) {
        return null();
    }

    handler.setFunctionBody(funNode, initializerBody);

    if (pc->superScopeNeedsHomeObject()) {
        funbox->setNeedsHomeObject();
    }

    if (!finishFunction())
        return null();

    if (!leaveInnerFunction(outerpc))
        return null();

    return funNode;
}

template <class ParseHandler>
bool
Parser<ParseHandler>::nextTokenContinuesLetDeclaration(TokenKind next, YieldHandling yieldHandling)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LET));

#ifdef DEBUG
    TokenKind verify;
    MOZ_ALWAYS_TRUE(tokenStream.peekToken(&verify));
    MOZ_ASSERT(next == verify);
#endif

    // Destructuring is (for once) the easy case.
    if (next == TOK_LB || next == TOK_LC)
        return true;

    // If we have the name "yield", the grammar parameter exactly states
    // whether this is okay.  (This wasn't true for SpiderMonkey's ancient
    // legacy generator syntax, but that's dead now.)  If YieldIsName,
    // declaration-parsing code will (if necessary) enforce a strict mode
    // restriction on defining "yield".  If YieldIsKeyword, consider this the
    // end of the declaration, in case ASI induces a semicolon that makes the
    // "yield" valid.
    if (next == TOK_YIELD)
        return yieldHandling == YieldIsName;

    // Somewhat similar logic applies for "await", except that it's not tracked
    // with an AwaitHandling argument.
    if (next == TOK_AWAIT)
        return !awaitIsKeyword();

    // Otherwise a let declaration must have a name.
    if (TokenKindIsPossibleIdentifier(next)) {
        // A "let" edge case deserves special comment.  Consider this:
        //
        //   let     // not an ASI opportunity
        //   let;
        //
        // Static semantics in §13.3.1.1 turn a LexicalDeclaration that binds
        // "let" into an early error.  Does this retroactively permit ASI so
        // that we should parse this as two ExpressionStatements?   No.  ASI
        // resolves during parsing.  Static semantics only apply to the full
        // parse tree with ASI applied.  No backsies!
        return true;
    }

    // Otherwise not a let declaration.
    return false;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::variableStatement(YieldHandling yieldHandling)
{
    ListNodeType vars = declarationList(yieldHandling, PNK_VAR);
    if (!vars)
        return null();
    if (!matchOrInsertSemicolonAfterExpression())
        return null();
    return vars;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::statement(YieldHandling yieldHandling)
{
    MOZ_ASSERT(checkOptionsCalled);

    JS_CHECK_RECURSION(context, return null());

    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();

    switch (tt) {
      // BlockStatement[?Yield, ?Return]
      case TOK_LC:
        return blockStatement(yieldHandling);

      // VariableStatement[?Yield]
      case TOK_VAR:
        return variableStatement(yieldHandling);

      // EmptyStatement
      case TOK_SEMI:
        return handler.newEmptyStatement(pos());

      // ExpressionStatement[?Yield].

      case TOK_YIELD: {
        // Don't use a ternary operator here due to obscure linker issues
        // around using static consts in the arms of a ternary.
        TokenStream::Modifier modifier;
        if (yieldExpressionsSupported())
            modifier = TokenStream::Operand;
        else
            modifier = TokenStream::None;

        TokenKind next;
        if (!tokenStream.peekToken(&next, modifier))
            return null();

        if (next == TOK_COLON)
            return labeledStatement(yieldHandling);

        return expressionStatement(yieldHandling);
      }

      default: {
        // Avoid getting next token with None.
        if (tt == TOK_AWAIT && pc->isAsync())
            return expressionStatement(yieldHandling);

        if (!TokenKindIsPossibleIdentifier(tt))
            return expressionStatement(yieldHandling);

        TokenKind next;
        if (!tokenStream.peekToken(&next))
            return null();

        // |let| here can only be an Identifier, not a declaration.  Give nicer
        // errors for declaration-looking typos.
        if (tt == TOK_LET) {
            bool forbiddenLetDeclaration = false;

            if (pc->sc()->strict() || versionNumber() >= JSVERSION_1_7) {
                // |let| can't be an Identifier in strict mode code.  Ditto for
                // non-standard JavaScript 1.7+.
                forbiddenLetDeclaration = true;
            } else if (next == TOK_LB) {
                // Enforce ExpressionStatement's 'let [' lookahead restriction.
                forbiddenLetDeclaration = true;
            } else if (next == TOK_LC || TokenKindIsPossibleIdentifier(next)) {
                // 'let {' and 'let foo' aren't completely forbidden, if ASI
                // causes 'let' to be the entire Statement.  But if they're
                // same-line, we can aggressively give a better error message.
                //
                // Note that this ignores 'yield' as TOK_YIELD: we'll handle it
                // correctly but with a worse error message.
                TokenKind nextSameLine;
                if (!tokenStream.peekTokenSameLine(&nextSameLine))
                    return null();

                MOZ_ASSERT(TokenKindIsPossibleIdentifier(nextSameLine) ||
                           nextSameLine == TOK_LC ||
                           nextSameLine == TOK_EOL);

                forbiddenLetDeclaration = nextSameLine != TOK_EOL;
            }

            if (forbiddenLetDeclaration) {
                error(JSMSG_FORBIDDEN_AS_STATEMENT, "lexical declarations");
                return null();
            }
        }

        // NOTE: It's unfortunately allowed to have a label named 'let' in
        //       non-strict code.  💯
        if (next == TOK_COLON)
            return labeledStatement(yieldHandling);

        return expressionStatement(yieldHandling);
      }

      case TOK_NEW:
        return expressionStatement(yieldHandling, PredictInvoked);

      // IfStatement[?Yield, ?Return]
      case TOK_IF:
        return ifStatement(yieldHandling);

      // BreakableStatement[?Yield, ?Return]
      //
      // BreakableStatement[Yield, Return]:
      //   IterationStatement[?Yield, ?Return]
      //   SwitchStatement[?Yield, ?Return]
      case TOK_DO:
        return doWhileStatement(yieldHandling);

      case TOK_WHILE:
        return whileStatement(yieldHandling);

      case TOK_FOR:
        return forStatement(yieldHandling);

      case TOK_SWITCH:
        return switchStatement(yieldHandling);

      // ContinueStatement[?Yield]
      case TOK_CONTINUE:
        return continueStatement(yieldHandling);

      // BreakStatement[?Yield]
      case TOK_BREAK:
        return breakStatement(yieldHandling);

      // [+Return] ReturnStatement[?Yield]
      case TOK_RETURN:
        // The Return parameter is only used here, and the effect is easily
        // detected this way, so don't bother passing around an extra parameter
        // everywhere.
        if (!pc->allowReturn()) {
            error(JSMSG_BAD_RETURN_OR_YIELD, js_return_str);
            return null();
        }
        return returnStatement(yieldHandling);

      // WithStatement[?Yield, ?Return]
      case TOK_WITH:
        return withStatement(yieldHandling);

      // LabelledStatement[?Yield, ?Return]
      // This is really handled by default and TOK_YIELD cases above.

      // ThrowStatement[?Yield]
      case TOK_THROW:
        return throwStatement(yieldHandling);

      // TryStatement[?Yield, ?Return]
      case TOK_TRY:
        return tryStatement(yieldHandling);

      // DebuggerStatement
      case TOK_DEBUGGER:
        return debuggerStatement();

      // |function| is forbidden by lookahead restriction (unless as child
      // statement of |if| or |else|, but Parser::consequentOrAlternative
      // handles that).
      case TOK_FUNCTION:
        error(JSMSG_FORBIDDEN_AS_STATEMENT, "function declarations");
        return null();

      // |class| is also forbidden by lookahead restriction.
      case TOK_CLASS:
        error(JSMSG_FORBIDDEN_AS_STATEMENT, "classes");
        return null();

      // ImportDeclaration (only inside modules)
      case TOK_IMPORT:
        return importDeclarationOrImportExpr(yieldHandling);

      // ExportDeclaration (only inside modules)
      case TOK_EXPORT:
        return exportDeclaration();

      // Miscellaneous error cases arguably better caught here than elsewhere.

      case TOK_CATCH:
        error(JSMSG_CATCH_WITHOUT_TRY);
        return null();

      case TOK_FINALLY:
        error(JSMSG_FINALLY_WITHOUT_TRY);
        return null();

      // NOTE: default case handled in the ExpressionStatement section.
    }
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::statementListItem(YieldHandling yieldHandling,
                                        bool canHaveDirectives /* = false */)
{
    MOZ_ASSERT(checkOptionsCalled);

    JS_CHECK_RECURSION(context, return null());

    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();

    switch (tt) {
      // BlockStatement[?Yield, ?Return]
      case TOK_LC:
        return blockStatement(yieldHandling);

      // VariableStatement[?Yield]
      case TOK_VAR:
        return variableStatement(yieldHandling);

      // EmptyStatement
      case TOK_SEMI:
        return handler.newEmptyStatement(pos());

      // ExpressionStatement[?Yield].
      //
      // These should probably be handled by a single ExpressionStatement
      // function in a default, not split up this way.
      case TOK_STRING:
        if (!canHaveDirectives && tokenStream.currentToken().atom() == context->names().useAsm) {
            if (!abortIfSyntaxParser())
                return null();
            if (!warning(JSMSG_USE_ASM_DIRECTIVE_FAIL))
                return null();
        }
        return expressionStatement(yieldHandling);

      case TOK_YIELD: {
        // Don't use a ternary operator here due to obscure linker issues
        // around using static consts in the arms of a ternary.
        TokenStream::Modifier modifier;
        if (yieldExpressionsSupported())
            modifier = TokenStream::Operand;
        else
            modifier = TokenStream::None;

        TokenKind next;
        if (!tokenStream.peekToken(&next, modifier))
            return null();

        if (next == TOK_COLON)
            return labeledStatement(yieldHandling);

        return expressionStatement(yieldHandling);
      }

      default: {
        // Avoid getting next token with None.
        if (tt == TOK_AWAIT && pc->isAsync())
            return expressionStatement(yieldHandling);

        if (!TokenKindIsPossibleIdentifier(tt))
            return expressionStatement(yieldHandling);

        TokenKind next;
        if (!tokenStream.peekToken(&next))
            return null();

        if (tt == TOK_LET && nextTokenContinuesLetDeclaration(next, yieldHandling))
            return lexicalDeclaration(yieldHandling, DeclarationKind::Let);

        if (tt == TOK_ASYNC) {
            TokenKind nextSameLine = TOK_EOF;
            if (!tokenStream.peekTokenSameLine(&nextSameLine))
                return null();
            if (nextSameLine == TOK_FUNCTION) {
                uint32_t toStringStart = pos().begin;
                tokenStream.consumeKnownToken(TOK_FUNCTION);
                return functionStmt(toStringStart, yieldHandling, NameRequired, AsyncFunction);
            }
        }

        if (next == TOK_COLON)
            return labeledStatement(yieldHandling);

        return expressionStatement(yieldHandling);
      }

      case TOK_NEW:
        return expressionStatement(yieldHandling, PredictInvoked);

      // IfStatement[?Yield, ?Return]
      case TOK_IF:
        return ifStatement(yieldHandling);

      // BreakableStatement[?Yield, ?Return]
      //
      // BreakableStatement[Yield, Return]:
      //   IterationStatement[?Yield, ?Return]
      //   SwitchStatement[?Yield, ?Return]
      case TOK_DO:
        return doWhileStatement(yieldHandling);

      case TOK_WHILE:
        return whileStatement(yieldHandling);

      case TOK_FOR:
        return forStatement(yieldHandling);

      case TOK_SWITCH:
        return switchStatement(yieldHandling);

      // ContinueStatement[?Yield]
      case TOK_CONTINUE:
        return continueStatement(yieldHandling);

      // BreakStatement[?Yield]
      case TOK_BREAK:
        return breakStatement(yieldHandling);

      // [+Return] ReturnStatement[?Yield]
      case TOK_RETURN:
        // The Return parameter is only used here, and the effect is easily
        // detected this way, so don't bother passing around an extra parameter
        // everywhere.
        if (!pc->allowReturn()) {
            error(JSMSG_BAD_RETURN_OR_YIELD, js_return_str);
            return null();
        }
        return returnStatement(yieldHandling);

      // WithStatement[?Yield, ?Return]
      case TOK_WITH:
        return withStatement(yieldHandling);

      // LabelledStatement[?Yield, ?Return]
      // This is really handled by default and TOK_YIELD cases above.

      // ThrowStatement[?Yield]
      case TOK_THROW:
        return throwStatement(yieldHandling);

      // TryStatement[?Yield, ?Return]
      case TOK_TRY:
        return tryStatement(yieldHandling);

      // DebuggerStatement
      case TOK_DEBUGGER:
        return debuggerStatement();

      // Declaration[Yield]:

      //   HoistableDeclaration[?Yield, ~Default]
      case TOK_FUNCTION:
        return functionStmt(pos().begin, yieldHandling, NameRequired);

      //   ClassDeclaration[?Yield, ~Default]
      case TOK_CLASS:
        return classDefinition(yieldHandling, ClassStatement, NameRequired);

      //   LexicalDeclaration[In, ?Yield]
      //     LetOrConst BindingList[?In, ?Yield]
      case TOK_CONST:
        // [In] is the default behavior, because for-loops specially parse
        // their heads to handle |in| in this situation.
        return lexicalDeclaration(yieldHandling, DeclarationKind::Const);

      // ImportDeclaration (only inside modules)
      case TOK_IMPORT:
        return importDeclarationOrImportExpr(yieldHandling);

      // ExportDeclaration (only inside modules)
      case TOK_EXPORT:
        return exportDeclaration();

      // Miscellaneous error cases arguably better caught here than elsewhere.

      case TOK_CATCH:
        error(JSMSG_CATCH_WITHOUT_TRY);
        return null();

      case TOK_FINALLY:
        error(JSMSG_FINALLY_WITHOUT_TRY);
        return null();

      // NOTE: default case handled in the ExpressionStatement section.
    }
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::expr(InHandling inHandling, YieldHandling yieldHandling,
                           TripledotHandling tripledotHandling,
                           PossibleError* possibleError /* = nullptr */,
                           InvokedPrediction invoked /* = PredictUninvoked */)
{
    Node pn = assignExpr(inHandling, yieldHandling, tripledotHandling,
                         possibleError, invoked);
    if (!pn)
        return null();

    bool matched;
    if (!tokenStream.matchToken(&matched, TOK_COMMA))
        return null();
    if (!matched)
        return pn;

    ListNodeType seq = handler.newCommaExpressionList(pn);
    if (!seq)
        return null();
    while (true) {
        // Trailing comma before the closing parenthesis is valid in an arrow
        // function parameters list: `(a, b, ) => body`. Check if we are
        // directly under CoverParenthesizedExpressionAndArrowParameterList,
        // and the next two tokens are closing parenthesis and arrow. If all
        // are present allow the trailing comma.
        if (tripledotHandling == TripledotAllowed) {
            TokenKind tt;
            if (!tokenStream.peekToken(&tt, TokenStream::Operand))
                return null();

            if (tt == TOK_RP) {
                tokenStream.consumeKnownToken(TOK_RP, TokenStream::Operand);

                if (!tokenStream.peekToken(&tt))
                    return null();
                if (tt != TOK_ARROW) {
                    error(JSMSG_UNEXPECTED_TOKEN, "expression", TokenKindToDesc(TOK_RP));
                    return null();
                }

                tokenStream.ungetToken();  // put back right paren
                tokenStream.addModifierException(TokenStream::NoneIsOperand);
                break;
            }
        }

        // Additional calls to assignExpr should not reuse the possibleError
        // which had been passed into the function. Otherwise we would lose
        // information needed to determine whether or not we're dealing with
        // a non-recoverable situation.
        PossibleError possibleErrorInner(*this);
        pn = assignExpr(inHandling, yieldHandling, tripledotHandling,
                        &possibleErrorInner);
        if (!pn)
            return null();

        if (!possibleError) {
            // Report any pending expression error.
            if (!possibleErrorInner.checkForExpressionError())
                return null();
        } else {
            possibleErrorInner.transferErrorsTo(possibleError);
        }

        handler.addList(seq, pn);

        if (!tokenStream.matchToken(&matched, TOK_COMMA))
            return null();
        if (!matched)
            break;
    }
    return seq;
}

/* These must be in the same order in several places:
 *   - the precedence table in Parser.cpp
 *   - the binary operators in ParseNode.h and TokenKind.h
 *   - the first and last binary operator markers in ParseNode.h
 */
static const JSOp ParseNodeKindToJSOp[] = {
    JSOP_COALESCE,
    JSOP_OR,
    JSOP_AND,
    JSOP_BITOR,
    JSOP_BITXOR,
    JSOP_BITAND,
    JSOP_STRICTEQ,
    JSOP_EQ,
    JSOP_STRICTNE,
    JSOP_NE,
    JSOP_LT,
    JSOP_LE,
    JSOP_GT,
    JSOP_GE,
    JSOP_INSTANCEOF,
    JSOP_IN,
    JSOP_LSH,
    JSOP_RSH,
    JSOP_URSH,
    JSOP_ADD,
    JSOP_SUB,
    JSOP_MUL,
    JSOP_DIV,
    JSOP_MOD,
    JSOP_POW
};

static inline JSOp
BinaryOpParseNodeKindToJSOp(ParseNodeKind pnk)
{
    MOZ_ASSERT(pnk >= PNK_BINOP_FIRST);
    MOZ_ASSERT(pnk <= PNK_BINOP_LAST);
#ifdef DEBUG
    int jsopArraySize = ArrayLength(ParseNodeKindToJSOp);
    int parseNodeKindListSize = PNK_BINOP_LAST - PNK_BINOP_FIRST + 1;
    MOZ_ASSERT(jsopArraySize == parseNodeKindListSize);
#endif
    return ParseNodeKindToJSOp[pnk - PNK_BINOP_FIRST];
}

static ParseNodeKind
BinaryOpTokenKindToParseNodeKind(TokenKind tok)
{
    MOZ_ASSERT(TokenKindIsBinaryOp(tok));
    return ParseNodeKind(PNK_BINOP_FIRST + (tok - TOK_BINOP_FIRST));
}

/* These must be in the same order in several places:
 *   - the JSOp code list in Parser.cpp
 *   - the binary operators in ParseNode.h and TokenKind.h
 */
static const int PrecedenceTable[] = {
    1,  /* PNK_COALESCE */
    2,  /* PNK_OR */
    3,  /* PNK_AND */
    4,  /* PNK_BITOR */
    5,  /* PNK_BITXOR */
    6,  /* PNK_BITAND */
    7,  /* PNK_STRICTEQ */
    7,  /* PNK_EQ */
    7,  /* PNK_STRICTNE */
    7,  /* PNK_NE */
    8,  /* PNK_LT */
    8,  /* PNK_LE */
    8,  /* PNK_GT */
    8,  /* PNK_GE */
    8,  /* PNK_INSTANCEOF */
    8,  /* PNK_IN */
    9,  /* PNK_LSH */
    9,  /* PNK_RSH */
    9,  /* PNK_URSH */
    10, /* PNK_ADD */
    10, /* PNK_SUB */
    11, /* PNK_STAR */
    11, /* PNK_DIV */
    11, /* PNK_MOD */
    12  /* PNK_POW */
};

static const int PRECEDENCE_CLASSES = 12;

static int
Precedence(ParseNodeKind pnk) {
    // Everything binds tighter than PNK_LIMIT, because we want to reduce all
    // nodes to a single node when we reach a token that is not another binary
    // operator.
    if (pnk == PNK_LIMIT)
        return 0;

    MOZ_ASSERT(pnk >= PNK_BINOP_FIRST);
    MOZ_ASSERT(pnk <= PNK_BINOP_LAST);
    return PrecedenceTable[pnk - PNK_BINOP_FIRST];
}

enum class EnforcedParentheses : uint8_t { CoalesceExpr, AndOrExpr, None };

template <typename ParseHandler>
MOZ_ALWAYS_INLINE typename ParseHandler::Node
Parser<ParseHandler>::orExpr1(InHandling inHandling, YieldHandling yieldHandling,
                              TripledotHandling tripledotHandling,
                              PossibleError* possibleError,
                              InvokedPrediction invoked /* = PredictUninvoked */)
{
    // Shift-reduce parser for the binary operator part of the JS expression
    // syntax.

    // Conceptually there's just one stack, a stack of pairs (lhs, op).
    // It's implemented using two separate arrays, though.
    Node nodeStack[PRECEDENCE_CLASSES];
    ParseNodeKind kindStack[PRECEDENCE_CLASSES];
    int depth = 0;
    Node pn;
    EnforcedParentheses unparenthesizedExpression = EnforcedParentheses::None;
    for (;;) {
        pn = unaryExpr(yieldHandling, tripledotHandling, possibleError, invoked);
        if (!pn)
            return pn;

        // If a binary operator follows, consume it and compute the
        // corresponding operator.
        TokenKind tok;
        if (!tokenStream.getToken(&tok))
            return null();

        ParseNodeKind pnk;
        if (tok == TOK_IN ? inHandling == InAllowed : TokenKindIsBinaryOp(tok)) {
            // We're definitely not in a destructuring context, so report any
            // pending expression error now.
            if (possibleError && !possibleError->checkForExpressionError())
                return null();

            switch (tok) {
              // Report an error for unary expressions on the LHS of **.
              case TOK_POW:
                if (handler.isUnparenthesizedUnaryExpression(pn)) {
                    error(JSMSG_BAD_POW_LEFTSIDE);
                    return null();
                }
                break;
              case TOK_OR:
              case TOK_AND:
                // Report an error if ?? is on the LHS of the expression.
                // Mixing other logical operators with the nullish coalescing
                // operator is disallowed unless one expression is parenthesized.
                if (unparenthesizedExpression == EnforcedParentheses::CoalesceExpr) {
                    error(JSMSG_BAD_COALESCE_MIXING);
                    return null();
                }
                // If we have not detected a mixing error at this point, record that
                // we have an unparenthesized expression, in case we have one later.
                unparenthesizedExpression = EnforcedParentheses::AndOrExpr;
                break;
              case TOK_COALESCE:
                if (unparenthesizedExpression == EnforcedParentheses::AndOrExpr) {
                    error(JSMSG_BAD_COALESCE_MIXING);
                    return null();
                }
                // If we have not detected a mixing error at this point, record that
                // we have an unparenthesized expression, in case we have one later.
                unparenthesizedExpression = EnforcedParentheses::CoalesceExpr;
                break;                
              default:
                // Do nothing in other cases.
                break;
            }

            pnk = BinaryOpTokenKindToParseNodeKind(tok);
        } else {
            tok = TOK_EOF;
            pnk = PNK_LIMIT;
        }

        // From this point on, destructuring defaults are definitely an error.
        possibleError = nullptr;

        // If pnk has precedence less than or equal to another operator on the
        // stack, reduce. This combines nodes on the stack until we form the
        // actual lhs of pnk.
        //
        // The >= in this condition works because it is appendOrCreateList's
        // job to decide if the operator in question is left- or
        // right-associative, and build the corresponding tree.
        while (depth > 0 && Precedence(kindStack[depth - 1]) >= Precedence(pnk)) {
            depth--;
            ParseNodeKind combiningPnk = kindStack[depth];
            JSOp combiningOp = BinaryOpParseNodeKindToJSOp(combiningPnk);
            pn = handler.appendOrCreateList(combiningPnk, nodeStack[depth], pn, pc, combiningOp);
            if (!pn)
                return pn;
        }

        if (pnk == PNK_LIMIT)
            break;

        nodeStack[depth] = pn;
        kindStack[depth] = pnk;
        depth++;
        MOZ_ASSERT(depth <= PRECEDENCE_CLASSES);
    }

    MOZ_ASSERT(depth == 0);
    return pn;
}

template <typename ParseHandler>
MOZ_ALWAYS_INLINE typename ParseHandler::Node
Parser<ParseHandler>::condExpr1(InHandling inHandling, YieldHandling yieldHandling,
                                TripledotHandling tripledotHandling,
                                PossibleError* possibleError,
                                InvokedPrediction invoked /* = PredictUninvoked */)
{
    Node condition = orExpr1(inHandling, yieldHandling, tripledotHandling, possibleError, invoked);

    if (!condition || !tokenStream.isCurrentTokenType(TOK_HOOK))
        return condition;

    Node thenExpr = assignExpr(InAllowed, yieldHandling, TripledotProhibited);
    if (!thenExpr)
        return null();

    if (!mustMatchToken(TOK_COLON, JSMSG_COLON_IN_COND)) {
        return null();
    }

    Node elseExpr = assignExpr(inHandling, yieldHandling, TripledotProhibited);
    if (!elseExpr)
        return null();

    // Advance to the next token; the caller is responsible for interpreting it.
    TokenKind ignored;
    if (!tokenStream.getToken(&ignored))
        return null();
    return handler.newConditional(condition, thenExpr, elseExpr);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::assignExpr(InHandling inHandling, YieldHandling yieldHandling,
                                 TripledotHandling tripledotHandling,
                                 PossibleError* possibleError /* = nullptr */,
                                 InvokedPrediction invoked /* = PredictUninvoked */)
{
    JS_CHECK_RECURSION(context, return null());

    // It's very common at this point to have a "detectably simple" expression,
    // i.e. a name/number/string token followed by one of the following tokens
    // that obviously isn't part of an expression: , ; : ) ] }
    //
    // (In Parsemark this happens 81.4% of the time;  in code with large
    // numeric arrays, such as some Kraken benchmarks, it happens more often.)
    //
    // In such cases, we can avoid the full expression parsing route through
    // assignExpr(), condExpr1(), orExpr1(), unaryExpr(), memberExpr(), and
    // primaryExpr().

    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();

    TokenPos exprPos = pos();

    bool endsExpr;

    // This only handles identifiers that *never* have special meaning anywhere
    // in the language.  Contextual keywords, reserved words in strict mode,
    // and other hard cases are handled outside this fast path.
    if (tt == TOK_NAME) {
        if (!tokenStream.nextTokenEndsExpr(&endsExpr))
            return null();
        if (endsExpr) {
            Rooted<PropertyName*> name(context, identifierReference(yieldHandling));
            if (!name)
                return null();

            return identifierReference(name);
        }
    }

    if (tt == TOK_NUMBER) {
        if (!tokenStream.nextTokenEndsExpr(&endsExpr))
            return null();
        if (endsExpr)
            return newNumber(tokenStream.currentToken());
    }

    if (tt == TOK_STRING) {
        if (!tokenStream.nextTokenEndsExpr(&endsExpr))
            return null();
        if (endsExpr)
            return stringLiteral();
    }

    if (tt == TOK_YIELD && yieldExpressionsSupported())
        return yieldExpression(inHandling);

    bool maybeAsyncArrow = false;
    if (tt == TOK_ASYNC) {
        TokenKind nextSameLine = TOK_EOF;
        if (!tokenStream.peekTokenSameLine(&nextSameLine))
            return null();

        if (TokenKindIsPossibleIdentifier(nextSameLine))
            maybeAsyncArrow = true;
    }

    tokenStream.ungetToken();

    // Save the tokenizer state in case we find an arrow function and have to
    // rewind.
    TokenStream::Position start(keepAtoms);
    tokenStream.tell(&start);

    PossibleError possibleErrorInner(*this);
    Node lhs;
    if (maybeAsyncArrow) {
        tokenStream.consumeKnownToken(TOK_ASYNC, TokenStream::Operand);

        TokenKind tt;
        if (!tokenStream.getToken(&tt))
            return null();
        MOZ_ASSERT(TokenKindIsPossibleIdentifier(tt));

        // Check yield validity here.
        RootedPropertyName name(context, bindingIdentifier(yieldHandling));
        if (!name)
            return null();

        if (!tokenStream.getToken(&tt))
            return null();
        if (tt != TOK_ARROW) {
            error(JSMSG_UNEXPECTED_TOKEN, "'=>' after argument list", TokenKindToDesc(tt));

            return null();
        }
    } else {
        lhs = condExpr1(inHandling, yieldHandling, tripledotHandling, &possibleErrorInner, invoked);
        if (!lhs) {
            return null();
        }
    }

    ParseNodeKind kind;
    JSOp op;
    switch (tokenStream.currentToken().type) {
      case TOK_ASSIGN:       kind = PNK_ASSIGN;       op = JSOP_NOP;    break;
      case TOK_ADDASSIGN:    kind = PNK_ADDASSIGN;    op = JSOP_ADD;    break;
      case TOK_SUBASSIGN:    kind = PNK_SUBASSIGN;    op = JSOP_SUB;    break;
      case TOK_COALESCEASSIGN:  kind = PNK_COALESCEASSIGN;  op = JSOP_COALESCE;  break;
      case TOK_ORASSIGN:     kind = PNK_ORASSIGN;     op = JSOP_OR;     break;
      case TOK_ANDASSIGN:    kind = PNK_ANDASSIGN;    op = JSOP_AND;    break;
      case TOK_BITORASSIGN:  kind = PNK_BITORASSIGN;  op = JSOP_BITOR;  break;
      case TOK_BITXORASSIGN: kind = PNK_BITXORASSIGN; op = JSOP_BITXOR; break;
      case TOK_BITANDASSIGN: kind = PNK_BITANDASSIGN; op = JSOP_BITAND; break;
      case TOK_LSHASSIGN:    kind = PNK_LSHASSIGN;    op = JSOP_LSH;    break;
      case TOK_RSHASSIGN:    kind = PNK_RSHASSIGN;    op = JSOP_RSH;    break;
      case TOK_URSHASSIGN:   kind = PNK_URSHASSIGN;   op = JSOP_URSH;   break;
      case TOK_MULASSIGN:    kind = PNK_MULASSIGN;    op = JSOP_MUL;    break;
      case TOK_DIVASSIGN:    kind = PNK_DIVASSIGN;    op = JSOP_DIV;    break;
      case TOK_MODASSIGN:    kind = PNK_MODASSIGN;    op = JSOP_MOD;    break;
      case TOK_POWASSIGN:    kind = PNK_POWASSIGN;    op = JSOP_POW;    break;

      case TOK_ARROW: {

        // A line terminator between ArrowParameters and the => should trigger a SyntaxError.
        tokenStream.ungetToken();
        TokenKind next;
        if (!tokenStream.peekTokenSameLine(&next))
            return null();
        MOZ_ASSERT(next == TOK_ARROW || next == TOK_EOL);

        if (next != TOK_ARROW) {
            error(JSMSG_LINE_BREAK_BEFORE_ARROW);
            return null();
        }
        tokenStream.consumeKnownToken(TOK_ARROW);

        bool isBlock = false;
        if (!tokenStream.peekToken(&next, TokenStream::Operand))
            return null();
        if (next == TOK_LC)
            isBlock = true;

        tokenStream.seek(start);

        if (!tokenStream.getToken(&next, TokenStream::Operand))
            return null();
        uint32_t toStringStart = pos().begin;
        tokenStream.ungetToken();

        FunctionAsyncKind asyncKind = SyncFunction;

        if (next == TOK_ASYNC) {
            tokenStream.consumeKnownToken(next, TokenStream::Operand);

            TokenKind nextSameLine = TOK_EOF;
            if (!tokenStream.peekTokenSameLine(&nextSameLine))
                return null();

            if (nextSameLine == TOK_ARROW) {
                tokenStream.ungetToken();
            } else {
                asyncKind = AsyncFunction;
            }
        }

        FunctionNodeType funNode = handler.newFunction(FunctionSyntaxKind::Arrow, pos());
        if (!funNode)
            return null();

        Node arrowFunc = functionDefinition(toStringStart, funNode, inHandling, yieldHandling, nullptr,
                                            FunctionSyntaxKind::Arrow, NotGenerator, asyncKind);
        if (!arrowFunc)
            return null();

        if (isBlock) {
            // This arrow function could be a non-trailing member of a comma
            // expression or a semicolon terminating a full expression.  If so,
            // the next token is that comma/semicolon, gotten with None:
            //
            //   a => {}, b; // as if (a => {}), b;
            //   a => {};
            //
            // But if this arrow function ends a statement, ASI permits the
            // next token to start an expression statement.  In that case the
            // next token must be gotten as Operand:
            //
            //   a => {} // complete expression statement
            //   /x/g;   // regular expression as a statement, *not* division
            //
            // Getting the second case right requires the first token-peek
            // after the arrow function use Operand, and that peek must occur
            // before Parser::expr() looks for a comma.  Do so here, then
            // immediately add the modifier exception needed for the first
            // case.
            //
            // Note that the second case occurs *only* if the arrow function
            // has block body.  An arrow function not ending in such, ends in
            // another AssignmentExpression that we can inductively assume was
            // peeked consistently.
            TokenKind ignored;
            if (!tokenStream.peekToken(&ignored, TokenStream::Operand))
                return null();
            tokenStream.addModifierException(TokenStream::NoneIsOperand);
        }
        return arrowFunc;
      }

      default:
        MOZ_ASSERT(!tokenStream.isCurrentTokenAssignment());
        if (!possibleError) {
            if (!possibleErrorInner.checkForExpressionError())
                return null();
        } else {
            possibleErrorInner.transferErrorsTo(possibleError);
        }
        tokenStream.ungetToken();
        return lhs;
    }

    // Verify the left-hand side expression doesn't have a forbidden form.
    if (handler.isUnparenthesizedDestructuringPattern(lhs)) {
        if (kind != PNK_ASSIGN) {
            error(JSMSG_BAD_DESTRUCT_ASS);
            return null();
        }

        if (!possibleErrorInner.checkForDestructuringErrorOrWarning())
            return null();
    } else if (handler.isNameAnyParentheses(lhs)) {
        if (const char* chars = handler.nameIsArgumentsEvalAnyParentheses(lhs, context)) {
            // |chars| is "arguments" or "eval" here.
            if (!strictModeErrorAt(exprPos.begin, JSMSG_BAD_STRICT_ASSIGN, chars))
                return null();
        }

        handler.adjustGetToSet(lhs);
    } else if (handler.isPropertyAccess(lhs)) {
        // Permitted: no additional testing/fixup needed.
    } else if (handler.isFunctionCall(lhs)) {
        // We don't have to worry about backward compatibility issues with the new
        // compound assignment operators, so we always throw here. Also that way we
        // don't have to worry if |f() &&= expr| should always throw an error or
        // only if |f()| returns true.
        if (kind == PNK_COALESCEASSIGN ||
            kind == PNK_ORASSIGN ||
            kind == PNK_ADDASSIGN) {
          errorAt(exprPos.begin, JSMSG_BAD_LEFTSIDE_OF_ASS);
          return null();
        }
        
        if (!strictModeErrorAt(exprPos.begin, JSMSG_BAD_LEFTSIDE_OF_ASS))
            return null();

        if (possibleError)
            possibleError->setPendingDestructuringErrorAt(exprPos, JSMSG_BAD_DESTRUCT_TARGET);
    } else {
        errorAt(exprPos.begin, JSMSG_BAD_LEFTSIDE_OF_ASS);
        return null();
    }

    if (!possibleErrorInner.checkForExpressionError())
        return null();

    Node rhs = assignExpr(inHandling, yieldHandling, TripledotProhibited);
    if (!rhs)
        return null();

    if (kind == PNK_ASSIGN)
        handler.checkAndSetIsDirectRHSAnonFunction(rhs);

    return handler.newAssignment(kind, lhs, rhs, op);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::isValidSimpleAssignmentTarget(Node node,
                                                    FunctionCallBehavior behavior /* = ForbidAssignmentToFunctionCalls */)
{
    // Note that this method implements *only* a boolean test.  Reporting an
    // error for the various syntaxes that fail this, and warning for the
    // various syntaxes that "pass" this but should not, occurs elsewhere.

    if (handler.isNameAnyParentheses(node)) {
        if (!pc->sc()->strict())
            return true;

        return !handler.nameIsArgumentsEvalAnyParentheses(node, context);
    }

    if (handler.isPropertyAccess(node))
        return true;

    if (behavior == PermitAssignmentToFunctionCalls) {
        if (handler.isFunctionCall(node))
            return true;
    }

    return false;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::checkIncDecOperand(Node operand, uint32_t operandOffset)
{
    if (handler.isNameAnyParentheses(operand)) {
        if (const char* chars = handler.nameIsArgumentsEvalAnyParentheses(operand, context)) {
            if (!strictModeErrorAt(operandOffset, JSMSG_BAD_STRICT_ASSIGN, chars))
                return false;
        }
    } else if (handler.isPropertyAccess(operand)) {
        // Permitted: no additional testing/fixup needed.
    } else if (handler.isFunctionCall(operand)) {
        // Assignment to function calls is forbidden in ES6.  We're still
        // somewhat concerned about sites using this in dead code, so forbid it
        // only in strict mode code (or if the werror option has been set), and
        // otherwise warn.
        if (!strictModeErrorAt(operandOffset, JSMSG_BAD_INCOP_OPERAND))
            return false;
    } else {
        errorAt(operandOffset, JSMSG_BAD_INCOP_OPERAND);
        return false;
    }

    MOZ_ASSERT(isValidSimpleAssignmentTarget(operand, PermitAssignmentToFunctionCalls),
               "inconsistent increment/decrement operand validation");
    return true;
}

template <typename ParseHandler>
typename ParseHandler::UnaryNodeType
Parser<ParseHandler>::unaryOpExpr(YieldHandling yieldHandling, ParseNodeKind kind, JSOp op,
                                  uint32_t begin)
{
    Node kid = unaryExpr(yieldHandling, TripledotProhibited);
    if (!kid)
        return null();
    return handler.newUnary(kind, op, begin, kid);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::optionalExpr(
    YieldHandling yieldHandling, TripledotHandling tripledotHandling,
    TokenKind tt, bool allowCallSyntax /* = true */,
    PossibleError* possibleError /* = nullptr */,
    InvokedPrediction invoked /* = PredictUninvoked */)
{
    JS_CHECK_RECURSION(context, return null());

    uint32_t begin = pos().begin;

    Node lhs = memberExpr(yieldHandling, tripledotHandling, tt,
                          /* allowCallSyntax = */ true, possibleError, invoked);

    if (!lhs) {
        return null();
    }

    // Note: TokenStream::None is equivalent to TokenStream::SlashIsDiv
    // in current Mozilla code, see bug 1539821.
    if (!tokenStream.peekToken(&tt, TokenStream::None)) {
        return null();
    }

    if (tt != TOK_OPTCHAIN) {
        return lhs;
    }

    while (true) {
        if (!tokenStream.getToken(&tt)) {
            return null();
        }

        if (tt == TOK_EOF) {
            break;
        }

        Node nextMember;
        if (tt == TOK_OPTCHAIN) {
            if (!tokenStream.getToken(&tt)) {
                return null();
            }
            if (TokenKindIsPossibleIdentifierName(tt)) {
                nextMember = memberPropertyAccess(lhs, OptionalKind::Optional);
                if (!nextMember) {
                    return null();
                }
            } else if (tt == TOK_LB) {
                nextMember = memberElemAccess(lhs, yieldHandling,
                                              OptionalKind::Optional);
                if (!nextMember) {
                    return null();
                }
            } else if (tt == TOK_LP) {
                nextMember = memberCall(tt, lhs, yieldHandling, possibleError,
                                        OptionalKind::Optional);
                if (!nextMember) {
                    return null();
                }
            } else {
                error(JSMSG_NAME_AFTER_DOT);
                return null();
            }
        } else if (tt == TOK_DOT) {
            if (!tokenStream.getToken(&tt)) {
                return null();
            }
            if (TokenKindIsPossibleIdentifierName(tt)) {
                nextMember = memberPropertyAccess(lhs);
                if (!nextMember) {
                    return null();
                }
            } else {
                error(JSMSG_NAME_AFTER_DOT);
                return null();
            }
        } else if (tt == TOK_LB) {
            nextMember = memberElemAccess(lhs, yieldHandling);
            if (!nextMember) {
                return null();
            }
        } else if (allowCallSyntax && tt == TOK_LP) {
            nextMember = memberCall(tt, lhs, yieldHandling, possibleError);
            if (!nextMember) {
                return null();
            }
        } else if (tt == TOK_TEMPLATE_HEAD || tt == TOK_NO_SUBS_TEMPLATE) {
            error(JSMSG_BAD_OPTIONAL_TEMPLATE);
            return null();
        } else {
            tokenStream.ungetToken();
            break;
        }

        MOZ_ASSERT(nextMember);
        lhs = nextMember;
    }

  return handler.newOptionalChain(begin, lhs);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::unaryExpr(YieldHandling yieldHandling, TripledotHandling tripledotHandling,
                                PossibleError* possibleError /* = nullptr */,
                                InvokedPrediction invoked /* = PredictUninvoked */)
{
    JS_CHECK_RECURSION(context, return null());

    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();
    uint32_t begin = pos().begin;
    switch (tt) {
      case TOK_VOID:
        return unaryOpExpr(yieldHandling, PNK_VOID, JSOP_VOID, begin);
      case TOK_NOT:
        return unaryOpExpr(yieldHandling, PNK_NOT, JSOP_NOT, begin);
      case TOK_BITNOT:
        return unaryOpExpr(yieldHandling, PNK_BITNOT, JSOP_BITNOT, begin);
      case TOK_ADD:
        return unaryOpExpr(yieldHandling, PNK_POS, JSOP_POS, begin);
      case TOK_SUB:
        return unaryOpExpr(yieldHandling, PNK_NEG, JSOP_NEG, begin);

      case TOK_TYPEOF: {
        // The |typeof| operator is specially parsed to distinguish its
        // application to a name, from its application to a non-name
        // expression:
        //
        //   // Looks up the name, doesn't find it and so evaluates to
        //   // "undefined".
        //   assertEq(typeof nonExistentName, "undefined");
        //
        //   // Evaluates expression, triggering a runtime ReferenceError for
        //   // the undefined name.
        //   typeof (1, nonExistentName);
        Node kid = unaryExpr(yieldHandling, TripledotProhibited);
        if (!kid)
            return null();

        return handler.newTypeof(begin, kid);
      }

      case TOK_INC:
      case TOK_DEC:
      {
        TokenKind tt2;
        if (!tokenStream.getToken(&tt2, TokenStream::Operand))
            return null();

        uint32_t operandOffset = pos().begin;
        Node operand = optionalExpr(yieldHandling, TripledotProhibited, tt2);
        if (!operand || !checkIncDecOperand(operand, operandOffset))
            return null();

        return handler.newUpdate((tt == TOK_INC) ? PNK_PREINCREMENT : PNK_PREDECREMENT,
                                 begin, operand);
      }

      case TOK_DELETE: {
        uint32_t exprOffset;
        if (!tokenStream.peekOffset(&exprOffset, TokenStream::Operand))
            return null();

        Node expr = unaryExpr(yieldHandling, TripledotProhibited);
        if (!expr)
            return null();

        // Per spec, deleting any unary expression is valid -- it simply
        // returns true -- except for one case that is illegal in strict mode.
        if (handler.isNameAnyParentheses(expr)) {
            if (!strictModeErrorAt(exprOffset, JSMSG_DEPRECATED_DELETE_OPERAND))
                return null();

            pc->sc()->setBindingsAccessedDynamically();
        }

        return handler.newDelete(begin, expr);
      }

      case TOK_AWAIT: {
        if (pc->isAsync()) {
            Node kid = unaryExpr(yieldHandling, tripledotHandling, possibleError, invoked);
            if (!kid)
                return null();
            pc->lastAwaitOffset = begin;
            return handler.newAwaitExpression(begin, kid);
        }
      }

        MOZ_FALLTHROUGH;

      default: {
        Node expr = optionalExpr(yieldHandling, tripledotHandling, tt,
                                 /* allowCallSyntax = */ true,
                                 possibleError, invoked);
        if (!expr)
            return null();

        /* Don't look across a newline boundary for a postfix incop. */
        if (!tokenStream.peekTokenSameLine(&tt))
            return null();

        if (tt != TOK_INC && tt != TOK_DEC)
            return expr;

        tokenStream.consumeKnownToken(tt);
        if (!checkIncDecOperand(expr, begin))
            return null();
        return handler.newUpdate((tt == TOK_INC) ? PNK_POSTINCREMENT : PNK_POSTDECREMENT,
                                 begin, expr);
      }
    }
}


/*** Comprehensions *******************************************************************************
 *
 * We currently support two flavors of comprehensions, all deprecated:
 *
 *     [for (V of OBJ) if (COND) EXPR]  // ES6-era array comprehension
 *     (for (V of OBJ) if (COND) EXPR)  // ES6-era generator expression
 *
 * (These flavors are called "ES6-era" because they were in ES6 draft
 * specifications for a while. Shortly after this syntax was implemented in SM,
 * TC39 decided to drop it.)
 */

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::generatorComprehensionLambda(unsigned begin)
{
    FunctionNodeType genfn = handler.newFunction(FunctionSyntaxKind::Expression, pos());
    if (!genfn)
        return null();

    ParseContext* outerpc = pc;

    // If we are off the main thread, the generator meta-objects have
    // already been created by js::StartOffThreadParseTask, so cx will not
    // be necessary.
    RootedObject proto(context);
    JSContext* cx = context->maybeJSContext();
    proto = GlobalObject::getOrCreateStarGeneratorFunctionPrototype(cx, context->global());
    if (!proto)
        return null();

    RootedFunction fun(context, newFunction(/* atom = */ nullptr, FunctionSyntaxKind::Expression,
                                            StarGenerator, SyncFunction, proto));
    if (!fun)
        return null();

    // Create box for fun->object early to root it.
    Directives directives(/* strict = */ outerpc->sc()->strict());
    FunctionBox* genFunbox = newFunctionBox(genfn, fun, /* toStringStart = */ 0, directives,
                                            StarGenerator, SyncFunction, /* tryAnnexB = */ false);
    if (!genFunbox)
        return null();
    genFunbox->isGenexpLambda = true;
    genFunbox->initWithEnclosingParseContext(outerpc, FunctionSyntaxKind::Expression);

    ParseContext genpc(this, genFunbox, /* newDirectives = */ nullptr);
    if (!genpc.init())
        return null();
    genpc.functionScope().useAsVarScope(&genpc);

    /*
     * We assume conservatively that any deoptimization flags in pc->sc()
     * come from the kid. So we propagate these flags into genfn. For code
     * simplicity we also do not detect if the flags were only set in the
     * kid and could be removed from pc->sc().
     */
    genFunbox->anyCxFlags = outerpc->sc()->anyCxFlags;

    if (!declareDotGeneratorName())
        return null();

    ListNodeType body = handler.newStatementList(TokenPos(begin, pos().end));
    if (!body)
        return null();

    Node comp = comprehension(StarGenerator);
    if (!comp)
        return null();

    if (!mustMatchToken(TOK_RP, JSMSG_PAREN_IN_PAREN)) {
        return null();
    }

    uint32_t end = pos().end;
    handler.setBeginPosition(comp, begin);
    handler.setEndPosition(comp, end);
    genFunbox->setEnd(end);
    handler.addStatementToList(body, comp);
    handler.setEndPosition(body, end);
    handler.setBeginPosition(genfn, begin);
    handler.setEndPosition(genfn, end);

    Node generator = newDotGeneratorName();
    if (!generator)
        return null();
    if (!handler.prependInitialYield(body, generator))
        return null();

    if (!propagateFreeNamesAndMarkClosedOverBindings(pc->varScope()))
        return null();
    if (!finishFunction())
        return null();
    if (!leaveInnerFunction(outerpc))
        return null();

    // Note that if we ever start syntax-parsing generators, we will also
    // need to propagate the closed-over variable set to the inner
    // lazyscript.
    if (!handler.setComprehensionLambdaBody(genfn, body))
        return null();

    return genfn;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::comprehensionFor(GeneratorKind comprehensionKind)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    uint32_t begin = pos().begin;

    if (!mustMatchToken(TOK_LP, JSMSG_PAREN_AFTER_FOR)) {
        return null();
    }

    // FIXME: Destructuring binding (bug 980828).

    if (!mustMatchToken(TokenKindIsPossibleIdentifier, JSMSG_NO_VARIABLE_NAME)) {
        return null();
    }
    RootedPropertyName name(context, bindingIdentifier(YieldIsKeyword));
    if (!name)
        return null();
    if (name == context->names().let) {
        error(JSMSG_LET_COMP_BINDING);
        return null();
    }
    TokenPos namePos = pos();
    Node lhs = newName(name);
    if (!lhs)
        return null();
    bool matched;
    if (!tokenStream.matchToken(&matched, TOK_OF))
        return null();
    if (!matched) {
        error(JSMSG_OF_AFTER_FOR_NAME);
        return null();
    }

    Node rhs = assignExpr(InAllowed, YieldIsKeyword, TripledotProhibited);
    if (!rhs)
        return null();

    if (!mustMatchToken(TOK_RP, JSMSG_PAREN_AFTER_FOR_OF_ITERABLE)) {
        return null();
    }

    TokenPos headPos(begin, pos().end);

    ParseContext::Scope scope(this);
    if (!scope.init(pc))
        return null();

    {
        // Push a temporary ForLoopLexicalHead Statement that allows for
        // lexical declarations, as they are usually allowed only in braced
        // statements.
        ParseContext::Statement forHeadStmt(pc, StatementKind::ForLoopLexicalHead);
        if (!noteDeclaredName(name, DeclarationKind::Let, namePos))
            return null();
    }

    Node decls = handler.newComprehensionBinding(lhs);
    if (!decls)
        return null();

    Node tail = comprehensionTail(comprehensionKind);
    if (!tail)
        return null();

    // Finish the lexical scope after parsing the tail.
    LexicalScopeNodeType lexicalScope = finishLexicalScope(scope, decls);
    if (!lexicalScope)
        return null();

    TernaryNodeType head = handler.newForInOrOfHead(PNK_FOROF, lexicalScope, rhs, headPos);
    if (!head)
        return null();

    return handler.newComprehensionFor(begin, head, tail);
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::comprehensionIf(GeneratorKind comprehensionKind)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_IF));

    uint32_t begin = pos().begin;

    if (!mustMatchToken(TOK_LP, JSMSG_PAREN_BEFORE_COND)) {
        return null();
    }
    Node cond = assignExpr(InAllowed, YieldIsKeyword, TripledotProhibited);
    if (!cond)
        return null();
    if (!mustMatchToken(TOK_RP, JSMSG_PAREN_AFTER_COND)) {
        return null();
    }

    /* Check for (a = b) and warn about possible (a == b) mistype. */
    if (handler.isUnparenthesizedAssignment(cond)) {
        if (!extraWarning(JSMSG_EQUAL_AS_ASSIGN))
            return null();
    }

    Node then = comprehensionTail(comprehensionKind);
    if (!then)
        return null();

    return handler.newIfStatement(begin, cond, then, null());
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::comprehensionTail(GeneratorKind comprehensionKind)
{
    JS_CHECK_RECURSION(context, return null());

    bool matched;
    if (!tokenStream.matchToken(&matched, TOK_FOR, TokenStream::Operand))
        return null();
    if (matched)
        return comprehensionFor(comprehensionKind);

    if (!tokenStream.matchToken(&matched, TOK_IF, TokenStream::Operand))
        return null();
    if (matched)
        return comprehensionIf(comprehensionKind);

    uint32_t begin = pos().begin;

    Node bodyExpr = assignExpr(InAllowed, YieldIsKeyword, TripledotProhibited);
    if (!bodyExpr)
        return null();

    if (comprehensionKind == NotGenerator)
        return handler.newArrayPush(begin, bodyExpr);

    MOZ_ASSERT(comprehensionKind == StarGenerator);
    Node yieldExpr = handler.newYieldExpression(begin, bodyExpr);
    if (!yieldExpr)
        return null();
    yieldExpr = handler.parenthesize(yieldExpr);

    return handler.newExprStatement(yieldExpr, pos().end);
}

// Parse an ES6-era generator or array comprehension, starting at the first
// `for`. The caller is responsible for matching the ending TOK_RP or TOK_RB.
template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::comprehension(GeneratorKind comprehensionKind)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    uint32_t startYieldOffset = pc->lastYieldOffset;

    Node body = comprehensionFor(comprehensionKind);
    if (!body)
        return null();

    if (comprehensionKind != NotGenerator && pc->lastYieldOffset != startYieldOffset) {
        errorAt(pc->lastYieldOffset, JSMSG_BAD_GENEXP_BODY, js_yield_str);
        return null();
    }

    return body;
}

template <typename ParseHandler>
typename ParseHandler::ListNodeType
Parser<ParseHandler>::arrayComprehension(uint32_t begin)
{
    Node inner = comprehension(NotGenerator);
    if (!inner)
        return null();

    if (!mustMatchToken(TOK_RB, JSMSG_BRACKET_AFTER_ARRAY_COMPREHENSION)) {
        return null();
    }

    ListNodeType comp = handler.newList(PNK_ARRAYCOMP, inner);
    if (!comp)
        return null();

    handler.setBeginPosition(comp, begin);
    handler.setEndPosition(comp, pos().end);

    return comp;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::generatorComprehension(uint32_t begin)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_FOR));

    // We have no problem parsing generator comprehensions inside lazy
    // functions, but the bytecode emitter currently can't handle them that way,
    // because when it goes to emit the code for the inner generator function,
    // it expects outer functions to have non-lazy scripts.
    if (!abortIfSyntaxParser())
        return null();

    Node genfn = generatorComprehensionLambda(begin);
    if (!genfn)
        return null();

    // Create a dummy argsList so that PNK_GENEXP can be handled as a full PN_BINARY call node
    Node argsList = handler.newArguments(pos());
    if (!argsList)
        return null();
    handler.setBeginPosition(argsList, pos().end);

    Node result = handler.newGenExp(genfn, argsList);
    if (!result)
        return null();
    handler.setBeginPosition(result, begin);
    handler.setEndPosition(result, pos().end);

    return result;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::assignExprWithoutYieldOrAwait(YieldHandling yieldHandling)
{
    uint32_t startYieldOffset = pc->lastYieldOffset;
    uint32_t startAwaitOffset = pc->lastAwaitOffset;
    Node res = assignExpr(InAllowed, yieldHandling, TripledotProhibited);
    if (res) {
        if (pc->lastYieldOffset != startYieldOffset) {
            errorAt(pc->lastYieldOffset, JSMSG_YIELD_IN_DEFAULT);
            return null();
        }
        if (pc->lastAwaitOffset != startAwaitOffset) {
            errorAt(pc->lastAwaitOffset, JSMSG_AWAIT_IN_DEFAULT);
            return null();
        }
    }
    return res;
}

template <typename ParseHandler>
typename ParseHandler::ListNodeType
Parser<ParseHandler>::argumentList(YieldHandling yieldHandling, bool* isSpread,
                                   PossibleError* possibleError /* = nullptr */)
{
    ListNodeType argsList = handler.newArguments(pos());
    if (!argsList)
        return null();

    bool matched;
    if (!tokenStream.matchToken(&matched, TOK_RP, TokenStream::Operand))
        return null();
    if (matched) {
        handler.setEndPosition(argsList, pos().end);
        return argsList;
    }

    while (true) {
        bool spread = false;
        uint32_t begin = 0;
        if (!tokenStream.matchToken(&matched, TOK_TRIPLEDOT, TokenStream::Operand))
            return null();
        if (matched) {
            spread = true;
            begin = pos().begin;
            *isSpread = true;
        }

        Node argNode = assignExpr(InAllowed, yieldHandling, TripledotProhibited, possibleError);
        if (!argNode)
            return null();
        if (spread) {
            argNode = handler.newSpread(begin, argNode);
            if (!argNode)
                return null();
        }

        handler.addList(argsList, argNode);

        bool matched;
        if (!tokenStream.matchToken(&matched, TOK_COMMA))
            return null();
        if (!matched)
            break;

        TokenKind tt;
        if (!tokenStream.peekToken(&tt, TokenStream::Operand))
            return null();
        if (tt == TOK_RP) {
            tokenStream.addModifierException(TokenStream::NoneIsOperand);
            break;
        }
    }

    if (!mustMatchToken(TOK_RP, JSMSG_PAREN_AFTER_ARGS)) {
        return null();
    }

    handler.setEndPosition(argsList, pos().end);
    return argsList;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::checkAndMarkSuperScope()
{
    if (!pc->sc()->allowSuperProperty())
        return false;
    pc->setSuperScopeNeedsHomeObject();
    return true;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::memberExpr(YieldHandling yieldHandling, TripledotHandling tripledotHandling,
                                 TokenKind tt, bool allowCallSyntax /* = true */,
                                 PossibleError* possibleError /* = nullptr */,
                                 InvokedPrediction invoked /* = PredictUninvoked */)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(tt));

    Node lhs;

    JS_CHECK_RECURSION(context, return null());

    /* Check for new expression first. */
    if (tt == TOK_NEW) {
        uint32_t newBegin = pos().begin;
        // Make sure this wasn't a |new.target| in disguise.
        BinaryNodeType newTarget;
        if (!tryNewTarget(&newTarget))
            return null();
        if (newTarget) {
            lhs = newTarget;
        } else {
            // Gotten by tryNewTarget
            tt = tokenStream.currentToken().type;
            Node ctorExpr = memberExpr(yieldHandling, TripledotProhibited, tt,
                                       /* allowCallSyntax = */ false,
                                       /* possibleError = */ nullptr, PredictInvoked);
            if (!ctorExpr)
                return null();

            // If we have encountered an optional chain, in the form of `new
            // ClassName?.()` then we need to throw, as this is disallowed
            // by the spec.
            bool optionalToken;
            if (!tokenStream.matchToken(&optionalToken, TOK_OPTCHAIN)) {
                return null();
            }
            if (optionalToken) {
                errorAt(newBegin, JSMSG_BAD_NEW_OPTIONAL);
                return null();
            }

            bool matched;
            if (!tokenStream.matchToken(&matched, TOK_LP))
                return null();

            bool isSpread = false;
            Node args;
            if (matched) {
                args = argumentList(yieldHandling, &isSpread);
            } else {
                args = handler.newArguments(pos());
            }
            
            if (!args)
                return null();

            lhs = handler.newNewExpression(newBegin, ctorExpr, args);
            if (!lhs)
                return null();

            if (isSpread)
                handler.setOp(lhs, JSOP_SPREADNEW);
        }
    } else if (tt == TOK_SUPER) {
        NameNodeType thisName = newThisName();
        if (!thisName)
            return null();
        lhs = handler.newSuperBase(thisName, pos());
        if (!lhs)
            return null();
    } else if (tt == TOK_IMPORT) {
        lhs = importExpr(yieldHandling, allowCallSyntax);
        if (!lhs)
            return null();
    } else {
        lhs = primaryExpr(yieldHandling, tripledotHandling, tt, possibleError, invoked);
        if (!lhs)
            return null();
    }

    MOZ_ASSERT_IF(handler.isSuperBase(lhs), tokenStream.isCurrentTokenType(TOK_SUPER));

    while (true) {
        if (!tokenStream.getToken(&tt))
            return null();
        if (tt == TOK_EOF)
            break;

        Node nextMember;
        if (tt == TOK_DOT) {
            if (!tokenStream.getToken(&tt))
                return null();
            if (TokenKindIsPossibleIdentifierName(tt)) {
                nextMember = memberPropertyAccess(lhs);
                if (!nextMember)
                    return null();
            } else {
                error(JSMSG_NAME_AFTER_DOT);
                return null();
            }
        } else if (tt == TOK_LB) {
            nextMember = memberElemAccess(lhs, yieldHandling);
            if (!nextMember)
                return null();
        } else if ((allowCallSyntax && tt == TOK_LP) ||
                   tt == TOK_TEMPLATE_HEAD ||
                   tt == TOK_NO_SUBS_TEMPLATE)
        {
            if (handler.isSuperBase(lhs)) {
                if (!pc->sc()->allowSuperCall()) {
                    error(JSMSG_BAD_SUPERCALL);
                    return null();
                }

                if (tt != TOK_LP) {
                    error(JSMSG_BAD_SUPER);
                    return null();
                }

                // Despite the fact that it's impossible to have |super()| in a
                // generator, we still inherit the yieldHandling of the
                // memberExpression, per spec. Curious.
                bool isSpread = false;
                Node args = argumentList(yieldHandling, &isSpread);
                if (!args)
                    return null();

                nextMember = handler.newSuperCall(lhs, args, isSpread);
                if (!nextMember)
                    return null();

                NameNodeType thisName = newThisName();
                if (!thisName)
                    return null();

                // XXX This was refactored out into memberSuperCall as part of
                // bug 1566143, but was not used elsewhere aside from here, so
                // I opted not to move this into a separate function. However,
                // I'm unsure if this will be needed eventually in the future.
                nextMember = handler.newSetThis(thisName, nextMember);
                if (!nextMember)
                    return null();

                if (!noteUsedName(context->names().dotInitializers))
                  return null();
            } else {
                nextMember = memberCall(tt, lhs, yieldHandling, possibleError);
                if (!nextMember)
                    return null();
            }
        } else {
            tokenStream.ungetToken();
            if (handler.isSuperBase(lhs))
                break;
            return lhs;
        }

        lhs = nextMember;
    }

    if (handler.isSuperBase(lhs)) {
        error(JSMSG_BAD_SUPER);
        return null();
    }

    return lhs;
}

template <typename ParseHandler>
typename ParseHandler::NameNodeType
Parser<ParseHandler>::newName(PropertyName* name)
{
    return newName(name, pos());
}

template <typename ParseHandler>
typename ParseHandler::NameNodeType
Parser<ParseHandler>::newName(PropertyName* name, TokenPos pos)
{
    return handler.newName(name, pos, context);
}

template <class ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::memberPropertyAccess(
    Node lhs, OptionalKind optionalKind /* = OptionalKind::NonOptional */)
{
    MOZ_ASSERT(TokenKindIsPossibleIdentifierName(tokenStream.currentToken().type));
    PropertyName* field = tokenStream.currentName();
    if (handler.isSuperBase(lhs) && !checkAndMarkSuperScope()) {
        error(JSMSG_BAD_SUPERPROP, "property");
        return null();
    }
    NameNodeType name = handler.newPropertyName(field, pos());
    if (!name)
        return null();    
    if (optionalKind == OptionalKind::Optional) {
        MOZ_ASSERT(!handler.isSuperBase(lhs));
        return handler.newOptionalPropertyAccess(lhs, name);
    }
    return handler.newPropertyAccess(lhs, name);
}

template <class ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::memberElemAccess(
    Node lhs, YieldHandling yieldHandling,
    OptionalKind optionalKind /* = OptionalKind::NonOptional */)
{
    MOZ_ASSERT(tokenStream.currentToken().type == TOK_LB);
    Node propExpr = expr(InAllowed, yieldHandling, TripledotProhibited);
    if (!propExpr) {
        return null();
    }

    if (!mustMatchToken(TOK_RB, JSMSG_BRACKET_IN_INDEX)) {
        return null();
    }

    if (handler.isSuperBase(lhs) && !checkAndMarkSuperScope()) {
        error(JSMSG_BAD_SUPERPROP, "member");
        return null();
    }

    if (optionalKind == OptionalKind::Optional) {
        MOZ_ASSERT(!handler.isSuperBase(lhs));
        return handler.newOptionalPropertyByValue(lhs, propExpr, pos().end);
    }
    return handler.newPropertyByValue(lhs, propExpr, pos().end);
}

template <class ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::memberCall(
    TokenKind tt, Node lhs, YieldHandling yieldHandling,
    PossibleError* possibleError /* = nullptr */,
    OptionalKind optionalKind /* = OptionalKind::NonOptional */)
{
    if (options().selfHostingMode && (handler.isPropertyAccess(lhs) ||
                                      handler.isOptionalPropertyAccess(lhs))) {
        error(JSMSG_SELFHOSTED_METHOD_CALL);
        return null();
    }

    MOZ_ASSERT(tt == TOK_LP ||
               tt == TOK_TEMPLATE_HEAD ||
               tt == TOK_NO_SUBS_TEMPLATE,
               "Unexpected token kind for member call");

    JSOp op = JSOP_CALL;
    bool maybeAsyncArrow = false;
    if (PropertyName* prop = handler.maybeDottedProperty(lhs)) {
        // Use the JSOP_FUN{APPLY,CALL} optimizations given the
        // right syntax.
        if (prop == context->names().apply) {
            op = JSOP_FUNAPPLY;
            if (pc->isFunctionBox()) {
                pc->functionBox()->usesApply = true;
            }
        } else if (prop == context->names().call) {
            op = JSOP_FUNCALL;
        }
    } else if (tt == TOK_LP && optionalKind == OptionalKind::NonOptional) {
        if (handler.isAsyncKeyword(lhs, context)) {
            // |async (| can be the start of an async arrow
            // function, so we need to defer reporting possible
            // errors from destructuring syntax. To give better
            // error messages, we only allow the AsyncArrowHead
            // part of the CoverCallExpressionAndAsyncArrowHead
            // syntax when the initial name is "async".
            maybeAsyncArrow = true;
        } else if (handler.isEvalAnyParentheses(lhs, context)) {
            // Select the right EVAL op and flag pc as having a
            // direct eval.
            op = pc->sc()->strict() ? JSOP_STRICTEVAL : JSOP_EVAL;
            pc->sc()->setBindingsAccessedDynamically();
            pc->sc()->setHasDirectEval();

            // In non-strict mode code, direct calls to eval can
            // add variables to the call object.
            if (pc->isFunctionBox() && !pc->sc()->strict()) {
                pc->functionBox()->setHasExtensibleScope();
            }

            // If we're in a method, mark the method as requiring
            // support for 'super', since direct eval code can use
            // it. (If we're not in a method, that's fine, so
            // ignore the return value.)
            checkAndMarkSuperScope();
        }
    }

    Node nextMember;
    if (tt == TOK_LP) {
        bool isSpread = false;
        PossibleError* asyncPossibleError = maybeAsyncArrow ?
                                            possibleError :
                                            nullptr;

        Node args = argumentList(yieldHandling, &isSpread, asyncPossibleError);
        if (!args) {
            return null();
        }

        if (isSpread) {
            if (op == JSOP_EVAL) {
                op = JSOP_SPREADEVAL;
            } else if (op == JSOP_STRICTEVAL) {
                op = JSOP_STRICTSPREADEVAL;
            } else {
                op = JSOP_SPREADCALL;
            }
        }

        if (optionalKind == OptionalKind::Optional) {
            nextMember = handler.newOptionalCall(lhs, args);
        } else {
            nextMember = handler.newCall(lhs, args);
        }
        if (!nextMember)
            return null();
    } else {
        ListNodeType args = handler.newArguments(pos());
        if (!args)
            return null();

        if (!taggedTemplate(yieldHandling, args, tt)) {
            return null();
        }
        if (optionalKind == OptionalKind::Optional) {
            error(JSMSG_BAD_OPTIONAL_TEMPLATE);
            return null();
        }

        nextMember = handler.newTaggedTemplate(lhs, args);
        if (!nextMember)
            return null();
    }
    handler.setOp(nextMember, op);

    return nextMember;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::checkLabelOrIdentifierReference(PropertyName* ident,
                                                      uint32_t offset,
                                                      YieldHandling yieldHandling,
                                                      TokenKind hint /* = TOK_LIMIT */)
{
    TokenKind tt;
    if (hint == TOK_LIMIT) {
        tt = ReservedWordTokenKind(ident);
    } else {
        MOZ_ASSERT(hint == ReservedWordTokenKind(ident), "hint doesn't match actual token kind");
        tt = hint;
    }

    if (!pc->sc()->allowArguments() && ident == context->names().arguments) {
        error(JSMSG_BAD_ARGUMENTS);
        return false;
    }

    if (tt == TOK_NAME || tt == TOK_PRIVATE_NAME)
        return true;
    if (TokenKindIsContextualKeyword(tt)) {
        if (tt == TOK_YIELD) {
            if (yieldHandling == YieldIsKeyword || versionNumber() >= JSVERSION_1_7) {
                errorAt(offset, JSMSG_RESERVED_ID, "yield");
                return false;
            }
            if (pc->sc()->needStrictChecks()) {
                if (!strictModeErrorAt(offset, JSMSG_RESERVED_ID, "yield"))
                    return false;
            }
            return true;
        }
        if (tt == TOK_AWAIT) {
            if (awaitIsKeyword() || awaitIsDisallowed()) {
                errorAt(offset, JSMSG_RESERVED_ID, "await");
                return false;
            }
            return true;
        }
        if (pc->sc()->needStrictChecks()) {
            if (tt == TOK_LET) {
                if (!strictModeErrorAt(offset, JSMSG_RESERVED_ID, "let"))
                    return false;
                return true;
            }
            if (tt == TOK_STATIC) {
                if (!strictModeErrorAt(offset, JSMSG_RESERVED_ID, "static"))
                    return false;
                return true;
            }
        }
        return true;
    }
    if (TokenKindIsStrictReservedWord(tt)) {
        if (pc->sc()->needStrictChecks()) {
            if (!strictModeErrorAt(offset, JSMSG_RESERVED_ID, ReservedWordToCharZ(tt)))
                return false;
        }
        return true;
    }
    if (TokenKindIsKeyword(tt) || TokenKindIsReservedWordLiteral(tt)) {
        errorAt(offset, JSMSG_INVALID_ID, ReservedWordToCharZ(tt));
        return false;
    }
    if (TokenKindIsFutureReservedWord(tt)) {
        errorAt(offset, JSMSG_RESERVED_ID, ReservedWordToCharZ(tt));
        return false;
    }
    MOZ_ASSERT_UNREACHABLE("Unexpected reserved word kind.");
    return false;
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::checkBindingIdentifier(PropertyName* ident,
                                             uint32_t offset,
                                             YieldHandling yieldHandling,
                                             TokenKind hint /* = TOK_LIMIT */)
{
    if (pc->sc()->needStrictChecks()) {
        if (ident == context->names().arguments) {
            if (!strictModeErrorAt(offset, JSMSG_BAD_STRICT_ASSIGN, "arguments"))
                return false;
            return true;
        }

        if (ident == context->names().eval) {
            if (!strictModeErrorAt(offset, JSMSG_BAD_STRICT_ASSIGN, "eval"))
                return false;
            return true;
        }
    }

    return checkLabelOrIdentifierReference(ident, offset, yieldHandling, hint);
}

template <typename ParseHandler>
PropertyName*
Parser<ParseHandler>::labelOrIdentifierReference(YieldHandling yieldHandling)
{
    // ES 2017 draft 12.1.1.
    //   StringValue of IdentifierName normalizes any Unicode escape sequences
    //   in IdentifierName hence such escapes cannot be used to write an
    //   Identifier whose code point sequence is the same as a ReservedWord.
    //
    // Use PropertyName* instead of TokenKind to reflect the normalization.

    // Unless the name contains escapes, we can reuse the current TokenKind
    // to determine if the name is a restricted identifier.
    TokenKind hint = !tokenStream.currentNameHasEscapes()
                     ? tokenStream.currentToken().type
                     : TOK_LIMIT;
    RootedPropertyName ident(context, tokenStream.currentName());
    if (!checkLabelOrIdentifierReference(ident, pos().begin, yieldHandling, hint))
        return nullptr;
    return ident;
}

template <typename ParseHandler>
PropertyName*
Parser<ParseHandler>::bindingIdentifier(YieldHandling yieldHandling)
{
    TokenKind hint = !tokenStream.currentNameHasEscapes()
                     ? tokenStream.currentToken().type
                     : TOK_LIMIT;
    RootedPropertyName ident(context, tokenStream.currentName());
    if (!checkBindingIdentifier(ident, pos().begin, yieldHandling, hint))
        return nullptr;
    return ident;
}

template <typename ParseHandler>
typename ParseHandler::NameNodeType
Parser<ParseHandler>::identifierReference(Handle<PropertyName*> name)
{
    NameNodeType id = newName(name);
    if (!id)
        return null();

    if (!noteUsedName(name))
        return null();

    return id;
}

template <typename ParseHandler>
typename ParseHandler::NameNodeType
Parser<ParseHandler>::stringLiteral()
{
    return handler.newStringLiteral(stopStringCompression(), pos());
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::noSubstitutionTaggedTemplate()
{
    if (tokenStream.hasInvalidTemplateEscape()) {
        tokenStream.clearInvalidTemplateEscape();
        return handler.newRawUndefinedLiteral(pos());
    }

    return handler.newTemplateStringLiteral(stopStringCompression(), pos());
}

template <typename ParseHandler>
typename ParseHandler::NameNodeType
Parser<ParseHandler>::noSubstitutionUntaggedTemplate()
{
    if (!tokenStream.checkForInvalidTemplateEscapeError())
        return null();

    return handler.newTemplateStringLiteral(stopStringCompression(), pos());
}

template <typename ParseHandler>
JSAtom * Parser<ParseHandler>::stopStringCompression() {
    JSAtom* atom = tokenStream.currentToken().atom();

    // Large strings are fast to parse but slow to compress. Stop compression on
    // them, so we don't wait for a long time for compression to finish at the
    // end of compilation.
    const size_t HUGE_STRING = 50000;
    if (sct && sct->active() && atom->length() >= HUGE_STRING)
        sct->abort();
    return atom;
}

template <typename ParseHandler>
typename ParseHandler::RegExpLiteralType
Parser<ParseHandler>::newRegExp()
{
    MOZ_ASSERT(!options().selfHostingMode);
    // Create the regexp even when doing a syntax parse, to check the regexp's syntax.
    const char16_t* chars = tokenStream.getTokenbuf().begin();
    size_t length = tokenStream.getTokenbuf().length();
    RegExpFlag flags = tokenStream.currentToken().regExpFlags();

    Rooted<RegExpObject*> reobj(context);
    reobj = RegExpObject::create(context, chars, length, flags, &tokenStream, alloc);
    if (!reobj)
        return null();

    return handler.newRegExp(reobj, pos(), *this);
}

template <>
BigIntLiteral*
Parser<FullParseHandler>::newBigInt()
{
    // The token's charBuffer contains the DecimalIntegerLiteral or
    // NumericLiteralBase production, and as such does not include the
    // BigIntLiteralSuffix (the trailing "n").  Note that NumericLiteralBase
    // productions may start with 0[bBoOxX], indicating binary/octal/hex.
    const auto& chars = tokenStream.getTokenbuf();
    mozilla::Range<const char16_t> source(chars.begin(), chars.length());

    BigInt* b = js::ParseBigIntLiteral(context, source);
    if (!b) {
        return null();
    }

    // newBigInt immediately puts "b" in a BigIntBox, which is allocated using
    // tempLifoAlloc, avoiding any potential GC.  Therefore it's OK to pass a
    // raw pointer.
    return handler.newBigInt(b, pos(), *this);
}

template <>
SyntaxParseHandler::BigIntLiteralType
Parser<SyntaxParseHandler>::newBigInt()
{
    // The tokenizer has already checked the syntax of the bigint.

    return handler.newBigInt();
}

template <typename ParseHandler>
void
Parser<ParseHandler>::checkDestructuringAssignmentTarget(Node expr, TokenPos exprPos,
                                                         PossibleError* possibleError)
{
    // Return early if a pending destructuring error is already present.
    if (possibleError->hasPendingDestructuringError())
        return;

    if (pc->sc()->needStrictChecks()) {
        if (handler.isArgumentsAnyParentheses(expr, context)) {
            if (pc->sc()->strict()) {
                possibleError->setPendingDestructuringErrorAt(exprPos,
                                                              JSMSG_BAD_STRICT_ASSIGN_ARGUMENTS);
            } else {
                possibleError->setPendingDestructuringWarningAt(exprPos,
                                                                JSMSG_BAD_STRICT_ASSIGN_ARGUMENTS);
            }
            return;
        }

        if (handler.isEvalAnyParentheses(expr, context)) {
            if (pc->sc()->strict()) {
                possibleError->setPendingDestructuringErrorAt(exprPos,
                                                              JSMSG_BAD_STRICT_ASSIGN_EVAL);
            } else {
                possibleError->setPendingDestructuringWarningAt(exprPos,
                                                                JSMSG_BAD_STRICT_ASSIGN_EVAL);
            }
            return;
        }
    }

    // The expression must be either a simple assignment target, i.e. a name
    // or a property accessor, or a nested destructuring pattern.
    if (!handler.isUnparenthesizedDestructuringPattern(expr) &&
        !handler.isNameAnyParentheses(expr) &&
        !handler.isPropertyAccess(expr))
    {
        // Parentheses are forbidden around destructuring *patterns* (but
        // allowed around names). Use our nicer error message for
        // parenthesized, nested patterns.
        if (handler.isParenthesizedDestructuringPattern(expr))
            possibleError->setPendingDestructuringErrorAt(exprPos, JSMSG_BAD_DESTRUCT_PARENS);
        else
            possibleError->setPendingDestructuringErrorAt(exprPos, JSMSG_BAD_DESTRUCT_TARGET);
    }
}

template <typename ParseHandler>
void
Parser<ParseHandler>::checkDestructuringAssignmentElement(Node expr, TokenPos exprPos,
                                                                 PossibleError* possibleError)
{
    // ES2018 draft rev 0719f44aab93215ed9a626b2f45bd34f36916834
    // 12.15.5 Destructuring Assignment
    //
    // AssignmentElement[Yield, Await]:
    //   DestructuringAssignmentTarget[?Yield, ?Await]
    //   DestructuringAssignmentTarget[?Yield, ?Await] Initializer[+In, ?Yield, ?Await]

    // If |expr| is an assignment element with an initializer expression, its
    // destructuring assignment target was already validated in assignExpr().
    // Otherwise we need to check that |expr| is a valid destructuring target.
    if (!handler.isUnparenthesizedAssignment(expr))
        checkDestructuringAssignmentTarget(expr, exprPos, possibleError);
}

template <typename ParseHandler>
typename ParseHandler::ListNodeType
Parser<ParseHandler>::arrayInitializer(YieldHandling yieldHandling, PossibleError* possibleError)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LB));

    uint32_t begin = pos().begin;
    ListNodeType literal = handler.newArrayLiteral(begin);
    if (!literal)
        return null();

    TokenKind tt;
    if (!tokenStream.getToken(&tt, TokenStream::Operand))
        return null();

    // Handle an ES6-era array comprehension first.
    if (tt == TOK_FOR)
        return arrayComprehension(begin);

    if (tt == TOK_RB) {
        /*
         * Mark empty arrays as non-constant, since we cannot easily
         * determine their type.
         */
        handler.setListHasNonConstInitializer(literal);
    } else {
        tokenStream.ungetToken();

        uint32_t index = 0;
        TokenStream::Modifier modifier = TokenStream::Operand;
        for (; ; index++) {
            if (index >= NativeObject::MAX_DENSE_ELEMENTS_COUNT) {
                error(JSMSG_ARRAY_INIT_TOO_BIG);
                return null();
            }

            TokenKind tt;
            if (!tokenStream.peekToken(&tt, TokenStream::Operand))
                return null();
            if (tt == TOK_RB)
                break;

            if (tt == TOK_COMMA) {
                tokenStream.consumeKnownToken(TOK_COMMA, TokenStream::Operand);
                if (!handler.addElision(literal, pos()))
                    return null();
            } else if (tt == TOK_TRIPLEDOT) {
                tokenStream.consumeKnownToken(TOK_TRIPLEDOT, TokenStream::Operand);
                uint32_t begin = pos().begin;

                TokenPos innerPos;
                if (!tokenStream.peekTokenPos(&innerPos, TokenStream::Operand))
                    return null();

                Node inner = assignExpr(InAllowed, yieldHandling, TripledotProhibited,
                                        possibleError);
                if (!inner)
                    return null();
                if (possibleError)
                    checkDestructuringAssignmentTarget(inner, innerPos, possibleError);
                if (!handler.addSpreadElement(literal, begin, inner))
                    return null();
            } else {
                TokenPos elementPos;
                if (!tokenStream.peekTokenPos(&elementPos, TokenStream::Operand))
                    return null();
                Node element = assignExpr(InAllowed, yieldHandling, TripledotProhibited,
                                          possibleError);
                if (!element)
                    return null();
                if (possibleError)
                    checkDestructuringAssignmentElement(element, elementPos, possibleError);
                if (foldConstants && !FoldConstants(context, &element, this))
                    return null();
                handler.addArrayElement(literal, element);
            }

            if (tt != TOK_COMMA) {
                /* If we didn't already match TOK_COMMA in above case. */
                bool matched;
                if (!tokenStream.matchToken(&matched, TOK_COMMA))
                    return null();
                if (!matched) {
                    modifier = TokenStream::None;
                    break;
                }
                if (tt == TOK_TRIPLEDOT && possibleError)
                    possibleError->setPendingDestructuringErrorAt(pos(), JSMSG_REST_WITH_COMMA);
            }
        }

        if (!mustMatchToken(TOK_RB, modifier,
                            [this, begin](TokenKind actual) {
                                this->reportMissingClosing(JSMSG_BRACKET_AFTER_LIST,
                                                           JSMSG_BRACKET_OPENED, begin);
                            }))
        {
            return null();
        }
    }
    handler.setEndPosition(literal, pos().end);
    return literal;
}

static JSAtom*
DoubleToAtom(ExclusiveContext* cx, double value)
{
    // This is safe because doubles can not be moved.
    Value tmp = DoubleValue(value);
    return ToAtom<CanGC>(cx, HandleValue::fromMarkedLocation(&tmp));
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::propertyName(YieldHandling yieldHandling,
                                   PropertyNameContext propertyNameContext,
                                   const Maybe<DeclarationKind>& maybeDecl, ListNodeType propList,
                                   MutableHandleAtom propAtom)
{
    // PropertyName[Yield, Await]:
    //   LiteralPropertyName
    //   ComputedPropertyName[?Yield, ?Await]
    //
    // LiteralPropertyName:
    //   IdentifierName
    //   StringLiteral
    //   NumericLiteral
    TokenKind ltok = tokenStream.currentToken().type;

    propAtom.set(nullptr);
    switch (ltok) {
      case TOK_NUMBER:
        propAtom.set(DoubleToAtom(context, tokenStream.currentToken().number()));
        if (!propAtom.get())
            return null();
        return newNumber(tokenStream.currentToken());

      case TOK_STRING: {
        propAtom.set(tokenStream.currentToken().atom());
        uint32_t index;
        if (propAtom->isIndex(&index)) {
            return handler.newNumber(index, NoDecimal, pos());
        }
        return stringLiteral();
      }

      case TOK_LB:
        return computedPropertyName(yieldHandling, maybeDecl, propList);

      default: {
        if (!TokenKindIsPossibleIdentifierName(ltok)) {
            error(JSMSG_UNEXPECTED_TOKEN, "property name", TokenKindToDesc(ltok));
            return null();
        }

        propAtom.set(tokenStream.currentName());
        return handler.newObjectLiteralPropertyName(propAtom, pos());
      }
    }
}

// True if `kind` can be the first token of a PropertyName.
static bool
TokenKindCanStartPropertyName(TokenKind tt)
{
  return TokenKindIsPossibleIdentifierName(tt) || tt == TOK_STRING ||
         tt == TOK_NUMBER || tt == TOK_LB ||
         tt == TOK_MUL;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::propertyOrMethodName(YieldHandling yieldHandling,
                                   PropertyNameContext propertyNameContext,
                                   const Maybe<DeclarationKind>& maybeDecl, ListNodeType propList,
                                   PropertyType* propType, MutableHandleAtom propAtom)
{
    // We're parsing an object literal, class, or destructuring pattern;
    // propertyNameContext tells which one. This method parses any of the
    // following, storing the corresponding PropertyType in `*propType` to tell
    // the caller what we parsed:
    //
    //     async [no LineTerminator here] PropertyName
    //                            ==> PropertyType::AsyncMethod
    //     async [no LineTerminator here] * PropertyName
    //                            ==> PropertyType::AsyncGeneratorMethod
    //     * PropertyName         ==> PropertyType::GeneratorMethod
    //     get PropertyName       ==> PropertyType::Getter
    //     set PropertyName       ==> PropertyType::Setter
    //     PropertyName :         ==> PropertyType::Normal
    //     PropertyName           ==> see below
    //
    // In the last case, where there's not a `:` token to consume, we peek at
    // (but don't consume) the next token to decide how to set `*propType`.
    //
    //     `,` or `}`             ==> PropertyType::Shorthand
    //     `(`                    ==> PropertyType::Method
    //     `=`, not in a class    ==> PropertyType::CoverInitializedName
    //     '=', in a class        ==> PropertyType::Field
    //     any token, in a class  ==> PropertyType::Field (ASI)
    //
    // The caller must check `*propType` and throw if whatever we parsed isn't
    // allowed here (for example, a getter in a destructuring pattern).
    //
    // This method does *not* match `static` (allowed in classes) or `...`
    // (allowed in object literals and patterns). The caller must take care of
    // those before calling this method.

    TokenKind ltok;
    if (!tokenStream.getToken(&ltok))
        return null();

    MOZ_ASSERT(ltok != TOK_RC, "caller should have handled TOK_RC");

    // Accept `async` and/or `*`, indicating an async or generator method;
    // or `get` or `set`, indicating an accessor.
    bool isGenerator = false;
    bool isAsync = false;
    bool isGetter = false;
    bool isSetter = false;

    if (ltok == TOK_ASYNC) {
        // `async` is also a PropertyName by itself (it's a conditional keyword),
        // so peek at the next token to see if we're really looking at a method.
        TokenKind tt = TOK_EOF;
        if (!tokenStream.peekTokenSameLine(&tt))
            return null();
        if (TokenKindCanStartPropertyName(tt))
        {
            isAsync = true;
            tokenStream.consumeKnownToken(tt);
            ltok = tt;
        }
    }

    if (ltok == TOK_MUL) {
        isGenerator = true;
        if (!tokenStream.getToken(&ltok))
            return null();
    }

    if (!isAsync && !isGenerator &&
        (ltok == TOK_GET || ltok == TOK_SET)) {
        // We have parsed |get| or |set|. Look for an accessor property
        // name next.
        TokenKind tt;
        if (!tokenStream.peekToken(&tt))
            return null();
        if (TokenKindCanStartPropertyName(tt)) {
            tokenStream.consumeKnownToken(tt);
            isGetter = (ltok == TOK_GET);
            isSetter = (ltok == TOK_SET);
        }
    }

    Node propName = propertyName(yieldHandling, propertyNameContext, maybeDecl,
                                 propList, propAtom);
    if (!propName)
        return null();

    // Grab the next token following the property/method name.
    // (If this isn't a colon, we're going to either put it back or throw.)
    TokenKind tt;
    if (!tokenStream.getToken(&tt))
        return null();

    if (tt == TOK_COLON) {
        if (isGenerator || isAsync || isGetter || isSetter) {
            error(JSMSG_BAD_PROP_ID);
            return null();
        }
        *propType = PropertyType::Normal;
        return propName;
    }

    if (propertyNameContext != PropertyNameInClass &&
        TokenKindIsPossibleIdentifierName(ltok) &&
        (tt == TOK_COMMA || tt == TOK_RC || tt == TOK_ASSIGN))
    {
        if (isGenerator || isAsync || isGetter || isSetter) {
            error(JSMSG_BAD_PROP_ID);
            return null();
        }
        tokenStream.ungetToken();
        *propType = tt == TOK_ASSIGN ?
                          PropertyType::CoverInitializedName :
                          PropertyType::Shorthand;
        return propName;
    }

    if (tt == TOK_LP) {
        tokenStream.ungetToken();
        if (isGenerator && isAsync)
            *propType = PropertyType::AsyncGeneratorMethod;
        else if (isGenerator)
            *propType = PropertyType::GeneratorMethod;
        else if (isAsync)
            *propType = PropertyType::AsyncMethod;
        else if (isGetter)
            *propType = PropertyType::Getter;
        else if (isSetter)
            *propType = PropertyType::Setter;
        else
            *propType = PropertyType::Method;
        return propName;
    }

    if (propertyNameContext == PropertyNameInClass) {
        if (isGenerator || isAsync || isGetter || isSetter) {
            error(JSMSG_BAD_PROP_ID);
            return null();
        }
        tokenStream.ungetToken();
        *propType = PropertyType::Field;
        return propName;
    }

    error(JSMSG_COLON_AFTER_ID);
    return null();
}

template <typename ParseHandler>
typename ParseHandler::UnaryNodeType
Parser<ParseHandler>::computedPropertyName(YieldHandling yieldHandling,
                                           const Maybe<DeclarationKind>& maybeDecl,
                                           ListNodeType literal)
{
    uint32_t begin = pos().begin;

    if (maybeDecl) {
        if (*maybeDecl == DeclarationKind::FormalParameter)
            pc->functionBox()->hasParameterExprs = true;
    } else {
        handler.setListHasNonConstInitializer(literal);
    }

    Node assignNode = assignExpr(InAllowed, yieldHandling, TripledotProhibited);
    if (!assignNode)
        return null();

    if (!mustMatchToken(TOK_RB, JSMSG_COMP_PROP_UNTERM_EXPR)) {
        return null();
    }
    return handler.newComputedName(assignNode, begin, pos().end);
}

template <typename ParseHandler>
typename ParseHandler::ListNodeType
Parser<ParseHandler>::objectLiteral(YieldHandling yieldHandling, PossibleError* possibleError)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LC));

    uint32_t openedPos = pos().begin;

    ListNodeType literal = handler.newObjectLiteral(pos().begin);
    if (!literal)
        return null();

    bool seenPrototypeMutation = false;
    bool seenCoverInitializedName = false;
    Maybe<DeclarationKind> declKind = Nothing();
    RootedAtom propAtom(context);
    for (;;) {
        TokenKind tt;
        if (!tokenStream.peekToken(&tt))
            return null();
        if (tt == TOK_RC)
            break;

        if (tt == TOK_TRIPLEDOT) {
            // object spread
            tokenStream.consumeKnownToken(TOK_TRIPLEDOT);
            uint32_t begin = pos().begin;

            TokenPos innerPos;
            if (!tokenStream.peekTokenPos(&innerPos, TokenStream::Operand))
                return null();

            Node inner = assignExpr(InAllowed, yieldHandling, TripledotProhibited,
                                    possibleError);
            if (!inner)
                return null();
            if (possibleError)
                checkDestructuringAssignmentTarget(inner, innerPos, possibleError);
            if (!handler.addSpreadProperty(literal, begin, inner))
                return null();
        } else {
            TokenPos namePos = tokenStream.nextToken().pos;

            PropertyType propType;
            Node propName = propertyOrMethodName(yieldHandling, PropertyNameInLiteral, declKind,
                                                 literal, &propType, &propAtom);
            if (!propName)

                return null();

            if (propType == PropertyType::Normal) {
                TokenPos exprPos;
                if (!tokenStream.peekTokenPos(&exprPos, TokenStream::Operand))
                    return null();

                Node propExpr = assignExpr(InAllowed, yieldHandling, TripledotProhibited,
                                           possibleError);
                if (!propExpr)
                    return null();

                handler.checkAndSetIsDirectRHSAnonFunction(propExpr);

                if (possibleError)
                    checkDestructuringAssignmentElement(propExpr, exprPos, possibleError);

                if (foldConstants && !FoldConstants(context, &propExpr, this))
                    return null();

                if (propAtom == context->names().proto) {
                    if (seenPrototypeMutation) {
                        // Directly report the error when we're not in a
                        // destructuring context.
                        if (!possibleError) {
                            errorAt(namePos.begin, JSMSG_DUPLICATE_PROTO_PROPERTY);
                            return null();
                        }

                        // Otherwise delay error reporting until we've
                        // determined whether or not we're destructuring.
                        possibleError->setPendingExpressionErrorAt(namePos,
                                                                   JSMSG_DUPLICATE_PROTO_PROPERTY);
                    }
                    seenPrototypeMutation = true;

                    // Note: this occurs *only* if we observe TOK_COLON!  Only
                    // __proto__: v mutates [[Prototype]].  Getters, setters,
                    // method/generator definitions, computed property name
                    // versions of all of these, and shorthands do not.
                    if (!handler.addPrototypeMutation(literal, namePos.begin, propExpr))
                        return null();
                } else {
                    if (!handler.isConstant(propExpr))
                        handler.setListHasNonConstInitializer(literal);

                    if (!handler.addPropertyDefinition(literal, propName, propExpr))
                        return null();
                }
            } else if (propType == PropertyType::Shorthand) {
                /*
                 * Support, e.g., |({x, y} = o)| as destructuring shorthand
                 * for |({x: x, y: y} = o)|, and |var o = {x, y}| as
                 * initializer shorthand for |var o = {x: x, y: y}|.
                 */
                Rooted<PropertyName*> name(context, identifierReference(yieldHandling));
                if (!name)
                    return null();

                NameNodeType nameExpr = identifierReference(name);
                if (!nameExpr)
                    return null();

                if (possibleError)
                    checkDestructuringAssignmentTarget(nameExpr, namePos, possibleError);

                if (!handler.addShorthand(literal, handler.asName(propName), nameExpr))
                    return null();
            } else if (propType == PropertyType::CoverInitializedName) {
                /*
                 * Support, e.g., |({x=1, y=2} = o)| as destructuring
                 * shorthand with default values, as per ES6 12.14.5
                 */
                Rooted<PropertyName*> name(context, identifierReference(yieldHandling));
                if (!name)
                    return null();

                Node lhs = identifierReference(name);
                if (!lhs)
                    return null();

                tokenStream.consumeKnownToken(TOK_ASSIGN);

                if (!seenCoverInitializedName) {
                    // "shorthand default" or "CoverInitializedName" syntax is
                    // only valid in the case of destructuring.
                    seenCoverInitializedName = true;

                    if (!possibleError) {
                        // Destructuring defaults are definitely not allowed
                        // in this object literal, because of something the
                        // caller knows about the preceding code. For example,
                        // maybe the preceding token is an operator:
                        // |x + {y=z}|.
                        error(JSMSG_COLON_AFTER_ID);
                        return null();
                    }

                    // Here we set a pending error so that later in the parse,
                    // once we've determined whether or not we're
                    // destructuring, the error can be reported or ignored
                    // appropriately.
                    possibleError->setPendingExpressionErrorAt(pos(), JSMSG_COLON_AFTER_ID);
                }

                if (const char* chars = handler.nameIsArgumentsEvalAnyParentheses(lhs, context)) {
                    // |chars| is "arguments" or "eval" here.
                    if (!strictModeErrorAt(namePos.begin, JSMSG_BAD_STRICT_ASSIGN, chars))
                        return null();
                }

                Node rhs = assignExpr(InAllowed, yieldHandling, TripledotProhibited);
                if (!rhs)
                    return null();

                handler.checkAndSetIsDirectRHSAnonFunction(rhs);

                BinaryNodeType propExpr = handler.newAssignment(PNK_ASSIGN, lhs, rhs, JSOP_NOP);
                if (!propExpr)
                    return null();

                if (!handler.addPropertyDefinition(literal, propName, propExpr))
                    return null();
            } else {
                RootedAtom funName(context);
                if (!tokenStream.isCurrentTokenType(TOK_RB)) {
                    funName = propAtom;

                    if (propType == PropertyType::Getter || propType == PropertyType::Setter) {
                        funName = prefixAccessorName(propType, propAtom);
                        if (!funName)
                            return null();
                    }
                }

                FunctionNodeType funNode = methodDefinition(namePos.begin, propType, funName);
                if (!funNode)
                    return null();

                handler.checkAndSetIsDirectRHSAnonFunction(funNode);

                JSOp op = JSOpFromPropertyType(propType);
                if (!handler.addObjectMethodDefinition(literal, propName, funNode, op))
                    return null();

                if (possibleError) {
                    possibleError->setPendingDestructuringErrorAt(namePos,
                                                                  JSMSG_BAD_DESTRUCT_TARGET);
                }
            }
        }

        bool matched;
        if (!tokenStream.matchToken(&matched, TOK_COMMA))
            return null();
        if (!matched)
            break;
        if (tt == TOK_TRIPLEDOT && possibleError)
            possibleError->setPendingDestructuringErrorAt(pos(), JSMSG_REST_WITH_COMMA);
    }

    if (!mustMatchToken(TOK_RC, TokenStream::None,
                        [this, openedPos](TokenKind actual) {
                            this->reportMissingClosing(JSMSG_CURLY_AFTER_LIST,
                                                       JSMSG_CURLY_OPENED, openedPos);
                        }))
    {
        return null();
    }

    handler.setEndPosition(literal, pos().end);
    return literal;
}

template <typename ParseHandler>
typename ParseHandler::FunctionNodeType
Parser<ParseHandler>::methodDefinition(uint32_t toStringStart, PropertyType propType,
                                       HandleAtom funName)
{
    FunctionSyntaxKind syntaxKind;
    switch (propType) {
      case PropertyType::Getter:
        syntaxKind = FunctionSyntaxKind::Getter;
        break;

      case PropertyType::GetterNoExpressionClosure:
        syntaxKind = FunctionSyntaxKind::GetterNoExpressionClosure;
        break;

      case PropertyType::Setter:
        syntaxKind = FunctionSyntaxKind::Setter;
        break;

      case PropertyType::SetterNoExpressionClosure:
        syntaxKind = FunctionSyntaxKind::SetterNoExpressionClosure;
        break;

      case PropertyType::Method:
      case PropertyType::GeneratorMethod:
      case PropertyType::AsyncMethod:
      case PropertyType::AsyncGeneratorMethod:
        syntaxKind = FunctionSyntaxKind::Method;
        break;

      case PropertyType::Constructor:
        syntaxKind = FunctionSyntaxKind::ClassConstructor;
        break;

      case PropertyType::DerivedConstructor:
        syntaxKind = FunctionSyntaxKind::DerivedClassConstructor;
        break;

      default:
        MOZ_CRASH("Parser: methodDefinition: unexpected property type");
    }

    GeneratorKind generatorKind = (propType == PropertyType::GeneratorMethod ||
                                   propType == PropertyType::AsyncGeneratorMethod)
                                  ? StarGenerator
                                  : NotGenerator;

    FunctionAsyncKind asyncKind = (propType == PropertyType::AsyncMethod ||
                                   propType == PropertyType::AsyncGeneratorMethod)
                                  ? AsyncFunction
                                  : SyncFunction;

    YieldHandling yieldHandling = GetYieldHandling(generatorKind);

    FunctionNodeType funNode = handler.newFunction(syntaxKind, pos());
    if (!funNode)
        return null();

    return functionDefinition(toStringStart, funNode, InAllowed, yieldHandling, funName,
                              syntaxKind, generatorKind, asyncKind);
}

template <typename ParseHandler>
bool
Parser<ParseHandler>::tryNewTarget(BinaryNodeType* newTarget)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_NEW));

    *newTarget = null();

    NullaryNodeType newHolder = handler.newPosHolder(pos());
    if (!newHolder)
        return false;

    uint32_t begin = pos().begin;

    // |new| expects to look for an operand, so we will honor that.
    TokenKind next;
    if (!tokenStream.getToken(&next, TokenStream::Operand))
        return false;

    // Don't unget the token, since lookahead cannot handle someone calling
    // getToken() with a different modifier. Callers should inspect currentToken().
    if (next != TOK_DOT)
        return true;

    if (!tokenStream.getToken(&next))
        return false;
    if (next != TOK_TARGET) {
        error(JSMSG_UNEXPECTED_TOKEN, "target", TokenKindToDesc(next));
        return false;
    }

    if (!pc->sc()->allowNewTarget()) {
        errorAt(begin, JSMSG_BAD_NEWTARGET);
        return false;
    }

    NullaryNodeType targetHolder = handler.newPosHolder(pos());
    if (!targetHolder)
        return false;

    *newTarget = handler.newNewTarget(newHolder, targetHolder);
    return !!*newTarget;
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::importExpr(YieldHandling yieldHandling, bool allowCallSyntax)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_IMPORT));

    Node importHolder = handler.newPosHolder(pos());
    if (!importHolder)
        return null();

    TokenKind next;
    if (!tokenStream.getToken(&next))
        return null();

    if (next == TOK_DOT) {
        if (!tokenStream.getToken(&next))
            return null();
        if (next != TOK_META) {
            error(JSMSG_UNEXPECTED_TOKEN, "meta", TokenKindToDesc(next));
            return null();
        }

        if (parseGoal() != ParseGoal::Module) {
            errorAt(pos().begin, JSMSG_IMPORT_META_OUTSIDE_MODULE);
            return null();
        }

        Node metaHolder = handler.newPosHolder(pos());
        if (!metaHolder)
            return null();

        return handler.newImportMeta(importHolder, metaHolder);
    } else if (next == TOK_LP && allowCallSyntax) {
        Node arg = assignExpr(InAllowed, yieldHandling, TripledotProhibited);
        if (!arg)
            return null();

        if (!mustMatchToken(TOK_RP, JSMSG_PAREN_AFTER_ARGS)) {
            return null();
        }

        if (!context->compartment()->runtimeFromAnyThread()->moduleDynamicImportHook && !abortIfSyntaxParser()) {
            return null();
        }

        return handler.newCallImport(importHolder, arg);
    } else {
        error(JSMSG_UNEXPECTED_TOKEN_NO_EXPECT, TokenKindToDesc(next));
        return null();
    }
}

template <class ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::primaryExpr(YieldHandling yieldHandling, TripledotHandling tripledotHandling,
                                  TokenKind tt, PossibleError* possibleError,
                                  InvokedPrediction invoked /* = PredictUninvoked */)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(tt));
    JS_CHECK_RECURSION(context, return null());

    switch (tt) {
      case TOK_FUNCTION:
        return functionExpr(pos().begin, invoked);

      case TOK_CLASS:
        return classDefinition(yieldHandling, ClassExpression, NameRequired);

      case TOK_LB:
        return arrayInitializer(yieldHandling, possibleError);

      case TOK_LC:
        return objectLiteral(yieldHandling, possibleError);

      case TOK_LP: {
        TokenKind next;
        if (!tokenStream.peekToken(&next, TokenStream::Operand))
            return null();

        if (next == TOK_RP) {
            // Not valid expression syntax, but this is valid in an arrow function
            // with no params: `() => body`.
            tokenStream.consumeKnownToken(next, TokenStream::Operand);

            if (!tokenStream.peekToken(&next))
                return null();
            if (next != TOK_ARROW) {
                error(JSMSG_UNEXPECTED_TOKEN, "expression", TokenKindToDesc(TOK_RP));
                return null();
            }

            // Now just return something that will allow parsing to continue.
            // It doesn't matter what; when we reach the =>, we will rewind and
            // reparse the whole arrow function. See Parser::assignExpr.
            return handler.newNullLiteral(pos());
        }

        if (next == TOK_FOR) {
            uint32_t begin = pos().begin;
            tokenStream.consumeKnownToken(next, TokenStream::Operand);
            return generatorComprehension(begin);
        }

        // Pass |possibleError| to support destructuring in arrow parameters.
        Node expr = exprInParens(InAllowed, yieldHandling, TripledotAllowed, possibleError);
        if (!expr)
            return null();
        if (!mustMatchToken(TOK_RP, JSMSG_PAREN_IN_PAREN)) {
            return null();
        }
        return handler.parenthesize(expr);
      }

      case TOK_TEMPLATE_HEAD:
        return templateLiteral(yieldHandling);

      case TOK_NO_SUBS_TEMPLATE:
        return noSubstitutionUntaggedTemplate();

      case TOK_STRING:
        return stringLiteral();

      default: {
        if (!TokenKindIsPossibleIdentifier(tt)) {
            error(JSMSG_UNEXPECTED_TOKEN, "expression", TokenKindToDesc(tt));
            return null();
        }

        if (tt == TOK_ASYNC) {
            TokenKind nextSameLine = TOK_EOF;
            if (!tokenStream.peekTokenSameLine(&nextSameLine))
                return null();

            if (nextSameLine == TOK_FUNCTION) {
                uint32_t toStringStart = pos().begin;
                tokenStream.consumeKnownToken(TOK_FUNCTION);
                return functionExpr(toStringStart, PredictUninvoked, AsyncFunction);
            }
        }

        Rooted<PropertyName*> name(context, identifierReference(yieldHandling));
        if (!name)
            return null();

        return identifierReference(name);
      }

      case TOK_REGEXP:
        return newRegExp();

      case TOK_NUMBER:
        return newNumber(tokenStream.currentToken());

      case TOK_BIGINT:
        return newBigInt();

      case TOK_TRUE:
        return handler.newBooleanLiteral(true, pos());
      case TOK_FALSE:
        return handler.newBooleanLiteral(false, pos());
      case TOK_THIS: {
        if (pc->isFunctionBox())
            pc->functionBox()->usesThis = true;
        NameNodeType thisName = null();
        if (pc->sc()->thisBinding() == ThisBinding::Function) {
            thisName = newThisName();
            if (!thisName)
                return null();
        }
        return handler.newThisLiteral(pos(), thisName);
      }
      case TOK_NULL:
        return handler.newNullLiteral(pos());

      case TOK_TRIPLEDOT: {
        // This isn't valid expression syntax, but it's valid in an arrow
        // function as a trailing rest param: `(a, b, ...rest) => body`.  Check
        // if it's directly under
        // CoverParenthesizedExpressionAndArrowParameterList, and check for a
        // name, closing parenthesis, and arrow, and allow it only if all are
        // present.
        if (tripledotHandling != TripledotAllowed) {
            error(JSMSG_UNEXPECTED_TOKEN, "expression", TokenKindToDesc(tt));
            return null();
        }

        TokenKind next;
        if (!tokenStream.getToken(&next))
            return null();

        if (next == TOK_LB || next == TOK_LC) {
            // Validate, but don't store the pattern right now. The whole arrow
            // function is reparsed in functionFormalParametersAndBody().
            if (!destructuringDeclaration(DeclarationKind::CoverArrowParameter, yieldHandling,
                                          next))
            {
                return null();
            }
        } else {
            // This doesn't check that the provided name is allowed, e.g. if
            // the enclosing code is strict mode code, any of "let", "yield",
            // or "arguments" should be prohibited.  Argument-parsing code
            // handles that.
            if (!TokenKindIsPossibleIdentifier(next)) {
                error(JSMSG_UNEXPECTED_TOKEN, "rest argument name", TokenKindToDesc(next));
                return null();
            }
        }

        if (!tokenStream.getToken(&next))
            return null();
        if (next != TOK_RP) {
            error(JSMSG_UNEXPECTED_TOKEN, "closing parenthesis", TokenKindToDesc(next));
            return null();
        }

        if (!tokenStream.peekToken(&next))
            return null();
        if (next != TOK_ARROW) {
            // Advance the scanner for proper error location reporting.
            tokenStream.consumeKnownToken(next);
            error(JSMSG_UNEXPECTED_TOKEN, "'=>' after argument list", TokenKindToDesc(next));
            return null();
        }

        tokenStream.ungetToken();  // put back right paren

        // Return an arbitrary expression node. See case TOK_RP above.
        return handler.newNullLiteral(pos());
      }
    }
}

template <typename ParseHandler>
typename ParseHandler::Node
Parser<ParseHandler>::exprInParens(InHandling inHandling, YieldHandling yieldHandling,
                                   TripledotHandling tripledotHandling,
                                   PossibleError* possibleError /* = nullptr */)
{
    MOZ_ASSERT(tokenStream.isCurrentTokenType(TOK_LP));
    return expr(inHandling, yieldHandling, tripledotHandling, possibleError, PredictInvoked);
}

bool
ParserBase::warnOnceAboutExprClosure()
{
    // We extensively use expression closures.
    // Disabling spew; see Issue #3061
#if 0
    JSContext* cx = context->maybeJSContext();
    if (!cx)
        return true;

    if (!cx->compartment()->warnedAboutExprClosure) {
        if (!warning(JSMSG_DEPRECATED_EXPR_CLOSURE))
            return false;
        cx->compartment()->warnedAboutExprClosure = true;
    }
#endif
    return true;
}

bool
ParserBase::warnOnceAboutForEach()
{
    JSContext* cx = context->maybeJSContext();
    if (!cx)
        return true;

    if (!cx->compartment()->warnedAboutForEach) {
        // Disabled warning spew.
        // if (!warning(JSMSG_DEPRECATED_FOR_EACH))
        //    return false;
        cx->compartment()->warnedAboutForEach = true;
    }
    return true;
}

template class Parser<FullParseHandler>;
template class Parser<SyntaxParseHandler>;

} /* namespace frontend */
} /* namespace js */
