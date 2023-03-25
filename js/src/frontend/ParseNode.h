/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_ParseNode_h
#define frontend_ParseNode_h

#include "mozilla/Attributes.h"

#include "builtin/ModuleObject.h"
#include "frontend/TokenStream.h"

namespace js {
namespace frontend {

class ParseContext;
class FullParseHandler;
class FunctionBox;
class ObjectBox;

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
    F(OBJECT_PROPERTY_NAME) \
    F(COMPUTED_NAME) \
    F(NUMBER) \
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
    F(CLASSMETHODLIST) \
    F(CLASSNAMES) \
    F(NEWTARGET) \
    F(POSHOLDER) \
    F(SUPERBASE) \
    F(SUPERCALL) \
    F(SETTHIS) \
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
    /* ParseNode::isAssignment assumes all these are consecutive. */ \
    F(ASSIGN) \
    F(ADDASSIGN) \
    F(SUBASSIGN) \
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
 * PNK_FUNCTION name        pn_funbox: ptr to js::FunctionBox holding function
 *                            object containing arg and var properties.  We
 *                            create the function object at parse (not emit)
 *                            time to specialize arg and var bytecodes early.
 *                          pn_body: PNK_PARAMSBODY, ordinarily;
 *                            PNK_LEXICALSCOPE for implicit function in genexpr
 * PNK_PARAMSBODY (ListNode)
 *   head: list of formal parameters with
 *           * Name node with non-empty name for SingleNameBinding without
 *             Initializer
 *           * Assign node for SingleNameBinding with Initializer
 *           * Name node with empty name for destructuring
 *               pn_expr: Array or Object for BindingPattern without
 *                        Initializer, Assign for BindingPattern with
 *                        Initializer
 *         followed by either:
 *           * StatementList node for function body statements
 *           * Return for expression closure
 *   count: number of formal parameters + 1
 * PNK_SPREAD   unary       pn_kid: expression being spread
 *
 * <Statements>
 * PNK_STATEMENTLIST (ListNode)
 *   head: list of N statements
 *   count: N >= 0
 * PNK_IF       ternary     pn_kid1: cond, pn_kid2: then, pn_kid3: else or null.
 *                            In body of a comprehension or desugared generator
 *                            expression, pn_kid2 is PNK_YIELD, PNK_ARRAYPUSH,
 *                            or (if the push was optimized away) empty
 *                            PNK_STATEMENTLIST.
 * PNK_SWITCH   binary      pn_left: discriminant
 *                          pn_right: PNK_LEXICALSCOPE node that contains the list
 *                            of PNK_CASE nodes, with at most one default node.
 *                          hasDefault: true if there's a default case
 * PNK_CASE     binary      pn_left: case-expression if CaseClause, or
 *                            null if DefaultClause
 *                          pn_right: PNK_STATEMENTLIST node for this case's
 *                            statements
 * PNK_WHILE    binary      pn_left: cond, pn_right: body
 * PNK_DOWHILE  binary      pn_left: body, pn_right: cond
 * PNK_FOR      binary      pn_left: either PNK_FORIN (for-in statement),
 *                            PNK_FOROF (for-of) or PNK_FORHEAD (for(;;))
 *                          pn_right: body
 * PNK_COMPREHENSIONFOR     pn_left: either PNK_FORIN or PNK_FOROF
 *              binary      pn_right: body
 * PNK_FORIN    ternary     pn_kid1: declaration or expression to left of 'in'
 *                          pn_kid2: null
 *                          pn_kid3: object expr to right of 'in'
 * PNK_FOROF    ternary     pn_kid1: declaration or expression to left of 'of'
 *                          pn_kid2: null
 *                          pn_kid3: expr to right of 'of'
 * PNK_FORHEAD  ternary     pn_kid1:  init expr before first ';' or nullptr
 *                          pn_kid2:  cond expr before second ';' or nullptr
 *                          pn_kid3:  update expr after second ';' or nullptr
 * PNK_THROW    unary       pn_op: JSOP_THROW, pn_kid: exception
 * PNK_TRY      ternary     pn_kid1: try block
 *                          pn_kid2: null or PNK_CATCHLIST list
 *                          pn_kid3: null or finally block
 * PNK_CATCHLIST list       pn_head: list of PNK_LEXICALSCOPE nodes, one per
 *                                   catch-block, each with pn_expr pointing
 *                                   to a PNK_CATCH node
 * PNK_CATCH    ternary     pn_kid1: PNK_NAME, PNK_ARRAY, or PNK_OBJECT catch var node
 *                                   (PNK_ARRAY or PNK_OBJECT if destructuring)
 *                          pn_kid2: null or the catch guard expression
 *                          pn_kid3: catch block statements
 * PNK_BREAK    name        pn_atom: label or null
 * PNK_CONTINUE name        pn_atom: label or null
 * PNK_WITH     binary      pn_left: head expr; pn_right: body;
 * PNK_VAR, PNK_LET, PNK_CONST (ListNode)
 *   head: list of N Name or Assign nodes
 *         each name node has either
 *           pn_used: false
 *           pn_atom: variable name
 *           pn_expr: initializer or null
 *         or
 *           pn_used: true
 *           pn_atom: variable name
 *           pn_lexdef: def node
 *         each assignment node has
 *           pn_left: Name with pn_used true and
 *                    pn_lexdef (NOT pn_expr) set
 *           pn_right: initializer
 *   count: N > 0
 * PNK_RETURN   unary       pn_kid: return expr or null
 * PNK_SEMI     unary       pn_kid: expr or null statement
 *                          pn_prologue: true if Directive Prologue member
 *                              in original source, not introduced via
 *                              constant folding or other tree rewriting
 * PNK_LABEL    name        pn_atom: label, pn_expr: labeled statement
 * PNK_IMPORT   binary      pn_left: PNK_IMPORT_SPEC_LIST import specifiers
 *                          pn_right: PNK_STRING module specifier
 * PNK_IMPORT_SPEC_LIST (ListNode)
 *   head: list of N ImportSpec nodes
 *   count: N >= 0 (N = 0 for `import {} from ...`)
 * PNK_EXPORT   unary       pn_kid: declaration expression
 * PNK_EXPORT_FROM binary   pn_left: PNK_EXPORT_SPEC_LIST export specifiers
 *                          pn_right: PNK_STRING module specifier
 * PNK_EXPORT_SPEC_LIST (ListNode)
 *   head: list of N ExportSpec nodes
 *   count: N >= 0 (N = 0 for `export {}`)
 * PNK_EXPORT_DEFAULT unary pn_kid: export default declaration or expression
 *
 * <Expressions>
 * All left-associated binary trees of the same type are optimized into lists
 * to avoid recursion when processing expression chains.
 * PNK_COMMA (ListNode)
 *   head: list of N comma-separated exprs
 *   count: N >= 2
 * PNK_ASSIGN   binary      pn_left: lvalue, pn_right: rvalue
 * PNK_ADDASSIGN,   binary  pn_left: lvalue, pn_right: rvalue
 * PNK_SUBASSIGN,           pn_op: JSOP_ADD for +=, etc.
 * PNK_BITORASSIGN,
 * PNK_BITXORASSIGN,
 * PNK_BITANDASSIGN,
 * PNK_LSHASSIGN,
 * PNK_RSHASSIGN,
 * PNK_URSHASSIGN,
 * PNK_MULASSIGN,
 * PNK_DIVASSIGN,
 * PNK_MODASSIGN,
 * PNK_POWASSIGN
 * PNK_CONDITIONAL ternary  (cond ? trueExpr : falseExpr)
 *                          pn_kid1: cond, pn_kid2: then, pn_kid3: else
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
 * PNK_POS,     unary       pn_kid: UNARY expr
 * PNK_NEG
 * PNK_VOID,    unary       pn_kid: UNARY expr
 * PNK_NOT,
 * PNK_BITNOT,
 * PNK_AWAIT
 * PNK_TYPEOFNAME, unary    pn_kid: UNARY expr
 * PNK_TYPEOFEXPR
 * PNK_PREINCREMENT, unary  pn_kid: MEMBER expr
 * PNK_POSTINCREMENT,
 * PNK_PREDECREMENT,
 * PNK_POSTDECREMENT
 * PNK_NEW      binary      pn_left: ctor expression on the left of the (
 *                          pn_right: Arguments
 * PNK_DELETENAME unary     pn_kid: PNK_NAME expr
 * PNK_DELETEPROP unary     pn_kid: PNK_DOT expr
 * PNK_DELETEELEM unary     pn_kid: PNK_ELEM expr
 * PNK_DELETEEXPR unary     pn_kid: MEMBER expr that's evaluated, then the
 *                          overall delete evaluates to true; can't be a kind
 *                          for a more-specific PNK_DELETE* unless constant
 *                          folding (or a similar parse tree manipulation) has
 *                          occurred
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
 * PNK_OPTDOT   binary      pn_left: MEMBER expr to left of .
 *                          short circuits back to PNK_OPTCHAIN if nullish.
 *                          pn_right: PropertyName to right of .
 * PNK_OPTELEM  binary      pn_left: MEMBER expr to left of [
 *                          short circuits back to PNK_OPTCHAIN if nullish.
 *                          pn_right: expr between [ and ]
 * PNK_OPTCALL  list        pn_head: list of call, arg1, arg2, ... argN
 *                          pn_count: 1 + N (where N is number of args)
 *                          call is a MEMBER expr naming a callable object,
 *                          short circuits back to PNK_OPTCHAIN if nullish.
 * PNK_PROPERTYNAME         pn_atom: property being accessed
 *              name        
 * PNK_DOT      binary      pn_left: MEMBER expr to left of .
 *                          pn_right: PropertyName to right of .
 * PNK_ELEM     binary      pn_left: MEMBER expr to left of [
 *                          pn_right: expr between [ and ]
 * PNK_CALL     binary      pn_left: callee expression on the left of the (
 *                          pn_right: Arguments
 * PNK_ARGUMENTS list       pn_head: list of arg1, arg2, ... argN
 *                          pn_count: N (where N is number of args)
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
 * PNK_COLON    binary      key-value pair in object initializer or
 *                          destructuring lhs
 *                          pn_left: property id, pn_right: value
 * PNK_SHORTHAND binary     Same fields as PNK_COLON. This is used for object
 *                          literal properties using shorthand ({x}).
 * PNK_COMPUTED_NAME unary  ES6 ComputedPropertyName.
 *                          pn_kid: the AssignmentExpression inside the square brackets
 * PNK_NAME,    name        pn_atom: name, string, or object atom
 * PNK_STRING               pn_op: JSOP_GETNAME, JSOP_STRING, or JSOP_OBJECT
 *                          If JSOP_GETNAME, pn_op may be JSOP_*ARG or JSOP_*VAR
 *                          telling const-ness and static analysis results
 * PNK_TEMPLATE_STRING_LIST (ListNode)
 *   head: list of alternating expr and template strings
 *           TemplateString [, expression, TemplateString]+
 *         there's at least one expression.  If the template literal contains
 *         no ${}-delimited expression, it's parsed as a single TemplateString
 * PNK_TEMPLATE_STRING      pn_atom: template string atom
                nullary     pn_op: JSOP_NOP
 * PNK_TAGGED_TEMPLATE      pn_left: tag expression
 *              binary      pn_right: Arguments, with the first being the call site object, then arg1, arg2, ... argN
 * PNK_CALLSITEOBJ (CallSiteNode)
 *   head:  an Array of raw TemplateString, then corresponding cooked
 *          TemplateString nodes
 *            Array [, cooked TemplateString]+
 *          where the Array is
 *            [raw TemplateString]+
 * PNK_REGEXP   nullary     pn_objbox: RegExp model object
 * PNK_NUMBER   dval        pn_dval: double value of numeric literal
 * PNK_TRUE,    nullary     pn_op: JSOp bytecode
 * PNK_FALSE,
 * PNK_NULL,
 * PNK_RAW_UNDEFINED
 *
 * PNK_THIS,        unary   pn_kid: '.this' Name if function `this`, else nullptr
 * PNK_SUPERBASE    unary   pn_kid: '.this' Name
 *
 * PNK_SUPERCALL    binary  pn_left: SuperBase pn_right: Arguments
 *
 * PNK_SETTHIS      binary  pn_left: '.this' Name, pn_right: SuperCall
 *
 * PNK_LEXICALSCOPE scope   pn_u.scope.bindings: scope bindings
 *                          pn_u.scope.body: scope body
 * PNK_GENERATOR    nullary
 * PNK_INITIALYIELD unary   pn_kid: generator object
 * PNK_YIELD,       unary   pn_kid: expr or null
 * PNK_YIELD_STAR,
 * PNK_ARRAYCOMP    list    pn_count: 1
 *                          pn_head: list of 1 element, which is block
 *                          enclosing for loop(s) and optionally
 *                          if-guarded PNK_ARRAYPUSH
 * PNK_ARRAYPUSH    unary   pn_op: JSOP_ARRAYCOMP
 *                          pn_kid: array comprehension expression
 * PNK_NOP          nullary
 */
enum ParseNodeArity
{
    PN_NULLARY,                         /* 0 kids, only pn_atom/pn_dval/etc. */
    PN_UNARY,                           /* one kid, plus a couple of scalars */
    PN_BINARY,                          /* two kids, plus a couple of scalars */
    PN_TERNARY,                         /* three kids */
    PN_CODE,                            /* module or function definition node */
    PN_LIST,                            /* generic singly linked list */
    PN_NAME,                            /* name, label, or regexp */
    PN_SCOPE                            /* lexical scope */
};

#define FOR_EACH_PARSENODE_SUBCLASS(macro) \
    macro(ListNode, ListNodeType, asList) \
    macro(CallSiteNode, CallSiteNodeType, asCallSite)

class LoopControlStatement;
class BreakStatement;
class ContinueStatement;
class ConditionalExpression;
class PropertyAccess;

#define DECLARE_CLASS(typeName, longTypeName, asMethodName) \
class typeName;
FOR_EACH_PARSENODE_SUBCLASS(DECLARE_CLASS)
#undef DECLARE_CLASS

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

    bool isAssignment() const {
        ParseNodeKind kind = getKind();
        return PNK_ASSIGNMENT_START <= kind && kind <= PNK_ASSIGNMENT_LAST;
    }

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
            ParseNode*  kid1;           /* condition, discriminant, etc. */
            ParseNode*  kid2;           /* then-part, case list, etc. */
            ParseNode*  kid3;           /* else-part, default case, etc. */
        } ternary;
        struct {                        /* two kids if binary */
            ParseNode*  left;
            ParseNode*  right;
            union {
                unsigned iflags;        /* JSITER_* flags for PNK_{COMPREHENSION,}FOR node */
                bool isStatic;          /* only for PNK_CLASSMETHOD */
                bool hasDefault;        /* only for PNK_SWITCH */
            };
        } binary;
        struct {                        /* one kid if unary */
            ParseNode*  kid;
            bool        prologue;       /* directive prologue member (as
                                           pn_prologue) */
        } unary;
        struct {                        /* name, labeled statement, etc. */
            union {
                JSAtom*      atom;      /* lexical name or label atom */
                ObjectBox*   objbox;    /* regexp object */
                FunctionBox* funbox;    /* function object */
            };
            ParseNode*  expr;           /* module or function body, var
                                           initializer, or argument default */
        } name;
        struct {
            LexicalScope::Data* bindings;
            ParseNode*          body;
        } scope;
        struct {
            double       value;         /* aligned numeric literal value */
            DecimalPoint decimalPoint;  /* Whether the number has a decimal point */
        } number;
        class {
            friend class LoopControlStatement;
            PropertyName*    label;    /* target of break/continue statement */
        } loopControl;
    } pn_u;

#define pn_objbox       pn_u.name.objbox
#define pn_funbox       pn_u.name.funbox
#define pn_body         pn_u.name.expr
#define pn_kid1         pn_u.ternary.kid1
#define pn_kid2         pn_u.ternary.kid2
#define pn_kid3         pn_u.ternary.kid3
#define pn_left         pn_u.binary.left
#define pn_right        pn_u.binary.right
#define pn_pval         pn_u.binary.pval
#define pn_iflags       pn_u.binary.iflags
#define pn_kid          pn_u.unary.kid
#define pn_prologue     pn_u.unary.prologue
#define pn_atom         pn_u.name.atom
#define pn_objbox       pn_u.name.objbox
#define pn_expr         pn_u.name.expr
#define pn_dval         pn_u.number.value


  public:
    /*
     * If |left| is a list of the given kind/left-associative op, append
     * |right| to it and return |left|.  Otherwise return a [left, right] list.
     */
    static ParseNode*
    appendOrCreateList(ParseNodeKind kind, JSOp op, ParseNode* left, ParseNode* right,
                       FullParseHandler* handler, ParseContext* pc);

    inline PropertyName* name() const;
    inline JSAtom* atom() const;

    ParseNode* expr() const {
        MOZ_ASSERT(pn_arity == PN_NAME || pn_arity == PN_CODE);
        return pn_expr;
    }

    bool isEmptyScope() const {
        MOZ_ASSERT(pn_arity == PN_SCOPE);
        return !pn_u.scope.bindings;
    }

    Handle<LexicalScope::Data*> scopeBindings() const {
        MOZ_ASSERT(!isEmptyScope());
        // Bindings' GC safety depend on the presence of an AutoKeepAtoms that
        // the rest of the frontend also depends on.
        return Handle<LexicalScope::Data*>::fromMarkedLocation(&pn_u.scope.bindings);
    }

    ParseNode* scopeBody() const {
        MOZ_ASSERT(pn_arity == PN_SCOPE);
        return pn_u.scope.body;
    }

    void setScopeBody(ParseNode* body) {
        MOZ_ASSERT(pn_arity == PN_SCOPE);
        pn_u.scope.body = body;
    }

    bool functionIsHoisted() const {
        MOZ_ASSERT(pn_arity == PN_CODE && getKind() == PNK_FUNCTION);
        MOZ_ASSERT(isOp(JSOP_LAMBDA) ||        // lambda, genexpr
                   isOp(JSOP_LAMBDA_ARROW) ||  // arrow function
                   isOp(JSOP_DEFFUN) ||        // non-body-level function statement
                   isOp(JSOP_NOP) ||           // body-level function stmt in global code
                   isOp(JSOP_GETLOCAL) ||      // body-level function stmt in function code
                   isOp(JSOP_GETARG) ||        // body-level function redeclaring formal
                   isOp(JSOP_INITLEXICAL));    // block-level function stmt
        return !isOp(JSOP_LAMBDA) && !isOp(JSOP_LAMBDA_ARROW) && !isOp(JSOP_DEFFUN);
    }

    /*
     * True if this statement node could be a member of a Directive Prologue: an
     * expression statement consisting of a single string literal.
     *
     * This considers only the node and its children, not its context. After
     * parsing, check the node's pn_prologue flag to see if it is indeed part of
     * a directive prologue.
     *
     * Note that a Directive Prologue can contain statements that cannot
     * themselves be directives (string literals that include escape sequences
     * or escaped newlines, say). This member function returns true for such
     * nodes; we use it to determine the extent of the prologue.
     */
    JSAtom* isStringExprStatement() const {
        if (getKind() == PNK_SEMI) {
            MOZ_ASSERT(pn_arity == PN_UNARY);
            ParseNode* kid = pn_kid;
            if (kid && kid->getKind() == PNK_STRING && !kid->pn_parens)
                return kid->pn_atom;
        }
        return nullptr;
    }

    /* True if pn is a parsenode representing a literal constant. */
    bool isLiteral() const {
        return isKind(PNK_NUMBER) ||
               isKind(PNK_STRING) ||
               isKind(PNK_TRUE) ||
               isKind(PNK_FALSE) ||
               isKind(PNK_NULL) ||
               isKind(PNK_RAW_UNDEFINED);
    }

    /* Return true if this node appears in a Directive Prologue. */
    bool isDirectivePrologueMember() const { return pn_prologue; }

    // True iff this is a for-in/of loop variable declaration (var/let/const).
    bool isForLoopDeclaration() const;

    void initNumber(double value, DecimalPoint decimalPoint) {
        MOZ_ASSERT(pn_arity == PN_NULLARY);
        MOZ_ASSERT(getKind() == PNK_NUMBER);
        pn_u.number.value = value;
        pn_u.number.decimalPoint = decimalPoint;
    }

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

struct NullaryNode : public ParseNode
{
    NullaryNode(ParseNodeKind kind, const TokenPos& pos)
      : ParseNode(kind, JSOP_NOP, PN_NULLARY, pos) {}
    NullaryNode(ParseNodeKind kind, JSOp op, const TokenPos& pos)
      : ParseNode(kind, op, PN_NULLARY, pos) {}

    // This constructor is for a few mad uses in the emitter. It populates
    // the pn_atom field even though that field belongs to a branch in pn_u
    // that nullary nodes shouldn't use -- bogus.
    NullaryNode(ParseNodeKind kind, JSOp op, const TokenPos& pos, JSAtom* atom)
      : ParseNode(kind, op, PN_NULLARY, pos)
    {
        pn_atom = atom;
    }

#ifdef DEBUG
    void dump();
#endif
};

struct UnaryNode : public ParseNode
{
    UnaryNode(ParseNodeKind kind, JSOp op, const TokenPos& pos, ParseNode* kid)
      : ParseNode(kind, op, PN_UNARY, pos)
    {
        pn_kid = kid;
    }

    static bool test(const ParseNode& node) {
        return node.isArity(PN_UNARY);
    }

#ifdef DEBUG
    void dump(int indent);
#endif
};

struct BinaryNode : public ParseNode
{
    BinaryNode(ParseNodeKind kind, JSOp op, const TokenPos& pos, ParseNode* left, ParseNode* right)
      : ParseNode(kind, op, PN_BINARY, pos)
    {
        pn_left = left;
        pn_right = right;
    }

    BinaryNode(ParseNodeKind kind, JSOp op, ParseNode* left, ParseNode* right)
      : ParseNode(kind, op, PN_BINARY, TokenPos::box(left->pn_pos, right->pn_pos))
    {
        pn_left = left;
        pn_right = right;
    }

#ifdef DEBUG
    void dump(int indent);
#endif
};

struct TernaryNode : public ParseNode
{
    TernaryNode(ParseNodeKind kind, JSOp op, ParseNode* kid1, ParseNode* kid2, ParseNode* kid3)
      : ParseNode(kind, op, PN_TERNARY,
                  TokenPos((kid1 ? kid1 : kid2 ? kid2 : kid3)->pn_pos.begin,
                           (kid3 ? kid3 : kid2 ? kid2 : kid1)->pn_pos.end))
    {
        pn_kid1 = kid1;
        pn_kid2 = kid2;
        pn_kid3 = kid3;
    }

    TernaryNode(ParseNodeKind kind, JSOp op, ParseNode* kid1, ParseNode* kid2, ParseNode* kid3,
                const TokenPos& pos)
      : ParseNode(kind, op, PN_TERNARY, pos)
    {
        pn_kid1 = kid1;
        pn_kid2 = kid2;
        pn_kid3 = kid3;
    }

#ifdef DEBUG
    void dump(int indent);
#endif
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
                   isKind(PNK_CLASSMETHODLIST));
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
                   isKind(PNK_CLASSMETHODLIST));
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

struct CodeNode : public ParseNode
{
    CodeNode(ParseNodeKind kind, JSOp op, const TokenPos& pos)
      : ParseNode(kind, op, PN_CODE, pos)
    {
        MOZ_ASSERT(kind == PNK_FUNCTION || kind == PNK_MODULE);
        MOZ_ASSERT_IF(kind == PNK_MODULE, op == JSOP_NOP);
        MOZ_ASSERT(op == JSOP_NOP || // statement, module
                   op == JSOP_LAMBDA_ARROW || // arrow function
                   op == JSOP_LAMBDA); // expression, method, comprehension, accessor, &c.
        MOZ_ASSERT(!pn_body);
        MOZ_ASSERT(!pn_objbox);
    }

  public:
#ifdef DEBUG
    void dump(int indent);
#endif
};

struct NameNode : public ParseNode
{
    NameNode(ParseNodeKind kind, JSOp op, JSAtom* atom, const TokenPos& pos)
      : ParseNode(kind, op, PN_NAME, pos)
    {
        pn_atom = atom;
        pn_expr = nullptr;
    }

    static bool test(const ParseNode& node) {
        return node.isArity(PN_NAME);
    }

#ifdef DEBUG
    void dump(int indent);
#endif
};

struct LexicalScopeNode : public ParseNode
{
    LexicalScopeNode(LexicalScope::Data* bindings, ParseNode* body)
      : ParseNode(PNK_LEXICALSCOPE, JSOP_NOP, PN_SCOPE, body->pn_pos)
    {
        pn_u.scope.bindings = bindings;
        pn_u.scope.body = body;
    }

    static bool test(const ParseNode& node) {
        return node.isKind(PNK_LEXICALSCOPE);
    }

#ifdef DEBUG
    void dump(int indent);
#endif
};

class LabeledStatement : public ParseNode
{
  public:
    LabeledStatement(PropertyName* label, ParseNode* stmt, uint32_t begin)
      : ParseNode(PNK_LABEL, JSOP_NOP, PN_NAME, TokenPos(begin, stmt->pn_pos.end))
    {
        pn_atom = label;
        pn_expr = stmt;
    }

    PropertyName* label() const {
        return pn_atom->asPropertyName();
    }

    ParseNode* statement() const {
        return pn_expr;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_LABEL);
        MOZ_ASSERT_IF(match, node.isArity(PN_NAME));
        MOZ_ASSERT_IF(match, node.isOp(JSOP_NOP));
        return match;
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

    ParseNode* caseExpression() const { return pn_left; }
    bool isDefault() const { return !caseExpression(); }
    ListNode* statementList() const { return &pn_right->as<ListNode>(); }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_CASE);
        MOZ_ASSERT_IF(match, node.isArity(PN_BINARY));
        MOZ_ASSERT_IF(match, node.isOp(JSOP_NOP));
        return match;
    }
};

class LoopControlStatement : public ParseNode
{
  protected:
    LoopControlStatement(ParseNodeKind kind, PropertyName* label, const TokenPos& pos)
      : ParseNode(kind, JSOP_NOP, PN_NULLARY, pos)
    {
        MOZ_ASSERT(kind == PNK_BREAK || kind == PNK_CONTINUE);
        pn_u.loopControl.label = label;
    }

