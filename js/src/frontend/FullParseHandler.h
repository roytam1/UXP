/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_FullParseHandler_h
#define frontend_FullParseHandler_h

#include "mozilla/Attributes.h"
#include "mozilla/PodOperations.h"

#include <cstddef> // std::nullptr_t
#include <string.h>

#include "frontend/ParseNode.h"
#include "frontend/SharedContext.h"

namespace js {
namespace frontend {

template <typename ParseHandler>
class Parser;

class SyntaxParseHandler;

// Parse handler used when generating a full parse tree for all code which the
// parser encounters.
class FullParseHandler
{
    ParseNodeAllocator allocator;
    TokenStream& tokenStream;

    ParseNode* allocParseNode(size_t size) {
        MOZ_ASSERT(size == sizeof(ParseNode));
        return static_cast<ParseNode*>(allocator.allocNode());
    }

    ParseNode* cloneNode(const ParseNode& other) {
        ParseNode* node = allocParseNode(sizeof(ParseNode));
        if (!node)
            return nullptr;
        mozilla::PodAssign(node, &other);
        return node;
    }

    /*
     * If this is a full parse to construct the bytecode for a function that
     * was previously lazily parsed, that lazy function and the current index
     * into its inner functions. We do not want to reparse the inner functions.
     */
    const Rooted<LazyScript*> lazyOuterFunction_;
    size_t lazyInnerFunctionIndex;
    size_t lazyClosedOverBindingIndex;

    const TokenPos& pos() {
        return tokenStream.currentToken().pos;
    }

  public:

    /*
     * If non-nullptr, points to a syntax parser which can be used for inner
     * functions. Cleared if language features not handled by the syntax parser
     * are encountered, in which case all future activity will use the full
     * parser.
     */
    Parser<SyntaxParseHandler>* syntaxParser;

    /* new_ methods for creating parse nodes. These report OOM on context. */
    JS_DECLARE_NEW_METHODS(new_, allocParseNode, inline)

    // FIXME: Use ListNode instead of ListNodeType as an alias (bug 1489008).
    using Node = ParseNode*;

#define DECLARE_TYPE(typeName, longTypeName, asMethodName) \
    using longTypeName = typeName*;
FOR_EACH_PARSENODE_SUBCLASS(DECLARE_TYPE)
#undef DECLARE_TYPE

    using NullNode = std::nullptr_t;

    bool isPropertyAccess(Node node) {
        return node->isKind(PNK_DOT) || node->isKind(PNK_ELEM);
    }

    bool isOptionalPropertyAccess(Node node) {
        return node->isKind(PNK_OPTDOT) || node->isKind(PNK_OPTELEM);
    }

    bool isFunctionCall(Node node) {
        // Note: super() is a special form, *not* a function call.
        return node->isKind(PNK_CALL);
    }

    static bool isUnparenthesizedDestructuringPattern(Node node) {
        return !node->isInParens() && (node->isKind(PNK_OBJECT) || node->isKind(PNK_ARRAY));
    }

    static bool isParenthesizedDestructuringPattern(Node node) {
        // Technically this isn't a destructuring pattern at all -- the grammar
        // doesn't treat it as such.  But we need to know when this happens to
        // consider it a SyntaxError rather than an invalid-left-hand-side
        // ReferenceError.
        return node->isInParens() && (node->isKind(PNK_OBJECT) || node->isKind(PNK_ARRAY));
    }

    static bool isDestructuringPatternAnyParentheses(Node node) {
        return isUnparenthesizedDestructuringPattern(node) ||
               isParenthesizedDestructuringPattern(node);
    }

    FullParseHandler(ExclusiveContext* cx, LifoAlloc& alloc,
                     TokenStream& tokenStream, Parser<SyntaxParseHandler>* syntaxParser,
                     LazyScript* lazyOuterFunction)
      : allocator(cx, alloc),
        tokenStream(tokenStream),
        lazyOuterFunction_(cx, lazyOuterFunction),
        lazyInnerFunctionIndex(0),
        lazyClosedOverBindingIndex(0),
        syntaxParser(syntaxParser)
    {}

    static NullNode null() { return NullNode(); }

#define DECLARE_AS(typeName, longTypeName, asMethodName) \
    static longTypeName asMethodName(Node node) { \
        return &node->as<typeName>(); \
    }
FOR_EACH_PARSENODE_SUBCLASS(DECLARE_AS)
#undef DECLARE_AS

    Node freeTree(Node pn) { return allocator.freeTree(pn); }
    void prepareNodeForMutation(Node pn) { return allocator.prepareNodeForMutation(pn); }
    const Token& currentToken() { return tokenStream.currentToken(); }

    NameNodeType newName(PropertyName* name, const TokenPos& pos, ExclusiveContext* cx)
    {
        return new_<NameNode>(PNK_NAME, JSOP_GETNAME, name, pos);
    }

    UnaryNodeType newComputedName(Node expr, uint32_t begin, uint32_t end) {
        TokenPos pos(begin, end);
        return new_<UnaryNode>(PNK_COMPUTED_NAME, JSOP_NOP, pos, expr);
    }

    NameNodeType newObjectLiteralPropertyName(JSAtom* atom, const TokenPos& pos) {
        return new_<NameNode>(PNK_OBJECT_PROPERTY_NAME, JSOP_NOP, atom, pos);
    }

    NumericLiteralType newNumber(double value, DecimalPoint decimalPoint, const TokenPos& pos) {
        return new_<NumericLiteral>(value, decimalPoint, pos);
    }

