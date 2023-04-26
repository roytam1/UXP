/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/FoldConstants.h"

#include "mozilla/FloatingPoint.h"

#include "jslibmath.h"

#include "frontend/ParseNode.h"
#include "frontend/Parser.h"
#include "js/Conversions.h"

#include "jscntxtinlines.h"
#include "jsobjinlines.h"

using namespace js;
using namespace js::frontend;

using mozilla::IsNaN;
using mozilla::IsNegative;
using mozilla::NegativeInfinity;
using mozilla::PositiveInfinity;
using JS::GenericNaN;
using JS::ToInt32;
using JS::ToUint32;

static bool
ContainsHoistedDeclaration(ExclusiveContext* cx, ParseNode* node, bool* result);

static bool
ListContainsHoistedDeclaration(ExclusiveContext* cx, ListNode* list, bool* result)
{
    for (ParseNode* node : list->contents()) {
        if (!ContainsHoistedDeclaration(cx, node, result))
            return false;
        if (*result)
            return true;
    }

    *result = false;
    return true;
}

// Determines whether the given ParseNode contains any declarations whose
// visibility will extend outside the node itself -- that is, whether the
// ParseNode contains any var statements.
//
// THIS IS NOT A GENERAL-PURPOSE FUNCTION.  It is only written to work in the
// specific context of deciding that |node|, as one arm of a PNK_IF controlled
// by a constant condition, contains a declaration that forbids |node| being
// completely eliminated as dead.
static bool
ContainsHoistedDeclaration(ExclusiveContext* cx, ParseNode* node, bool* result)
{
    JS_CHECK_RECURSION(cx, return false);

  restart:

    // With a better-typed AST, we would have distinct parse node classes for
    // expressions and for statements and would characterize expressions with
    // ExpressionKind and statements with StatementKind.  Perhaps someday.  In
    // the meantime we must characterize every ParseNodeKind, even the
    // expression/sub-expression ones that, if we handle all statement kinds
    // correctly, we'll never see.
    switch (node->getKind()) {
      // Base case.
      case PNK_VAR:
        *result = true;
        return true;

      // Non-global lexical declarations are block-scoped (ergo not hoistable).
      case PNK_LET:
      case PNK_CONST:
        MOZ_ASSERT(node->is<ListNode>());
        *result = false;
        return true;

      // Similarly to the lexical declarations above, classes cannot add hoisted
      // declarations
      case PNK_CLASS:
        MOZ_ASSERT(node->is<ClassNode>());
        *result = false;
        return true;

      // Function declarations *can* be hoisted declarations.  But in the
      // magical world of the rewritten frontend, the declaration necessitated
      // by a nested function statement, not at body level, doesn't require
      // that we preserve an unreachable function declaration node against
      // dead-code removal.
      case PNK_FUNCTION:
        *result = false;
        return true;

      case PNK_MODULE:
        *result = false;
        return true;

      // Statements with no sub-components at all.
      case PNK_NOP: // induced by function f() {} function f() {}
        MOZ_ASSERT(node->is<NullaryNode>());
        *result = false;
        return true;

      case PNK_DEBUGGER:
        MOZ_ASSERT(node->is<DebuggerStatement>());
        *result = false;
        return true;

      // Statements containing only an expression have no declarations.
      case PNK_SEMI:
      case PNK_THROW:
      case PNK_RETURN:
        MOZ_ASSERT(node->is<UnaryNode>());
        *result = false;
        return true;

      // These two aren't statements in the spec, but we sometimes insert them
      // in statement lists anyway.
      case PNK_INITIALYIELD:
      case PNK_YIELD_STAR:
      case PNK_YIELD:
        MOZ_ASSERT(node->is<UnaryNode>());
        *result = false;
        return true;

      // Other statements with no sub-statement components.
      case PNK_BREAK:
      case PNK_CONTINUE:
      case PNK_IMPORT:
      case PNK_IMPORT_SPEC_LIST:
      case PNK_IMPORT_SPEC:
      case PNK_EXPORT_FROM:
      case PNK_EXPORT_DEFAULT:
      case PNK_EXPORT_SPEC_LIST:
      case PNK_EXPORT_SPEC:
      case PNK_EXPORT:
      case PNK_EXPORT_BATCH_SPEC:
      case PNK_CALL_IMPORT:
        *result = false;
        return true;

      // Statements possibly containing hoistable declarations only in the left
      // half, in ParseNode terms -- the loop body in AST terms.
      case PNK_DOWHILE:
        return ContainsHoistedDeclaration(cx, node->as<BinaryNode>().left(), result);

      // Statements possibly containing hoistable declarations only in the
      // right half, in ParseNode terms -- the loop body or nested statement
      // (usually a block statement), in AST terms.
      case PNK_WHILE:
      case PNK_WITH:
        return ContainsHoistedDeclaration(cx, node->as<BinaryNode>().right(), result);

      case PNK_LABEL:
        return ContainsHoistedDeclaration(cx, node->as<LabeledStatement>().statement(), result);

      // Statements with more complicated structures.

      // if-statement nodes may have hoisted declarations in their consequent
      // and alternative components.
      case PNK_IF: {
        TernaryNode* ifNode = &node->as<TernaryNode>();
        ParseNode* consequent = ifNode->kid2();
        if (!ContainsHoistedDeclaration(cx, consequent, result))
            return false;
        if (*result)
            return true;

        if ((node = ifNode->kid3()))
            goto restart;

        *result = false;
        return true;
      }

      // Legacy array and generator comprehensions use PNK_IF to represent
      // conditions specified in the comprehension tail: for example,
      // [x for (x in obj) if (x)].  The consequent of such PNK_IF nodes is
      // either PNK_YIELD in a PNK_SEMI statement (generator comprehensions) or
      // PNK_ARRAYPUSH (array comprehensions) .  The first case is consistent
      // with normal if-statement structure with consequent/alternative as
      // statements.  The second case is abnormal and requires that we not
      // banish PNK_ARRAYPUSH to the unreachable list, handling it explicitly.
      //
      // We could require that this one weird PNK_ARRAYPUSH case be packaged in
      // a PNK_SEMI, for consistency.  That requires careful bytecode emitter
      // adjustment that seems unwarranted for a deprecated feature.
      case PNK_ARRAYPUSH:
        *result = false;
        return true;

      // try-statements have statements to execute, and one or both of a
      // catch-list and a finally-block.
      case PNK_TRY: {
        TernaryNode* tryNode = &node->as<TernaryNode>();
        MOZ_ASSERT(tryNode->kid2() || tryNode->kid3(),
                   "must have either catch(es) or finally");

        ParseNode* tryBlock = tryNode->kid1();
        if (!ContainsHoistedDeclaration(cx, tryBlock, result))
            return false;
        if (*result)
            return true;

        if (tryNode->kid2()) {
            if (ListNode* catchList = &tryNode->kid2()->as<ListNode>()) {
                for (ParseNode* scopeNode : catchList->contents()) {
                    LexicalScopeNode* lexicalScope = &scopeNode->as<LexicalScopeNode>();

                    TernaryNode* catchNode = &lexicalScope->scopeBody()->as<TernaryNode>();
                    MOZ_ASSERT(catchNode->isKind(PNK_CATCH));

                    ParseNode* catchStatements = catchNode->kid3();
                    if (!ContainsHoistedDeclaration(cx, catchStatements, result))
                        return false;
                    if (*result)
                        return true;
                }
            }
        }

        if (ParseNode* finallyBlock = tryNode->kid3())
            return ContainsHoistedDeclaration(cx, finallyBlock, result);

        *result = false;
        return true;
      }

      // A switch node's left half is an expression; only its right half (a
      // list of cases/defaults, or a block node) could contain hoisted
      // declarations.
      case PNK_SWITCH: {
        SwitchStatement* switchNode = &node->as<SwitchStatement>();
        return ContainsHoistedDeclaration(cx, &switchNode->lexicalForCaseList(), result);
      }

      case PNK_CASE: {
        CaseClause* caseClause = &node->as<CaseClause>();
        return ContainsHoistedDeclaration(cx, caseClause->statementList(), result);
      }

      case PNK_FOR:
      case PNK_COMPREHENSIONFOR: {
        ForNode* forNode = &node->as<ForNode>();
        TernaryNode* loopHead = forNode->head();
        MOZ_ASSERT(loopHead->isKind(PNK_FORHEAD) ||
                   loopHead->isKind(PNK_FORIN) ||
                   loopHead->isKind(PNK_FOROF));

        if (loopHead->isKind(PNK_FORHEAD)) {
            // for (init?; cond?; update?), with only init possibly containing
            // a hoisted declaration.  (Note: a lexical-declaration |init| is
            // (at present) hoisted in SpiderMonkey parlance -- but such
            // hoisting doesn't extend outside of this statement, so it is not
            // hoisting in the sense meant by ContainsHoistedDeclaration.)
            ParseNode* init = loopHead->kid1();
            if (init && init->isKind(PNK_VAR)) {
                *result = true;
                return true;
            }
        } else {
            MOZ_ASSERT(loopHead->isKind(PNK_FORIN) || loopHead->isKind(PNK_FOROF));

            // for each? (target in ...), where only target may introduce
            // hoisted declarations.
            //
            //   -- or --
            //
            // for (target of ...), where only target may introduce hoisted
            // declarations.
            //
            // Either way, if |target| contains a declaration, it's |loopHead|'s
            // first kid.
            ParseNode* decl = loopHead->kid1();
            if (decl && decl->isKind(PNK_VAR)) {
                *result = true;
                return true;
            }
        }

        ParseNode* loopBody = forNode->body();
        return ContainsHoistedDeclaration(cx, loopBody, result);
      }

      case PNK_LEXICALSCOPE: {
        LexicalScopeNode* scope = &node->as<LexicalScopeNode>();
        ParseNode* expr = scope->scopeBody();

        if (expr->isKind(PNK_FOR) || expr->is<FunctionNode>())
            return ContainsHoistedDeclaration(cx, expr, result);

        MOZ_ASSERT(expr->isKind(PNK_STATEMENTLIST));
        return ListContainsHoistedDeclaration(cx, &scope->scopeBody()->as<ListNode>(), result);
      }

      // List nodes with all non-null children.
      case PNK_STATEMENTLIST:
        return ListContainsHoistedDeclaration(cx, &node->as<ListNode>(), result);

      // Grammar sub-components that should never be reached directly by this
      // method, because some parent component should have asserted itself.
      case PNK_OBJECT_PROPERTY_NAME:
      case PNK_COMPUTED_NAME:
      case PNK_SPREAD:
      case PNK_MUTATEPROTO:
      case PNK_COLON:
      case PNK_SHORTHAND:
      case PNK_CONDITIONAL:
      case PNK_TYPEOFNAME:
      case PNK_TYPEOFEXPR:
      case PNK_AWAIT:
      case PNK_VOID:
      case PNK_NOT:
      case PNK_BITNOT:
      case PNK_DELETENAME:
      case PNK_DELETEPROP:
      case PNK_DELETEELEM:
      case PNK_DELETEEXPR:
      case PNK_DELETEOPTCHAIN:
      case PNK_POS:
      case PNK_NEG:
      case PNK_PREINCREMENT:
      case PNK_POSTINCREMENT:
      case PNK_PREDECREMENT:
      case PNK_POSTDECREMENT:
      case PNK_COALESCE:
      case PNK_OR:
      case PNK_AND:
      case PNK_BITOR:
      case PNK_BITXOR:
      case PNK_BITAND:
      case PNK_STRICTEQ:
      case PNK_EQ:
      case PNK_STRICTNE:
      case PNK_NE:
      case PNK_LT:
      case PNK_LE:
      case PNK_GT:
      case PNK_GE:
      case PNK_INSTANCEOF:
      case PNK_IN:
      case PNK_LSH:
      case PNK_RSH:
      case PNK_URSH:
      case PNK_ADD:
      case PNK_SUB:
      case PNK_STAR:
      case PNK_DIV:
      case PNK_MOD:
      case PNK_POW:
      case PNK_INITPROP:
      case PNK_ASSIGN:
      case PNK_ADDASSIGN:
      case PNK_SUBASSIGN:
      case PNK_COALESCEASSIGN:
      case PNK_ORASSIGN:
      case PNK_ANDASSIGN:
      case PNK_BITORASSIGN:
      case PNK_BITXORASSIGN:
      case PNK_BITANDASSIGN:
      case PNK_LSHASSIGN:
      case PNK_RSHASSIGN:
      case PNK_URSHASSIGN:
      case PNK_MULASSIGN:
      case PNK_DIVASSIGN:
      case PNK_MODASSIGN:
      case PNK_POWASSIGN:
      case PNK_COMMA:
      case PNK_ARRAY:
      case PNK_OBJECT:
      case PNK_PROPERTYNAME:
      case PNK_DOT:
      case PNK_ELEM:
      case PNK_ARGUMENTS:
      case PNK_CALL:
      case PNK_OPTCHAIN:
      case PNK_OPTDOT:
      case PNK_OPTELEM:
      case PNK_OPTCALL:
      case PNK_NAME:
      case PNK_PRIVATE_NAME:
      case PNK_TEMPLATE_STRING:
      case PNK_TEMPLATE_STRING_LIST:
      case PNK_TAGGED_TEMPLATE:
      case PNK_CALLSITEOBJ:
      case PNK_STRING:
      case PNK_REGEXP:
      case PNK_TRUE:
      case PNK_FALSE:
      case PNK_NULL:
      case PNK_RAW_UNDEFINED:
      case PNK_THIS:
      case PNK_ELISION:
      case PNK_NUMBER:
      case PNK_NEW:
      case PNK_GENERATOR:
      case PNK_GENEXP:
      case PNK_ARRAYCOMP:
      case PNK_PARAMSBODY:
      case PNK_CATCHLIST:
      case PNK_CATCH:
      case PNK_FORIN:
      case PNK_FOROF:
      case PNK_FORHEAD:
      case PNK_CLASSFIELD:
      case PNK_STATICCLASSBLOCK:
      case PNK_CLASSMEMBERLIST:
      case PNK_CLASSNAMES:
      case PNK_NEWTARGET:
      case PNK_IMPORT_META:
      case PNK_POSHOLDER:
      case PNK_SUPERCALL:
      case PNK_SUPERBASE:
      case PNK_SETTHIS:
        MOZ_CRASH("ContainsHoistedDeclaration should have indicated false on "
                  "some parent node without recurring to test this node");

      case PNK_LIMIT: // invalid sentinel value
        MOZ_CRASH("unexpected PNK_LIMIT in node");
    }

    MOZ_CRASH("invalid node kind");
}