  public:
    /* Label associated with this break/continue statement, if any. */
    PropertyName* label() const {
        return pn_u.loopControl.label;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_BREAK) || node.isKind(PNK_CONTINUE);
        MOZ_ASSERT_IF(match, node.isArity(PN_NULLARY));
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
        MOZ_ASSERT_IF(match, node.isArity(PN_NULLARY));
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
        MOZ_ASSERT_IF(match, node.isArity(PN_NULLARY));
        MOZ_ASSERT_IF(match, node.isOp(JSOP_NOP));
        return match;
    }
};

class DebuggerStatement : public ParseNode
{
  public:
    explicit DebuggerStatement(const TokenPos& pos)
      : ParseNode(PNK_DEBUGGER, JSOP_NOP, PN_NULLARY, pos)
    { }
};

class ConditionalExpression : public ParseNode
{
  public:
    ConditionalExpression(ParseNode* condition, ParseNode* thenExpr, ParseNode* elseExpr)
      : ParseNode(PNK_CONDITIONAL, JSOP_NOP, PN_TERNARY,
                  TokenPos(condition->pn_pos.begin, elseExpr->pn_pos.end))
    {
        MOZ_ASSERT(condition);
        MOZ_ASSERT(thenExpr);
        MOZ_ASSERT(elseExpr);
        pn_u.ternary.kid1 = condition;
        pn_u.ternary.kid2 = thenExpr;
        pn_u.ternary.kid3 = elseExpr;
    }