    BooleanLiteralType newBooleanLiteral(bool cond, const TokenPos& pos) {
        return new_<BooleanLiteral>(cond, pos);
    }

    NameNodeType newStringLiteral(JSAtom* atom, const TokenPos& pos) {
        return new_<NameNode>(PNK_STRING, JSOP_NOP, atom, pos);
    }

    NameNodeType newTemplateStringLiteral(JSAtom* atom, const TokenPos& pos) {
        return new_<NameNode>(PNK_TEMPLATE_STRING, JSOP_NOP, atom, pos);
    }

    CallSiteNodeType newCallSiteObject(uint32_t begin) {
        CallSiteNode* callSiteObj = new_<CallSiteNode>(begin);
        if (!callSiteObj)
            return null();

        ListNode* rawNodes = newArrayLiteral(callSiteObj->pn_pos.begin);
        if (!rawNodes)
            return null();

        addArrayElement(callSiteObj, rawNodes);

        return callSiteObj;
    }

    void addToCallSiteObject(CallSiteNodeType callSiteObj, Node rawNode,
                             Node cookedNode) {
        MOZ_ASSERT(callSiteObj->isKind(PNK_CALLSITEOBJ));

        addArrayElement(callSiteObj, cookedNode);
        addArrayElement(callSiteObj->rawNodes(), rawNode);

        /*
         * We don't know when the last noSubstTemplate will come in, and we
         * don't want to deal with this outside this method
         */
        setEndPosition(callSiteObj, callSiteObj->rawNodes());
    }

    ThisLiteralType newThisLiteral(const TokenPos& pos, Node thisName) {
        return new_<ThisLiteral>(pos, thisName);
    }

    NullLiteralType newNullLiteral(const TokenPos& pos) {
        return new_<NullLiteral>(pos);
    }

    RawUndefinedLiteralType newRawUndefinedLiteral(const TokenPos& pos) {
        return new_<RawUndefinedLiteral>(pos);
    }

    // The Boxer object here is any object that can allocate ObjectBoxes.
    // Specifically, a Boxer has a .newObjectBox(T) method that accepts a
    // Rooted<RegExpObject*> argument and returns an ObjectBox*.
    template <class Boxer>
    RegExpLiteralType newRegExp(RegExpObject* reobj, const TokenPos& pos, Boxer& boxer) {
        ObjectBox* objbox = boxer.newObjectBox(reobj);
        if (!objbox)
            return null();
        return new_<RegExpLiteral>(objbox, pos);
    }

    ConditionalExpressionType newConditional(Node cond, Node thenExpr, Node elseExpr) {
        return new_<ConditionalExpression>(cond, thenExpr, elseExpr);
    }

    UnaryNodeType newDelete(uint32_t begin, Node expr) {
        if (expr->isKind(PNK_NAME)) {
            expr->setOp(JSOP_DELNAME);
            return newUnary(PNK_DELETENAME, JSOP_NOP, begin, expr);
        }

        if (expr->isKind(PNK_DOT))
            return newUnary(PNK_DELETEPROP, JSOP_NOP, begin, expr);

        if (expr->isKind(PNK_ELEM))
            return newUnary(PNK_DELETEELEM, JSOP_NOP, begin, expr);

        if (expr->isKind(PNK_OPTCHAIN)) {
            ParseNode* kid = expr->as<UnaryNode>().kid();
            // Handle property deletion explicitly. OptionalCall is handled
            // via DeleteExpr.
            if (kid->isKind(PNK_DOT) ||
                kid->isKind(PNK_OPTDOT) ||
                kid->isKind(PNK_ELEM) ||
                kid->isKind(PNK_OPTELEM)) {
              return newUnary(PNK_DELETEOPTCHAIN, JSOP_NOP, begin, kid);
            }
        }

        return newUnary(PNK_DELETEEXPR, JSOP_NOP, begin, expr);
    }

    UnaryNodeType newTypeof(uint32_t begin, Node kid) {
        TokenPos pos(begin, kid->pn_pos.end);
        ParseNodeKind kind = kid->isKind(PNK_NAME) ? PNK_TYPEOFNAME : PNK_TYPEOFEXPR;
        return new_<UnaryNode>(kind, JSOP_NOP, pos, kid);
    }

    NullaryNodeType newNullary(ParseNodeKind kind, JSOp op, const TokenPos& pos) {
        return new_<NullaryNode>(kind, op, pos);
    }

    UnaryNodeType newUnary(ParseNodeKind kind, JSOp op, uint32_t begin, Node kid) {
        TokenPos pos(begin, kid ? kid->pn_pos.end : begin + 1);
        return new_<UnaryNode>(kind, op, pos, kid);
    }

    UnaryNodeType newUpdate(ParseNodeKind kind, uint32_t begin, Node kid) {
        TokenPos pos(begin, kid->pn_pos.end);
        return new_<UnaryNode>(kind, JSOP_NOP, pos, kid);
    }

    UnaryNodeType newSpread(uint32_t begin, Node kid) {
        TokenPos pos(begin, kid->pn_pos.end);
        return new_<UnaryNode>(PNK_SPREAD, JSOP_NOP, pos, kid);
    }

    Node newArrayPush(uint32_t begin, Node kid) {
        TokenPos pos(begin, kid->pn_pos.end);
        return new_<UnaryNode>(PNK_ARRAYPUSH, JSOP_ARRAYPUSH, pos, kid);
    }

