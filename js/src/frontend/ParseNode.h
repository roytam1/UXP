/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_ParseNode_h
#define frontend_ParseNode_h

#include <functional>

#include "mozilla/Attributes.h"

#include "builtin/ModuleObject.h"
#include "frontend/TokenStream.h"
#include "vm/BigIntType.h"

namespace js {
namespace frontend {

class ParseContext;
class FullParseHandler;
class FunctionBox;
class ObjectBox;
class BigIntBox;

#define FOR_EACH_PARSE_NODE_KIND(F) \
    F(NOP) \
    F(SEMI) \
    F(COMMA) \
    F(CONDITIONAL) \
    F(COLON) \
    F(SHORTHAND) \
    F(POS) \
    F(NEG) \
    F(PREINCREMENT) \
    F(POSTINCREMENT) \
    F(PREDECREMENT) \
    F(POSTDECREMENT) \
    F(PROPERTYNAME) \
    F(DOT) \
    F(ELEM) \
    F(OPTDOT) \
    F(OPTCHAIN) \
    F(OPTELEM) \
    F(OPTCALL) \
    F(ARRAY) \
    F(ELISION) \
    F(STATEMENTLIST) \
    F(LABEL) \
    F(OBJECT) \
    F(CALL) \
    F(ARGUMENTS) \
    F(NAME) \
    F(PRIVATE_NAME) \
    F(OBJECT_PROPERTY_NAME) \
    F(COMPUTED_NAME) \
    F(NUMBER) \
    F(BIGINT) \
    F(STRING) \
    F(TEMPLATE_STRING_LIST) \
    F(TEMPLATE_STRING) \
    F(TAGGED_TEMPLATE) \
    F(CALLSITEOBJ) \
    F(REGEXP) \
    F(TRUE) \
    F(FALSE) \
    F(NULL) \
    F(RAW_UNDEFINED) \
    F(THIS) \
    F(FUNCTION) \
    F(MODULE) \
    F(IF) \
    F(SWITCH) \
    F(CASE) \
    F(WHILE) \
    F(DOWHILE) \
    F(FOR) \
    F(COMPREHENSIONFOR) \
    F(BREAK) \
    F(CONTINUE) \
    F(VAR) \
    F(CONST) \
    F(WITH) \
    F(RETURN) \
    F(NEW) \
    /* Delete operations.  These must be sequential. */ \
    F(DELETENAME) \
    F(DELETEPROP) \
    F(DELETEELEM) \
    F(DELETEEXPR) \
    F(DELETEOPTCHAIN) \
    F(TRY) \
    F(CATCH) \
    F(CATCHLIST) \
    F(THROW) \
    F(DEBUGGER) \
    F(GENERATOR) \
    F(INITIALYIELD) \
    F(YIELD) \
    F(YIELD_STAR) \
    F(GENEXP) \
    F(ARRAYCOMP) \
    F(ARRAYPUSH) \
    F(LEXICALSCOPE) \
    F(LET) \
    F(IMPORT) \
    F(IMPORT_SPEC_LIST) \
    F(IMPORT_SPEC) \
    F(EXPORT) \
    F(EXPORT_FROM) \
    F(EXPORT_DEFAULT) \
    F(EXPORT_SPEC_LIST) \
    F(EXPORT_SPEC) \
    F(EXPORT_BATCH_SPEC) \
    F(FORIN) \
    F(FOROF) \
    F(FORHEAD) \
    F(PARAMSBODY) \
    F(SPREAD) \
    F(MUTATEPROTO) \
    F(CLASS) \
    F(CLASSMETHOD) \
    F(STATICCLASSBLOCK) \
    F(CLASSFIELD) \
    F(CLASSMEMBERLIST) \
    F(CLASSNAMES) \
    F(NEWTARGET) \
    F(POSHOLDER) \
    F(SUPERBASE) \
    F(SUPERCALL) \
    F(SETTHIS) \
    F(INITPROP) \
    F(IMPORT_META) \
    F(CALL_IMPORT) \
    \
    /* Unary operators. */ \
    F(TYPEOFNAME) \
    F(TYPEOFEXPR) \
    F(VOID) \
    F(NOT) \
    F(BITNOT) \
    F(AWAIT) \
    \
    /* \
     * Binary operators. \
     * These must be in the same order in several places: \
     *   - the precedence table and JSOp code list in Parser.cpp \
     *   - the binary operators in TokenKind.h \
     *   - the first and last binary operator markers in ParseNode.h \
     */ \
    F(COALESCE) \
    F(OR) \
    F(AND) \
    F(BITOR) \
    F(BITXOR) \
    F(BITAND) \
    F(STRICTEQ) \
    F(EQ) \
    F(STRICTNE) \
    F(NE) \
    F(LT) \
    F(LE) \
    F(GT) \
    F(GE) \
    F(INSTANCEOF) \
    F(IN) \
    F(LSH) \
    F(RSH) \
    F(URSH) \
    F(ADD) \
    F(SUB) \
    F(STAR) \
    F(DIV) \
    F(MOD) \
    F(POW) \
    \
    /* Assignment operators (= += -= etc.). */ \
    /* AssignmentNode::test assumes all these are consecutive. */ \
    F(ASSIGN) \
    F(ADDASSIGN) \
    F(SUBASSIGN) \
    F(COALESCEASSIGN) \
    F(ORASSIGN) \
    F(ANDASSIGN) \
    F(BITORASSIGN) \
    F(BITXORASSIGN) \
    F(BITANDASSIGN) \
    F(LSHASSIGN) \
    F(RSHASSIGN) \
    F(URSHASSIGN) \
    F(MULASSIGN) \
    F(DIVASSIGN) \
    F(MODASSIGN) \
    F(POWASSIGN)

/*
 * Parsing builds a tree of nodes that directs code generation.  This tree is
 * not a concrete syntax tree in all respects (for example, || and && are left
 * associative, but (A && B && C) translates into the right-associated tree
 * <A && <B && C>> so that code generation can emit a left-associative branch
 * around <B && C> when A is false).  Nodes are labeled by kind, with a
 * secondary JSOp label when needed.
 *
 * The long comment after this enum block describes the kinds in detail.
 */
enum ParseNodeKind
{
#define EMIT_ENUM(name) PNK_##name,
    FOR_EACH_PARSE_NODE_KIND(EMIT_ENUM)
#undef EMIT_ENUM
    PNK_LIMIT, /* domain size */
    /*
     * Binary operator markers.
     * These must be in the same order in several places:
     *   - the precedence table and JSOp code list in Parser.cpp
     *   - the binary operators in ParseNode.h and TokenKind.h
     */
    PNK_BINOP_FIRST = PNK_COALESCE,
    PNK_BINOP_LAST = PNK_POW,
    PNK_ASSIGNMENT_START = PNK_ASSIGN,
    PNK_ASSIGNMENT_LAST = PNK_POWASSIGN
};

inline bool
IsDeleteKind(ParseNodeKind kind)
{
    return PNK_DELETENAME <= kind && kind <= PNK_DELETEEXPR;
}

inline bool
IsTypeofKind(ParseNodeKind kind)
{
    return PNK_TYPEOFNAME <= kind && kind <= PNK_TYPEOFEXPR;
}

/*
 * <Definitions>
 * PNK_FUNCTION (FunctionNode)
 *   funbox: ptr to js::FunctionBox holding function object containing arg and
 *           var properties.  We create the function object at parse (not emit)
 *           time to specialize arg and var bytecodes early.
 *   body: PNK_PARAMSBODY or null for lazily-parsed function, ordinarily;
 *         PNK_LEXICALSCOPE for implicit function in genexpr
 *   syntaxKind: the syntax of the function
 * PNK_PARAMSBODY (ListNode)
 *   head: list of formal parameters with
 *           * Name node with non-empty name for SingleNameBinding without
 *             Initializer
 *           * Assign node for SingleNameBinding with Initializer
 *           * Name node with empty name for destructuring
 *               expr: Array or Object for BindingPattern without
 *                     Initializer, Assign for BindingPattern with
 *                     Initializer
 *         followed by either:
 *           * StatementList node for function body statements
 *           * Return for expression closure
 *   count: number of formal parameters + 1
 * PNK_SPREAD (UnaryNode)
 *   kid: expression being spread
 * PNK_CLASS (ClassNode)
 *   kid1: PNK_CLASSNAMES for class name. can be null for anonymous class.
 *   kid2: expression after `extends`. null if no expression
 *   kid3: PNK_LEXICALSCOPE which contains PNK_CLASSMEMBERLIST as scopeBody
 * PNK_CLASSNAMES (ClassNames)
 *   left: Name node for outer binding, or null if the class is an expression
 *         that doesn't create an outer binding
 *   right: Name node for inner binding
 * PNK_CLASSMEMBERLIST (ListNode)
 *   head: list of N PNK_CLASSMETHOD, PNK_CLASSFIELD or PNK_STATICCLASSBLOCK nodes
 *   count: N >= 0
 * PNK_CLASSMETHOD (ClassMethod)
 *   name: propertyName
 *   method: methodDefinition
 * PNK_CLASSFIELD (ClassField)
 *   name: fieldName
 *   initializer: field initializer or null
 * PNK_STATICCLASSBLOCK (StaticClassBlock)
 *   block: block initializer
 * PNK_MODULE (ModuleNode)
 *   body: statement list of the module
 *
 * <Statements>
 * PNK_STATEMENTLIST (ListNode)
 *   head: list of N statements
 *   count: N >= 0
 * PNK_IF (TernaryNode)     In body of a comprehension or desugared PNK_GENEXP:
 *   kid1: cond
 *   kid2: then             = PNK_YIELD, PNK_ARRAYPUSH, (empty) PNK_STATEMENTLIST
 *   kid3: else or null
 * PNK_SWITCH (SwitchStatement)
 *   left: discriminant
 *   right: LexicalScope node that contains the list of Case nodes, with at
 *          most one default node.
 *   hasDefault: true if there's a default case
 * PNK_CASE (CaseClause)
 *   left: case-expression if CaseClause, or null if DefaultClause
 *   right: StatementList node for this case's statements
 * PNK_WHILE (BinaryNode)
 *   left: cond
 *   right: body
 * PNK_DOWHILE (BinaryNode)
 *   left: body
 *   right: cond
 * PNK_FOR (ForNode)
 *   left: one of
 *           * PNK_FORIN: for (x in y) ...
 *           * PNK_FOROF: for (x of x) ...
 *           * PNK_FORHEAD: for (;;) ...
 *   right: body
 * PNK_COMPREHENSIONFOR (ForNode)
 *   left: either PNK_FORIN or PNK_FOROF
 *   right: body
 * PNK_FORIN (TernaryNode)
 *   kid1: declaration or expression to left of 'in'
 *   kid2: null
 *   kid3: object expr to right of 'in'
 * PNK_FOROF (TernaryNode)
 *   kid1: declaration or expression to left of 'of'
 *   kid2: null
 *   kid3: expr to right of 'of'
 * PNK_FORHEAD (TernaryNode)
 *   kid1:  init expr before first ';' or nullptr
 *   kid2:  cond expr before second ';' or nullptr
 *   kid3:  update expr after second ';' or nullptr
 * PNK_THROW (UnaryNode)
 *   kid: thrown exception
 * PNK_TRY (TryNode)
 *   body: try block
 *   catchList: null or PNK_CATCHLIST list
 *   finallyBlock: null or finally block
 * PNK_CATCHLIST (ListNode)
 *    head: list of PNK_LEXICALSCOPE nodes, one per catch-block,
 *           each with scopeBody pointing to a PNK_CATCH node
 * PNK_CATCH (TernaryNode)
 *   kid1: PNK_NAME, PNK_ARRAY, or PNK_OBJECT catch binding node (PNK_ARRAY or PNK_OBJECT if destructuring)
           or null if optional catch binding
 *   kid2: null or the catch guard expression
 *   kid3: catch block statements
 * PNK_BREAK (BreakStatement)
 *   label: label or null
 * PNK_CONTINUE (ContinueStatement)
 *   label: label or null
 * PNK_WITH (BinaryNode)
 *   left: head expr
 *   right: body
 * PNK_VAR, PNK_LET, PNK_CONST (ListNode)
 *   head: list of N Name or Assign nodes
 *         each name node has either
 *           atom: variable name
 *           expr: initializer or null
 *         or
 *           atom: variable name
 *         each assignment node has
 *           left: pattern
 *           right: initializer
 *   count: N > 0
 * PNK_RETURN  (UnaryNode)
 *   kid: returned expression, or null if none
 * PNK_SEMI (UnaryNode)
 *   kid: expr
 *   prologue: true if Directive Prologue member in original source, not
 *             introduced via constant folding or other tree rewriting
 * PNK_NOP (NullaryNode)
 *   (no fields)
 * PNK_LABEL (LabeledStatement)
 *   atom: label
 *   expr: labeled statement
 * PNK_IMPORT (BinaryNode)
 *   left: PNK_IMPORT_SPEC_LIST import specifiers
 *   right: PNK_STRING module specifier
 * PNK_IMPORT_SPEC_LIST (ListNode)
 *   head: list of N ImportSpec nodes
 *   count: N >= 0 (N = 0 for `import {} from ...`)
 * PNK_IMPORT_SPEC (BinaryNode)
 *   left: import name
 *   right: local binding name
 * PNK_EXPORT (UnaryNode)
 *   kid: declaration expression
 * PNK_EXPORT_FROM (BinaryNode)
 *   left: PNK_EXPORT_SPEC_LIST export specifiers
 *   right: PNK_STRING module specifier
 * PNK_EXPORT_SPEC_LIST (ListNode)
 *   head: list of N ExportSpec nodes
 *   count: N >= 0 (N = 0 for `export {}`)
 * PNK_EXPORT (BinaryNode)
 *   left: local binding name
 *   right: export name
 * PNK_EXPORT_DEFAULT (BinaryNode)
 *   left: export default declaration or expression
 *   right: PNK_NAME node for assignment
 *
 * <Expressions>
 * All left-associated binary trees of the same type are optimized into lists
 * to avoid recursion when processing expression chains.
 * PNK_COMMA (ListNode)
 *   head: list of N comma-separated exprs
 *   count: N >= 2
 * PNK_INITPROP (BinaryNode)
 *   left: target of assignment, base-class setter will not be invoked
 *   right: value to assign
 * PNK_ASSIGN (AssignmentNode)
 *   left: target of assignment
 *   right: value to assign
 * PNK_ADDASSIGN, PNK_SUBASSIGN,
 * PNK_COALESCEASSIGN, PNK_ORASSIGN, PNK_ANDASSIGN,
 * PNK_BITORASSIGN, PNK_BITXORASSIGN, PNK_BITANDASSIGN,
 * PNK_LSHASSIGN, PNK_RSHASSIGN, PNK_URSHASSIGN,
 * PNK_MULASSIGN, PNK_DIVASSIGN, PNK_MODASSIGN, PNK_POWASSIGN (AssignmentNode)
 *   left: target of assignment
 *   right: value to assign
 *   pn_op: JSOP_ADD for +=, etc
 * PNK_CONDITIONAL (ConditionalExpression)
 *   (cond ? thenExpr : elseExpr)
 *   kid1: cond
 *   kid2: thenExpr
 *   kid3: elseExpr
 * PNK_COALESCE,   list     pn_head; list of pn_count subexpressions
 * PNK_OR, PNK_AND, PNK_BITOR,
 * PNK_BITXOR, PNK_BITAND, PNK_EQ,
 * PNK_NE, PNK_STRICTEQ, PNK_STRICTNE,
 * PNK_LT, PNK_LE, PNK_GT, PNK_GE,
 * PNK_LSH, PNK_RSH, PNK_URSH,
 * PNK_ADD, PNK_SUB, PNK_STAR,
 * PNK_DIV, PNK_MOD, PNK_POW (ListNode)
 *   head: list of N subexpressions
 *         All of these operators are left-associative except Pow which is
 *         right-associative, but still forms a list (see comments in
 *         ParseNode::appendOrCreateList).
 *   count: N >= 2
 *
 * PNK_POS, PNK_NEG, PNK_VOID, PNK_NOT, PNK_BITNOT,
 * PNK_TYPEOFNAME, PNK_TYPEOFEXPR (UnaryNode)
 *   kid: unary expr
 * PNK_PREINCREMENT, PNK_POSTINCREMENT,
 * PNK_PREDECREMENT, PNK_POSTDECREMENT (UnaryNode)
 *   kid: member expr
 * PNK_NEW (BinaryNode)
 *   left: ctor expression on the left of the '('
 *   right: Arguments
 * PNK_DELETENAME, PNK_DELETEPROP, PNK_DELETEELEM,
 * PNK_DELETEEXPR (UnaryNode)
 *   kid: expression that's evaluated, then the overall delete evaluates to
 *        true; can't be a kind for a more-specific ParseNodeKind::Delete*
 *        unless constant folding (or a similar parse tree manipulation) has
 *        occurred
 *          * DeleteName: PNK_NAME expr
 *          * DeleteProp: PNK_DOT expr
 *          * DeleteElem: PNK_ELEM expr
 *          * DeleteExpr: MEMBER expr
 * PNK_DELETEOPTCHAIN unary pn_kid: MEMBER expr that's evaluated, then the
 *                          overall delete evaluates to true; If constant
 *                          folding occurs, PNK_ELEM may become PNK_DOT.
 *                          PNK_OPTELEM does not get folded into PNK_OPTDOT.
 * PNK_OPTCHAIN unary       pn_kid: expression that is evaluated as a chain.
 *                          Contains one or more optional nodes. Its first node
 *                          is always an optional node, such as PNK_OPTELEM,
 *                          PNK_OPTDOT, or PNK_OPTCALL. This will shortcircuit
 *                          and return 'undefined' without evaluating the rest
 *                          of the expression if any of the optional nodes it
 *                          contains are nullish. An optional chain can also
 *                          contain nodes such as PNK_DOT, PNK_ELEM, PNK_NAME,
 *                          PNK_CALL, etc. These are evaluated normally.
 * PNK_PROPERTYNAME (NameNode)
 *   atom: property name being accessed
 * PNK_DOT, PNK_OPTDOT (PropertyAccess)         short circuits back to PNK_OPTCHAIN if nullish.
 *   left: MEMBER expr to left of '.'
 *   right: PropertyName to right of '.'
 * PNK_ELEM, PNK_OPTELEM (PropertyByValue)      short circuits back to PNK_OPTCHAIN if nullish.
 *   left: MEMBER expr to left of '['
 *   right: expr between '[' and ']'
 * PNK_CALL, PNK_OPTCALL (BinaryNode)           short circuits back to PNK_OPTCHAIN if nullish.
 *   left: callee expression on the left of the '('
 *   right: Arguments
 * PNK_GENEXP   binary      Exactly like PNK_CALL, used for the implicit call
 *                          in the desugaring of a generator-expression.
 * PNK_ARGUMENTS (ListNode)
 *   head: list of arg1, arg2, ... argN
 *   count: N >= 0
 * PNK_ARRAY (ListNode)
 *   head: list of N array element expressions
 *         holes ([,,]) are represented by Elision nodes,
 *         spread elements ([...X]) are represented by Spread nodes
 *   count: N >= 0
 * PNK_OBJECT (ListNode)
 *   head: list of N nodes, each item is one of:
 *           * MutateProto
 *           * Colon
 *           * Shorthand
 *           * Spread
 *   count: N >= 0
 * PNK_COLON (BinaryNode)
 *   key-value pair in object initializer or destructuring lhs
 *   left: property id
 *   right: value
 * PNK_SHORTHAND (BinaryNode)
 *   Same fields as Colon. This is used for object literal properties using
 *   shorthand ({x}).
 * PNK_COMPUTED_NAME (UnaryNode)
 *   ES6 ComputedPropertyName.
 *   kid: the AssignmentExpression inside the square brackets
 *                          pn_kid: the AssignmentExpression inside the square brackets
 * PNK_NAME (NameNode)
 *   atom: name, or object atom
 *   pn_op: JSOP_GETNAME, JSOP_STRING, or JSOP_OBJECT
 *          If JSOP_GETNAME, pn_op may be JSOP_*ARG or JSOP_*VAR telling
 *          const-ness and static analysis results
 * PNK_STRING (NameNode)
 *   atom: string
 * PNK_TEMPLATE_STRING_LIST (ListNode)
 *   head: list of alternating expr and template strings
 *           TemplateString [, expression, TemplateString]+
 *         there's at least one expression.  If the template literal contains
 *         no ${}-delimited expression, it's parsed as a single TemplateString
 * PNK_TEMPLATE_STRING (NameNode)
 *   atom: template string atom
 * PNK_TAGGED_TEMPLATE (BinaryNode)
 *   left: tag expression
 *   right: Arguments, with the first being the call site object, then
 *          arg1, arg2, ... argN
 * PNK_CALLSITEOBJ (CallSiteNode)
 *   head:  an Array of raw TemplateString, then corresponding cooked
 *          TemplateString nodes
 *            Array [, cooked TemplateString]+
 *          where the Array is
 *            [raw TemplateString]+
 * PNK_REGEXP (RegExpLiteral)
 *   regexp: RegExp model object
 * PNK_NUMBER (NumericLiteral)
 *   value: double value of numeric literal
 * PNK_BIGINT (BigIntLiteral)
 *   box: BigIntBox holding BigInt* value
 * PNK_TRUE, PNK_FALSE (BooleanLiteral)
 *   pn_op: JSOp bytecode
 * PNK_NULL (NullLiteral)
 *   pn_op: JSOp bytecode
 * PNK_RAW_UNDEFINED (RawUndefinedLiteral)
 *   pn_op: JSOp bytecode
 *
 * PNK_THIS (UnaryNode)
 *   kid: '.this' Name if function `this`, else nullptr
 * PNK_SUPERBASE (UnaryNode)
 *   kid: '.this' Name
 *
 * PNK_SUPERCALL (BinaryNode)
 *   left: SuperBase
 *   right: Arguments
 * PNK_SETTHIS (BinaryNode)
 *   left: '.this' Name
 *   right: SuperCall
 * PNK_LEXICALSCOPE (LexicalScopeNode)
 *   scopeBindings: scope bindings
 *   scopeBody: scope body
 * PNK_GENERATOR (NullaryNode)
 * PNK_INITIALYIELD (UnaryNode)
 *   kid: generator object
 * PNK_YIELD, PNK_YIELD_STAR, PNK_AWAIT (UnaryNode)
 *   kid: expr or null
 * PNK_ARRAYCOMP    list    pn_count: 1
 *                          pn_head: list of 1 element, which is block
 *                          enclosing for loop(s) and optionally
 *                          if-guarded PNK_ARRAYPUSH
 * PNK_ARRAYPUSH    unary   pn_op: JSOP_ARRAYCOMP
 *                          pn_kid: array comprehension expression
 * PNK_NOP (NullaryNode)
 * PNK_IMPORT_META (BinaryNode)
 * PNK_CALL_IMPORT (BinaryNode)
 */
enum ParseNodeArity
{
    PN_NULLARY,                         /* 0 kids */
    PN_UNARY,                           /* one kid, plus a couple of scalars */
    PN_BINARY,                          /* two kids, plus a couple of scalars */
    PN_TERNARY,                         /* three kids */
    PN_FUNCTION,                        /* function definition node */
    PN_MODULE,                          /* module node */
    PN_LIST,                            /* generic singly linked list */
    PN_NAME,                            /* name, label, string */
    PN_NUMBER,                          /* numeric literal */
    PN_BIGINT,                          /* BigInt literal */
    PN_REGEXP,                          /* regexp literal */
    PN_LOOP,                            /* loop control (break/continue) */
    PN_SCOPE                            /* lexical scope */
};

#define FOR_EACH_PARSENODE_SUBCLASS(macro) \
    macro(BinaryNode, BinaryNodeType, asBinary) \
    macro(AssignmentNode, AssignmentNodeType, asAssignment) \
    macro(CaseClause, CaseClauseType, asCaseClause) \
    macro(ClassMethod, ClassMethodType, asClassMethod) \
    macro(ClassField, ClassFieldType, asClassField) \
    macro(StaticClassBlock, StaticClassBlockType, asStaticClassBlock) \
    macro(ClassNames, ClassNamesType, asClassNames) \
    macro(ForNode, ForNodeType, asFor) \
    macro(PropertyAccess, PropertyAccessType, asPropertyAccess) \
    macro(PropertyByValue, PropertyByValueType, asPropertyByValue) \
    macro(OptionalPropertyAccess, OptionalPropertyAccessType, asOptionalPropertyAccess) \
    macro(OptionalPropertyByValue, OptionalPropertyByValueType, asOptionalPropertyByValue) \
    macro(SwitchStatement, SwitchStatementType, asSwitchStatement) \
    \
    macro(FunctionNode, FunctionNodeType, asFunction) \
    macro(ModuleNode, ModuleNodeType, asModule) \
    \
    macro(LexicalScopeNode, LexicalScopeNodeType, asLexicalScope) \
    \
    macro(ListNode, ListNodeType, asList) \
    macro(CallSiteNode, CallSiteNodeType, asCallSite) \
    \
    macro(LoopControlStatement, LoopControlStatementType, asLoopControlStatement) \
    macro(BreakStatement, BreakStatementType, asBreakStatement) \
    macro(ContinueStatement, ContinueStatementType, asContinueStatement) \
    \
    macro(NameNode, NameNodeType, asName) \
    macro(LabeledStatement, LabeledStatementType, asLabeledStatement) \
    \
    macro(NullaryNode, NullaryNodeType, asNullary) \
    macro(BooleanLiteral, BooleanLiteralType, asBooleanLiteral) \
    macro(DebuggerStatement, DebuggerStatementType, asDebuggerStatement) \
    macro(NullLiteral, NullLiteralType, asNullLiteral) \
    macro(RawUndefinedLiteral, RawUndefinedLiteralType, asRawUndefinedLiteral) \
    \
    macro(NumericLiteral, NumericLiteralType, asNumericLiteral) \
    macro(BigIntLiteral, BigIntLiteralType, asBigIntLiteral) \
    \
    macro(RegExpLiteral, RegExpLiteralType, asRegExpLiteral) \
    \
    macro(TernaryNode, TernaryNodeType, asTernary) \
    macro(ClassNode, ClassNodeType, asClass) \
    macro(ConditionalExpression, ConditionalExpressionType, asConditionalExpression) \
    macro(TryNode, TryNodeType, asTry) \
    \
    macro(UnaryNode, UnaryNodeType, asUnary) \
    macro(ThisLiteral, ThisLiteralType, asThisLiteral)

#define DECLARE_CLASS(typeName, longTypeName, asMethodName) \
class typeName;
FOR_EACH_PARSENODE_SUBCLASS(DECLARE_CLASS)
#undef DECLARE_CLASS

enum class FunctionSyntaxKind
{
    Expression,                                  // A non-arrow function expression.
    Statement,                                   // A named function appearing as a Statement.
    Arrow,
    Method,                                      // Method of a class or object.
    FieldInitializer,                            // Field initializers desugar to methods.
    StaticClassBlock,                            // Mostly static class blocks act similar to field initializers, however,
                                                 // there is some difference in static semantics.
    ClassConstructor,
    DerivedClassConstructor,
    Getter,
    GetterNoExpressionClosure,                   // Deprecated syntax: get a() this._a; (bug 1203742)
    Setter,
    SetterNoExpressionClosure                    // Deprecated syntax: set a(x) this._a = x;
};

static inline bool
IsConstructorKind(FunctionSyntaxKind kind)
{
    return kind == FunctionSyntaxKind::ClassConstructor ||
           kind == FunctionSyntaxKind::DerivedClassConstructor;
}

static inline bool
IsGetterKind(FunctionSyntaxKind kind)
{
    return kind == FunctionSyntaxKind::Getter ||
           kind == FunctionSyntaxKind::GetterNoExpressionClosure;
}

static inline bool
IsSetterKind(FunctionSyntaxKind kind)
{
    return kind == FunctionSyntaxKind::Setter ||
           kind == FunctionSyntaxKind::SetterNoExpressionClosure;
}

static inline bool
IsMethodDefinitionKind(FunctionSyntaxKind kind)
{
    return kind == FunctionSyntaxKind::Method ||
           kind == FunctionSyntaxKind::FieldInitializer ||
           IsConstructorKind(kind) ||
           IsGetterKind(kind) || IsSetterKind(kind);
}

class ParseNode
{
    uint16_t pn_type;   /* PNK_* type */
    uint8_t pn_op;      /* see JSOp enum and jsopcode.tbl */
    uint8_t pn_arity:4; /* see ParseNodeArity enum */
    bool pn_parens:1;   /* this expr was enclosed in parens */
    bool pn_rhs_anon_fun:1;  /* this expr is anonymous function or class that
                              * is a direct RHS of PNK_ASSIGN or PNK_COLON of
                              * property, that needs SetFunctionName. */