    ParseNode& condition() const {
        return *pn_u.ternary.kid1;
    }

    ParseNode& thenExpression() const {
        return *pn_u.ternary.kid2;
    }

    ParseNode& elseExpression() const {
        return *pn_u.ternary.kid3;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_CONDITIONAL);
        MOZ_ASSERT_IF(match, node.isArity(PN_TERNARY));
        MOZ_ASSERT_IF(match, node.isOp(JSOP_NOP));
        return match;
    }
};

class ThisLiteral : public UnaryNode
{
  public:
    ThisLiteral(const TokenPos& pos, ParseNode* thisName)
      : UnaryNode(PNK_THIS, JSOP_NOP, pos, thisName)
    { }
};

class NullLiteral : public ParseNode
{
  public:
    explicit NullLiteral(const TokenPos& pos) : ParseNode(PNK_NULL, JSOP_NULL, PN_NULLARY, pos) { }
};

// This is only used internally, currently just for tagged templates.
// It represents the value 'undefined' (aka `void 0`), like NullLiteral
// represents the value 'null'.
class RawUndefinedLiteral : public ParseNode
{
  public:
    explicit RawUndefinedLiteral(const TokenPos& pos)
      : ParseNode(PNK_RAW_UNDEFINED, JSOP_UNDEFINED, PN_NULLARY, pos) { }
};