/*
 * Fold from one constant type to another.
 * XXX handles only strings and numbers for now
 */
static bool
FoldType(ExclusiveContext* cx, ParseNode* pn, ParseNodeKind kind)
{
    if (!pn->isKind(kind)) {
        switch (kind) {
          case PNK_NUMBER:
            if (pn->isKind(PNK_STRING)) {
                double d;
                if (!StringToNumber(cx, pn->as<NameNode>().atom(), &d))
                    return false;
                pn->setKind(PNK_NUMBER);
                pn->setArity(PN_NUMBER);
                pn->setOp(JSOP_DOUBLE);
                pn->as<NumericLiteral>().setValue(d);
            }
            break;

          case PNK_STRING:
            if (pn->isKind(PNK_NUMBER)) {
                JSAtom* atom = NumberToAtom(cx, pn->as<NumericLiteral>().value());
                if (!atom)
                    return false;
                pn->setKind(PNK_STRING);
                pn->setArity(PN_NAME);
                pn->setOp(JSOP_STRING);
                pn->as<NameNode>().setAtom(atom);
            }
            break;

          default:;
        }
    }
    return true;
}

// Remove a ParseNode, **pnp, from a parse tree, putting another ParseNode,
// *pn, in its place.
//
// pnp points to a ParseNode pointer. This must be the only pointer that points
// to the parse node being replaced. The replacement, *pn, is unchanged except
// for its pn_next pointer; updating that is necessary if *pn's new parent is a
// list node.
static void
ReplaceNode(ParseNode** pnp, ParseNode* pn)
{
    pn->pn_next = (*pnp)->pn_next;
    *pnp = pn;
}

static bool
IsEffectless(ParseNode* node)
{
    return node->isKind(PNK_TRUE) ||
           node->isKind(PNK_FALSE) ||
           node->isKind(PNK_STRING) ||
           node->isKind(PNK_TEMPLATE_STRING) ||
           node->isKind(PNK_NUMBER) ||
           node->isKind(PNK_NULL) ||
           node->isKind(PNK_RAW_UNDEFINED) ||
           node->isKind(PNK_FUNCTION) ||
           node->isKind(PNK_GENEXP);
}

enum Truthiness { Truthy, Falsy, Unknown };