    ParseNode(const ParseNode& other) = delete;
    void operator=(const ParseNode& other) = delete;

  public:
    ParseNode(ParseNodeKind kind, JSOp op, ParseNodeArity arity)
      : pn_type(kind),
        pn_op(op),
        pn_arity(arity),
        pn_parens(false),
        pn_rhs_anon_fun(false),
        pn_pos(0, 0),
        pn_next(nullptr)
    {
        MOZ_ASSERT(kind < PNK_LIMIT);
        memset(&pn_u, 0, sizeof pn_u);
    }

    ParseNode(ParseNodeKind kind, JSOp op, ParseNodeArity arity, const TokenPos& pos)
      : pn_type(kind),
        pn_op(op),
        pn_arity(arity),
        pn_parens(false),
        pn_rhs_anon_fun(false),
        pn_pos(pos),
        pn_next(nullptr)
    {
        MOZ_ASSERT(kind < PNK_LIMIT);
        memset(&pn_u, 0, sizeof pn_u);
    }

    JSOp getOp() const                     { return JSOp(pn_op); }
    void setOp(JSOp op)                    { pn_op = op; }
    bool isOp(JSOp op) const               { return getOp() == op; }

    ParseNodeKind getKind() const {
        MOZ_ASSERT(pn_type < PNK_LIMIT);
        return ParseNodeKind(pn_type);
    }
    void setKind(ParseNodeKind kind) {
        MOZ_ASSERT(kind < PNK_LIMIT);
        pn_type = kind;
    }
    bool isKind(ParseNodeKind kind) const  { return getKind() == kind; }