    BinaryNodeType newBinary(ParseNodeKind kind, JSOp op = JSOP_NOP) {
        return new_<BinaryNode>(kind, op, pos(), (ParseNode*) nullptr, (ParseNode*) nullptr);
    }
    BinaryNodeType newBinary(ParseNodeKind kind, Node left, JSOp op = JSOP_NOP) {
        return new_<BinaryNode>(kind, op, left->pn_pos, left, (ParseNode*) nullptr);
    }
    BinaryNodeType newBinary(ParseNodeKind kind, Node left, Node right, JSOp op = JSOP_NOP) {
        TokenPos pos(left->pn_pos.begin, right->pn_pos.end);
        return new_<BinaryNode>(kind, op, pos, left, right);
    }
    Node appendOrCreateList(ParseNodeKind kind, Node left, Node right, ParseContext* pc,
                            JSOp op = JSOP_NOP)
    {
        return ParseNode::appendOrCreateList(kind, op, left, right, this, pc);
    }

    TernaryNodeType newTernary(ParseNodeKind kind, Node first, Node second, Node third,
                               JSOp op = JSOP_NOP)
    {
        return new_<TernaryNode>(kind, op, first, second, third);
    }

    // Expressions

    ListNodeType newArrayLiteral(uint32_t begin) {
        ListNode* literal = new_<ListNode>(PNK_ARRAY, TokenPos(begin, begin + 1));
        // Later in this stack: remove dependency on this opcode.
        if (literal)
            literal->setOp(JSOP_NEWINIT);
        return literal;
    }

    MOZ_MUST_USE bool addElision(ListNodeType literal, const TokenPos& pos) {
        NullaryNode* elision = new_<NullaryNode>(PNK_ELISION, pos);
        if (!elision)
            return false;
        literal->append(elision);
        literal->setHasArrayHoleOrSpread();
        literal->setHasNonConstInitializer();
        return true;
    }

    MOZ_MUST_USE bool addSpreadElement(ListNodeType literal, uint32_t begin, Node inner) {
        ParseNode* spread = newSpread(begin, inner);
        if (!spread)
            return false;
        literal->append(spread);
        literal->setHasArrayHoleOrSpread();
        literal->setHasNonConstInitializer();
        return true;
    }

    void addArrayElement(ListNodeType literal, Node element) {
        if (!element->isConstant())
            literal->setHasNonConstInitializer();
        literal->append(element);
    }

    BinaryNodeType newCall(Node callee, Node args) {
        return new_<BinaryNode>(PNK_CALL, JSOP_CALL, callee, args);
    }

    BinaryNodeType newOptionalCall(Node callee, Node args) {
        return new_<BinaryNode>(PNK_OPTCALL, JSOP_CALL, callee, args);
    }

    ListNodeType newArguments(const TokenPos& pos) {
        return new_<ListNode>(PNK_ARGUMENTS, JSOP_NOP, pos);
    }

    BinaryNodeType newSuperCall(Node callee, Node args, bool isSpread) {
        JSOp op = isSpread ? JSOP_SPREADSUPERCALL : JSOP_SUPERCALL;
        TokenPos pos(callee->pn_pos.begin, args->pn_pos.end);
        return new_<BinaryNode>(PNK_SUPERCALL, op, pos, callee, args);
    }

    BinaryNodeType newTaggedTemplate(Node tag, Node args) {
        return new_<BinaryNode>(PNK_TAGGED_TEMPLATE, JSOP_CALL, tag, args);
    }

    BinaryNodeType newGenExp(Node callee, Node args) {
        return new_<BinaryNode>(PNK_GENEXP, JSOP_CALL, callee, args);
    }

    ListNodeType newObjectLiteral(uint32_t begin) {
        ListNode* literal = new_<ListNode>(PNK_OBJECT, TokenPos(begin, begin + 1));
        // Later in this stack: remove dependency on this opcode.
        if (literal)
            literal->setOp(JSOP_NEWINIT);
        return literal;
    }

    ClassNodeType newClass(Node name, Node heritage, LexicalScopeNodeType memberBlock,
                           const TokenPos& pos)
    {
        return new_<ClassNode>(name, heritage, memberBlock, pos);
    }
    ListNodeType newClassMemberList(uint32_t begin) {
        return new_<ListNode>(PNK_CLASSMEMBERLIST, TokenPos(begin, begin + 1));
    }
    ClassNamesType newClassNames(Node outer, Node inner, const TokenPos& pos) {
        return new_<ClassNames>(outer, inner, pos);
    }
    BinaryNodeType newNewTarget(NullaryNodeType newHolder, NullaryNodeType targetHolder) {
        return new_<BinaryNode>(PNK_NEWTARGET, JSOP_NOP, newHolder, targetHolder);
    }
    NullaryNodeType newPosHolder(const TokenPos& pos) {
        return new_<NullaryNode>(PNK_POSHOLDER, pos);
    }
    UnaryNodeType newSuperBase(Node thisName, const TokenPos& pos) {
        return new_<UnaryNode>(PNK_SUPERBASE, JSOP_NOP, pos, thisName);
    }

    MOZ_MUST_USE bool addPrototypeMutation(ListNodeType literal, uint32_t begin, Node expr) {
        // Object literals with mutated [[Prototype]] are non-constant so that
        // singleton objects will have Object.prototype as their [[Prototype]].
        literal->setHasNonConstInitializer();

        UnaryNode* mutation = newUnary(PNK_MUTATEPROTO, JSOP_NOP, begin, expr);
        if (!mutation)
            return false;
        literal->append(mutation);
        return true;
    }