class BooleanLiteral : public ParseNode
{
  public:
    BooleanLiteral(bool b, const TokenPos& pos)
      : ParseNode(b ? PNK_TRUE : PNK_FALSE, b ? JSOP_TRUE : JSOP_FALSE, PN_NULLARY, pos)
    { }
};

class RegExpLiteral : public NullaryNode
{
  public:
    RegExpLiteral(ObjectBox* reobj, const TokenPos& pos)
      : NullaryNode(PNK_REGEXP, JSOP_REGEXP, pos)
    {
        pn_objbox = reobj;
    }

    ObjectBox* objbox() const { return pn_objbox; }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_REGEXP);
        MOZ_ASSERT_IF(match, node.isArity(PN_NULLARY));
        MOZ_ASSERT_IF(match, node.isOp(JSOP_REGEXP));
        return match;
    }
};

class PropertyAccessBase : public BinaryNode
{
  public:
    /*
     * PropertyAccess nodes can have any expression/'super' as left-hand
     * side, but the name must be a ParseNodeKind::PropertyName node.
     */
    PropertyAccessBase(ParseNodeKind kind, ParseNode* lhs, ParseNode* name, uint32_t begin, uint32_t end)
      : BinaryNode(kind, JSOP_NOP, TokenPos(begin, end), lhs, name)
    {
        MOZ_ASSERT(lhs != nullptr);
        MOZ_ASSERT(name != nullptr);
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_DOT) ||
                     node.isKind(PNK_OPTDOT);
        MOZ_ASSERT_IF(match, node.isArity(PN_BINARY));
        MOZ_ASSERT_IF(match, node.pn_right->isKind(PNK_PROPERTYNAME));
        return match;
    }

    ParseNode& expression() const {
        return *pn_u.binary.left;
    }

    PropertyName& name() const {
        return *pn_u.binary.right->pn_atom->asPropertyName();
    }

    JSAtom* nameAtom() const {
        return pn_u.binary.right->pn_atom;
    }
};