    ParseNodeArity getArity() const        { return ParseNodeArity(pn_arity); }
    bool isArity(ParseNodeArity a) const   { return getArity() == a; }
    void setArity(ParseNodeArity a)        { pn_arity = a; }

    bool isBinaryOperation() const {
        ParseNodeKind kind = getKind();
        return PNK_BINOP_FIRST <= kind && kind <= PNK_BINOP_LAST;
    }

    /* Boolean attributes. */
    bool isInParens() const                { return pn_parens; }
    bool isLikelyIIFE() const              { return isInParens(); }
    void setInParens(bool enabled)         { pn_parens = enabled; }

    bool isDirectRHSAnonFunction() const {
        return pn_rhs_anon_fun;
    }
    void setDirectRHSAnonFunction(bool enabled) {
        pn_rhs_anon_fun = enabled;
    }

    TokenPos            pn_pos;         /* two 16-bit pairs here, for 64 bits */
    ParseNode*          pn_next;        /* intrinsic link in parent PN_LIST */

    union {
        struct {                        /* list of next-linked nodes */
          private:
            friend class ListNode;
            ParseNode*  head;           /* first node in list */
            ParseNode** tail;           /* ptr to last node's pn_next in list */
            uint32_t    count;          /* number of nodes in list */
            uint32_t    xflags;         /* see ListNode class */
        } list;
        struct {                        /* ternary: if, for(;;), ?: */
          private:
            friend class TernaryNode;
            ParseNode*  kid1;           /* condition, discriminant, etc. */
            ParseNode*  kid2;           /* then-part, case list, etc. */
            ParseNode*  kid3;           /* else-part, default case, etc. */
        } ternary;
        struct {                        /* two kids if binary */
          private:
            friend class BinaryNode;
            friend class ForNode;
            friend class ClassField;
            friend class ClassMethod;
            friend class PropertyAccessBase;
            friend class SwitchStatement;
            ParseNode*  left;
            ParseNode*  right;
            union {
                unsigned iflags;        /* JSITER_* flags for PNK_{COMPREHENSION,}FOR node */
                bool isStatic;          /* only for PNK_CLASSMETHOD and PNK_CLASSFIELD */
                bool hasDefault;        /* only for PNK_SWITCH */
            };
        } binary;
        struct {                        /* one kid if unary */
          private:
            friend class UnaryNode;
            ParseNode*  kid;
            bool        prologue;       /* directive prologue member */
        } unary;
        struct {                        /* name, labeled statement, etc. */
          private:
            friend class NameNode;
            JSAtom*     atom;           /* lexical name or label atom */
            ParseNode*  initOrStmt;     /* var initializer, argument default,
                                         * or label statement target */
        } name;
        struct {
          private:
            friend class RegExpLiteral;
            ObjectBox* objbox;
        } regexp;
        struct {
          private:
            friend class FunctionNode;
            FunctionBox* funbox;
            ParseNode* body;
            FunctionSyntaxKind syntaxKind;
        } function;
        struct {
          private:
            friend class ModuleNode;
            ParseNode* body;
        } module;
        struct {
          private:
            friend class LexicalScopeNode;
            LexicalScope::Data* bindings;
            ParseNode*          body;
        } scope;
        struct {
          private:
            friend class NumericLiteral;
            double       value;         /* aligned numeric literal value */
            DecimalPoint decimalPoint;  /* Whether the number has a decimal point */
        } number;
        struct {
          private:
            friend class BigIntLiteral;
            BigIntBox* box;
        } bigint;
        class {
          private:
            friend class LoopControlStatement;
            PropertyName*    label;    /* target of break/continue statement */
        } loopControl;
    } pn_u;