    MOZ_MUST_USE bool addPropertyDefinition(ListNodeType literal, Node key, Node val) {
        MOZ_ASSERT(literal->isKind(PNK_OBJECT));
        MOZ_ASSERT(key->isKind(PNK_NUMBER) ||
                   key->isKind(PNK_OBJECT_PROPERTY_NAME) ||
                   key->isKind(PNK_STRING) ||
                   key->isKind(PNK_COMPUTED_NAME));

        BinaryNode* propdef = newBinary(PNK_COLON, key, val, JSOP_INITPROP);
        if (!propdef)
            return false;

        if (!val->isConstant()) {
            literal->setHasNonConstInitializer();
        }
        literal->append(propdef);
        return true;
    }

    MOZ_MUST_USE bool addShorthand(ListNodeType literal, NameNodeType name, NameNodeType expr) {
        MOZ_ASSERT(literal->isKind(PNK_OBJECT));
        MOZ_ASSERT(name->isKind(PNK_OBJECT_PROPERTY_NAME));
        MOZ_ASSERT(expr->isKind(PNK_NAME));
        MOZ_ASSERT(name->atom() == expr->atom());

        literal->setHasNonConstInitializer();
        BinaryNode* propdef = newBinary(PNK_SHORTHAND, name, expr, JSOP_INITPROP);
        if (!propdef)
            return false;
        literal->append(propdef);
        return true;
    }

    MOZ_MUST_USE bool addSpreadProperty(ListNodeType literal, uint32_t begin, Node inner) {
        MOZ_ASSERT(literal->isKind(PNK_OBJECT));

        literal->setHasNonConstInitializer();
        ParseNode* spread = newSpread(begin, inner);
        if (!spread)
            return false;
        literal->append(spread);
        return true;
    }

    MOZ_MUST_USE bool addObjectMethodDefinition(ListNodeType literal, Node key, FunctionNodeType funNode,
                                                JSOp op)
    {
        MOZ_ASSERT(key->isKind(PNK_NUMBER) ||
                   key->isKind(PNK_OBJECT_PROPERTY_NAME) ||
                   key->isKind(PNK_STRING) ||
                   key->isKind(PNK_COMPUTED_NAME));
        literal->setHasNonConstInitializer();

        ParseNode* propdef = newBinary(PNK_COLON, key, funNode, op);
        if (!propdef)
            return false;
        literal->append(propdef);
        return true;
    }

    MOZ_MUST_USE ClassMethod* newClassMethodDefinition(Node key, FunctionNodeType funNode,
                                                       JSOp op, bool isStatic)
    {
        MOZ_ASSERT(isUsableAsObjectPropertyName(key));

        return new_<ClassMethod>(key, funNode, op, isStatic);
    }

    MOZ_MUST_USE ClassField* newClassFieldDefinition(Node name, FunctionNodeType initializer, bool isStatic)
    {
        MOZ_ASSERT(isUsableAsObjectPropertyName(name));

        return new_<ClassField>(name, initializer, isStatic);
    }

    MOZ_MUST_USE StaticClassBlock* newStaticClassBlock(FunctionNodeType block)
    {
        return new_<StaticClassBlock>(block);
    }

    MOZ_MUST_USE bool addClassMemberDefinition(ListNodeType memberList, Node member)
    {
        MOZ_ASSERT(memberList->isKind(PNK_CLASSMEMBERLIST));
        // Constructors can be surrounded by LexicalScopes.
        MOZ_ASSERT(member->isKind(PNK_CLASSMETHOD) ||
                   member->isKind(PNK_CLASSFIELD) ||
                   member->isKind(PNK_STATICCLASSBLOCK) ||
                   (member->isKind(PNK_LEXICALSCOPE) &&
                    member->as<LexicalScopeNode>().scopeBody()->isKind(PNK_CLASSMETHOD)));

        addList(/* list = */ memberList, /* kid = */ member);
        return true;
    }

    UnaryNodeType newInitialYieldExpression(uint32_t begin, Node gen) {
        TokenPos pos(begin, begin + 1);
        return new_<UnaryNode>(PNK_INITIALYIELD, JSOP_INITIALYIELD, pos, gen);
    }

    UnaryNodeType newYieldExpression(uint32_t begin, Node value) {
        TokenPos pos(begin, value ? value->pn_pos.end : begin + 1);
        return new_<UnaryNode>(PNK_YIELD, JSOP_YIELD, pos, value);
    }

    UnaryNodeType newYieldStarExpression(uint32_t begin, Node value) {
        TokenPos pos(begin, value->pn_pos.end);
        return new_<UnaryNode>(PNK_YIELD_STAR, JSOP_NOP, pos, value);
    }

    UnaryNodeType newAwaitExpression(uint32_t begin, Node value) {
        TokenPos pos(begin, value ? value->pn_pos.end : begin + 1);
        return new_<UnaryNode>(PNK_AWAIT, JSOP_AWAIT, pos, value);
    }

    UnaryNodeType newOptionalChain(uint32_t begin, Node value) {
        TokenPos pos(begin, value->pn_pos.end);
        return new_<UnaryNode>(PNK_OPTCHAIN, JSOP_NOP, pos, value);
    }

    // Statements

    ListNodeType newStatementList(const TokenPos& pos) {
        return new_<ListNode>(PNK_STATEMENTLIST, pos);
    }

    MOZ_MUST_USE bool isFunctionStmt(Node stmt) {
        while (stmt->isKind(PNK_LABEL))
            stmt = stmt->as<LabeledStatement>().statement();
        return stmt->is<FunctionNode>();
    }

    void addStatementToList(ListNodeType list, Node stmt) {
        MOZ_ASSERT(list->isKind(PNK_STATEMENTLIST));

        list->append(stmt);

        if (isFunctionStmt(stmt)) {
            // Notify the emitter that the block contains body-level function
            // definitions that should be processed before the rest of nodes.
            list->setHasTopLevelFunctionDeclarations();
        }
    }