class PropertyAccess : public PropertyAccessBase
{
  public:
    PropertyAccess(ParseNode* lhs, ParseNode* name, uint32_t begin, uint32_t end)
      : PropertyAccessBase(PNK_DOT, lhs, name, begin, end)
    {
        MOZ_ASSERT(lhs != nullptr);
        MOZ_ASSERT(name != nullptr);
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_DOT);
        MOZ_ASSERT_IF(match, node.isArity(PN_BINARY));
        MOZ_ASSERT_IF(match, node.pn_right->isKind(PNK_PROPERTYNAME));
        return match;
    }

    bool isSuper() const {
        // PNK_SUPERBASE cannot result from any expression syntax.
        return expression().isKind(PNK_SUPERBASE);
    }
};

class OptionalPropertyAccess : public PropertyAccessBase
{
  public:
    OptionalPropertyAccess(ParseNode* lhs, ParseNode* name, uint32_t begin, uint32_t end)
      : PropertyAccessBase(PNK_OPTDOT, lhs, name, begin, end)
    {
        MOZ_ASSERT(lhs != nullptr);
        MOZ_ASSERT(name != nullptr);
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_OPTDOT);
        MOZ_ASSERT_IF(match, node.isArity(PN_BINARY));
        MOZ_ASSERT_IF(match, node.pn_right->isKind(PNK_PROPERTYNAME));
        return match;
    }
};

