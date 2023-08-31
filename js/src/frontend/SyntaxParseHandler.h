/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_SyntaxParseHandler_h
#define frontend_SyntaxParseHandler_h

#include "mozilla/Attributes.h"

#include <string.h>

#include "frontend/ParseNode.h"
#include "frontend/TokenStream.h"

namespace js {
namespace frontend {

template <typename ParseHandler>
class Parser;

// Parse handler used when processing the syntax in a block of code, to generate
// the minimal information which is required to detect syntax errors and allow
// bytecode to be emitted for outer functions.
//
// When parsing, we start at the top level with a full parse, and when possible
// only check the syntax for inner functions, so that they can be lazily parsed
// into bytecode when/if they first run. Checking the syntax of a function is
// several times faster than doing a full parse/emit, and lazy parsing improves
// both performance and memory usage significantly when pages contain large
// amounts of code that never executes (which happens often).
class SyntaxParseHandler
{
    // Remember the last encountered name or string literal during syntax parses.
    JSAtom* lastAtom;
    TokenPos lastStringPos;
    TokenStream& tokenStream;

  public:
    enum Node {
        NodeFailure = 0,
        NodeGeneric,
        NodeGetProp,
        NodeStringExprStatement,
        NodeReturn,
        NodeBreak,
        NodeThrow,
        NodeEmptyStatement,

        NodeVarDeclaration,
        NodeLexicalDeclaration,

        NodeFunctionDefinition,

        // This is needed for proper assignment-target handling.  ES6 formally
        // requires function calls *not* pass IsValidSimpleAssignmentTarget,
        // but at last check there were still sites with |f() = 5| and similar
        // in code not actually executed (or at least not executed enough to be
        // noticed).
        NodeFunctionCall,
        NodeOptionalFunctionCall,

        // Nodes representing *parenthesized* IsValidSimpleAssignmentTarget
        // nodes.  We can't simply treat all such parenthesized nodes
        // identically, because in assignment and increment/decrement contexts
        // ES6 says that parentheses constitute a syntax error.
        //
        //   var obj = {};
        //   var val;
        //   (val) = 3; (obj.prop) = 4;       // okay per ES5's little mind
        //   [(a)] = [3]; [(obj.prop)] = [4]; // invalid ES6 syntax
        //   // ...and so on for the other IsValidSimpleAssignmentTarget nodes
        //
        // We don't know in advance in the current parser when we're parsing
        // in a place where name parenthesization changes meaning, so we must
        // have multiple node values for these cases.
        NodeParenthesizedArgumentsName,
        NodeParenthesizedEvalName,
        NodeParenthesizedName,

        NodeDottedProperty,
        NodeOptionalDottedProperty,
        NodeElement,
        NodeOptionalElement,

        // Destructuring target patterns can't be parenthesized: |([a]) = [3];|
        // must be a syntax error.  (We can't use NodeGeneric instead of these
        // because that would trigger invalid-left-hand-side ReferenceError
        // semantics when SyntaxError semantics are desired.)
        NodeParenthesizedArray,
        NodeParenthesizedObject,

        // In rare cases a parenthesized |node| doesn't have the same semantics
        // as |node|.  Each such node has a special Node value, and we use a
        // different Node value to represent the parenthesized form.  See also
        // is{Unp,P}arenthesized*(Node), parenthesize(Node), and the various
        // functions that deal in NodeUnparenthesized* below.

        // Nodes representing unparenthesized names.
        NodeUnparenthesizedArgumentsName,
        NodeUnparenthesizedEvalName,
        NodeUnparenthesizedName,

        // Node representing the "async" name, which may actually be a
        // contextual keyword.
        NodePotentialAsyncKeyword,
        
        // Node representing a private name. Handled mostly like NodeUnparenthesizedName. 
        NodePrivateName,

        // Valuable for recognizing potential destructuring patterns.
        NodeUnparenthesizedArray,
        NodeUnparenthesizedObject,

        // The directive prologue at the start of a FunctionBody or ScriptBody
        // is the longest sequence (possibly empty) of string literal
        // expression statements at the start of a function.  Thus we need this
        // to treat |"use strict";| as a possible Use Strict Directive and
        // |("use strict");| as a useless statement.
        NodeUnparenthesizedString,