  public:
    /*
     * If |left| is a list of the given kind/left-associative op, append
     * |right| to it and return |left|.  Otherwise return a [left, right] list.
     */
    static ParseNode*
    appendOrCreateList(ParseNodeKind kind, JSOp op, ParseNode* left, ParseNode* right,
                       FullParseHandler* handler, ParseContext* pc);

    inline PropertyName* name() const;

    /* True if pn is a parsenode representing a literal constant. */
    bool isLiteral() const {
        return isKind(PNK_NUMBER) ||
               isKind(PNK_BIGINT) ||
               isKind(PNK_STRING) ||
               isKind(PNK_TRUE) ||
               isKind(PNK_FALSE) ||
               isKind(PNK_NULL) ||
               isKind(PNK_RAW_UNDEFINED);
    }

    // True iff this is a for-in/of loop variable declaration (var/let/const).
    bool isForLoopDeclaration() const;

    enum AllowConstantObjects {
        DontAllowObjects = 0,
        AllowObjects,
        ForCopyOnWriteArray
    };

    MOZ_MUST_USE bool getConstantValue(ExclusiveContext* cx, AllowConstantObjects allowObjects,
                                       MutableHandleValue vp, Value* compare = nullptr,
                                       size_t ncompare = 0, NewObjectKind newKind = TenuredObject);
    inline bool isConstant();

    template <class NodeType>
    inline bool is() const {
        return NodeType::test(*this);
    }

    /* Casting operations. */
    template <class NodeType>
    inline NodeType& as() {
        MOZ_ASSERT(NodeType::test(*this));
        return *static_cast<NodeType*>(this);
    }

    template <class NodeType>
    inline const NodeType& as() const {
        MOZ_ASSERT(NodeType::test(*this));
        return *static_cast<const NodeType*>(this);
    }

#ifdef DEBUG
    void dump();
    void dump(int indent);
#endif
};

class NullaryNode : public ParseNode
{
  public:
    NullaryNode(ParseNodeKind kind, const TokenPos& pos)
      : ParseNode(kind, JSOP_NOP, PN_NULLARY, pos) {}
    NullaryNode(ParseNodeKind kind, JSOp op, const TokenPos& pos)
      : ParseNode(kind, op, PN_NULLARY, pos) {}

    static bool test(const ParseNode& node) {
        return node.isArity(PN_NULLARY);
    }

#ifdef DEBUG
    void dump();
#endif
};

class NameNode : public ParseNode
{
  protected:
    NameNode(ParseNodeKind kind, JSOp op, JSAtom* atom, ParseNode* initOrStmt, const TokenPos& pos)
      : ParseNode(kind, op, PN_NAME, pos)
    {
        pn_u.name.atom = atom;
        pn_u.name.initOrStmt = initOrStmt;
    }

  public:
    NameNode(ParseNodeKind kind, JSOp op, JSAtom* atom, const TokenPos& pos)
      : ParseNode(kind, op, PN_NAME, pos)
    {
        pn_u.name.atom = atom;
        pn_u.name.initOrStmt = nullptr;
    }

    static bool test(const ParseNode& node) {
        return node.isArity(PN_NAME);
    }

#ifdef DEBUG
    void dump(int indent);
#endif

    JSAtom* atom() const {
        return pn_u.name.atom;
    }
    
    bool isPrivateName() const {
        return atom()->asPropertyName()->latin1OrTwoByteChar(0) == '#';
    }

    ParseNode* initializer() const {
        return pn_u.name.initOrStmt;
    }

    void setAtom(JSAtom* atom) {
        pn_u.name.atom = atom;
    }

    void setInitializer(ParseNode* init) {
        pn_u.name.initOrStmt = init;
    }

    // Methods used by FoldConstants.cpp.
    ParseNode** unsafeInitializerReference() {
        return &pn_u.name.initOrStmt;
    }
};

class UnaryNode : public ParseNode
{
  public:
    UnaryNode(ParseNodeKind kind, JSOp op, const TokenPos& pos, ParseNode* kid)
      : ParseNode(kind, op, PN_UNARY, pos)
    {
        pn_u.unary.kid = kid;
    }

    static bool test(const ParseNode& node) {
        return node.isArity(PN_UNARY);
    }

#ifdef DEBUG
    void dump(int indent);
#endif

    ParseNode* kid() const {
        return pn_u.unary.kid;
    }

    /* Return true if this node appears in a Directive Prologue. */
    bool isDirectivePrologueMember() const {
        return pn_u.unary.prologue;
    }

    void setIsDirectivePrologueMember() {
        pn_u.unary.prologue = true;
    }

    /*
     * Non-null if this is a statement node which could be a member of a
     * Directive Prologue: an expression statement consisting of a single
     * string literal.
     *
     * This considers only the node and its children, not its context. After
     * parsing, check the node's prologue flag to see if it is indeed part of
     * a directive prologue.
     *
     * Note that a Directive Prologue can contain statements that cannot
     * themselves be directives (string literals that include escape sequences
     * or escaped newlines, say). This member function returns true for such
     * nodes; we use it to determine the extent of the prologue.
     */
    JSAtom* isStringExprStatement() const {
        if (isKind(PNK_SEMI)) {
            ParseNode* expr = kid();
            if (expr && expr->isKind(PNK_STRING) && !expr->isInParens()) {
                return kid()->as<NameNode>().atom();
            }
        }
        return nullptr;
    }