class PropertyByValueBase : public ParseNode
{
  public:
    PropertyByValueBase(ParseNodeKind kind, ParseNode* lhs, ParseNode* propExpr,
                    uint32_t begin, uint32_t end)
      : ParseNode(kind, JSOP_NOP, PN_BINARY, TokenPos(begin, end))
    {
        pn_u.binary.left = lhs;
        pn_u.binary.right = propExpr;
    }

    ParseNode& expression() const {
        return *pn_u.binary.left;
    }

    ParseNode& key() const {
        return *pn_u.binary.right;
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_ELEM) ||
                     node.isKind(PNK_OPTELEM);
        MOZ_ASSERT_IF(match, node.isArity(PN_BINARY));
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

    bool isSuper() const {
        return pn_left->isKind(PNK_SUPERBASE);
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

struct ClassMethod : public BinaryNode {
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
        MOZ_ASSERT_IF(match, node.isArity(PN_BINARY));
        return match;
    }

    ParseNode& name() const {
        return *pn_u.binary.left;
    }
    ParseNode& method() const {
        return *pn_u.binary.right;
    }
    bool isStatic() const {
        return pn_u.binary.isStatic;
    }
};

struct SwitchStatement : public BinaryNode {
    SwitchStatement(uint32_t begin, ParseNode* discriminant, ParseNode* lexicalForCaseList,
                    bool hasDefault)
      : BinaryNode(PNK_SWITCH, JSOP_NOP,
                   TokenPos(begin, lexicalForCaseList->pn_pos.end),
                   discriminant, lexicalForCaseList)
    {
#ifdef DEBUG
        MOZ_ASSERT(lexicalForCaseList->isKind(PNK_LEXICALSCOPE));
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
        MOZ_ASSERT_IF(match, node.isArity(PN_BINARY));
        return match;
    }