        // Legacy generator expressions of the form |(expr for (...))| and
        // array comprehensions of the form |[expr for (...)]|) don't permit
        // |expr| to be a comma expression.  Thus we need this to treat
        // |(a(), b for (x in []))| as a syntax error and
        // |((a(), b) for (x in []))| as a generator that calls |a| and then
        // yields |b| each time it's resumed.
        NodeUnparenthesizedCommaExpr,

        // Assignment expressions in condition contexts could be typos for
        // equality checks.  (Think |if (x = y)| versus |if (x == y)|.)  Thus
        // we need this to treat |if (x = y)| as a possible typo and
        // |if ((x = y))| as a deliberate assignment within a condition.
        //
        // (Technically this isn't needed, as these are *only* extraWarnings
        // warnings, and parsing with that option disables syntax parsing.  But
        // it seems best to be consistent, and perhaps the syntax parser will
        // eventually enforce extraWarnings and will require this then.)
        NodeUnparenthesizedAssignment,

        // This node is necessary to determine if the base operand in an
        // exponentiation operation is an unparenthesized unary expression.
        // We want to reject |-2 ** 3|, but still need to allow |(-2) ** 3|.
        NodeUnparenthesizedUnary,

        // This node is necessary to determine if the LHS of a property access is
        // super related.
        NodeSuperBase
    };

#define DECLARE_TYPE(typeName, longTypeName, asMethodName) \
    using longTypeName = Node;
FOR_EACH_PARSENODE_SUBCLASS(DECLARE_TYPE)
#undef DECLARE_TYPE

    using NullNode = Node;

    bool isPropertyAccess(Node node) {
        return node == NodeDottedProperty || node == NodeElement;
    }

    bool isOptionalPropertyAccess(Node node) {
        return node == NodeOptionalDottedProperty || node == NodeOptionalElement;
    }

    bool isFunctionCall(Node node) {
        // Note: super() is a special form, *not* a function call.
        return node == NodeFunctionCall;
    }

    static bool isUnparenthesizedDestructuringPattern(Node node) {
        return node == NodeUnparenthesizedArray || node == NodeUnparenthesizedObject;
    }

    static bool isParenthesizedDestructuringPattern(Node node) {
        // Technically this isn't a destructuring target at all -- the grammar
        // doesn't treat it as such.  But we need to know when this happens to
        // consider it a SyntaxError rather than an invalid-left-hand-side
        // ReferenceError.
        return node == NodeParenthesizedArray || node == NodeParenthesizedObject;
    }

    static bool isDestructuringPatternAnyParentheses(Node node) {
        return isUnparenthesizedDestructuringPattern(node) ||
                isParenthesizedDestructuringPattern(node);
    }

  public:
    SyntaxParseHandler(ExclusiveContext* cx, LifoAlloc& alloc,
                       TokenStream& tokenStream, Parser<SyntaxParseHandler>* syntaxParser,
                       LazyScript* lazyOuterFunction)
      : lastAtom(nullptr),
        tokenStream(tokenStream)
    {}

    static NullNode null() { return NodeFailure; }

#define DECLARE_AS(typeName, longTypeName, asMethodName) \
    static longTypeName asMethodName(Node node) { \
        return node; \
    }
FOR_EACH_PARSENODE_SUBCLASS(DECLARE_AS)
#undef DECLARE_AS

    void prepareNodeForMutation(Node node) {}
    void freeTree(Node node) {}

    void trace(JSTracer* trc) {}

    NameNodeType newName(PropertyName* name, const TokenPos& pos, ExclusiveContext* cx) {
        lastAtom = name;
        if (name == cx->names().arguments)
            return NodeUnparenthesizedArgumentsName;
        if (pos.begin + strlen("async") == pos.end && name == cx->names().async)
            return NodePotentialAsyncKeyword;
        if (name == cx->names().eval)
            return NodeUnparenthesizedEvalName;
        if (name->length() >= 1 && name->latin1OrTwoByteChar(0) == '#')
            return NodePrivateName;
        return NodeUnparenthesizedName;
    }

    UnaryNodeType newComputedName(Node expr, uint32_t start, uint32_t end) {
        return NodeGeneric;
    }

    NameNodeType newObjectLiteralPropertyName(JSAtom* atom, const TokenPos& pos) {
        return NodeUnparenthesizedName;
    }