static Truthiness
Boolish(ParseNode* pn, bool isNullish = false)
{
    switch (pn->getKind()) {
      case PNK_NUMBER: {
        bool isNonZeroNumber = (pn->as<NumericLiteral>().value() != 0 && !IsNaN(pn->as<NumericLiteral>().value()));
        return (isNullish || isNonZeroNumber) ? Truthy : Falsy;
      }

      case PNK_STRING:
      case PNK_TEMPLATE_STRING: {
        bool isNonZeroLengthString = (pn->as<NameNode>().atom()->length() > 0);
        return (isNullish || isNonZeroLengthString) ? Truthy : Falsy;
      }

      case PNK_TRUE:
      case PNK_FUNCTION:
      case PNK_GENEXP:
        return Truthy;

      case PNK_FALSE:
        return isNullish ? Truthy : Falsy;

      case PNK_NULL:
      case PNK_RAW_UNDEFINED:
        return Falsy;

      case PNK_VOID: {
        // |void <foo>| evaluates to |undefined| which isn't truthy.  But the
        // sense of this method requires that the expression be literally
        // replaceable with true/false: not the case if the nested expression
        // is effectful, might throw, &c.  Walk past the |void| (and nested
        // |void| expressions, for good measure) and check that the nested
        // expression doesn't break this requirement before indicating falsity.
        do {
            pn = pn->as<UnaryNode>().kid();
        } while (pn->isKind(PNK_VOID));

        return IsEffectless(pn) ? Falsy : Unknown;
      }

      default:
        return Unknown;
    }
}

static bool
Fold(ExclusiveContext* cx, ParseNode** pnp, Parser<FullParseHandler>& parser, bool inGenexpLambda);

static bool
FoldCondition(ExclusiveContext* cx, ParseNode** nodePtr, Parser<FullParseHandler>& parser,
              bool inGenexpLambda)
{
    // Conditions fold like any other expression...
    if (!Fold(cx, nodePtr, parser, inGenexpLambda))
        return false;

    // ...but then they sometimes can be further folded to constants.
    ParseNode* node = *nodePtr;
    Truthiness t = Boolish(node);
    if (t != Unknown) {
        // We can turn function nodes into constant nodes here, but mutating
        // function nodes is tricky --- in particular, mutating a function node
        // that appears on a method list corrupts the method list. However,
        // methods are M's in statements of the form 'this.foo = M;', which we
        // never fold, so we're okay.
        parser.prepareNodeForMutation(node);
        if (t == Truthy) {
            node->setKind(PNK_TRUE);
            node->setOp(JSOP_TRUE);
        } else {
            node->setKind(PNK_FALSE);
            node->setOp(JSOP_FALSE);
        }
        node->setArity(PN_NULLARY);
    }

    return true;
}

static bool
FoldTypeOfExpr(ExclusiveContext* cx, UnaryNode* node, Parser<FullParseHandler>& parser,
               bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_TYPEOFEXPR));

    if (!Fold(cx, node->unsafeKidReference(), parser, inGenexpLambda))
        return false;

    ParseNode* expr = node->kid();

    // Constant-fold the entire |typeof| if given a constant with known type.
    RootedPropertyName result(cx);
    if (expr->isKind(PNK_STRING) || expr->isKind(PNK_TEMPLATE_STRING))
        result = cx->names().string;
    else if (expr->isKind(PNK_NUMBER))
        result = cx->names().number;
    else if (expr->isKind(PNK_NULL))
        result = cx->names().object;
    else if (expr->isKind(PNK_TRUE) || expr->isKind(PNK_FALSE))
        result = cx->names().boolean;
    else if (expr->is<FunctionNode>())
        result = cx->names().function;

    if (result) {
        parser.prepareNodeForMutation(node);

        node->setKind(PNK_STRING);
        node->setArity(PN_NAME);
        node->setOp(JSOP_NOP);
        node->as<NameNode>().setAtom(result);
    }

    return true;
}

static bool
FoldDeleteExpr(ExclusiveContext* cx, UnaryNode* node, Parser<FullParseHandler>& parser,
               bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_DELETEEXPR));

    if (!Fold(cx, node->unsafeKidReference(), parser, inGenexpLambda))
        return false;

    ParseNode* expr = node->kid();

    // Expression deletion evaluates the expression, then evaluates to true.
    // For effectless expressions, eliminate the expression evaluation.
    if (IsEffectless(expr)) {
        parser.prepareNodeForMutation(node);
        node->setKind(PNK_TRUE);
        node->setArity(PN_NULLARY);
        node->setOp(JSOP_TRUE);
    }

    return true;
}

static bool
FoldDeleteElement(ExclusiveContext* cx, UnaryNode* node, Parser<FullParseHandler>& parser,
                  bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_DELETEELEM));
    MOZ_ASSERT(node->kid()->isKind(PNK_ELEM));

    if (!Fold(cx, node->unsafeKidReference(), parser, inGenexpLambda))
        return false;

    ParseNode* expr = node->kid();

    // If we're deleting an element, but constant-folding converted our
    // element reference into a dotted property access, we must *also*
    // morph the node's kind.
    //
    // In principle this also applies to |super["foo"] -> super.foo|,
    // but we don't constant-fold |super["foo"]| yet.
    MOZ_ASSERT(expr->isKind(PNK_ELEM) || expr->isKind(PNK_DOT));
    if (expr->isKind(PNK_DOT))
        node->setKind(PNK_DELETEPROP);

    return true;
}

static bool
FoldDeleteProperty(ExclusiveContext* cx, UnaryNode* node, Parser<FullParseHandler>& parser,
                   bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_DELETEPROP));
    MOZ_ASSERT(node->kid()->isKind(PNK_DOT));

#ifdef DEBUG
    ParseNodeKind oldKind = node->kid()->getKind();
#endif

    if (!Fold(cx, node->unsafeKidReference(), parser, inGenexpLambda))
        return false;

    MOZ_ASSERT(node->kid()->isKind(oldKind),
               "kind should have remained invariant under folding");

    return true;
}

static bool
FoldNot(ExclusiveContext* cx, UnaryNode* node, Parser<FullParseHandler>& parser,
        bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_NOT));

    if (!FoldCondition(cx, node->unsafeKidReference(), parser, inGenexpLambda))
        return false;

    ParseNode* expr = node->kid();

    if (expr->isKind(PNK_NUMBER)) {
        double d = expr->as<NumericLiteral>().value();

        parser.prepareNodeForMutation(node);
        if (d == 0 || IsNaN(d)) {
            node->setKind(PNK_TRUE);
            node->setOp(JSOP_TRUE);
        } else {
            node->setKind(PNK_FALSE);
            node->setOp(JSOP_FALSE);
        }
        node->setArity(PN_NULLARY);
    } else if (expr->isKind(PNK_TRUE) || expr->isKind(PNK_FALSE)) {
        bool newval = !expr->isKind(PNK_TRUE);

        parser.prepareNodeForMutation(node);
        node->setKind(newval ? PNK_TRUE : PNK_FALSE);
        node->setArity(PN_NULLARY);
        node->setOp(newval ? JSOP_TRUE : JSOP_FALSE);
    }

    return true;
}

static bool
FoldUnaryArithmetic(ExclusiveContext* cx, UnaryNode* node, Parser<FullParseHandler>& parser,
                    bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_BITNOT) || node->isKind(PNK_POS) || node->isKind(PNK_NEG),
               "need a different method for this node kind");

    if (!Fold(cx, node->unsafeKidReference(), parser, inGenexpLambda))
        return false;

    ParseNode* expr = node->kid();

    if (expr->isKind(PNK_NUMBER) || expr->isKind(PNK_TRUE) || expr->isKind(PNK_FALSE)) {
        double d = expr->isKind(PNK_NUMBER)
                   ? expr->as<NumericLiteral>().value()
                   : double(expr->isKind(PNK_TRUE));

        if (node->isKind(PNK_BITNOT))
            d = ~ToInt32(d);
        else if (node->isKind(PNK_NEG))
            d = -d;
        else
            MOZ_ASSERT(node->isKind(PNK_POS)); // nothing to do

        parser.prepareNodeForMutation(node);
        node->setKind(PNK_NUMBER);
        node->setArity(PN_NUMBER);
        node->setOp(JSOP_DOUBLE);
        node->as<NumericLiteral>().setValue(d);
    }

    return true;
}

static bool
FoldIncrementDecrement(ExclusiveContext* cx, UnaryNode* incDec, Parser<FullParseHandler>& parser,
                       bool inGenexpLambda)
{
    MOZ_ASSERT(incDec->isKind(PNK_PREINCREMENT) ||
               incDec->isKind(PNK_POSTINCREMENT) ||
               incDec->isKind(PNK_PREDECREMENT) ||
               incDec->isKind(PNK_POSTDECREMENT));

    MOZ_ASSERT(parser.isValidSimpleAssignmentTarget(incDec->kid(),
                      Parser<FullParseHandler>::PermitAssignmentToFunctionCalls));

    if (!Fold(cx, incDec->unsafeKidReference(), parser, inGenexpLambda))
        return false;

    MOZ_ASSERT(parser.isValidSimpleAssignmentTarget(incDec->kid(),
                      Parser<FullParseHandler>::PermitAssignmentToFunctionCalls));

    return true;
}