    ParseNode& discriminant() const {
        return *pn_u.binary.left;
    }
    ParseNode& lexicalForCaseList() const {
        return *pn_u.binary.right;
    }
    bool hasDefault() const {
        return pn_u.binary.hasDefault;
    }
};

struct ClassNames : public BinaryNode {
    ClassNames(ParseNode* outerBinding, ParseNode* innerBinding, const TokenPos& pos)
      : BinaryNode(PNK_CLASSNAMES, JSOP_NOP, pos, outerBinding, innerBinding)
    {
        MOZ_ASSERT_IF(outerBinding, outerBinding->isKind(PNK_NAME));
        MOZ_ASSERT(innerBinding->isKind(PNK_NAME));
        MOZ_ASSERT_IF(outerBinding, innerBinding->pn_atom == outerBinding->pn_atom);
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_CLASSNAMES);
        MOZ_ASSERT_IF(match, node.isArity(PN_BINARY));
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
    ParseNode* outerBinding() const {
        return pn_u.binary.left;
    }
    ParseNode* innerBinding() const {
        return pn_u.binary.right;
    }
};

struct ClassNode : public TernaryNode {
    ClassNode(ParseNode* names, ParseNode* heritage, ParseNode* methodsOrBlock,
              const TokenPos& pos)
      : TernaryNode(PNK_CLASS, JSOP_NOP, names, heritage, methodsOrBlock, pos)
    {
        MOZ_ASSERT_IF(names, names->is<ClassNames>());
        MOZ_ASSERT(methodsOrBlock->is<LexicalScopeNode>() ||
                   methodsOrBlock->isKind(PNK_CLASSMETHODLIST));
    }