    NumericLiteralType newNumber(double value, DecimalPoint decimalPoint, const TokenPos& pos) { return NodeGeneric; }
    BigIntLiteralType newBigInt() { return NodeGeneric; }
    BooleanLiteralType newBooleanLiteral(bool cond, const TokenPos& pos) { return NodeGeneric; }

    NameNodeType newStringLiteral(JSAtom* atom, const TokenPos& pos) {
        lastAtom = atom;
        lastStringPos = pos;
        return NodeUnparenthesizedString;
    }

    NameNodeType newTemplateStringLiteral(JSAtom* atom, const TokenPos& pos) {
        return NodeGeneric;
    }

    CallSiteNodeType newCallSiteObject(uint32_t begin) {
        return NodeGeneric;
    }

    void addToCallSiteObject(CallSiteNodeType callSiteObj, Node rawNode, Node cookedNode) {}

    UnaryNodeType newThisLiteral(const TokenPos& pos, Node thisName) { return NodeGeneric; }
    NullLiteralType newNullLiteral(const TokenPos& pos) { return NodeGeneric; }
    RawUndefinedLiteralType newRawUndefinedLiteral(const TokenPos& pos) { return NodeGeneric; }

    template <class Boxer>
    RegExpLiteralType newRegExp(RegExpObject* reobj, const TokenPos& pos, Boxer& boxer) { return NodeGeneric; }

    ConditionalExpressionType newConditional(Node cond, Node thenExpr, Node elseExpr) {
        return NodeGeneric;
    }

    Node newElision() { return NodeGeneric; }

    UnaryNodeType newDelete(uint32_t begin, Node expr) {
        return NodeUnparenthesizedUnary;
    }

    UnaryNodeType newTypeof(uint32_t begin, Node kid) {
        return NodeUnparenthesizedUnary;
    }

    NullaryNodeType newNullary(ParseNodeKind kind, JSOp op, const TokenPos& pos) {
        return NodeGeneric;
    }

    UnaryNodeType newUnary(ParseNodeKind kind, JSOp op, uint32_t begin, Node kid) {
        return NodeUnparenthesizedUnary;
    }

    UnaryNodeType newUpdate(ParseNodeKind kind, uint32_t begin, Node kid) {
        return NodeGeneric;
    }

    UnaryNodeType newSpread(uint32_t begin, Node kid) {
        return NodeGeneric;
    }

    Node newArrayPush(uint32_t begin, Node kid) {
        return NodeGeneric;
    }

    Node newBinary(ParseNodeKind kind, JSOp op = JSOP_NOP) { return NodeGeneric; }
    Node newBinary(ParseNodeKind kind, Node left, JSOp op = JSOP_NOP) { return NodeGeneric; }
    Node newBinary(ParseNodeKind kind, Node left, Node right, JSOp op = JSOP_NOP) {
        return NodeGeneric;
    }
    Node appendOrCreateList(ParseNodeKind kind, Node left, Node right,
                            ParseContext* pc, JSOp op = JSOP_NOP) {
        return NodeGeneric;
    }

    Node newTernary(ParseNodeKind kind, Node first, Node second, Node third, JSOp op = JSOP_NOP) {
        return NodeGeneric;
    }

    // Expressions

    ListNodeType newArrayLiteral(uint32_t begin) { return NodeUnparenthesizedArray; }
    MOZ_MUST_USE bool addElision(ListNodeType literal, const TokenPos& pos) { return true; }
    MOZ_MUST_USE bool addSpreadElement(ListNodeType literal, uint32_t begin, Node inner) { return true; }
    void addArrayElement(ListNodeType literal, Node element) { }

    BinaryNodeType newCall(Node callee, Node args) { return NodeFunctionCall; }
    BinaryNodeType newOptionalCall(Node callee, Node args) { return NodeOptionalFunctionCall; }
    ListNodeType newArguments(const TokenPos& pos) { return NodeGeneric; }
    BinaryNodeType newSuperCall(Node callee, Node args, bool isSpread) { return NodeGeneric; }
    BinaryNodeType newTaggedTemplate(Node callee, Node args) { return NodeGeneric; }
    Node newGenExp(Node callee, Node args) { return NodeGeneric; }