static bool
FoldLogical(ExclusiveContext* cx, ParseNode** nodePtr, Parser<FullParseHandler>& parser,
          bool inGenexpLambda)
{
    ListNode* node = &(*nodePtr)->as<ListNode>();

    bool isCoalesceNode = node->isKind(PNK_COALESCE);
    bool isOrNode = node->isKind(PNK_OR);
    bool isAndNode = node->isKind(PNK_AND);

    MOZ_ASSERT(isCoalesceNode || isOrNode || isAndNode);

    ParseNode** elem = node->unsafeHeadReference();
    do {
        if (!Fold(cx, elem, parser, inGenexpLambda))
            return false;

        Truthiness t = Boolish(*elem, isCoalesceNode);

        // If we don't know the constant-folded node's truthiness, we can't
        // reduce this node with its surroundings.  Continue folding any
        // remaining nodes.
        if (t == Unknown) {
            elem = &(*elem)->pn_next;
            continue;
        }

        bool terminateEarly = (isOrNode && t == Truthy) ||
                              (isAndNode && t == Falsy) ||
                              (isCoalesceNode && t == Truthy);

        // If the constant-folded node's truthiness will terminate the
        // condition -- `a || true || expr` or `b && false && expr` or
        // `false ?? c ?? expr` -- then trailing nodes will never be
        // evaluated. Truncate the list after the known-truthiness node,
        // as it's the overall result.
        if (terminateEarly) {
            ParseNode* afterNext;
            for (ParseNode* next = (*elem)->pn_next; next; next = afterNext) {
                afterNext = next->pn_next;
                parser.handler.freeTree(next);
                node->unsafeDecrementCount();
            }

            // Terminate the original and/or list at the known-truthiness
            // node.
            (*elem)->pn_next = nullptr;
            elem = &(*elem)->pn_next;
            break;
        }

        // We've encountered a vacuous node that'll never short- circuit
        // evaluation.
        if ((*elem)->pn_next) {
            // This node is never the overall result when there are
            // subsequent nodes.  Remove it.
            ParseNode* elt = *elem;
            *elem = elt->pn_next;
            parser.handler.freeTree(elt);
            node->unsafeDecrementCount();
        } else {
            // Otherwise this node is the result of the overall expression,
            // so leave it alone.  And we're done.
            elem = &(*elem)->pn_next;
            break;
        }
    } while (*elem);

    node->unsafeReplaceTail(elem);

    // If we removed nodes, we may have to replace a one-element list with
    // its element.
    if (node->count() == 1) {
        ParseNode* first = node->head();
        ReplaceNode(nodePtr, first);

        node->setKind(PNK_NULL);
        node->setArity(PN_NULLARY);
        parser.freeTree(node);
    }

    return true;
}

static bool
FoldConditional(ExclusiveContext* cx, ParseNode** nodePtr, Parser<FullParseHandler>& parser,
                bool inGenexpLambda)
{
    ParseNode** nextNode = nodePtr;

    do {
        // |nextNode| on entry points to the C?T:F expression to be folded.
        // Reset it to exit the loop in the common case where F isn't another
        // ?: expression.
        nodePtr = nextNode;
        nextNode = nullptr;

        TernaryNode* node = &(*nodePtr)->as<TernaryNode>();
        MOZ_ASSERT(node->isKind(PNK_CONDITIONAL));
        ParseNode** expr = node->unsafeKid1Reference();
        if (!FoldCondition(cx, expr, parser, inGenexpLambda))
            return false;

        ParseNode** ifTruthy = node->unsafeKid2Reference();
        if (!Fold(cx, ifTruthy, parser, inGenexpLambda))
            return false;

        ParseNode** ifFalsy = node->unsafeKid3Reference();

        // If our C?T:F node has F as another ?: node, *iteratively* constant-
        // fold F *after* folding C and T (and possibly eliminating C and one
        // of T/F entirely); otherwise fold F normally.  Making |nextNode| non-
        // null causes this loop to run again to fold F.
        //
        // Conceivably we could instead/also iteratively constant-fold T, if T
        // were more complex than F.  Such an optimization is unimplemented.
        if ((*ifFalsy)->isKind(PNK_CONDITIONAL)) {
            MOZ_ASSERT((*ifFalsy)->is<TernaryNode>());
            nextNode = ifFalsy;
        } else {
            if (!Fold(cx, ifFalsy, parser, inGenexpLambda))
                return false;
        }

        // Try to constant-fold based on the condition expression.
        Truthiness t = Boolish(*expr);
        if (t == Unknown)
            continue;

        // Otherwise reduce 'C ? T : F' to T or F as directed by C.
        ParseNode* replacement;
        ParseNode* discarded;
        if (t == Truthy) {
            replacement = *ifTruthy;
            discarded = *ifFalsy;
        } else {
            replacement = *ifFalsy;
            discarded = *ifTruthy;
        }

        // Otherwise perform a replacement.  This invalidates |nextNode|, so
        // reset it (if the replacement requires folding) or clear it (if
        // |ifFalsy| is dead code) as needed.
        if (nextNode)
            nextNode = (*nextNode == replacement) ? nodePtr : nullptr;
        ReplaceNode(nodePtr, replacement);

        parser.freeTree(discarded);
    } while (nextNode);

    return true;
}

static bool
FoldIf(ExclusiveContext* cx, ParseNode** nodePtr, Parser<FullParseHandler>& parser,
       bool inGenexpLambda)
{
    ParseNode** nextNode = nodePtr;

    do {
        // |nextNode| on entry points to the initial |if| to be folded.  Reset
        // it to exit the loop when the |else| arm isn't another |if|.
        nodePtr = nextNode;
        nextNode = nullptr;

        TernaryNode* node = &(*nodePtr)->as<TernaryNode>();
        MOZ_ASSERT(node->isKind(PNK_IF));
        ParseNode** expr = node->unsafeKid1Reference();
        if (!FoldCondition(cx, expr, parser, inGenexpLambda))
            return false;

        ParseNode** consequent = node->unsafeKid2Reference();
        if (!Fold(cx, consequent, parser, inGenexpLambda))
            return false;

        ParseNode** alternative = node->unsafeKid3Reference();
        if (*alternative) {
            // If in |if (C) T; else F;| we have |F| as another |if|,
            // *iteratively* constant-fold |F| *after* folding |C| and |T| (and
            // possibly completely replacing the whole thing with |T| or |F|);
            // otherwise fold F normally.  Making |nextNode| non-null causes
            // this loop to run again to fold F.
            if ((*alternative)->isKind(PNK_IF)) {
                MOZ_ASSERT((*alternative)->is<TernaryNode>());
                nextNode = alternative;
            } else {
                if (!Fold(cx, alternative, parser, inGenexpLambda))
                    return false;
            }
        }

        // Eliminate the consequent or alternative if the condition has
        // constant truthiness.  Don't eliminate if we have an |if (0)| in
        // trailing position in a generator expression, as this is a special
        // form we can't fold away.
        Truthiness t = Boolish(*expr);
        if (t == Unknown || inGenexpLambda)
            continue;

        // Careful!  Either of these can be null: |replacement| in |if (0) T;|,
        // and |discarded| in |if (true) T;|.
        ParseNode* replacement;
        ParseNode* discarded;
        if (t == Truthy) {
            replacement = *consequent;
            discarded = *alternative;
        } else {
            replacement = *alternative;
            discarded = *consequent;
        }

        bool performReplacement = true;
        if (discarded) {
            // A declaration that hoists outside the discarded arm prevents the
            // |if| from being folded away.
            bool containsHoistedDecls;
            if (!ContainsHoistedDeclaration(cx, discarded, &containsHoistedDecls))
                return false;

            performReplacement = !containsHoistedDecls;
        }

        if (!performReplacement)
            continue;

        if (!replacement) {
            // If there's no replacement node, we have a constantly-false |if|
            // with no |else|.  Replace the entire thing with an empty
            // statement list.
            parser.prepareNodeForMutation(node);
            node->setKind(PNK_STATEMENTLIST);
            node->setArity(PN_LIST);
            node->as<ListNode>().makeEmpty();
        } else {
            // Replacement invalidates |nextNode|, so reset it (if the
            // replacement requires folding) or clear it (if |alternative|
            // is dead code) as needed.
            if (nextNode)
                nextNode = (*nextNode == replacement) ? nodePtr : nullptr;
            ReplaceNode(nodePtr, replacement);

            // Morph the original node into a discardable node, then
            // aggressively free it and the discarded arm (if any) to suss out
            // any bugs in the preceding logic.
            node->setKind(PNK_STATEMENTLIST);
            node->setArity(PN_LIST);
            node->as<ListNode>().makeEmpty();
            if (discarded)
                node->as<ListNode>().append(discarded);
            parser.freeTree(node);
        }
    } while (nextNode);

    return true;
}