    void addCaseStatementToList(ListNodeType list, CaseClauseType caseClause) {
        MOZ_ASSERT(list->isKind(PNK_STATEMENTLIST));

        list->append(caseClause);

        if (caseClause->statementList()->hasTopLevelFunctionDeclarations())
            list->setHasTopLevelFunctionDeclarations();
    }

    MOZ_MUST_USE bool prependInitialYield(ListNodeType stmtList, Node genName) {
        MOZ_ASSERT(stmtList->isKind(PNK_STATEMENTLIST));

        TokenPos yieldPos(stmtList->pn_pos.begin, stmtList->pn_pos.begin + 1);
        NullaryNode* makeGen = new_<NullaryNode>(PNK_GENERATOR, yieldPos);
        if (!makeGen)
            return false;

        MOZ_ASSERT(genName->getOp() == JSOP_GETNAME);
        genName->setOp(JSOP_SETNAME);
        ParseNode* genInit = newBinary(PNK_ASSIGN, genName, makeGen);
        if (!genInit)
            return false;

        UnaryNode* initialYield = newInitialYieldExpression(yieldPos.begin, genInit);
        if (!initialYield)
            return false;

        stmtList->prepend(initialYield);
        return true;
    }

    BinaryNodeType newSetThis(Node thisName, Node val) {
        MOZ_ASSERT(thisName->getOp() == JSOP_GETNAME);
        thisName->setOp(JSOP_SETNAME);
        return newBinary(PNK_SETTHIS, thisName, val);
    }

    UnaryNodeType newEmptyStatement(const TokenPos& pos) {
        return new_<UnaryNode>(PNK_SEMI, JSOP_NOP, pos, (ParseNode*) nullptr);
    }

    BinaryNodeType newImportDeclaration(Node importSpecSet, Node moduleSpec, const TokenPos& pos)
    {
        BinaryNode* pn = new_<BinaryNode>(PNK_IMPORT, JSOP_NOP, pos,
                                          importSpecSet, moduleSpec);
        return pn;
    }

    UnaryNodeType newExportDeclaration(Node kid, const TokenPos& pos) {
        return new_<UnaryNode>(PNK_EXPORT, JSOP_NOP, pos, kid);
    }

    BinaryNodeType newExportFromDeclaration(uint32_t begin, Node exportSpecSet, Node moduleSpec) {
        BinaryNode* decl = new_<BinaryNode>(PNK_EXPORT_FROM, JSOP_NOP, exportSpecSet, moduleSpec);
        if (!decl)
            return null();
        decl->pn_pos.begin = begin;
        return decl;
    }

    BinaryNodeType newExportDefaultDeclaration(Node kid, Node maybeBinding, const TokenPos& pos) {
        if (maybeBinding) {
            MOZ_ASSERT(maybeBinding->isKind(PNK_NAME));
            MOZ_ASSERT(!maybeBinding->isInParens());

            checkAndSetIsDirectRHSAnonFunction(kid);
        }
        return new_<BinaryNode>(PNK_EXPORT_DEFAULT, JSOP_NOP, pos, kid, maybeBinding);
    }

    BinaryNodeType newImportMeta(Node importHolder, Node metaHolder) {
        return new_<BinaryNode>(PNK_IMPORT_META, JSOP_NOP, importHolder, metaHolder);
    }

    BinaryNodeType newCallImport(Node importHolder, Node singleArg) {
        return new_<BinaryNode>(PNK_CALL_IMPORT, JSOP_DYNAMIC_IMPORT, importHolder, singleArg);
    }

    UnaryNodeType newExprStatement(Node expr, uint32_t end) {
        MOZ_ASSERT(expr->pn_pos.end <= end);
        return new_<UnaryNode>(PNK_SEMI, JSOP_NOP, TokenPos(expr->pn_pos.begin, end), expr);
    }

    TernaryNodeType newIfStatement(uint32_t begin, Node cond, Node thenBranch, Node elseBranch) {
        TernaryNode* node = new_<TernaryNode>(PNK_IF, JSOP_NOP, cond, thenBranch, elseBranch);
        if (!node)
            return null();
        node->pn_pos.begin = begin;
        return node;
    }

    BinaryNodeType newDoWhileStatement(Node body, Node cond, const TokenPos& pos) {
        return new_<BinaryNode>(PNK_DOWHILE, JSOP_NOP, pos, body, cond);
    }

    BinaryNodeType newWhileStatement(uint32_t begin, Node cond, Node body) {
        TokenPos pos(begin, body->pn_pos.end);
        return new_<BinaryNode>(PNK_WHILE, JSOP_NOP, pos, cond, body);
    }

    ForNodeType newForStatement(uint32_t begin, TernaryNodeType forHead, Node body, unsigned iflags)
    {
        return new_<ForNode>(TokenPos(begin, body->pn_pos.end), forHead, body, iflags);
    }

    ForNodeType newComprehensionFor(uint32_t begin, TernaryNodeType forHead, Node body) {
        // A PNK_COMPREHENSIONFOR node is binary: left is loop control, right
        // is the body.
        MOZ_ASSERT(forHead->isKind(PNK_FORIN) || forHead->isKind(PNK_FOROF));
        ForNode* pn = new_<ForNode>(TokenPos(begin, body->pn_pos.end), forHead, body, JSOP_ITER);
        if (!pn)
            return null();
        pn->setKind(PNK_COMPREHENSIONFOR);
        return pn;
    }