    ListNodeType newObjectLiteral(uint32_t begin) { return NodeUnparenthesizedObject; }
    ListNodeType newClassMemberList(uint32_t begin) { return NodeGeneric; }
    ClassNamesType newClassNames(Node outer, Node inner, const TokenPos& pos) { return NodeGeneric; }
    ClassNodeType newClass(Node name, Node heritage, Node methodBlock, const TokenPos& pos) { return NodeGeneric; }

    LexicalScopeNodeType newLexicalScope(Node body) {
        return NodeLexicalDeclaration;
    }

    BinaryNodeType newNewTarget(NullaryNodeType newHolder, NullaryNodeType targetHolder) {
        return NodeGeneric;
    }
    NullaryNodeType newPosHolder(const TokenPos& pos) { return NodeGeneric; }
    UnaryNodeType newSuperBase(Node thisName, const TokenPos& pos) { return NodeSuperBase; }

    MOZ_MUST_USE bool addPrototypeMutation(ListNodeType literal, uint32_t begin, Node expr) { return true; }
    MOZ_MUST_USE bool addPropertyDefinition(ListNodeType literal, Node name, Node expr) { return true; }
    MOZ_MUST_USE bool addShorthand(ListNodeType literal, NameNodeType name, NameNodeType expr) { return true; }
    MOZ_MUST_USE bool addSpreadProperty(ListNodeType literal, uint32_t begin, Node inner) { return true; }
    MOZ_MUST_USE bool addObjectMethodDefinition(ListNodeType literal, Node name, FunctionNodeType funNode, JSOp op) { return true; }
    MOZ_MUST_USE Node newClassMethodDefinition(Node key, FunctionNodeType funNode, JSOp op, bool isStatic) { return NodeGeneric; }
    MOZ_MUST_USE Node newClassFieldDefinition(Node name, FunctionNodeType initializer, bool isStatic) { return NodeGeneric; }
    MOZ_MUST_USE Node newStaticClassBlock(FunctionNodeType block) { return NodeGeneric; }
    MOZ_MUST_USE bool addClassMemberDefinition(ListNodeType memberList, Node member) { return true; }
    UnaryNodeType newYieldExpression(uint32_t begin, Node value) { return NodeGeneric; }
    UnaryNodeType newYieldStarExpression(uint32_t begin, Node value) { return NodeGeneric; }
    UnaryNodeType newAwaitExpression(uint32_t begin, Node value) { return NodeGeneric; }
    Node newOptionalChain(uint32_t begin, Node value) { return NodeGeneric; }

    // Statements

    ListNodeType newStatementList(const TokenPos& pos) { return NodeGeneric; }
    void addStatementToList(ListNodeType list, Node stmt) {}
    void addCaseStatementToList(ListNodeType list, CaseClauseType caseClause) {}
    MOZ_MUST_USE bool prependInitialYield(ListNodeType stmtList, Node genName) { return true; }
    UnaryNodeType newEmptyStatement(const TokenPos& pos) { return NodeEmptyStatement; }

    UnaryNodeType newExportDeclaration(Node kid, const TokenPos& pos) {
        return NodeGeneric;
    }
    BinaryNodeType newExportFromDeclaration(uint32_t begin, Node exportSpecSet, Node moduleSpec) {
        return NodeGeneric;
    }
    BinaryNodeType newExportDefaultDeclaration(Node kid, Node maybeBinding, const TokenPos& pos) {
        return NodeGeneric;
    }
    Node newImportMeta(Node importHolder, Node metaHolder) {
        return NodeGeneric;
    }
    Node newCallImport(Node importHolder, Node singleArg) {
        return NodeGeneric;
    }

    BinaryNodeType newSetThis(Node thisName, Node value) { return value; }

    UnaryNodeType newExprStatement(Node expr, uint32_t end) {
        return expr == NodeUnparenthesizedString ? NodeStringExprStatement : NodeGeneric;
    }