static bool
FoldFunction(ExclusiveContext* cx, FunctionNode* node, Parser<FullParseHandler>& parser,
             bool inGenexpLambda)
{
    // Don't constant-fold inside "use asm" code, as this could create a parse
    // tree that doesn't type-check as asm.js.
    if (node->funbox()->useAsmOrInsideUseAsm())
        return true;

    // Note: pn_body is null for lazily-parsed functions.
    if (node->body()) {
        if (!Fold(cx, node->unsafeBodyReference(), parser, node->funbox()->isGenexpLambda))
            return false;
    }

    return true;
}

static double
ComputeBinary(ParseNodeKind kind, double left, double right)
{
    if (kind == PNK_ADD)
        return left + right;

    if (kind == PNK_SUB)
        return left - right;

    if (kind == PNK_STAR)
        return left * right;

    if (kind == PNK_MOD)
        return right == 0 ? GenericNaN() : js_fmod(left, right);

    if (kind == PNK_URSH)
        return ToUint32(left) >> (ToUint32(right) & 31);

    if (kind == PNK_DIV) {
        if (right == 0) {
#if defined(XP_WIN)
            /* XXX MSVC miscompiles such that (NaN == 0) */
            if (IsNaN(right))
                return GenericNaN();
#endif
            if (left == 0 || IsNaN(left))
                return GenericNaN();
            if (IsNegative(left) != IsNegative(right))
                return NegativeInfinity<double>();
            return PositiveInfinity<double>();
        }

        return left / right;
    }

    MOZ_ASSERT(kind == PNK_LSH || kind == PNK_RSH);

    int32_t i = ToInt32(left);
    uint32_t j = ToUint32(right) & 31;
    return int32_t((kind == PNK_LSH) ? uint32_t(i) << j : i >> j);
}

static bool
FoldModule(ExclusiveContext* cx, ModuleNode* node, Parser<FullParseHandler>& parser)
{
    MOZ_ASSERT(node->body());
    return Fold(cx, node->unsafeBodyReference(), parser, false);
}

static bool
FoldBinaryArithmetic(ExclusiveContext* cx, ListNode* node, Parser<FullParseHandler>& parser,
                     bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_SUB) ||
               node->isKind(PNK_STAR) ||
               node->isKind(PNK_LSH) ||
               node->isKind(PNK_RSH) ||
               node->isKind(PNK_URSH) ||
               node->isKind(PNK_DIV) ||
               node->isKind(PNK_MOD));
    MOZ_ASSERT(node->count() >= 2);

    // Fold each operand, ideally into a number.
    ParseNode** listp = node->unsafeHeadReference();
    for (; *listp; listp = &(*listp)->pn_next) {
        if (!Fold(cx, listp, parser, inGenexpLambda))
            return false;

        if (!FoldType(cx, *listp, PNK_NUMBER))
            return false;
    }

    node->unsafeReplaceTail(listp);

    // Now fold all leading numeric terms together into a single number.
    // (Trailing terms for the non-shift operations can't be folded together
    // due to floating point imprecision.  For example, if |x === -2**53|,
    // |x - 1 - 1 === -2**53| but |x - 2 === -2**53 - 2|.  Shifts could be
    // folded, but it doesn't seem worth the effort.)
    ParseNode* elem = node->head();
    ParseNode* next = elem->pn_next;
    if (elem->isKind(PNK_NUMBER)) {
        ParseNodeKind kind = node->getKind();
        while (true) {
            if (!next || !next->isKind(PNK_NUMBER))
                break;

            double d = ComputeBinary(kind, elem->as<NumericLiteral>().value(), next->as<NumericLiteral>().value());

            ParseNode* afterNext = next->pn_next;
            parser.freeTree(next);
            next = afterNext;
            elem->pn_next = next;

            elem->setKind(PNK_NUMBER);
            elem->setArity(PN_NUMBER);
            elem->setOp(JSOP_DOUBLE);
            elem->as<NumericLiteral>().setValue(d);

            node->unsafeDecrementCount();
        }

        if (node->count() == 1) {
            MOZ_ASSERT(node->head() == elem);
            MOZ_ASSERT(elem->isKind(PNK_NUMBER));

            double d = elem->as<NumericLiteral>().value();
            node->setKind(PNK_NUMBER);
            node->setArity(PN_NUMBER);
            node->setOp(JSOP_DOUBLE);
            node->as<NumericLiteral>().setValue(d);

            parser.freeTree(elem);
        }
    }

    return true;
}

static bool
FoldExponentiation(ExclusiveContext* cx, ListNode* node, Parser<FullParseHandler>& parser,
                   bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_POW));
    MOZ_ASSERT(node->count() >= 2);

    // Fold each operand, ideally into a number.
    ParseNode** listp = node->unsafeHeadReference();
    for (; *listp; listp = &(*listp)->pn_next) {
        if (!Fold(cx, listp, parser, inGenexpLambda))
            return false;

        if (!FoldType(cx, *listp, PNK_NUMBER))
            return false;
    }

    node->unsafeReplaceTail(listp);

    // Unlike all other binary arithmetic operators, ** is right-associative:
    // 2**3**5 is 2**(3**5), not (2**3)**5.  As list nodes singly-link their
    // children, full constant-folding requires either linear space or dodgy
    // in-place linked list reversal.  So we only fold one exponentiation: it's
    // easy and addresses common cases like |2**32|.
    if (node->count() > 2)
        return true;

    ParseNode* base = node->head();
    ParseNode* exponent = base->pn_next;
    if (!base->isKind(PNK_NUMBER) || !exponent->isKind(PNK_NUMBER))
        return true;

    double d1 = base->as<NumericLiteral>().value();
    double d2 = exponent->as<NumericLiteral>().value();

    parser.prepareNodeForMutation(node);
    node->setKind(PNK_NUMBER);
    node->setArity(PN_NUMBER);
    node->setOp(JSOP_DOUBLE);
    node->as<NumericLiteral>().setValue(ecmaPow(d1, d2));
    return true;
}

static bool
FoldList(ExclusiveContext* cx, ListNode* list, Parser<FullParseHandler>& parser,
         bool inGenexpLambda)
{
    ParseNode** elem = list->unsafeHeadReference();
    for (; *elem; elem = &(*elem)->pn_next) {
        if (!Fold(cx, elem, parser, inGenexpLambda))
            return false;
    }

    list->unsafeReplaceTail(elem);

    return true;
}

static bool
FoldReturn(ExclusiveContext* cx, UnaryNode* node, Parser<FullParseHandler>& parser,
           bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_RETURN));

    if (node->kid()) {
        if (!Fold(cx, node->unsafeKidReference(), parser, inGenexpLambda))
            return false;
    }

    return true;
}

static bool
FoldTry(ExclusiveContext* cx, TryNode* node, Parser<FullParseHandler>& parser,
        bool inGenexpLambda)
{
    ParseNode** statements = node->unsafeKid1Reference();
    if (!Fold(cx, statements, parser, inGenexpLambda))
        return false;

    ParseNode** catchList = node->unsafeKid2Reference();
    if (*catchList) {
        if (!Fold(cx, catchList, parser, inGenexpLambda))
            return false;
    }

    ParseNode** finally = node->unsafeKid3Reference();
    if (*finally) {
        if (!Fold(cx, finally, parser, inGenexpLambda))
            return false;
    }

    return true;
}

static bool
FoldCatch(ExclusiveContext* cx, TernaryNode* node, Parser<FullParseHandler>& parser,
          bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_CATCH));

    ParseNode** binding = node->unsafeKid1Reference();
    if (*binding) {
        if (!Fold(cx, binding, parser, inGenexpLambda))
            return false;
    }

    ParseNode** cond = node->unsafeKid2Reference();
    if (*cond) {
        if (!FoldCondition(cx, cond, parser, inGenexpLambda))
            return false;
    }

    if (ParseNode** statements = node->unsafeKid3Reference()) {
        if (!Fold(cx, statements, parser, inGenexpLambda))
            return false;
    }

    return true;
}