    // Methods used by FoldConstants.cpp.
    ParseNode** unsafeKidReference() {
        return &pn_u.unary.kid;
    }
};

class BinaryNode : public ParseNode
{
  public:
    BinaryNode(ParseNodeKind kind, JSOp op, const TokenPos& pos, ParseNode* left, ParseNode* right)
      : ParseNode(kind, op, PN_BINARY, pos)
    {
        pn_u.binary.left = left;
        pn_u.binary.right = right;
    }

    BinaryNode(ParseNodeKind kind, JSOp op, ParseNode* left, ParseNode* right)
      : ParseNode(kind, op, PN_BINARY, TokenPos::box(left->pn_pos, right->pn_pos))
    {
        pn_u.binary.left = left;
        pn_u.binary.right = right;
    }

    static bool test(const ParseNode& node) {
        return node.isArity(PN_BINARY);
    }

#ifdef DEBUG
    void dump(int indent);
#endif

    ParseNode* left() const {
        return pn_u.binary.left;
    }

    ParseNode* right() const {
        return pn_u.binary.right;
    }

    // Methods used by FoldConstants.cpp.
    // caller are responsible for keeping the list consistent.
    ParseNode** unsafeLeftReference() {
        return &pn_u.binary.left;
    }

    ParseNode** unsafeRightReference() {
        return &pn_u.binary.right;
    }
};

class AssignmentNode : public BinaryNode
{
  public:
    AssignmentNode(ParseNodeKind kind, JSOp op, ParseNode* left, ParseNode* right)
      : BinaryNode(kind, op, TokenPos(left->pn_pos.begin, right->pn_pos.end), left, right)
    {}

    static bool test(const ParseNode& node) {
        ParseNodeKind kind = node.getKind();
        bool match = PNK_ASSIGNMENT_START <= kind &&
                     kind <= PNK_ASSIGNMENT_LAST;
        MOZ_ASSERT_IF(match, node.is<BinaryNode>());
        return match;
    }
};

class ForNode : public BinaryNode
{
  public:
    ForNode(const TokenPos& pos, ParseNode* forHead, ParseNode* body, unsigned iflags)
      : BinaryNode(PNK_FOR,
                   forHead->isKind(PNK_FORIN) ? JSOP_ITER : JSOP_NOP,
                   pos, forHead, body)
    {
        MOZ_ASSERT(forHead->isKind(PNK_FORIN) ||
                   forHead->isKind(PNK_FOROF) ||
                   forHead->isKind(PNK_FORHEAD));
        pn_u.binary.iflags = iflags;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_FOR) || node.isKind(PNK_COMPREHENSIONFOR);
        MOZ_ASSERT_IF(match, node.is<BinaryNode>());
        return match;
    }

    TernaryNode* head() const {
        return &left()->as<TernaryNode>();
    }

    ParseNode* body() const {
        return right();
    }

    unsigned iflags() const {
        return pn_u.binary.iflags;
    }
};

class TernaryNode : public ParseNode
{
  public:
    TernaryNode(ParseNodeKind kind, JSOp op, ParseNode* kid1, ParseNode* kid2, ParseNode* kid3)
      : TernaryNode(kind, op, kid1, kid2, kid3,
                    TokenPos((kid1 ? kid1 : kid2 ? kid2 : kid3)->pn_pos.begin,
                             (kid3 ? kid3 : kid2 ? kid2 : kid1)->pn_pos.end))
    {}

    TernaryNode(ParseNodeKind kind, JSOp op, ParseNode* kid1, ParseNode* kid2, ParseNode* kid3,
                const TokenPos& pos)
      : ParseNode(kind, op, PN_TERNARY, pos)
    {
        pn_u.ternary.kid1 = kid1;
        pn_u.ternary.kid2 = kid2;
        pn_u.ternary.kid3 = kid3;
    }

    static bool test(const ParseNode& node) {
        return node.isArity(PN_TERNARY);
    }

#ifdef DEBUG
    void dump(int indent);
#endif

    ParseNode* kid1() const {
        return pn_u.ternary.kid1;
    }

    ParseNode* kid2() const {
        return pn_u.ternary.kid2;
    }

    ParseNode* kid3() const {
        return pn_u.ternary.kid3;
    }

    // Methods used by FoldConstants.cpp.
    ParseNode** unsafeKid1Reference() {
        return &pn_u.ternary.kid1;
    }

    ParseNode** unsafeKid2Reference() {
        return &pn_u.ternary.kid2;
    }

    ParseNode** unsafeKid3Reference() {
        return &pn_u.ternary.kid3;
    }
};

class ListNode : public ParseNode
{
  private:
    // xflags bits.

    // Statement list has top-level function statements.
    static constexpr uint32_t hasTopLevelFunctionDeclarationsBit = 0x01;

    // One or more of
    //   * array has holes
    //   * array has spread node
    static constexpr uint32_t hasArrayHoleOrSpreadBit = 0x02;

    // Array/Object/Class initializer has non-constants.
    //   * array has holes
    //   * array has spread node
    //   * array has element which is known not to be constant
    //   * array has no element
    //   * object/class has __proto__
    //   * object/class has property which is known not to be constant
    //   * object/class shorthand property
    //   * object/class spread property
    //   * object/class has method
    //   * object/class has computed property
    static constexpr uint32_t hasNonConstInitializerBit = 0x04;

  public:
    ListNode(ParseNodeKind kind, const TokenPos& pos)
      : ParseNode(kind, JSOP_NOP, PN_LIST, pos)
    {
        makeEmpty();
    }

    ListNode(ParseNodeKind kind, JSOp op, const TokenPos& pos)
      : ParseNode(kind, op, PN_LIST, pos)
    {
        makeEmpty();
    }

    ListNode(ParseNodeKind kind, JSOp op, ParseNode* kid)
      : ParseNode(kind, op, PN_LIST, kid->pn_pos)
    {
        if (kid->pn_pos.begin < pn_pos.begin) {
            pn_pos.begin = kid->pn_pos.begin;
        }
        pn_pos.end = kid->pn_pos.end;

        pn_u.list.head = kid;
        pn_u.list.tail = &kid->pn_next;
        pn_u.list.count = 1;
        pn_u.list.xflags = 0;
    }

    static bool test(const ParseNode& node) {
        return node.isArity(PN_LIST);
    }

#ifdef DEBUG
    void dump(int indent);
#endif

    ParseNode* head() const {
        return pn_u.list.head;
    }

    ParseNode** tail() const {
        return pn_u.list.tail;
    }

    uint32_t count() const {
        return pn_u.list.count;
    }

    bool empty() const {
        return count() == 0;
    }

    MOZ_MUST_USE bool hasTopLevelFunctionDeclarations() const {
        MOZ_ASSERT(isKind(PNK_STATEMENTLIST));
        return pn_u.list.xflags & hasTopLevelFunctionDeclarationsBit;
    }

    MOZ_MUST_USE bool hasArrayHoleOrSpread() const {
        MOZ_ASSERT(isKind(PNK_ARRAY));
        return pn_u.list.xflags & hasArrayHoleOrSpreadBit;
    }

    MOZ_MUST_USE bool hasNonConstInitializer() const {
        MOZ_ASSERT(isKind(PNK_ARRAY) ||
                   isKind(PNK_OBJECT) ||
                   isKind(PNK_CLASSMEMBERLIST));
        return pn_u.list.xflags & hasNonConstInitializerBit;
    }

    void setHasTopLevelFunctionDeclarations() {
        MOZ_ASSERT(isKind(PNK_STATEMENTLIST));
        pn_u.list.xflags |= hasTopLevelFunctionDeclarationsBit;
    }

    void setHasArrayHoleOrSpread() {
        MOZ_ASSERT(isKind(PNK_ARRAY));
        pn_u.list.xflags |= hasArrayHoleOrSpreadBit;
    }

    void setHasNonConstInitializer() {
        MOZ_ASSERT(isKind(PNK_ARRAY) ||
                   isKind(PNK_OBJECT) ||
                   isKind(PNK_CLASSMEMBERLIST));
        pn_u.list.xflags |= hasNonConstInitializerBit;
    }

    void checkConsistency() const
#ifndef DEBUG
    {}
#endif
    ;

    /*
     * Compute a pointer to the last element in a singly-linked list. NB: list
     * must be non-empty -- this is asserted!
     */
    ParseNode* last() const {
        MOZ_ASSERT(!empty());
        //
        // ParseNode                      ParseNode
        // +-----+---------+-----+        +-----+---------+-----+
        // | ... | pn_next | ... | +-...->| ... | pn_next | ... |
        // +-----+---------+-----+ |      +-----+---------+-----+
        // ^       |               |      ^     ^
        // |       +---------------+      |     |
        // |                              |     tail()
        // |                              |
        // head()                         last()
        //
        return (ParseNode*)(uintptr_t(tail()) - offsetof(ParseNode, pn_next));
    }

    void replaceLast(ParseNode* node) {
        MOZ_ASSERT(!empty());
        pn_pos.end = node->pn_pos.end;

        ParseNode* item = head();
        ParseNode* lastNode = last();
        MOZ_ASSERT(item);
        if (item == lastNode) {
            pn_u.list.head = node;
        } else {
            while (item->pn_next != lastNode) {
                MOZ_ASSERT(item->pn_next);
                item = item->pn_next;
            }
            item->pn_next = node;
        }
        pn_u.list.tail = &node->pn_next;
    }

    void makeEmpty() {
        pn_u.list.head = nullptr;
        pn_u.list.tail = &pn_u.list.head;
        pn_u.list.count = 0;
        pn_u.list.xflags = 0;
    }

    void append(ParseNode* item) {
        MOZ_ASSERT(item->pn_pos.begin >= pn_pos.begin);
        pn_pos.end = item->pn_pos.end;
        *pn_u.list.tail = item;
        pn_u.list.tail = &item->pn_next;
        pn_u.list.count++;
    }

    void prepend(ParseNode* item) {
        item->pn_next = pn_u.list.head;
        pn_u.list.head = item;
        if (pn_u.list.tail == &pn_u.list.head) {
            pn_u.list.tail = &item->pn_next;
        }
        pn_u.list.count++;
    }