    ListNodeType newComprehensionBinding(Node kid) {
        MOZ_ASSERT(kid->isKind(PNK_NAME));
        return new_<ListNode>(PNK_LET, JSOP_NOP, kid);
    }

    TernaryNodeType newForHead(Node init, Node test, Node update, const TokenPos& pos) {
        return new_<TernaryNode>(PNK_FORHEAD, JSOP_NOP, init, test, update, pos);
    }

    TernaryNodeType newForInOrOfHead(ParseNodeKind kind, Node target, Node iteratedExpr,
                                     const TokenPos& pos)
    {
        MOZ_ASSERT(kind == PNK_FORIN || kind == PNK_FOROF);
        return new_<TernaryNode>(kind, JSOP_NOP, target, nullptr, iteratedExpr, pos);
    }

    SwitchStatementType newSwitchStatement(uint32_t begin, Node discriminant,
                                           LexicalScopeNodeType lexicalForCaseList, bool hasDefault)
    {
        return new_<SwitchStatement>(begin, discriminant, lexicalForCaseList, hasDefault);
    }

    CaseClauseType newCaseOrDefault(uint32_t begin, Node expr, Node body) {
        return new_<CaseClause>(expr, body, begin);
    }

    ContinueStatementType newContinueStatement(PropertyName* label, const TokenPos& pos) {
        return new_<ContinueStatement>(label, pos);
    }

    BreakStatementType newBreakStatement(PropertyName* label, const TokenPos& pos) {
        return new_<BreakStatement>(label, pos);
    }

    UnaryNodeType newReturnStatement(Node expr, const TokenPos& pos) {
        MOZ_ASSERT_IF(expr, pos.encloses(expr->pn_pos));
        return new_<UnaryNode>(PNK_RETURN, JSOP_RETURN, pos, expr);
    }

    BinaryNodeType newWithStatement(uint32_t begin, Node expr, Node body) {
        return new_<BinaryNode>(PNK_WITH, JSOP_NOP, TokenPos(begin, body->pn_pos.end),
                                expr, body);
    }

    LabeledStatementType newLabeledStatement(PropertyName* label, Node stmt, uint32_t begin) {
        return new_<LabeledStatement>(label, stmt, begin);
    }

    UnaryNodeType newThrowStatement(Node expr, const TokenPos& pos) {
        MOZ_ASSERT(pos.encloses(expr->pn_pos));
        return new_<UnaryNode>(PNK_THROW, JSOP_THROW, pos, expr);
    }

    TernaryNodeType newTryStatement(uint32_t begin, Node body, ListNodeType catchList,
                                    Node finallyBlock) {
        return new_<TryNode>(begin, body, catchList, finallyBlock);
    }

    DebuggerStatementType newDebuggerStatement(const TokenPos& pos) {
        return new_<DebuggerStatement>(pos);
    }

    NameNodeType newPropertyName(PropertyName* name, const TokenPos& pos) {
        return new_<NameNode>(PNK_PROPERTYNAME, JSOP_NOP, name, pos);
    }

    PropertyAccessType newPropertyAccess(Node expr, NameNodeType key) {
        return new_<PropertyAccess>(expr, key, expr->pn_pos.begin, key->pn_pos.end);
    }

    PropertyByValueType newPropertyByValue(Node lhs, Node index, uint32_t end) {
        return new_<PropertyByValue>(lhs, index, lhs->pn_pos.begin, end);
    }

    OptionalPropertyAccessType newOptionalPropertyAccess(Node expr, NameNodeType key) {
        return new_<OptionalPropertyAccess>(expr, key, expr->pn_pos.begin, key->pn_pos.end);
    }

    OptionalPropertyByValueType newOptionalPropertyByValue(Node lhs, Node index, uint32_t end) {
        return new_<OptionalPropertyByValue>(lhs, index, lhs->pn_pos.begin, end);
    }

    inline MOZ_MUST_USE bool addCatchBlock(ListNodeType catchList, LexicalScopeNodeType lexicalScope,
                                           Node catchBinding, Node catchGuard, Node catchBody);

    inline MOZ_MUST_USE bool setLastFunctionFormalParameterDefault(FunctionNodeType funNode,
                                                                   Node defaultValue);
    inline void setLastFunctionFormalParameterDestructuring(Node funcpn, Node pn);

    void checkAndSetIsDirectRHSAnonFunction(Node pn) {
        if (IsAnonymousFunctionDefinition(pn))
            pn->setDirectRHSAnonFunction(true);
    }

    FunctionNodeType newFunction(FunctionSyntaxKind syntaxKind, const TokenPos& pos) {
        return new_<FunctionNode>(syntaxKind, pos);
    }

    bool setComprehensionLambdaBody(FunctionNodeType funNode, ListNodeType body) {
        MOZ_ASSERT(body->isKind(PNK_STATEMENTLIST));
        ListNode* paramsBody = newList(PNK_PARAMSBODY, body);
        if (!paramsBody)
            return false;
        setFunctionFormalParametersAndBody(funNode, paramsBody);
        return true;
    }
    void setFunctionFormalParametersAndBody(FunctionNodeType funNode, ListNodeType paramsBody) {
        MOZ_ASSERT_IF(paramsBody, paramsBody->isKind(PNK_PARAMSBODY));
        funNode->setBody(paramsBody);
    }
    void setFunctionBox(FunctionNodeType funNode, FunctionBox* funbox) {
        funNode->setFunbox(funbox);
        funbox->functionNode = funNode;
    }
    void addFunctionFormalParameter(FunctionNodeType funNode, Node argpn) {
        addList(/* list = */ funNode->body(), /* child = */ argpn);
    }
    void setFunctionBody(FunctionNodeType funNode, LexicalScopeNodeType body) {
        MOZ_ASSERT(funNode->body()->isKind(PNK_PARAMSBODY));
        addList(/* list = */ funNode->body(), /* child = */ body);
    }