    static bool test(const ParseNode& node) {
        bool match = node.isKind(PNK_CLASS);
        MOZ_ASSERT_IF(match, node.isArity(PN_TERNARY));
        return match;
    }

    ClassNames* names() const {
        return pn_kid1 ? &pn_kid1->as<ClassNames>() : nullptr;
    }
    ParseNode* heritage() const {
        return pn_kid2;
    }
    ListNode* methodList() const {
        if (pn_kid3->isKind(PNK_CLASSMETHODLIST))
            return &pn_kid3->as<ListNode>();

        MOZ_ASSERT(pn_kid3->is<LexicalScopeNode>());
        ListNode* list = &pn_kid3->scopeBody()->as<ListNode>();
        MOZ_ASSERT(list->isKind(PNK_CLASSMETHODLIST));
        return list;
    }
    Handle<LexicalScope::Data*> scopeBindings() const {
        MOZ_ASSERT(pn_kid3->is<LexicalScopeNode>());
        return pn_kid3->scopeBindings();
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

class ObjectBox
{
  public:
    JSObject* object;

    ObjectBox(JSObject* object, ObjectBox* traceLink);
    bool isFunctionBox() { return object->is<JSFunction>(); }
    FunctionBox* asFunctionBox();
    virtual void trace(JSTracer* trc);

    static void TraceList(JSTracer* trc, ObjectBox* listHead);

  protected:
    friend struct CGObjectList;

    ObjectBox* traceLink;
    ObjectBox* emitLink;

    ObjectBox(JSFunction* function, ObjectBox* traceLink);
};

enum ParseReportKind
{
    ParseError,
    ParseWarning,
    ParseExtraWarning,
    ParseStrictError
};

enum FunctionSyntaxKind
{
    Expression,
    Statement,
    Arrow,
    Method,
    ClassConstructor,
    DerivedClassConstructor,
    Getter,
    GetterNoExpressionClosure,
    Setter,
    SetterNoExpressionClosure
};

static inline bool
IsConstructorKind(FunctionSyntaxKind kind)
{
    return kind == ClassConstructor || kind == DerivedClassConstructor;
}

static inline bool
IsGetterKind(FunctionSyntaxKind kind)
{
    return kind == Getter || kind == GetterNoExpressionClosure;
}

static inline bool
IsSetterKind(FunctionSyntaxKind kind)
{
    return kind == Setter || kind == SetterNoExpressionClosure;
}

static inline bool
IsMethodDefinitionKind(FunctionSyntaxKind kind)
{
    return kind == Method || IsConstructorKind(kind) ||
           IsGetterKind(kind) || IsSetterKind(kind);
}

static inline ParseNode*
FunctionFormalParametersList(ParseNode* fn, unsigned* numFormals)
{
    MOZ_ASSERT(fn->isKind(PNK_FUNCTION));
    ListNode* argsBody = &fn->pn_body->as<ListNode>();
    MOZ_ASSERT(argsBody->isKind(PNK_PARAMSBODY));
    *numFormals = argsBody->count();
    if (*numFormals > 0 &&
        argsBody->last()->isKind(PNK_LEXICALSCOPE) &&
        argsBody->last()->scopeBody()->isKind(PNK_STATEMENTLIST))
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