    // Methods used by FoldConstants.cpp.
    // Caller is responsible for keeping the list consistent.
    ParseNode** unsafeHeadReference() {
        return &pn_u.list.head;
    }

    void unsafeReplaceTail(ParseNode** newTail) {
        pn_u.list.tail = newTail;
        checkConsistency();
    }

    void unsafeSetTail(ParseNode* newTail) {
        *pn_u.list.tail = newTail;
    }

    void unsafeDecrementCount() {
        MOZ_ASSERT(count() > 1);
        pn_u.list.count--;
    }

  private:
    // Classes to iterate over ListNode contents:
    //
    // Usage:
    //   ListNode* list;
    //   for (ParseNode* item : list->contents()) {
    //     // item is ParseNode* typed.
    //   }
    class iterator
    {
      private:
        ParseNode* node_;

        friend class ListNode;
        explicit iterator(ParseNode* node)
          : node_(node)
        {}

      public:
        bool operator==(const iterator& other) const {
            return node_ == other.node_;
        }

        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

        iterator& operator++() {
            node_ = node_->pn_next;
            return *this;
        }

        ParseNode* operator*() {
            return node_;
        }

        const ParseNode* operator*() const {
            return node_;
        }
    };

    class range
    {
      private:
        ParseNode* begin_;
        ParseNode* end_;

        friend class ListNode;
        range(ParseNode* begin, ParseNode* end)
          : begin_(begin),
            end_(end)
        {}

      public:
        iterator begin() {
            return iterator(begin_);
        }

        iterator end() {
            return iterator(end_);
        }

        const iterator begin() const {
            return iterator(begin_);
        }

        const iterator end() const {
            return iterator(end_);
        }

        const iterator cbegin() const {
            return begin();
        }

        const iterator cend() const {
            return end();
        }
    };

    typedef std::function<bool(ParseNode*)> predicate_fun;

#ifdef DEBUG
  MOZ_MUST_USE bool contains(ParseNode* target) const {
      MOZ_ASSERT(target);
      for (ParseNode* node : contents()) {
          if (target == node) {
              return true;
          }
      }
      return false;
  }
#endif

  public:
    range contents() {
        return range(head(), nullptr);
    }

    const range contents() const {
        return range(head(), nullptr);
    }

    range contentsFrom(ParseNode* begin) {
        MOZ_ASSERT_IF(begin, contains(begin));
        return range(begin, nullptr);
    }

    const range contentsFrom(ParseNode* begin) const {
        MOZ_ASSERT_IF(begin, contains(begin));
        return range(begin, nullptr);
    }

    range contentsTo(ParseNode* end) {
        MOZ_ASSERT_IF(end, contains(end));
        return range(head(), end);
    }

    const range contentsTo(ParseNode* end) const {
        MOZ_ASSERT_IF(end, contains(end));
        return range(head(), end);
    }

    // Predicate functions, like their counterparts in C++17
    size_t count_if(predicate_fun predicate) const {
        size_t count = 0;
        for (ParseNode* node = head(); node; node = node->pn_next) {
            if (predicate(node))
                count++;
        }
        return count;
    }

    bool any_of(predicate_fun predicate) const {
        for (ParseNode* node = head(); node; node = node->pn_next) {
            if (predicate(node))
                return true;
        }
        return false;
    }
};

inline bool
ParseNode::isForLoopDeclaration() const
{
    if (isKind(PNK_VAR) || isKind(PNK_LET) || isKind(PNK_CONST)) {
        MOZ_ASSERT(!as<ListNode>().empty());
        return true;
    }

    return false;
}

class FunctionNode : public ParseNode
{
  public:
    FunctionNode(FunctionSyntaxKind syntaxKind, const TokenPos& pos)
      : ParseNode(PNK_FUNCTION, JSOP_NOP, PN_FUNCTION, pos)
    {
        MOZ_ASSERT(!pn_u.function.body);
        MOZ_ASSERT(!pn_u.function.funbox);
        pn_u.function.syntaxKind = syntaxKind;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_FUNCTION);
        MOZ_ASSERT_IF(match, node.isArity(PN_FUNCTION));
        return match;
    }

#ifdef DEBUG
    void dump(int indent);
#endif

    FunctionBox* funbox() const {
        return pn_u.function.funbox;
    }

    ListNode* body() const {
        return pn_u.function.body ? &pn_u.function.body->as<ListNode>() : nullptr;
    }

    void setFunbox(FunctionBox* funbox) {
        pn_u.function.funbox = funbox;
    }

    void setBody(ListNode* body) {
        pn_u.function.body = body;
    }

    // Methods used by FoldConstants.cpp.
    ParseNode** unsafeBodyReference() {
        return &pn_u.function.body;
    }

    FunctionSyntaxKind syntaxKind() const { return pn_u.function.syntaxKind; }

    bool functionIsHoisted() const {
        return syntaxKind() == FunctionSyntaxKind::Statement;
    }
};

class ModuleNode : public ParseNode
{
  public:
    ModuleNode(const TokenPos& pos)
      : ParseNode(PNK_MODULE, JSOP_NOP, PN_MODULE, pos)
    {
        MOZ_ASSERT(!pn_u.module.body);
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_MODULE);
        MOZ_ASSERT_IF(match, node.isArity(PN_MODULE));
        return match;
    }

#ifdef DEBUG
    void dump(int indent);
#endif

    ListNode* body() const {
        return &pn_u.module.body->as<ListNode>();
    }

    void setBody(ListNode* body) {
        pn_u.module.body = body;
    }

    // Methods used by FoldConstants.cpp.
    ParseNode** unsafeBodyReference() {
        return &pn_u.module.body;
    }
};

class NumericLiteral : public ParseNode
{
  public:
    NumericLiteral(double value, DecimalPoint decimalPoint, const TokenPos& pos)
      : ParseNode(PNK_NUMBER, JSOP_NOP, PN_NUMBER, pos)
    {
        pn_u.number.value = value;
        pn_u.number.decimalPoint = decimalPoint;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_NUMBER);
        MOZ_ASSERT_IF(match, node.isArity(PN_NUMBER));
        return match;
    }

#ifdef DEBUG
    void dump(int indent);
#endif

    double value() const {
        return pn_u.number.value;
    }

    DecimalPoint decimalPoint() const {
        return pn_u.number.decimalPoint;
    }

    void setValue(double v) {
        pn_u.number.value = v;
    }
};

class BigIntLiteral : public ParseNode
{
  public:
    BigIntLiteral(BigIntBox* bibox, const TokenPos& pos)
      : ParseNode(PNK_BIGINT, JSOP_NOP, PN_BIGINT, pos)
    {
        pn_u.bigint.box = bibox;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_BIGINT);
        MOZ_ASSERT_IF(match, node.isArity(PN_BIGINT));
        return match;
    }

#ifdef DEBUG
    void dump(int indent);
#endif

    BigIntBox* box() const {
        return pn_u.bigint.box;
    }
};

class LexicalScopeNode : public ParseNode
{
  public:
    LexicalScopeNode(LexicalScope::Data* bindings, ParseNode* body)
      : ParseNode(PNK_LEXICALSCOPE, JSOP_NOP, PN_SCOPE, body->pn_pos)
    {
        pn_u.scope.bindings = bindings;
        pn_u.scope.body = body;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_LEXICALSCOPE);
        MOZ_ASSERT_IF(match, node.isArity(PN_SCOPE));
        return match;
    }

#ifdef DEBUG
    void dump(int indent);
#endif

    Handle<LexicalScope::Data*> scopeBindings() const {
        MOZ_ASSERT(!isEmptyScope());
        // Bindings' GC safety depend on the presence of an AutoKeepAtoms that
        // the rest of the frontend also depends on.
        return Handle<LexicalScope::Data*>::fromMarkedLocation(&pn_u.scope.bindings);
    }

    ParseNode* scopeBody() const {
        return pn_u.scope.body;
    }

    void setScopeBody(ParseNode* body) {
        pn_u.scope.body = body;
    }

    bool isEmptyScope() const {
        return !pn_u.scope.bindings;
    }

    ParseNode** unsafeScopeBodyReference() {
        return &pn_u.scope.body;
    }
};

class LabeledStatement : public NameNode
{
  public:
    LabeledStatement(PropertyName* label, ParseNode* stmt, uint32_t begin)
      : NameNode(PNK_LABEL, JSOP_NOP, label, stmt, TokenPos(begin, stmt->pn_pos.end))
    { }

    PropertyName* label() const {
        return atom()->asPropertyName();
    }

    ParseNode* statement() const {
        return initializer();
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_LABEL);
        MOZ_ASSERT_IF(match, node.isArity(PN_NAME));
        MOZ_ASSERT_IF(match, node.isOp(JSOP_NOP));
        return match;
    }

    // Methods used by FoldConstants.cpp.
    ParseNode** unsafeStatementReference() {
        return unsafeInitializerReference();
    }
};

// Inside a switch statement, a CaseClause is a case-label and the subsequent
// statements. The same node type is used for DefaultClauses. The only
// difference is that their caseExpression() is null.
class CaseClause : public BinaryNode
{
  public:
    CaseClause(ParseNode* expr, ParseNode* stmts, uint32_t begin)
      : BinaryNode(PNK_CASE, JSOP_NOP, TokenPos(begin, stmts->pn_pos.end), expr, stmts) {}

    ParseNode* caseExpression() const {
        return left();
    }

    bool isDefault() const {
        return !caseExpression();
    }

    ListNode* statementList() const {
        return &right()->as<ListNode>();
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_CASE);
        MOZ_ASSERT_IF(match, node.is<BinaryNode>());
        MOZ_ASSERT_IF(match, node.isOp(JSOP_NOP));
        return match;
    }
};