    ModuleNodeType newModule() {
        return new_<ModuleNode>(pos());
    }

    BinaryNodeType newNewExpression(uint32_t begin, Node ctor, Node args) {
        return new_<BinaryNode>(PNK_NEW, JSOP_NEW, TokenPos(begin, args->pn_pos.end), ctor, args);
    }

    LexicalScopeNodeType newLexicalScope(LexicalScope::Data* bindings, Node body) {
        return new_<LexicalScopeNode>(bindings, body);
    }

    AssignmentNodeType newAssignment(ParseNodeKind kind, Node lhs, Node rhs, JSOp op) {
        return new_<AssignmentNode>(kind, op, lhs, rhs);
    }

    bool isUnparenthesizedYieldExpression(Node node) {
        return node->isKind(PNK_YIELD) && !node->isInParens();
    }

    bool isUnparenthesizedCommaExpression(Node node) {
        return node->isKind(PNK_COMMA) && !node->isInParens();
    }

    bool isUnparenthesizedAssignment(Node node) {
        if (node->isKind(PNK_ASSIGN) && !node->isInParens()) {
            // PNK_ASSIGN is also (mis)used for things like |var name = expr;|.
            // But this method is only called on actual expressions, so we can
            // just assert the node's op is the one used for plain assignment.
            MOZ_ASSERT(node->isOp(JSOP_NOP));
            return true;
        }

        return false;
    }

    bool isUnparenthesizedUnaryExpression(Node node) {
        if (!node->isInParens()) {
            ParseNodeKind kind = node->getKind();
            return kind == PNK_VOID || kind == PNK_NOT || kind == PNK_BITNOT || kind == PNK_POS ||
                   kind == PNK_NEG || IsTypeofKind(kind) || IsDeleteKind(kind);
        }
        return false;
    }

    bool isReturnStatement(Node node) {
        return node->isKind(PNK_RETURN);
    }

    bool isStatementPermittedAfterReturnStatement(ParseNode *node) {
        ParseNodeKind kind = node->getKind();
        return kind == PNK_FUNCTION || kind == PNK_VAR || kind == PNK_BREAK || kind == PNK_THROW ||
               (kind == PNK_SEMI && !node->as<UnaryNode>().kid());
    }

    bool isSuperBase(Node node) {
        return node->isKind(PNK_SUPERBASE);
    }

    bool isUsableAsObjectPropertyName(ParseNode* node) {
        return node->isKind(PNK_NUMBER) ||
               node->isKind(PNK_OBJECT_PROPERTY_NAME) ||
               node->isKind(PNK_STRING) ||
               node->isKind(PNK_COMPUTED_NAME);
    }

    inline MOZ_MUST_USE bool finishInitializerAssignment(NameNodeType nameNode, Node init);

    void setBeginPosition(Node pn, Node oth) {
        setBeginPosition(pn, oth->pn_pos.begin);
    }
    void setBeginPosition(Node pn, uint32_t begin) {
        pn->pn_pos.begin = begin;
        MOZ_ASSERT(pn->pn_pos.begin <= pn->pn_pos.end);
    }

    void setEndPosition(Node pn, Node oth) {
        setEndPosition(pn, oth->pn_pos.end);
    }
    void setEndPosition(Node pn, uint32_t end) {
        pn->pn_pos.end = end;
        MOZ_ASSERT(pn->pn_pos.begin <= pn->pn_pos.end);
    }

    void setPosition(Node pn, const TokenPos& pos) {
        pn->pn_pos = pos;
    }
    TokenPos getPosition(Node pn) {
        return pn->pn_pos;
    }

    bool isDeclarationKind(ParseNodeKind kind) {
        return kind == PNK_VAR || kind == PNK_LET || kind == PNK_CONST;
    }

    ListNodeType newList(ParseNodeKind kind, JSOp op = JSOP_NOP) {
        MOZ_ASSERT(!isDeclarationKind(kind));
        return new_<ListNode>(kind, op, pos());
    }

    ListNodeType newList(ParseNodeKind kind, const TokenPos& pos, JSOp op = JSOP_NOP) {
        MOZ_ASSERT(!isDeclarationKind(kind));
        return new_<ListNode>(kind, op, pos);
    }

    ListNodeType newList(ParseNodeKind kind, Node kid, JSOp op = JSOP_NOP) {
        MOZ_ASSERT(!isDeclarationKind(kind));
        return new_<ListNode>(kind, op, kid);
    }

    ListNodeType newDeclarationList(ParseNodeKind kind, JSOp op = JSOP_NOP) {
        MOZ_ASSERT(isDeclarationKind(kind));
        return new_<ListNode>(kind, op, pos());
    }

    ListNodeType newDeclarationList(ParseNodeKind kind, Node kid, JSOp op = JSOP_NOP) {
        MOZ_ASSERT(isDeclarationKind(kind));
        return new_<ListNode>(kind, op, kid);
    }

    bool isDeclarationList(Node node) {
        return isDeclarationKind(node->getKind());
    }

    Node singleBindingFromDeclaration(ListNodeType decl) {
        MOZ_ASSERT(isDeclarationList(decl));
        MOZ_ASSERT(decl->count() == 1);
        return decl->head();
    }