    TernaryNodeType newIfStatement(uint32_t begin, Node cond, Node thenBranch, Node elseBranch) {
        return NodeGeneric;
    }
    BinaryNodeType newDoWhileStatement(Node body, Node cond, const TokenPos& pos) { return NodeGeneric; }
    BinaryNodeType newWhileStatement(uint32_t begin, Node cond, Node body) { return NodeGeneric; }
    SwitchStatementType newSwitchStatement(uint32_t begin, Node discriminant,
                                           LexicalScopeNodeType lexicalForCaseList, bool hasDefault)
    {
        return NodeGeneric;
    }
    CaseClauseType newCaseOrDefault(uint32_t begin, Node expr, Node body) { return NodeGeneric; }
    ContinueStatementType newContinueStatement(PropertyName* label, const TokenPos& pos) { return NodeGeneric; }
    BreakStatementType newBreakStatement(PropertyName* label, const TokenPos& pos) { return NodeBreak; }
    UnaryNodeType newReturnStatement(Node expr, const TokenPos& pos) { return NodeReturn; }
    BinaryNodeType newWithStatement(uint32_t begin, Node expr, Node body) { return NodeGeneric; }

    LabeledStatementType newLabeledStatement(PropertyName* label, Node stmt, uint32_t begin) {
        return NodeGeneric;
    }

    UnaryNodeType newThrowStatement(Node expr, const TokenPos& pos) { return NodeThrow; }
    Node newTryStatement(uint32_t begin, Node body, ListNodeType catchList, Node finallyBlock) {
        return NodeGeneric;
    }
    DebuggerStatementType newDebuggerStatement(const TokenPos& pos) { return NodeGeneric; }

    NameNodeType newPropertyName(PropertyName* name, const TokenPos& pos) {
        lastAtom = name;
        return NodeGeneric;
    }

    PropertyAccessType newPropertyAccess(Node expr, NameNodeType key) {
        return NodeDottedProperty;
    }

    Node newOptionalPropertyAccess(Node expr, NameNodeType key) {
        return NodeOptionalDottedProperty;
    }

    PropertyByValueType newPropertyByValue(Node lhs, Node index, uint32_t end) { return NodeElement; }

    Node newOptionalPropertyByValue(Node pn, Node kid, uint32_t end) { return NodeOptionalElement; }

    MOZ_MUST_USE bool addCatchBlock(ListNodeType catchList, LexicalScopeNodeType lexicalScope,
                                    Node catchBinding, Node catchGuard, Node catchBody) { return true; }

    MOZ_MUST_USE bool setLastFunctionFormalParameterDefault(FunctionNodeType funNode, Node pn) { return true; }

    void checkAndSetIsDirectRHSAnonFunction(Node pn) {}

    FunctionNodeType newFunction(FunctionSyntaxKind syntaxKind, const TokenPos& pos) { return NodeFunctionDefinition; }

    bool setComprehensionLambdaBody(FunctionNodeType funNode, ListNodeType body) { return true; }
    void setFunctionFormalParametersAndBody(FunctionNodeType funNode, ListNodeType paramsBody) {}
    void setFunctionBody(FunctionNodeType funNode, LexicalScopeNodeType body) {}
    void setFunctionBox(FunctionNodeType funNode, FunctionBox* funbox) {}
    void addFunctionFormalParameter(FunctionNodeType funNode, Node argpn) {}

    ForNodeType newForStatement(uint32_t begin, TernaryNodeType forHead, Node body, unsigned iflags) {
        return NodeGeneric;
    }

    Node newComprehensionFor(uint32_t begin, Node forHead, Node body) {
        return NodeGeneric;
    }

    Node newComprehensionBinding(Node kid) {
        // Careful: we're asking this well after the name was parsed, so the
        // value returned may not correspond to |kid|'s actual name.  But it
        // *will* be truthy iff |kid| was a name, so we're safe.
        MOZ_ASSERT(isUnparenthesizedName(kid));
        return NodeGeneric;
    }

    TernaryNodeType newForHead(Node init, Node test, Node update, const TokenPos& pos) {
        return NodeGeneric;
    }

    TernaryNodeType newForInOrOfHead(ParseNodeKind kind, Node target, Node iteratedExpr, const TokenPos& pos) {
        return NodeGeneric;
    }

    MOZ_MUST_USE bool finishInitializerAssignment(NameNodeType nameNode, Node init) { return true; }

    void setBeginPosition(Node pn, Node oth) {}
    void setBeginPosition(Node pn, uint32_t begin) {}

    void setEndPosition(Node pn, Node oth) {}
    void setEndPosition(Node pn, uint32_t end) {}