class LoopControlStatement : public ParseNode
{
  protected:
    LoopControlStatement(ParseNodeKind kind, PropertyName* label, const TokenPos& pos)
      : ParseNode(kind, JSOP_NOP, PN_LOOP, pos)
    {
        MOZ_ASSERT(kind == PNK_BREAK || kind == PNK_CONTINUE);
        pn_u.loopControl.label = label;
    }

  public:
    /* Label associated with this break/continue statement, if any. */
    PropertyName* label() const {
        return pn_u.loopControl.label;
    }

#ifdef DEBUG
    void dump(int indent);
#endif

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_BREAK) || node.isKind(PNK_CONTINUE);
        MOZ_ASSERT_IF(match, node.isArity(PN_LOOP));
        MOZ_ASSERT_IF(match, node.isOp(JSOP_NOP));
        return match;
    }
};

class BreakStatement : public LoopControlStatement
{
  public:
    BreakStatement(PropertyName* label, const TokenPos& pos)
      : LoopControlStatement(PNK_BREAK, label, pos)
    { }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_BREAK);
        MOZ_ASSERT_IF(match, node.is<LoopControlStatement>());
        MOZ_ASSERT_IF(match, node.isOp(JSOP_NOP));
        return match;
    }
};

class ContinueStatement : public LoopControlStatement
{
  public:
    ContinueStatement(PropertyName* label, const TokenPos& pos)
      : LoopControlStatement(PNK_CONTINUE, label, pos)
    { }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_CONTINUE);
        MOZ_ASSERT_IF(match, node.is<LoopControlStatement>());
        MOZ_ASSERT_IF(match, node.isOp(JSOP_NOP));
        return match;
    }
};

class DebuggerStatement : public NullaryNode
{
  public:
    explicit DebuggerStatement(const TokenPos& pos)
      : NullaryNode(PNK_DEBUGGER, JSOP_NOP, pos)
    { }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_DEBUGGER);
        MOZ_ASSERT_IF(match, node.is<NullaryNode>());
        return match;
    }
};

class ConditionalExpression : public TernaryNode
{
  public:
    ConditionalExpression(ParseNode* condition, ParseNode* thenExpr, ParseNode* elseExpr)
      : TernaryNode(PNK_CONDITIONAL, JSOP_NOP, condition, thenExpr, elseExpr,
                    TokenPos(condition->pn_pos.begin, elseExpr->pn_pos.end))
    {
        MOZ_ASSERT(condition);
        MOZ_ASSERT(thenExpr);
        MOZ_ASSERT(elseExpr);
    }

    ParseNode& condition() const {
        return *kid1();
    }

    ParseNode& thenExpression() const {
        return *kid2();
    }

    ParseNode& elseExpression() const {
        return *kid3();
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_CONDITIONAL);
        MOZ_ASSERT_IF(match, node.is<TernaryNode>());
        MOZ_ASSERT_IF(match, node.isOp(JSOP_NOP));
        return match;
    }
};

class TryNode : public TernaryNode
{
  public:
    TryNode(uint32_t begin, ParseNode* body, ListNode* catchList, ParseNode* finallyBlock)
      : TernaryNode(PNK_TRY, JSOP_NOP, body, catchList, finallyBlock,
                    TokenPos(begin, (finallyBlock ? finallyBlock : catchList)->pn_pos.end))
    {
        MOZ_ASSERT(body);
        MOZ_ASSERT(catchList || finallyBlock);
        MOZ_ASSERT_IF(catchList, catchList->isKind(PNK_CATCHLIST));
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_TRY);
        MOZ_ASSERT_IF(match, node.is<TernaryNode>());
        return match;
    }

    ParseNode* body() const {
        return kid1();
    }

    ListNode* catchList() const {
        return kid2() ? &kid2()->as<ListNode>() : nullptr;
    }

    ParseNode* finallyBlock() const {
        return kid3();
    }
};

class ThisLiteral : public UnaryNode
{
  public:
    ThisLiteral(const TokenPos& pos, ParseNode* thisName)
      : UnaryNode(PNK_THIS, JSOP_NOP, pos, thisName)
    { }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_THIS);
        MOZ_ASSERT_IF(match, node.is<UnaryNode>());
        return match;
    }
};

class NullLiteral : public NullaryNode
{
  public:
    explicit NullLiteral(const TokenPos& pos)
      : NullaryNode(PNK_NULL, JSOP_NULL, pos)
    { }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_NULL);
        MOZ_ASSERT_IF(match, node.is<NullaryNode>());
        return match;
    }
};

// This is only used internally, currently just for tagged templates and the
// initial value of fields without initializers. It represents the value
// 'undefined' (aka `void 0`), like NullLiteral represents the value 'null'.
class RawUndefinedLiteral : public NullaryNode
{
  public:
    explicit RawUndefinedLiteral(const TokenPos& pos)
      : NullaryNode(PNK_RAW_UNDEFINED, JSOP_UNDEFINED, pos) { }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_RAW_UNDEFINED);
        MOZ_ASSERT_IF(match, node.is<NullaryNode>());
        return match;
    }
};

class BooleanLiteral : public NullaryNode
{
  public:
    BooleanLiteral(bool b, const TokenPos& pos)
      : NullaryNode(b ? PNK_TRUE : PNK_FALSE, b ? JSOP_TRUE : JSOP_FALSE, pos)
    { }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_TRUE) || node.isKind(PNK_FALSE);
        MOZ_ASSERT_IF(match, node.is<NullaryNode>());
        return match;
    }
};

class RegExpLiteral : public ParseNode
{
  public:
    RegExpLiteral(ObjectBox* reobj, const TokenPos& pos)
      : ParseNode(PNK_REGEXP, JSOP_REGEXP, PN_REGEXP, pos)
    {
        pn_u.regexp.objbox = reobj;
    }

    ObjectBox* objbox() const {
        return pn_u.regexp.objbox;
    }

#ifdef DEBUG
    void dump(int indent);
#endif

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_REGEXP);
        MOZ_ASSERT_IF(match, node.isArity(PN_REGEXP));
        MOZ_ASSERT_IF(match, node.isOp(JSOP_REGEXP));
        return match;
    }
};

class PropertyAccessBase : public BinaryNode
{
  public:
    /*
     * PropertyAccess nodes can have any expression/'super' as left-hand
     * side, but the name must be a PNK_PROPERTYNAME node.
     */
    PropertyAccessBase(ParseNodeKind kind, ParseNode* lhs, NameNode* name, uint32_t begin, uint32_t end)
      : BinaryNode(kind, JSOP_NOP, TokenPos(begin, end), lhs, name)
    {
        MOZ_ASSERT(lhs);
        MOZ_ASSERT(name);
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_DOT) ||
                     node.isKind(PNK_OPTDOT);
        MOZ_ASSERT_IF(match, node.is<BinaryNode>());
        MOZ_ASSERT_IF(match, node.as<BinaryNode>().right()->isKind(PNK_PROPERTYNAME));
        return match;
    }

    ParseNode& expression() const {
        return *left();
    }

    NameNode& key() const {
        return right()->as<NameNode>();
    }

    // Method used by BytecodeEmitter::emitPropLHS for optimization.
    // Those methods allow expression to temporarily be nullptr for
    // optimization purpose.
    ParseNode* maybeExpression() const {
        return left();
    }

    void setExpression(ParseNode* pn) {
        pn_u.binary.left = pn;
    }

    PropertyName& name() const {
        return *right()->as<NameNode>().atom()->asPropertyName();
    }

    bool isSuper() const {
        // PNK_SUPERBASE cannot result from any expression syntax.
        return expression().isKind(PNK_SUPERBASE);
    }
};

class PropertyAccess : public PropertyAccessBase
{
  public:
    PropertyAccess(ParseNode* lhs, NameNode* name, uint32_t begin, uint32_t end)
      : PropertyAccessBase(PNK_DOT, lhs, name, begin, end)
    {
        MOZ_ASSERT(lhs != nullptr);
        MOZ_ASSERT(name != nullptr);
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_DOT);
        MOZ_ASSERT_IF(match, node.is<BinaryNode>());
        MOZ_ASSERT_IF(match, node.as<BinaryNode>().right()->isKind(PNK_PROPERTYNAME));
        return match;
    }
};

class OptionalPropertyAccess : public PropertyAccessBase
{
  public:
    OptionalPropertyAccess(ParseNode* lhs, NameNode* name, uint32_t begin, uint32_t end)
      : PropertyAccessBase(PNK_OPTDOT, lhs, name, begin, end)
    {
        MOZ_ASSERT(lhs != nullptr);
        MOZ_ASSERT(name != nullptr);
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_OPTDOT);
        MOZ_ASSERT_IF(match, node.is<BinaryNode>());
        MOZ_ASSERT_IF(match, node.as<BinaryNode>().right()->isKind(PNK_PROPERTYNAME));
        return match;
    }
};

class PropertyByValueBase : public BinaryNode
{
  public:
    PropertyByValueBase(ParseNodeKind kind, ParseNode* lhs, ParseNode* propExpr,
                    uint32_t begin, uint32_t end)
      : BinaryNode(kind, JSOP_NOP, TokenPos(begin, end), lhs, propExpr)
    {}

    ParseNode& expression() const {
        return *left();
    }

    ParseNode& key() const {
        return *right();
    }

    bool isSuper() const {
        return left()->isKind(PNK_SUPERBASE);
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_ELEM) ||
                     node.isKind(PNK_OPTELEM);
        MOZ_ASSERT_IF(match, node.is<BinaryNode>());
        return match;
    }
};

class PropertyByValue : public PropertyByValueBase {
  public:
    PropertyByValue(ParseNode* lhs, ParseNode* propExpr, uint32_t begin,
                    uint32_t end)
      : PropertyByValueBase(PNK_ELEM, lhs, propExpr, begin, end) {}

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_ELEM);
        MOZ_ASSERT_IF(match, node.isArity(PN_BINARY));
        return match;
    }
};