    ListNodeType newCatchList() {
        return new_<ListNode>(PNK_CATCHLIST, JSOP_NOP, pos());
    }

    ListNodeType newCommaExpressionList(Node kid) {
        return newList(PNK_COMMA, kid, JSOP_NOP);
    }

    void addList(ListNodeType list, Node kid) {
        list->append(kid);
    }

    void setOp(Node pn, JSOp op) {
        pn->setOp(op);
    }
    void setListHasNonConstInitializer(ListNodeType literal) {
        literal->setHasNonConstInitializer();
    }
    template <typename NodeType>
    MOZ_MUST_USE NodeType parenthesize(NodeType node) {
        node->setInParens(true);
        return node;
    }
    template <typename NodeType>
    MOZ_MUST_USE NodeType setLikelyIIFE(NodeType node) {
        return parenthesize(node);
    }
    void setInDirectivePrologue(UnaryNodeType exprStmt) {
        exprStmt->setIsDirectivePrologueMember();
    }

    bool isConstant(Node pn) {
        return pn->isConstant();
    }

    bool isUnparenthesizedName(Node node) {
        return node->isKind(PNK_NAME) && !node->isInParens();
    }

    bool isNameAnyParentheses(Node node) {
        return node->isKind(PNK_NAME);
    }

    bool isArgumentsAnyParentheses(Node node, ExclusiveContext* cx) {
        return node->isKind(PNK_NAME) && node->as<NameNode>().atom() == cx->names().arguments;
    }

    bool isEvalAnyParentheses(Node node, ExclusiveContext* cx) {
        return node->isKind(PNK_NAME) && node->as<NameNode>().atom() == cx->names().eval;
    }

    const char* nameIsArgumentsEvalAnyParentheses(Node node, ExclusiveContext* cx) {
        MOZ_ASSERT(isNameAnyParentheses(node),
                   "must only call this function on known names");

        if (isEvalAnyParentheses(node, cx))
            return js_eval_str;
        if (isArgumentsAnyParentheses(node, cx))
            return js_arguments_str;
        return nullptr;
    }

    bool isAsyncKeyword(Node node, ExclusiveContext* cx) {
        return node->isKind(PNK_NAME) &&
               node->pn_pos.begin + strlen("async") == node->pn_pos.end &&
               node->as<NameNode>().atom() == cx->names().async;
    }

    bool isCall(Node pn) {
        return pn->isKind(PNK_CALL);
    }
    PropertyName* maybeDottedProperty(Node pn) {
        return pn->is<PropertyAccessBase>() ?
               &pn->as<PropertyAccessBase>().name() :
               nullptr;
    }
    JSAtom* isStringExprStatement(Node pn, TokenPos* pos) {
        if (pn->is<UnaryNode>()) {
            UnaryNode* unary = &pn->as<UnaryNode>();
            if (JSAtom* atom = unary->isStringExprStatement()) {
                *pos = unary->kid()->pn_pos;
                return atom;
            }
        }
        return nullptr;
    }

    void adjustGetToSet(Node node) {
        node->setOp(node->isOp(JSOP_GETLOCAL) ? JSOP_SETLOCAL : JSOP_SETNAME);
    }

    void disableSyntaxParser() {
        syntaxParser = nullptr;
    }

    bool canSkipLazyInnerFunctions() {
        return !!lazyOuterFunction_;
    }
    bool canSkipLazyClosedOverBindings() {
        return !!lazyOuterFunction_;
    }
    LazyScript* lazyOuterFunction() {
        return lazyOuterFunction_;
    }
    JSFunction* nextLazyInnerFunction() {
        MOZ_ASSERT(lazyInnerFunctionIndex < lazyOuterFunction()->numInnerFunctions());
        return lazyOuterFunction()->innerFunctions()[lazyInnerFunctionIndex++];
    }
    JSAtom* nextLazyClosedOverBinding() {
        MOZ_ASSERT(lazyClosedOverBindingIndex < lazyOuterFunction()->numClosedOverBindings());
        return lazyOuterFunction()->closedOverBindings()[lazyClosedOverBindingIndex++];
    }
};

inline bool
FullParseHandler::addCatchBlock(ListNodeType catchList, LexicalScopeNodeType lexicalScope,
                                Node catchBinding, Node catchGuard, Node catchBody)
{
    ParseNode* catchpn = newTernary(PNK_CATCH, catchBinding, catchGuard, catchBody);
    if (!catchpn)
        return false;
    catchList->append(lexicalScope);
    lexicalScope->setScopeBody(catchpn);
    return true;
}

inline bool
FullParseHandler::setLastFunctionFormalParameterDefault(FunctionNodeType funNode, Node defaultValue)
{
    MOZ_ASSERT(funNode->isKind(PNK_FUNCTION));
    ListNode* body = funNode->body();
    ParseNode* arg = body->last();
    ParseNode* pn = newBinary(PNK_ASSIGN, arg, defaultValue, JSOP_NOP);
    if (!pn)
        return false;

    checkAndSetIsDirectRHSAnonFunction(defaultValue);

    body->replaceLast(pn);

    return true;
}

inline bool
FullParseHandler::finishInitializerAssignment(NameNodeType nameNode, Node init)
{
    MOZ_ASSERT(nameNode->isKind(PNK_NAME));
    MOZ_ASSERT(!nameNode->isInParens());

    nameNode->setInitializer(init);
    nameNode->setOp(JSOP_SETNAME);

    /* The declarator's position must include the initializer. */
    nameNode->pn_pos.end = init->pn_pos.end;
    return true;
}

} // namespace frontend
} // namespace js

#endif /* frontend_FullParseHandler_h */