    void setPosition(Node pn, const TokenPos& pos) {}
    TokenPos getPosition(Node pn) {
        return tokenStream.currentToken().pos;
    }

    ListNodeType newList(ParseNodeKind kind, JSOp op = JSOP_NOP) {
        MOZ_ASSERT(kind != PNK_VAR);
        MOZ_ASSERT(kind != PNK_LET);
        MOZ_ASSERT(kind != PNK_CONST);
        return NodeGeneric;
    }
    ListNodeType newList(ParseNodeKind kind, const TokenPos& pos, JSOp op = JSOP_NOP) {
        return newList(kind, op);
    }
    ListNodeType newList(ParseNodeKind kind, Node kid, JSOp op = JSOP_NOP) {
        return newList(kind, op);
    }

    ListNodeType newDeclarationList(ParseNodeKind kind, JSOp op = JSOP_NOP) {
        if (kind == PNK_VAR)
            return NodeVarDeclaration;
        MOZ_ASSERT(kind == PNK_LET || kind == PNK_CONST);
        return NodeLexicalDeclaration;
    }
    ListNodeType newDeclarationList(ParseNodeKind kind, Node kid, JSOp op = JSOP_NOP) {
        return newDeclarationList(kind, op);
    }

    bool isDeclarationList(Node node) {
        return node == NodeVarDeclaration || node == NodeLexicalDeclaration;
    }

    Node singleBindingFromDeclaration(ListNodeType decl) {
        MOZ_ASSERT(isDeclarationList(decl));

        // This is, unfortunately, very dodgy.  Obviously NodeVarDeclaration
        // and NodeLexicalDeclaration can store no info on the arbitrary
        // number of bindings it could contain.
        //
        // But this method is called only for cloning for-in/of declarations
        // as initialization targets.  That context simplifies matters.  If the
        // binding is a single name, it'll always syntax-parse (or it would
        // already have been rejected as assigning/binding a forbidden name).
        // Otherwise the binding is a destructuring pattern.  But syntax
        // parsing would *already* have aborted when it saw a destructuring
        // pattern.  So we can just say any old thing here, because the only
        // time we'll be wrong is a case that syntax parsing has already
        // rejected.  Use NodeUnparenthesizedName so the SyntaxParseHandler
        // Parser::cloneLeftHandSide can assert it sees only this.
        return NodeUnparenthesizedName;
    }

    Node newCatchList() {
        return newList(PNK_CATCHLIST, JSOP_NOP);
    }

    ListNodeType newCommaExpressionList(Node kid) {
        return NodeUnparenthesizedCommaExpr;
    }

    void addList(ListNodeType list, Node kid) {
        MOZ_ASSERT(list == NodeGeneric ||
                   list == NodeUnparenthesizedArray ||
                   list == NodeUnparenthesizedObject ||
                   list == NodeUnparenthesizedCommaExpr ||
                   list == NodeVarDeclaration ||
                   list == NodeLexicalDeclaration ||
                   list == NodeFunctionCall ||
                   list == NodeOptionalFunctionCall);
    }


    BinaryNodeType newNewExpression(uint32_t begin, Node ctor, Node args) {
        return NodeGeneric;
    }

    AssignmentNodeType newAssignment(ParseNodeKind kind, Node lhs, Node rhs, JSOp op) {
        if (kind == PNK_ASSIGN)
            return NodeUnparenthesizedAssignment;
        return newBinary(kind, lhs, rhs, op);
    }

    bool isUnparenthesizedCommaExpression(Node node) {
        return node == NodeUnparenthesizedCommaExpr;
    }

    bool isUnparenthesizedAssignment(Node node) {
        return node == NodeUnparenthesizedAssignment;
    }

    bool isUnparenthesizedUnaryExpression(Node node) {
        return node == NodeUnparenthesizedUnary;
    }

    bool isReturnStatement(Node node) {
        return node == NodeReturn;
    }

    bool isStatementPermittedAfterReturnStatement(Node pn) {
        return pn == NodeFunctionDefinition || pn == NodeVarDeclaration ||
               pn == NodeBreak ||
               pn == NodeThrow ||
               pn == NodeEmptyStatement;
    }

    bool isSuperBase(Node pn) {
        return pn == NodeSuperBase;
    }