class OptionalPropertyByValue : public PropertyByValueBase {
 public:
    OptionalPropertyByValue(ParseNode* lhs, ParseNode* propExpr,
                            uint32_t begin, uint32_t end)
      : PropertyByValueBase(PNK_OPTELEM, lhs, propExpr, begin, end) {}

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_OPTELEM);
        MOZ_ASSERT_IF(match, node.isArity(PN_BINARY));
        return match;
    }
};

/*
 * A CallSiteNode represents the implicit call site object argument in a TaggedTemplate.
 */
class CallSiteNode : public ListNode
{
  public:
    explicit CallSiteNode(uint32_t begin): ListNode(PNK_CALLSITEOBJ, TokenPos(begin, begin + 1)) {}

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_CALLSITEOBJ);
        MOZ_ASSERT_IF(match, node.is<ListNode>());
        return match;
    }

    MOZ_MUST_USE bool getRawArrayValue(ExclusiveContext* cx, MutableHandleValue vp) {
        return head()->getConstantValue(cx, AllowObjects, vp);
    }

    ListNode* rawNodes() const {
        MOZ_ASSERT(head());
        return &head()->as<ListNode>();
    }
};

class ClassMethod : public BinaryNode
{
  public:
    /*
     * Method definitions often keep a name and function body that overlap,
     * so explicitly define the beginning and end here.
     */
    ClassMethod(ParseNode* name, ParseNode* body, JSOp op, bool isStatic)
      : BinaryNode(PNK_CLASSMETHOD, op, TokenPos(name->pn_pos.begin, body->pn_pos.end), name, body)
    {
        pn_u.binary.isStatic = isStatic;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_CLASSMETHOD);
        MOZ_ASSERT_IF(match, node.is<BinaryNode>());
        return match;
    }

    ParseNode& name() const {
        return *left();
    }
    FunctionNode& method() const {
        return right()->as<FunctionNode>();
    }
    bool isStatic() const {
        return pn_u.binary.isStatic;
    }
};


class ClassField : public BinaryNode
{
  public:
    ClassField(ParseNode* name, ParseNode* initializer, bool isStatic)
      : BinaryNode(PNK_CLASSFIELD, JSOP_NOP,
                   TokenPos::box(name->pn_pos, initializer->pn_pos),
                   name, initializer)
    {
        pn_u.binary.isStatic = isStatic;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_CLASSFIELD);
        MOZ_ASSERT_IF(match, node.isArity(PN_BINARY));
        return match;
    }

    ParseNode& name() const { return *left(); }

    FunctionNode* initializer() const { return &right()->as<FunctionNode>(); }

    bool isStatic() const {
        return pn_u.binary.isStatic;
    }
};

// Hold onto the function generated for a class static block like
//
// class A {
//  static { /* this static block */ }
// }
//
class StaticClassBlock : public UnaryNode
{
  public:
    explicit StaticClassBlock(FunctionNode* function)
        : UnaryNode(PNK_STATICCLASSBLOCK, JSOP_NOP, function->pn_pos, function) {
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_STATICCLASSBLOCK);
        MOZ_ASSERT_IF(match, node.is<UnaryNode>());
        return match;
    }
    FunctionNode* function() const { return &kid()->as<FunctionNode>(); }
};


class SwitchStatement : public BinaryNode
{
  public:
    SwitchStatement(uint32_t begin, ParseNode* discriminant, LexicalScopeNode* lexicalForCaseList,
                    bool hasDefault)
      : BinaryNode(PNK_SWITCH, JSOP_NOP,
                   TokenPos(begin, lexicalForCaseList->pn_pos.end),
                   discriminant, lexicalForCaseList)
    {
#ifdef DEBUG
        ListNode* cases = &lexicalForCaseList->scopeBody()->as<ListNode>();
        MOZ_ASSERT(cases->isKind(PNK_STATEMENTLIST));
        bool found = false;
        for (ParseNode* item : cases->contents()) {
            CaseClause* caseNode = &item->as<CaseClause>();
            if (caseNode->isDefault()) {
                found = true;
                break;
            }
        }
        MOZ_ASSERT(found == hasDefault);
#endif

        pn_u.binary.hasDefault = hasDefault;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_SWITCH);
        MOZ_ASSERT_IF(match, node.is<BinaryNode>());
        return match;
    }

    ParseNode& discriminant() const {
        return *left();
    }
    LexicalScopeNode& lexicalForCaseList() const {
        return right()->as<LexicalScopeNode>();
    }
    bool hasDefault() const {
        return pn_u.binary.hasDefault;
    }
};

class ClassNames : public BinaryNode
{
  public:
    ClassNames(ParseNode* outerBinding, ParseNode* innerBinding, const TokenPos& pos)
      : BinaryNode(PNK_CLASSNAMES, JSOP_NOP, pos, outerBinding, innerBinding)
    {
        MOZ_ASSERT_IF(outerBinding, outerBinding->isKind(PNK_NAME));
        MOZ_ASSERT(innerBinding->isKind(PNK_NAME));
        MOZ_ASSERT_IF(outerBinding,
                      innerBinding->as<NameNode>().atom() == outerBinding->as<NameNode>().atom());
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_CLASSNAMES);
        MOZ_ASSERT_IF(match, node.is<BinaryNode>());
        return match;
    }

    /*
     * Classes require two definitions: The first "outer" binding binds the
     * class into the scope in which it was declared. the outer binding is a
     * mutable lexial binding. The second "inner" binding binds the class by
     * name inside a block in which the methods are evaulated. It is immutable,
     * giving the methods access to the static members of the class even if
     * the outer binding has been overwritten.
     */
    NameNode* outerBinding() const {
        if (ParseNode* binding = left()) {
            return &binding->as<NameNode>();
        }
        return nullptr;
    }
    NameNode* innerBinding() const {
        return &right()->as<NameNode>();
    }
};

class ClassNode : public TernaryNode
{
  public:
    ClassNode(ParseNode* names, ParseNode* heritage, LexicalScopeNode* memberBlock,
              const TokenPos& pos)
      : TernaryNode(PNK_CLASS, JSOP_NOP, names, heritage, memberBlock, pos)
    {
        MOZ_ASSERT_IF(names, names->is<ClassNames>());
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_CLASS);
        MOZ_ASSERT_IF(match, node.is<TernaryNode>());
        return match;
    }

    ClassNames* names() const {
        return kid1() ? &kid1()->as<ClassNames>() : nullptr;
    }
    ParseNode* heritage() const {
        return kid2();
    }
    ListNode* memberList() const {
        ListNode* list = &kid3()->as<LexicalScopeNode>().scopeBody()->as<ListNode>();
        MOZ_ASSERT(list->isKind(PNK_CLASSMEMBERLIST));
        return list;
    }
    LexicalScopeNode* scopeBindings() const {
        LexicalScopeNode* scope = &kid3()->as<LexicalScopeNode>();
        return scope->isEmptyScope() ? nullptr : scope;
    }
};

#ifdef DEBUG
void DumpParseTree(ParseNode* pn, int indent = 0);
#endif

class ParseNodeAllocator
{
  public:
    explicit ParseNodeAllocator(ExclusiveContext* cx, LifoAlloc& alloc)
      : cx(cx), alloc(alloc), freelist(nullptr)
    {}

    void* allocNode();
    void freeNode(ParseNode* pn);
    ParseNode* freeTree(ParseNode* pn);
    void prepareNodeForMutation(ParseNode* pn);

  private:
    ExclusiveContext* cx;
    LifoAlloc& alloc;
    ParseNode* freelist;
};

inline bool
ParseNode::isConstant()
{
    switch (pn_type) {
      case PNK_NUMBER:
      case PNK_STRING:
      case PNK_TEMPLATE_STRING:
      case PNK_NULL:
      case PNK_RAW_UNDEFINED:
      case PNK_FALSE:
      case PNK_TRUE:
        return true;
      case PNK_ARRAY:
      case PNK_OBJECT:
        MOZ_ASSERT(isOp(JSOP_NEWINIT));
        return !as<ListNode>().hasNonConstInitializer();
      default:
        return false;
    }
}

class TraceListNode
{
  protected:
    js::gc::Cell* gcThing;
    TraceListNode* traceLink;

    TraceListNode(js::gc::Cell* gcThing, TraceListNode* traceLink);

    bool isBigIntBox() const { return gcThing->is<BigInt>(); }
    bool isObjectBox() const { return gcThing->is<JSObject>(); }

    BigIntBox* asBigIntBox();
    ObjectBox* asObjectBox();

    virtual void trace(JSTracer* trc);

  public:
    static void TraceList(JSTracer* trc, TraceListNode* listHead);
};

class BigIntBox : public TraceListNode
{
  public:
    BigIntBox(BigInt* bi, TraceListNode* link);
    BigInt* value() const { return gcThing->as<BigInt>(); }
};

class ObjectBox : public TraceListNode
{
  protected:
    friend struct CGObjectList;
    ObjectBox* emitLink;
 
    ObjectBox(JSFunction* function, TraceListNode* link);

  public:
    ObjectBox(JSObject* obj, TraceListNode* link);

    JSObject* object() const { return gcThing->as<JSObject>(); }

    bool isFunctionBox() const { return object()->is<JSFunction>(); }
    FunctionBox* asFunctionBox();
};

enum ParseReportKind
{
    ParseError,
    ParseWarning,
    ParseExtraWarning,
    ParseStrictError
};

static inline ParseNode*
FunctionFormalParametersList(ParseNode* fn, unsigned* numFormals)
{
    MOZ_ASSERT(fn->isKind(PNK_FUNCTION));
    ListNode* argsBody = fn->as<FunctionNode>().body();
    MOZ_ASSERT(argsBody->isKind(PNK_PARAMSBODY));
    *numFormals = argsBody->count();
    if (*numFormals > 0 &&
        argsBody->last()->is<LexicalScopeNode>() &&
        argsBody->last()->as<LexicalScopeNode>().scopeBody()->isKind(PNK_STATEMENTLIST))
    {
        (*numFormals)--;
    }
    return argsBody->head();
}

bool
IsAnonymousFunctionDefinition(ParseNode* pn);

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_ParseNode_h */