static bool
FoldClass(ExclusiveContext* cx, ClassNode* node, Parser<FullParseHandler>& parser,
          bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_CLASS));
    ParseNode** classNames = node->unsafeKid1Reference();
    if (*classNames) {
        if (!Fold(cx, classNames, parser, inGenexpLambda))
            return false;
    }

    ParseNode** heritage = node->unsafeKid2Reference();
    if (*heritage) {
        if (!Fold(cx, heritage, parser, inGenexpLambda))
            return false;
    }

    ParseNode** body = node->unsafeKid3Reference();
    return Fold(cx, body, parser, inGenexpLambda);
}

static bool
FoldElement(ExclusiveContext* cx, ParseNode** nodePtr, Parser<FullParseHandler>& parser,
            bool inGenexpLambda)
{
    PropertyByValueBase* elem = &(*nodePtr)->as<PropertyByValueBase>();

    if (!Fold(cx, elem->unsafeLeftReference(), parser, inGenexpLambda))
        return false;

    if (!Fold(cx, elem->unsafeRightReference(), parser, inGenexpLambda))
        return false;

    ParseNode* expr = &elem->expression();
    ParseNode* key = &elem->key();
    PropertyName* name = nullptr;
    if (key->isKind(PNK_STRING)) {
        JSAtom* atom = key->as<NameNode>().atom();
        uint32_t index;

        if (atom->isIndex(&index)) {
            // Optimization 1: We have something like expr["100"]. This is
            // equivalent to expr[100] which is faster.
            key->setKind(PNK_NUMBER);
            key->setArity(PN_NUMBER);
            key->setOp(JSOP_DOUBLE);
            key->as<NumericLiteral>().setValue(index);
        } else {
            name = atom->asPropertyName();
        }
    } else if (key->isKind(PNK_NUMBER)) {
        double number = key->as<NumericLiteral>().value();
        if (number != ToUint32(number)) {
            // Optimization 2: We have something like expr[3.14]. The number
            // isn't an array index, so it converts to a string ("3.14"),
            // enabling optimization 3 below.
            JSAtom* atom = ToAtom<NoGC>(cx, DoubleValue(number));
            if (!atom)
                return false;
            name = atom->asPropertyName();
        }
    }

    // If we don't have a name, we can't optimize to getprop.
    if (!name)
        return true;

    // Optimization 3: We have expr["foo"] where foo is not an index.  Convert
    // to a property access (like expr.foo) that optimizes better downstream.
    NameNode* nameNode = parser.handler.newPropertyName(name, key->pn_pos);
    if (!nameNode)
        return false;
    ParseNode* dottedAccess;
    if (elem->isKind(PNK_OPTELEM)) {
        dottedAccess = parser.handler.newOptionalPropertyAccess(expr, nameNode);
    } else {
        dottedAccess = parser.handler.newPropertyAccess(expr, nameNode);
    }
    if (!dottedAccess) {
        return false;
    }
    dottedAccess->setInParens(elem->isInParens());
    ReplaceNode(nodePtr, dottedAccess);

    // If we've replaced |expr["prop"]| with |expr.prop|, we can now free the
    // |"prop"| and |expr["prop"]| nodes -- but not the |expr| node that we're
    // now using as a sub-node of |dottedAccess|.  Munge |expr["prop"]| into a
    // node with |"prop"| as its only child, that'll pass AST sanity-checking
    // assertions during freeing, then free it.
    elem->setKind(PNK_TYPEOFEXPR);
    elem->setArity(PN_UNARY);
    *(elem->as<UnaryNode>().unsafeKidReference()) = key;
    parser.freeTree(elem);

    return true;
}

static bool
FoldAdd(ExclusiveContext* cx, ParseNode** nodePtr, Parser<FullParseHandler>& parser,
        bool inGenexpLambda)
{
    ListNode* node = &(*nodePtr)->as<ListNode>();

    MOZ_ASSERT(node->isKind(PNK_ADD));
    MOZ_ASSERT(node->count() >= 2);

    // Generically fold all operands first.
    if (!FoldList(cx, node, parser, inGenexpLambda))
        return false;

    // Fold leading numeric operands together:
    //
    //   (1 + 2 + x)  becomes  (3 + x)
    //
    // Don't go past the leading operands: additions after a string are
    // string concatenations, not additions: ("1" + 2 + 3 === "123").
    ParseNode* current = node->head();
    ParseNode* next = current->pn_next;
    if (current->isKind(PNK_NUMBER)) {
        do {
            if (!next->isKind(PNK_NUMBER))
                break;

            NumericLiteral* num = &current->as<NumericLiteral>();

            num->setValue(num->value() + next->as<NumericLiteral>().value());
            current->pn_next = next->pn_next;
            parser.freeTree(next);
            next = current->pn_next;

            node->unsafeDecrementCount();
        } while (next);
    }

    // If any operands remain, attempt string concatenation folding.
    do {
        // If no operands remain, we're done.
        if (!next)
            break;

        // (number + string) is string concatenation *only* at the start of
        // the list: (x + 1 + "2" !== x + "12") when x is a number.
        if (current->isKind(PNK_NUMBER) && next->isKind(PNK_STRING)) {
            if (!FoldType(cx, current, PNK_STRING))
                return false;
            next = current->pn_next;
        }

        // The first string forces all subsequent additions to be
        // string concatenations.
        do {
            if (current->isKind(PNK_STRING))
                break;

            current = next;
            next = next->pn_next;
        } while (next);

        // If there's nothing left to fold, we're done.
        if (!next)
            break;

        RootedString combination(cx);
        RootedString tmp(cx);
        do {
            // Create a rope of the current string and all succeeding
            // constants that we can convert to strings, then atomize it
            // and replace them all with that fresh string.
            MOZ_ASSERT(current->isKind(PNK_STRING));

            combination = current->as<NameNode>().atom();

            do {
                // Try folding the next operand to a string.
                if (!FoldType(cx, next, PNK_STRING))
                    return false;

                // Stop glomming once folding doesn't produce a string.
                if (!next->isKind(PNK_STRING))
                    break;

                // Add this string to the combination and remove the node.
                tmp = next->as<NameNode>().atom();
                combination = ConcatStrings<CanGC>(cx, combination, tmp);
                if (!combination)
                    return false;

                current->pn_next = next->pn_next;
                parser.freeTree(next);
                next = current->pn_next;

                node->unsafeDecrementCount();
            } while (next);

            // Replace |current|'s string with the entire combination.
            MOZ_ASSERT(current->isKind(PNK_STRING));
            combination = AtomizeString(cx, combination);
            if (!combination)
                return false;
            current->as<NameNode>().setAtom(&combination->asAtom());


            // If we're out of nodes, we're done.
            if (!next)
                break;

            current = next;
            next = current->pn_next;

            // If we're out of nodes *after* the non-foldable-to-string
            // node, we're done.
            if (!next)
                break;

            // Otherwise find the next node foldable to a string, and loop.
            do {
                current = next;
                next = current->pn_next;

                if (!FoldType(cx, current, PNK_STRING))
                    return false;
                next = current->pn_next;
            } while (!current->isKind(PNK_STRING) && next);
        } while (next);
    } while (false);

    MOZ_ASSERT(!next, "must have considered all nodes here");
    MOZ_ASSERT(!current->pn_next, "current node must be the last node");

    node->unsafeReplaceTail(&current->pn_next);

    if (node->count() == 1) {
        // We reduced the list to a constant.  Replace the PNK_ADD node
        // with that constant.
        ReplaceNode(nodePtr, current);

        // Free the old node to aggressively verify nothing uses it.
        node->setKind(PNK_TRUE);
        node->setArity(PN_NULLARY);
        node->setOp(JSOP_TRUE);
        parser.freeTree(node);
    }

    return true;
}

static bool
FoldCall(ExclusiveContext* cx, BinaryNode* node, Parser<FullParseHandler>& parser,
         bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_CALL) ||
               node->isKind(PNK_OPTCALL) ||
               node->isKind(PNK_SUPERCALL) ||
               node->isKind(PNK_NEW) ||
               node->isKind(PNK_TAGGED_TEMPLATE));

    // Don't fold a parenthesized callable component in an invocation, as this
    // might cause a different |this| value to be used, changing semantics:
    //
    //   var prop = "global";
    //   var obj = { prop: "obj", f: function() { return this.prop; } };
    //   assertEq((true ? obj.f : null)(), "global");
    //   assertEq(obj.f(), "obj");
    //   assertEq((true ? obj.f : null)``, "global");
    //   assertEq(obj.f``, "obj");
    //
    // As an exception to this, we do allow folding the function in
    // `(function() { ... })()` (the module pattern), because that lets us
    // constant fold code inside that function.
    //
    // See bug 537673 and bug 1182373.
    ParseNode* callee = node->left();
    if (node->isKind(PNK_NEW) || !callee->isInParens() || callee->is<FunctionNode>()) {
        if (!Fold(cx, node->unsafeLeftReference(), parser, inGenexpLambda))
            return false;
    }

    if (!Fold(cx, node->unsafeRightReference(), parser, inGenexpLambda))
        return false;

    return true;
}