    void setOp(Node pn, JSOp op) {}
    void setListHasNonConstInitializer(ListNodeType literal) {}
    MOZ_MUST_USE Node parenthesize(Node node) {
        // A number of nodes have different behavior upon parenthesization, but
        // only in some circumstances.  Convert these nodes to special
        // parenthesized forms.
        if (node == NodeUnparenthesizedArgumentsName)
            return NodeParenthesizedArgumentsName;
        if (node == NodeUnparenthesizedEvalName)
            return NodeParenthesizedEvalName;
        if (node == NodeUnparenthesizedName || node == NodePotentialAsyncKeyword)
            return NodeParenthesizedName;

        if (node == NodeUnparenthesizedArray)
            return NodeParenthesizedArray;
        if (node == NodeUnparenthesizedObject)
            return NodeParenthesizedObject;

        // Other nodes need not be recognizable after parenthesization; convert
        // them to a generic node.
        if (node == NodeUnparenthesizedString ||
            node == NodeUnparenthesizedCommaExpr ||
            node == NodeUnparenthesizedAssignment ||
            node == NodeUnparenthesizedUnary)
        {
            return NodeGeneric;
        }

        // In all other cases, the parenthesized form of |node| is equivalent
        // to the unparenthesized form: return |node| unchanged.
        return node;
    }
    template <typename NodeType>
    MOZ_MUST_USE NodeType setLikelyIIFE(NodeType node) {
        return node; // Remain in syntax-parse mode.
    }
    void setInDirectivePrologue(UnaryNodeType exprStmt) {}

    bool isConstant(Node pn) { return false; }

    bool isUnparenthesizedName(Node node) {
        return node == NodeUnparenthesizedArgumentsName ||
               node == NodeUnparenthesizedEvalName ||
               node == NodeUnparenthesizedName ||
               node == NodePotentialAsyncKeyword ||
               node == NodePrivateName;
    }

    bool isNameAnyParentheses(Node node) {
        if (isUnparenthesizedName(node))
            return true;
        return node == NodeParenthesizedArgumentsName ||
               node == NodeParenthesizedEvalName ||
               node == NodeParenthesizedName;
    }

    bool isPrivateName(Node node) {
        return node == NodePrivateName;
    }

    bool isArgumentsAnyParentheses(Node node, ExclusiveContext* cx) {
        return node == NodeUnparenthesizedArgumentsName || node == NodeParenthesizedArgumentsName;
    }

    bool isEvalAnyParentheses(Node node, ExclusiveContext* cx) {
        return node == NodeUnparenthesizedEvalName || node == NodeParenthesizedEvalName;
    }

    const char* nameIsArgumentsEvalAnyParentheses(Node node, ExclusiveContext* cx) {
        MOZ_ASSERT(isNameAnyParentheses(node),
                   "must only call this method on known names");

        if (isEvalAnyParentheses(node, cx))
            return js_eval_str;
        if (isArgumentsAnyParentheses(node, cx))
            return js_arguments_str;
        return nullptr;
    }

    bool isAsyncKeyword(Node node, ExclusiveContext* cx) {
        return node == NodePotentialAsyncKeyword;
    }

    PropertyName* maybeDottedProperty(Node node) {
        // Note: |super.apply(...)| is a special form that calls an "apply"
        // method retrieved from one value, but using a *different* value as
        // |this|.  It's not really eligible for the funapply/funcall
        // optimizations as they're currently implemented (assuming a single
        // value is used for both retrieval and |this|).
        if (node != NodeDottedProperty && node != NodeOptionalDottedProperty)
            return nullptr;
        return lastAtom->asPropertyName();
    }

    JSAtom* isStringExprStatement(Node pn, TokenPos* pos) {
        if (pn == NodeStringExprStatement) {
            *pos = lastStringPos;
            return lastAtom;
        }
        return nullptr;
    }

    bool canSkipLazyInnerFunctions() {
        return false;
    }
    bool canSkipLazyClosedOverBindings() {
        return false;
    }
    JSAtom* nextLazyClosedOverBinding() {
        MOZ_CRASH("SyntaxParseHandler::canSkipLazyClosedOverBindings must return false");
    }

    void adjustGetToSet(Node node) {}

    void disableSyntaxParser() {
    }
};

} // namespace frontend
} // namespace js

#endif /* frontend_SyntaxParseHandler_h */