static bool
FoldArguments(ExclusiveContext* cx, ListNode* node, Parser<FullParseHandler>& parser,
              bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_ARGUMENTS));
    ParseNode** listp = node->unsafeHeadReference();

    for (; *listp; listp = &(*listp)->pn_next) {
        if (!Fold(cx, listp, parser, inGenexpLambda))
            return false;
    }

    node->unsafeReplaceTail(listp);
    return true;
}

static bool
FoldForInOrOf(ExclusiveContext* cx, TernaryNode* node, Parser<FullParseHandler>& parser,
              bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_FORIN) || node->isKind(PNK_FOROF));
    MOZ_ASSERT(!node->kid2());

    return Fold(cx, node->unsafeKid1Reference(), parser, inGenexpLambda) &&
           Fold(cx, node->unsafeKid3Reference(), parser, inGenexpLambda);
}

static bool
FoldForHead(ExclusiveContext* cx, TernaryNode* node, Parser<FullParseHandler>& parser,
            bool inGenexpLambda)
{
    MOZ_ASSERT(node->isKind(PNK_FORHEAD));
    ParseNode** init = node->unsafeKid1Reference();
    if (*init) {
        if (!Fold(cx, init, parser, inGenexpLambda))
            return false;
    }

    ParseNode** test = node->unsafeKid2Reference();
    if (*test) {
        if (!FoldCondition(cx, test, parser, inGenexpLambda))
            return false;

        if ((*test)->isKind(PNK_TRUE)) {
            parser.freeTree(*test);
            *test = nullptr;
        }
    }

    ParseNode** update = node->unsafeKid3Reference();
    if (*update) {
        if (!Fold(cx, update, parser, inGenexpLambda))
            return false;
    }

    return true;
}

static bool
FoldDottedProperty(ExclusiveContext* cx, PropertyAccessBase* prop, Parser<FullParseHandler>& parser,
                   bool inGenexpLambda)
{
    // Iterate through a long chain of dotted property accesses to find the
    // most-nested non-dotted property node, then fold that.
    ParseNode** nested = prop->unsafeLeftReference();
    while ((*nested)->isKind(PNK_DOT) || (*nested)->isKind(PNK_OPTDOT)) {
        nested = (*nested)->as<PropertyAccessBase>().unsafeLeftReference();
    }

    return Fold(cx, nested, parser, inGenexpLambda);
}

static bool
FoldName(ExclusiveContext* cx, NameNode* nameNode, Parser<FullParseHandler>& parser,
         bool inGenexpLambda)
{
    MOZ_ASSERT(nameNode->isKind(PNK_NAME));

    if (!nameNode->initializer())
        return true;

    return Fold(cx, nameNode->unsafeInitializerReference(), parser, inGenexpLambda);
}

bool
Fold(ExclusiveContext* cx, ParseNode** pnp, Parser<FullParseHandler>& parser, bool inGenexpLambda)
{
    JS_CHECK_RECURSION(cx, return false);

    ParseNode* pn = *pnp;
#ifdef DEBUG
    ParseNodeKind kind = pn->getKind();
#endif

    switch (pn->getKind()) {
      case PNK_NOP:
      case PNK_TRUE:
      case PNK_FALSE:
      case PNK_NULL:
      case PNK_RAW_UNDEFINED:
      case PNK_ELISION:
      case PNK_GENERATOR:
      case PNK_EXPORT_BATCH_SPEC:
      case PNK_POSHOLDER:
        MOZ_ASSERT(pn->is<NullaryNode>());
        return true;

      case PNK_DEBUGGER:
        MOZ_ASSERT(pn->is<DebuggerStatement>());
        return true;

      case PNK_BREAK:
        MOZ_ASSERT(pn->is<BreakStatement>());
        return true;

      case PNK_CONTINUE:
        MOZ_ASSERT(pn->is<ContinueStatement>());
        return true;

      case PNK_OBJECT_PROPERTY_NAME:
      case PNK_PRIVATE_NAME:
      case PNK_STRING:
      case PNK_TEMPLATE_STRING:
        MOZ_ASSERT(pn->is<NameNode>());
        return true;

      case PNK_REGEXP:
        MOZ_ASSERT(pn->is<RegExpLiteral>());
        return true;

      case PNK_NUMBER:
        MOZ_ASSERT(pn->is<NumericLiteral>());
        return true;

      case PNK_SUPERBASE:
      case PNK_TYPEOFNAME: {
#ifdef DEBUG
        UnaryNode* node = &pn->as<UnaryNode>();
        NameNode* nameNode = &node->kid()->as<NameNode>();
        MOZ_ASSERT(nameNode->isKind(PNK_NAME));
        MOZ_ASSERT(!nameNode->initializer());
#endif
        return true;
      }

      case PNK_TYPEOFEXPR:
        return FoldTypeOfExpr(cx, &pn->as<UnaryNode>(), parser, inGenexpLambda);

      case PNK_DELETENAME: {
        MOZ_ASSERT(pn->as<UnaryNode>().kid()->isKind(PNK_NAME));
        return true;
      }

      case PNK_DELETEEXPR:
        return FoldDeleteExpr(cx, &pn->as<UnaryNode>(), parser, inGenexpLambda);

      case PNK_DELETEELEM:
        return FoldDeleteElement(cx, &pn->as<UnaryNode>(), parser, inGenexpLambda);

      case PNK_DELETEPROP:
        return FoldDeleteProperty(cx, &pn->as<UnaryNode>(), parser, inGenexpLambda);

      case PNK_CONDITIONAL:
        MOZ_ASSERT((*pnp)->is<TernaryNode>());
        return FoldConditional(cx, pnp, parser, inGenexpLambda);

      case PNK_IF:
        MOZ_ASSERT((*pnp)->is<TernaryNode>());
        return FoldIf(cx, pnp, parser, inGenexpLambda);

      case PNK_NOT:
        return FoldNot(cx, &pn->as<UnaryNode>(), parser, inGenexpLambda);

      case PNK_BITNOT:
      case PNK_POS:
      case PNK_NEG:
        return FoldUnaryArithmetic(cx, &pn->as<UnaryNode>(), parser, inGenexpLambda);

      case PNK_PREINCREMENT:
      case PNK_POSTINCREMENT:
      case PNK_PREDECREMENT:
      case PNK_POSTDECREMENT:
        return FoldIncrementDecrement(cx, &pn->as<UnaryNode>(), parser, inGenexpLambda);

      case PNK_THROW:
      case PNK_ARRAYPUSH:
      case PNK_MUTATEPROTO:
      case PNK_COMPUTED_NAME:
      case PNK_STATICCLASSBLOCK:
      case PNK_SPREAD:
      case PNK_EXPORT:
      case PNK_VOID:
        return Fold(cx, pn->as<UnaryNode>().unsafeKidReference(), parser, inGenexpLambda);

      case PNK_EXPORT_DEFAULT:
      case PNK_GENEXP:
        return Fold(cx, pn->as<BinaryNode>().unsafeLeftReference(), parser, inGenexpLambda);

      case PNK_DELETEOPTCHAIN:
      case PNK_OPTCHAIN:
      case PNK_SEMI:
      case PNK_THIS: {
        UnaryNode* node = &pn->as<UnaryNode>();
        ParseNode** expr = node->unsafeKidReference();
        if (*expr)
            return Fold(cx, expr, parser, inGenexpLambda);
        return true;
      }

      case PNK_COALESCE:
      case PNK_AND:
      case PNK_OR:
        MOZ_ASSERT((*pnp)->is<ListNode>());
        return FoldLogical(cx, pnp, parser, inGenexpLambda);

      case PNK_FUNCTION:
        return FoldFunction(cx, &pn->as<FunctionNode>(), parser, inGenexpLambda);

      case PNK_MODULE:
        return FoldModule(cx, &pn->as<ModuleNode>(), parser);

      case PNK_SUB:
      case PNK_STAR:
      case PNK_LSH:
      case PNK_RSH:
      case PNK_URSH:
      case PNK_DIV:
      case PNK_MOD:
        return FoldBinaryArithmetic(cx, &pn->as<ListNode>(), parser, inGenexpLambda);

      case PNK_POW:
        return FoldExponentiation(cx, &pn->as<ListNode>(), parser, inGenexpLambda);

      // Various list nodes not requiring care to minimally fold.  Some of
      // these could be further folded/optimized, but we don't make the effort.
      case PNK_BITOR:
      case PNK_BITXOR:
      case PNK_BITAND:
      case PNK_STRICTEQ:
      case PNK_EQ:
      case PNK_STRICTNE:
      case PNK_NE:
      case PNK_LT:
      case PNK_LE:
      case PNK_GT:
      case PNK_GE:
      case PNK_INSTANCEOF:
      case PNK_IN:
      case PNK_COMMA:
      case PNK_ARRAY:
      case PNK_OBJECT:
      case PNK_ARRAYCOMP:
      case PNK_STATEMENTLIST:
      case PNK_CLASSMEMBERLIST:
      case PNK_CATCHLIST:
      case PNK_TEMPLATE_STRING_LIST:
      case PNK_VAR:
      case PNK_CONST:
      case PNK_LET:
      case PNK_PARAMSBODY:
      case PNK_CALLSITEOBJ:
      case PNK_EXPORT_SPEC_LIST:
      case PNK_IMPORT_SPEC_LIST:
        return FoldList(cx, &pn->as<ListNode>(), parser, inGenexpLambda);

      case PNK_INITIALYIELD: {
#ifdef DEBUG
        AssignmentNode* assignNode = &pn->as<UnaryNode>().kid()->as<AssignmentNode>();
        MOZ_ASSERT(assignNode->left()->isKind(PNK_NAME));
        MOZ_ASSERT(assignNode->right()->isKind(PNK_GENERATOR));
#endif
        return true;
      }

      case PNK_YIELD_STAR:
        return Fold(cx, pn->as<UnaryNode>().unsafeKidReference(), parser, inGenexpLambda);

      case PNK_YIELD:
      case PNK_AWAIT: {
        UnaryNode* node = &pn->as<UnaryNode>();
        if (!node->kid())
            return true;
        return Fold(cx, node->unsafeKidReference(), parser, inGenexpLambda);
      }

      case PNK_RETURN:
        return FoldReturn(cx, &pn->as<UnaryNode>(), parser, inGenexpLambda);

      case PNK_TRY:
        return FoldTry(cx, &pn->as<TryNode>(), parser, inGenexpLambda);

      case PNK_CATCH:
        return FoldCatch(cx, &pn->as<TernaryNode>(), parser, inGenexpLambda);

      case PNK_CLASS:
        return FoldClass(cx, &pn->as<ClassNode>(), parser, inGenexpLambda);

      case PNK_OPTELEM:
      case PNK_ELEM:
        MOZ_ASSERT((*pnp)->is<PropertyByValueBase>());
        return FoldElement(cx, pnp, parser, inGenexpLambda);

      case PNK_ADD:
        MOZ_ASSERT((*pnp)->is<ListNode>());
        return FoldAdd(cx, pnp, parser, inGenexpLambda);

      case PNK_CALL:
      case PNK_OPTCALL:
      case PNK_NEW:
      case PNK_SUPERCALL:
      case PNK_TAGGED_TEMPLATE:
        return FoldCall(cx, &pn->as<BinaryNode>(), parser, inGenexpLambda);

      case PNK_ARGUMENTS:
        return FoldArguments(cx, &pn->as<ListNode>(), parser, inGenexpLambda);

      case PNK_SWITCH:
      case PNK_COLON:
      case PNK_INITPROP:
      case PNK_ASSIGN:
      case PNK_ADDASSIGN:
      case PNK_SUBASSIGN:
      case PNK_COALESCEASSIGN:
      case PNK_ORASSIGN:
      case PNK_ANDASSIGN:
      case PNK_BITORASSIGN:
      case PNK_BITANDASSIGN:
      case PNK_BITXORASSIGN:
      case PNK_LSHASSIGN:
      case PNK_RSHASSIGN:
      case PNK_URSHASSIGN:
      case PNK_DIVASSIGN:
      case PNK_MODASSIGN:
      case PNK_MULASSIGN:
      case PNK_POWASSIGN:
      case PNK_IMPORT:
      case PNK_EXPORT_FROM:
      case PNK_SHORTHAND:
      case PNK_FOR:
      case PNK_COMPREHENSIONFOR:
      case PNK_CLASSMETHOD:
      case PNK_IMPORT_SPEC:
      case PNK_EXPORT_SPEC:
      case PNK_SETTHIS: {
        BinaryNode* node = &pn->as<BinaryNode>();
        return Fold(cx, node->unsafeLeftReference(), parser, inGenexpLambda) &&
               Fold(cx, node->unsafeRightReference(), parser, inGenexpLambda);
      }

      case PNK_CLASSFIELD: {
        ClassField* node = &pn->as<ClassField>();
        if (node->initializer()) {
            if (!Fold(cx, node->unsafeRightReference(), parser, inGenexpLambda)) {
                return false;
            }
        }
        return true;
      }

      case PNK_NEWTARGET:
      case PNK_IMPORT_META:{
#ifdef DEBUG
        BinaryNode* node = &pn->as<BinaryNode>();
        MOZ_ASSERT(node->left()->isKind(PNK_POSHOLDER));
        MOZ_ASSERT(node->right()->isKind(PNK_POSHOLDER));
#endif
        return true;
      }

      case PNK_CALL_IMPORT: {
        BinaryNode* node = &pn->as<BinaryNode>();
        MOZ_ASSERT(node->left()->isKind(PNK_POSHOLDER));
        return Fold(cx, node->unsafeRightReference(), parser, inGenexpLambda);
      }

      case PNK_CLASSNAMES: {
        ClassNames* names = &pn->as<ClassNames>();
        if (names->outerBinding()) {
            if (!Fold(cx, names->unsafeLeftReference(), parser, inGenexpLambda)) {
                return false;
            }
        }
        return Fold(cx, names->unsafeRightReference(), parser, inGenexpLambda);
      }

      case PNK_DOWHILE: {
        BinaryNode* node = &pn->as<BinaryNode>();
        return Fold(cx, node->unsafeLeftReference(), parser, inGenexpLambda) &&
               FoldCondition(cx, node->unsafeRightReference(), parser, inGenexpLambda);
      }

      case PNK_WHILE: {
        BinaryNode* node = &pn->as<BinaryNode>();
        return FoldCondition(cx, node->unsafeLeftReference(), parser, inGenexpLambda) &&
               Fold(cx, node->unsafeRightReference(), parser, inGenexpLambda);
      }

      case PNK_CASE: {
        CaseClause* caseClause = &pn->as<CaseClause>();

        // left (caseExpression) is null for DefaultClauses.
        if (caseClause->left()) {
            if (!Fold(cx, caseClause->unsafeLeftReference(), parser, inGenexpLambda))
                return false;
        }
        return Fold(cx, caseClause->unsafeRightReference(), parser, inGenexpLambda);
      }

      case PNK_WITH: {
        BinaryNode* node = &pn->as<BinaryNode>();
        return Fold(cx, node->unsafeLeftReference(), parser, inGenexpLambda) &&
               Fold(cx, node->unsafeRightReference(), parser, inGenexpLambda);
      }

      case PNK_FORIN:
      case PNK_FOROF:
        return FoldForInOrOf(cx, &pn->as<TernaryNode>(), parser, inGenexpLambda);

      case PNK_FORHEAD:
        return FoldForHead(cx, &pn->as<TernaryNode>(), parser, inGenexpLambda);

      case PNK_LABEL:
        return Fold(cx, pn->as<LabeledStatement>().unsafeStatementReference(), parser, inGenexpLambda);

      case PNK_PROPERTYNAME:
        MOZ_CRASH("unreachable, handled by ::Dot");

      case PNK_OPTDOT:
      case PNK_DOT:
        return FoldDottedProperty(cx, &pn->as<PropertyAccessBase>(), parser, inGenexpLambda);

      case PNK_LEXICALSCOPE: {
        LexicalScopeNode* node = &pn->as<LexicalScopeNode>();
        if (!node->scopeBody())
            return true;
        return Fold(cx, node->unsafeScopeBodyReference(), parser, inGenexpLambda);
      }

      case PNK_NAME:
        return FoldName(cx, &pn->as<NameNode>(), parser, inGenexpLambda);

      case PNK_LIMIT: // invalid sentinel value
        MOZ_CRASH("invalid node kind");
    }

    MOZ_CRASH("shouldn't reach here");
    return false;
}

bool
frontend::FoldConstants(ExclusiveContext* cx, ParseNode** pnp, Parser<FullParseHandler>* parser)
{
    // Don't constant-fold inside "use asm" code, as this could create a parse
    // tree that doesn't type-check as asm.js.
    if (parser->pc->useAsmOrInsideUseAsm())
        return true;

    return Fold(cx, pnp, *parser, false);
}
