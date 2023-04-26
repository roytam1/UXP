/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/ParseNode-inl.h"

#include "frontend/Parser.h"

#include "jscntxtinlines.h"

using namespace js;
using namespace js::frontend;

using mozilla::ArrayLength;
using mozilla::IsFinite;

#ifdef DEBUG
void
ListNode::checkConsistency() const
{
    ParseNode* const* tailNode;
    uint32_t actualCount = 0;
    if (const ParseNode* last = head()) {
        const ParseNode* pn = last;
        while (pn) {
            last = pn;
            pn = pn->pn_next;
            actualCount++;
        }

        tailNode = &last->pn_next;
    } else {
        tailNode = &pn_u.list.head;
    }
    MOZ_ASSERT(tail() == tailNode);
    MOZ_ASSERT(count() == actualCount);
}
#endif

/* Add |node| to |parser|'s free node list. */
void
ParseNodeAllocator::freeNode(ParseNode* pn)
{
    /* Catch back-to-back dup recycles. */
    MOZ_ASSERT(pn != freelist);

#ifdef DEBUG
    /* Poison the node, to catch attempts to use it without initializing it. */
    memset(pn, 0xab, sizeof(*pn));
#endif

    pn->pn_next = freelist;
    freelist = pn;
}

namespace {

/*
 * A work pool of ParseNodes. The work pool is a stack, chained together
 * by nodes' pn_next fields. We use this to avoid creating deep C++ stacks
 * when recycling deep parse trees.
 *
 * Since parse nodes are probably allocated in something close to the order
 * they appear in a depth-first traversal of the tree, making the work pool
 * a stack should give us pretty good locality.
 */
class NodeStack {
  public:
    NodeStack() : top(nullptr) { }
    bool empty() { return top == nullptr; }
    void push(ParseNode* pn) {
        pn->pn_next = top;
        top = pn;
    }
    /* Push the children of the PN_LIST node |pn| on the stack. */
    void pushList(ListNode* pn) {
        /* This clobbers pn->pn_head if the list is empty; should be okay. */
        pn->unsafeSetTail(top);
        top = pn->head();
    }
    ParseNode* pop() {
        MOZ_ASSERT(!empty());
        ParseNode* hold = top; /* my kingdom for a prog1 */
        top = top->pn_next;
        return hold;
    }
  private:
    ParseNode* top;
};

} /* anonymous namespace */

enum class PushResult { Recyclable, CleanUpLater };

static PushResult
PushFunctionNodeChildren(FunctionNode* node, NodeStack* stack)
{
    /*
     * Function nodes are linked into the function box tree, and may appear
     * on method lists. Both of those lists are singly-linked, so trying to
     * update them now could result in quadratic behavior when recycling
     * trees containing many functions; and the lists can be very long. So
     * we put off cleaning the lists up until just before function
     * analysis, when we call CleanFunctionList.
     *
     * In fact, we can't recycle the parse node yet, either: it may appear
     * on a method list, and reusing the node would corrupt that. Instead,
     * we clear its funbox pointer to mark it as deleted;
     * CleanFunctionList recycles it as well.
     *
     * We do recycle the nodes around it, though, so we must clear pointers
     * to them to avoid leaving dangling references where someone can find
     * them.
     */
    node->setFunbox(nullptr);
    if (node->body())
        stack->push(node->body());
    node->setBody(nullptr);

    return PushResult::CleanUpLater;
}

static PushResult
PushModuleNodeChildren(ModuleNode* node, NodeStack* stack)
{
    stack->push(node->body());
    node->setBody(nullptr);

    return PushResult::CleanUpLater;
}

static PushResult
PushNameNodeChildren(NameNode* node, NodeStack* stack)
{
    if (node->initializer())
        stack->push(node->initializer());
    node->setInitializer(nullptr);
    return PushResult::Recyclable;
}

static PushResult
PushScopeNodeChildren(LexicalScopeNode* node, NodeStack* stack)
{
    if (node->scopeBody())
        stack->push(node->scopeBody());
    node->setScopeBody(nullptr);
    return PushResult::Recyclable;
}

static PushResult
PushListNodeChildren(ListNode* node, NodeStack* stack)
{
    node->checkConsistency();

    stack->pushList(node);

    return PushResult::Recyclable;
}

static PushResult
PushUnaryNodeChild(UnaryNode* node, NodeStack* stack)
{
    stack->push(node->kid());

    return PushResult::Recyclable;
}

/*
 * Push the children of |pn| on |stack|. Return true if |pn| itself could be
 * safely recycled, or false if it must be cleaned later (pn_used and pn_defn
 * nodes, and all function nodes; see comments for CleanFunctionList in
 * SemanticAnalysis.cpp). Some callers want to free |pn|; others
 * (js::ParseNodeAllocator::prepareNodeForMutation) don't care about |pn|, and
 * just need to take care of its children.
 */
static PushResult
PushNodeChildren(ParseNode* pn, NodeStack* stack)
{
    switch (pn->getKind()) {
      // Trivial nodes that refer to no nodes, are referred to by nothing
      // but their parents, are never used, and are never a definition.
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
        return PushResult::Recyclable;

      case PNK_DEBUGGER:
          MOZ_ASSERT(pn->is<DebuggerStatement>());
          return PushResult::Recyclable;

      case PNK_BREAK:
          MOZ_ASSERT(pn->is<BreakStatement>());
          return PushResult::Recyclable;

      case PNK_CONTINUE:
          MOZ_ASSERT(pn->is<ContinueStatement>());
          return PushResult::Recyclable;

      case PNK_OBJECT_PROPERTY_NAME:
      case PNK_STRING:
      case PNK_TEMPLATE_STRING:
        MOZ_ASSERT(pn->is<NameNode>());
        return PushResult::Recyclable;

      case PNK_REGEXP:
        MOZ_ASSERT(pn->is<RegExpLiteral>());
        return PushResult::Recyclable;

      case PNK_NUMBER:
        MOZ_ASSERT(pn->is<NumericLiteral>());
        return PushResult::Recyclable;

      // Nodes with a single non-null child.
      case PNK_TYPEOFNAME:
      case PNK_TYPEOFEXPR:
      case PNK_VOID:
      case PNK_NOT:
      case PNK_BITNOT:
      case PNK_THROW:
      case PNK_DELETENAME:
      case PNK_DELETEPROP:
      case PNK_DELETEELEM:
      case PNK_DELETEEXPR:
      case PNK_POS:
      case PNK_NEG:
      case PNK_PREINCREMENT:
      case PNK_POSTINCREMENT:
      case PNK_PREDECREMENT:
      case PNK_POSTDECREMENT:
      case PNK_COMPUTED_NAME:
      case PNK_STATICCLASSBLOCK:
      case PNK_ARRAYPUSH:
      case PNK_SPREAD:
      case PNK_MUTATEPROTO:
      case PNK_EXPORT:
      case PNK_SUPERBASE:
        return PushUnaryNodeChild(&pn->as<UnaryNode>(), stack);

      // Nodes with a single nullable child.
      case PNK_OPTCHAIN:
      case PNK_DELETEOPTCHAIN:
      case PNK_THIS:
      case PNK_SEMI: {
        UnaryNode* un = &pn->as<UnaryNode>();
        if (un->kid())
            stack->push(un->kid());
        return PushResult::Recyclable;
      }

      // Binary nodes with two non-null children.

      // All assignment and compound assignment nodes qualify.
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
      // ...and a few others.
      case PNK_OPTELEM:
      case PNK_ELEM:
      case PNK_IMPORT_SPEC:
      case PNK_EXPORT_SPEC:
      case PNK_COLON:
      case PNK_SHORTHAND:
      case PNK_DOWHILE:
      case PNK_WHILE:
      case PNK_SWITCH:
      case PNK_NEW:
      case PNK_OPTDOT:
      case PNK_DOT:
      case PNK_OPTCALL:
      case PNK_CALL:
      case PNK_SUPERCALL:
      case PNK_TAGGED_TEMPLATE:
      case PNK_GENEXP:
      case PNK_CLASSMETHOD:
      case PNK_NEWTARGET:
      case PNK_SETTHIS:
      case PNK_FOR:
      case PNK_COMPREHENSIONFOR:
      case PNK_IMPORT_META:
      case PNK_CALL_IMPORT:
      case PNK_WITH: {
        BinaryNode* bn = &pn->as<BinaryNode>();
        stack->push(bn->left());
        stack->push(bn->right());
        return PushResult::Recyclable;
      }

      // Default clauses are PNK_CASE but do not have case expressions.
      // Named class expressions do not have outer binding nodes.
      // So both are binary nodes with a possibly-null pn_left.
      case PNK_CASE:
      case PNK_CLASSNAMES: {
        BinaryNode* bn = &pn->as<BinaryNode>();
        if (bn->left())
            stack->push(bn->left());
        stack->push(bn->right());
        return PushResult::Recyclable;
      }

      // The child is an assignment of a PNK_GENERATOR node to the
      // '.generator' local, for a synthesized, prepended initial yield.
      case PNK_INITIALYIELD: {
        UnaryNode* un = &pn->as<UnaryNode>();
#ifdef DEBUG
        MOZ_ASSERT(un->kid()->isKind(PNK_ASSIGN));
        BinaryNode* bn = &un->kid()->as<BinaryNode>();
        MOZ_ASSERT(bn->left()->isKind(PNK_NAME) &&
                   bn->right()->isKind(PNK_GENERATOR));
#endif
        stack->push(un->kid());
        return PushResult::Recyclable;
      }

      // The child is the expression being yielded.
      case PNK_YIELD_STAR:
      case PNK_YIELD:
      case PNK_AWAIT: {
        UnaryNode* un = &pn->as<UnaryNode>();
        if (un->kid())
            stack->push(un->kid());
        return PushResult::Recyclable;
      }

      // A return node's child is what you'd expect: the return expression,
      // if any.
      case PNK_RETURN: {
        UnaryNode* un = &pn->as<UnaryNode>();
        if (un->kid())
            stack->push(un->kid());
        return PushResult::Recyclable;
      }

      // Import and export-from nodes have a list of specifiers on the left
      // and a module string on the right.
      case PNK_IMPORT:
      case PNK_EXPORT_FROM: {
        BinaryNode* bn = &pn->as<BinaryNode>();
        MOZ_ASSERT_IF(pn->isKind(PNK_IMPORT), bn->left()->isKind(PNK_IMPORT_SPEC_LIST));
        MOZ_ASSERT_IF(pn->isKind(PNK_EXPORT_FROM), bn->left()->isKind(PNK_EXPORT_SPEC_LIST));
        MOZ_ASSERT(bn->left()->isArity(PN_LIST));
        MOZ_ASSERT(bn->right()->isKind(PNK_STRING));
        stack->pushList(&bn->left()->as<ListNode>());
        stack->push(bn->right());
        return PushResult::Recyclable;
      }

      case PNK_EXPORT_DEFAULT: {
        BinaryNode* bn = &pn->as<BinaryNode>();
        MOZ_ASSERT_IF(bn->right(), bn->right()->isKind(PNK_NAME));
        stack->push(bn->left());
        if (bn->right())
            stack->push(bn->right());
        return PushResult::Recyclable;
      }

      case PNK_CLASSFIELD: {
          BinaryNode* bn = &pn->as<BinaryNode>();
          stack->push(bn->left());
          if (bn->right())
              stack->push(bn->right());
          return PushResult::Recyclable;
      }

      // Ternary nodes with all children non-null.
      case PNK_CONDITIONAL: {
        TernaryNode* tn = &pn->as<TernaryNode>();
        stack->push(tn->kid1());
        stack->push(tn->kid2());
        stack->push(tn->kid3());
        return PushResult::Recyclable;
      }

      // For for-in and for-of, the first child is the left-hand side of the
      // 'in' or 'of' (a declaration or an assignment target). The second
      // child is always null, and the third child is the expression looped
      // over.  For example, in |for (var p in obj)|, the first child is |var
      // p|, the second child is null, and the third child is |obj|.
      case PNK_FORIN:
      case PNK_FOROF: {
        TernaryNode* tn = &pn->as<TernaryNode>();
        MOZ_ASSERT(!tn->kid2());
        stack->push(tn->kid1());
        stack->push(tn->kid3());
        return PushResult::Recyclable;
      }

      // for (;;) nodes have one child per optional component of the loop head.
      case PNK_FORHEAD: {
        TernaryNode* tn = &pn->as<TernaryNode>();
        if (tn->kid1())
            stack->push(tn->kid1());
        if (tn->kid2())
            stack->push(tn->kid2());
        if (tn->kid3())
            stack->push(tn->kid3());
        return PushResult::Recyclable;
      }

      // classes might have an optional node for the heritage, as well as the names
      case PNK_CLASS: {
        TernaryNode* tn = &pn->as<TernaryNode>();
        if (tn->kid1())
            stack->push(tn->kid1());
        if (tn->kid2())
            stack->push(tn->kid2());
        stack->push(tn->kid3());
        return PushResult::Recyclable;
      }

      // if-statement nodes have condition and consequent children and a
      // possibly-null alternative.
      case PNK_IF: {
        TernaryNode* tn = &pn->as<TernaryNode>();
        stack->push(tn->kid1());
        stack->push(tn->kid2());
        if (tn->kid3())
            stack->push(tn->kid3());
        return PushResult::Recyclable;
      }

      // try-statements have statements to execute, and one or both of a
      // catch-list and a finally-block.
      case PNK_TRY: {
        TernaryNode* tn = &pn->as<TernaryNode>();
        MOZ_ASSERT(tn->kid2() || tn->kid3());
        stack->push(tn->kid1());
        if (tn->kid2())
            stack->push(tn->kid2());
        if (tn->kid3())
            stack->push(tn->kid3());
        return PushResult::Recyclable;
      }

      // A catch node has an (optional) first kid as catch-variable pattern,
      // the second kid as (optional) catch condition (which, records the
      // |<cond>| in SpiderMonkey's |catch (e if <cond>)| extension), and
      // third kid as the statements in the catch block.
      case PNK_CATCH: {
        TernaryNode* tn = &pn->as<TernaryNode>();
        if (tn->kid1())
            stack->push(tn->kid1());
        if (tn->kid2())
            stack->push(tn->kid2());
        stack->push(tn->kid3());
        return PushResult::Recyclable;
      }

      // List nodes with all non-null children.
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
      case PNK_COMMA:
      case PNK_ARRAY:
      case PNK_OBJECT:
      case PNK_TEMPLATE_STRING_LIST:
      case PNK_CALLSITEOBJ:
      case PNK_VAR:
      case PNK_CONST:
      case PNK_LET:
      case PNK_ARGUMENTS:
      case PNK_CATCHLIST:
      case PNK_STATEMENTLIST:
      case PNK_IMPORT_SPEC_LIST:
      case PNK_EXPORT_SPEC_LIST:
      case PNK_PARAMSBODY:
      case PNK_CLASSMEMBERLIST:
        return PushListNodeChildren(&pn->as<ListNode>(), stack);

      // Array comprehension nodes are lists with a single child:
      // PNK_COMPREHENSIONFOR for comprehensions, PNK_LEXICALSCOPE for legacy
      // comprehensions.  Probably this should be a non-list eventually.
      case PNK_ARRAYCOMP: {
        ListNode* literal = &pn->as<ListNode>();
        MOZ_ASSERT(literal->count() == 1);
        MOZ_ASSERT(literal->head()->isKind(PNK_LEXICALSCOPE) ||
                   literal->head()->isKind(PNK_COMPREHENSIONFOR));
        return PushListNodeChildren(literal, stack);
      }

      case PNK_LABEL:
      case PNK_NAME:
      case PNK_PROPERTYNAME:
        return PushNameNodeChildren(&pn->as<NameNode>(), stack);

      case PNK_LEXICALSCOPE:
        return PushScopeNodeChildren(&pn->as<LexicalScopeNode>(), stack);

      case PNK_FUNCTION:
        return PushFunctionNodeChildren(&pn->as<FunctionNode>(), stack);
      case PNK_MODULE:
        return PushModuleNodeChildren(&pn->as<ModuleNode>(), stack);

      case PNK_LIMIT: // invalid sentinel value
        MOZ_CRASH("invalid node kind");
    }

    MOZ_CRASH("bad ParseNodeKind");
    return PushResult::CleanUpLater;
}

/*
 * Prepare |pn| to be mutated in place into a new kind of node. Recycle all
 * |pn|'s recyclable children (but not |pn| itself!), and disconnect it from
 * metadata structures (the function box tree).
 */
void
ParseNodeAllocator::prepareNodeForMutation(ParseNode* pn)
{
    // Nothing to do for nullary nodes.
    if (pn->isArity(PN_NULLARY))
        return;

    // Put |pn|'s children (but not |pn| itself) on a work stack.
    NodeStack stack;
    PushNodeChildren(pn, &stack);

    // For each node on the work stack, push its children on the work stack,
    // and free the node if we can.
    while (!stack.empty()) {
        pn = stack.pop();
        if (PushNodeChildren(pn, &stack) == PushResult::Recyclable)
            freeNode(pn);
    }
}

/*
 * Return the nodes in the subtree |pn| to the parser's free node list, for
 * reallocation.
 */
ParseNode*
ParseNodeAllocator::freeTree(ParseNode* pn)
{
    if (!pn)
        return nullptr;

    ParseNode* savedNext = pn->pn_next;

    NodeStack stack;
    for (;;) {
        if (PushNodeChildren(pn, &stack) == PushResult::Recyclable)
            freeNode(pn);
        if (stack.empty())
            break;
        pn = stack.pop();
    }

    return savedNext;
}

/*
 * Allocate a ParseNode from parser's node freelist or, failing that, from
 * cx's temporary arena.
 */
void*
ParseNodeAllocator::allocNode()
{
    if (ParseNode* pn = freelist) {
        freelist = pn->pn_next;
        return pn;
    }

    LifoAlloc::AutoFallibleScope fallibleAllocator(&alloc);
    void* p = alloc.alloc(sizeof (ParseNode));
    if (!p)
        ReportOutOfMemory(cx);
    return p;
}

ParseNode*
ParseNode::appendOrCreateList(ParseNodeKind kind, JSOp op, ParseNode* left, ParseNode* right,
                              FullParseHandler* handler, ParseContext* pc)
{
    // The asm.js specification is written in ECMAScript grammar terms that
    // specify *only* a binary tree.  It's a royal pain to implement the asm.js
    // spec to act upon n-ary lists as created below.  So for asm.js, form a
    // binary tree of lists exactly as ECMAScript would by skipping the
    // following optimization.
    if (!pc->useAsmOrInsideUseAsm()) {
        // Left-associative trees of a given operator (e.g. |a + b + c|) are
        // binary trees in the spec: (+ (+ a b) c) in Lisp terms.  Recursively
        // processing such a tree, exactly implemented that way, would blow the
        // the stack.  We use a list node that uses O(1) stack to represent
        // such operations: (+ a b c).
        //
        // (**) is right-associative; per spec |a ** b ** c| parses as
        // (** a (** b c)). But we treat this the same way, creating a list
        // node: (** a b c). All consumers must understand that this must be
        // processed with a right fold, whereas the list (+ a b c) must be
        // processed with a left fold because (+) is left-associative.
        //
        if (left->isKind(kind) &&
            left->isOp(op) &&
            (CodeSpec[op].format & JOF_LEFTASSOC ||
             (kind == PNK_POW && !left->pn_parens)))
        {
            ListNode* list = &left->as<ListNode>();

            list->append(right);
            list->pn_pos.end = right->pn_pos.end;

            return list;
        }
    }

    ListNode* list = handler->new_<ListNode>(kind, op, left);
    if (!list)
        return nullptr;

    list->append(right);
    return list;
}

#ifdef DEBUG

static const char * const parseNodeNames[] = {
#define STRINGIFY(name) #name,
    FOR_EACH_PARSE_NODE_KIND(STRINGIFY)
#undef STRINGIFY
};

void
frontend::DumpParseTree(ParseNode* pn, int indent)
{
    if (pn == nullptr)
        fprintf(stderr, "#NULL");
    else
        pn->dump(indent);
}

static void
IndentNewLine(int indent)
{
    fputc('\n', stderr);
    for (int i = 0; i < indent; ++i)
        fputc(' ', stderr);
}

void
ParseNode::dump()
{
    dump(0);
    fputc('\n', stderr);
}

void
ParseNode::dump(int indent)
{
    switch (pn_arity) {
      case PN_NULLARY:
        as<NullaryNode>().dump();
        return;
      case PN_UNARY:
        as<UnaryNode>().dump(indent);
        return;
      case PN_BINARY:
        as<BinaryNode>().dump(indent);
        return;
      case PN_TERNARY:
        as<TernaryNode>().dump(indent);
        return;
      case PN_FUNCTION:
        as<FunctionNode>().dump(indent);
        return;
      case PN_MODULE:
        as<ModuleNode>().dump(indent);
      case PN_LIST:
        as<ListNode>().dump(indent);
        return;
      case PN_NAME:
        as<NameNode>().dump(indent);
        return;
      case PN_NUMBER:
        as<NumericLiteral>().dump(indent);
        return;
      case PN_REGEXP:
        as<RegExpLiteral>().dump(indent);
        return;
      case PN_LOOP:
        as<LoopControlStatement>().dump(indent);
        return;
      case PN_SCOPE:
        as<LexicalScopeNode>().dump(indent);
        return;
    }
    fprintf(stderr, "#<BAD NODE %p, kind=%u, arity=%u>",
            (void*) this, unsigned(getKind()), unsigned(pn_arity));
}

void
NullaryNode::dump()
{
    switch (getKind()) {
      case PNK_TRUE:  fprintf(stderr, "#true");  break;
      case PNK_FALSE: fprintf(stderr, "#false"); break;
      case PNK_NULL:  fprintf(stderr, "#null");  break;
      case PNK_RAW_UNDEFINED: fprintf(stderr, "#undefined"); break;

      default:
        fprintf(stderr, "(%s)", parseNodeNames[getKind()]);
    }
}

void
NumericLiteral::dump(int indent)
{
    ToCStringBuf cbuf;
    const char* cstr = NumberToCString(nullptr, &cbuf, value());
    if (!IsFinite(value())) {
        fprintf(stderr, "#");
    }
    if (cstr) {
        fprintf(stderr, "%s", cstr);
    } else {
        fprintf(stderr, "%g", value());
    }
}

void
RegExpLiteral::dump(int indent)
{
    fprintf(stderr, "(%s)", parseNodeNames[size_t(getKind())]);
}

void
LoopControlStatement::dump(int indent)
{
    const char* name = parseNodeNames[size_t(getKind())];
    fprintf(stderr, "(%s", name);
    if (label()) {
        fprintf(stderr, " ");
        label()->dumpCharsNoNewline();
    }
    fprintf(stderr, ")");
}

void
UnaryNode::dump(int indent)
{
    const char* name = parseNodeNames[getKind()];
    fprintf(stderr, "(%s ", name);
    indent += strlen(name) + 2;
    DumpParseTree(kid(), indent);
    fprintf(stderr, ")");
}

void
BinaryNode::dump(int indent)
{
    if (isKind(PNK_DOT)) {
        fprintf(stderr, "(.");

        DumpParseTree(right(), indent + 2);

        fprintf(stderr, " ");
        if (as<PropertyAccess>().isSuper())
            fprintf(stderr, "super");
        else
            DumpParseTree(left(), indent + 2);

        fprintf(stderr, ")");
        return;
    }

    const char* name = parseNodeNames[getKind()];
    fprintf(stderr, "(%s ", name);
    indent += strlen(name) + 2;
    DumpParseTree(left(), indent);
    IndentNewLine(indent);
    DumpParseTree(right(), indent);
    fprintf(stderr, ")");
}

void
TernaryNode::dump(int indent)
{
    const char* name = parseNodeNames[getKind()];
    fprintf(stderr, "(%s ", name);
    indent += strlen(name) + 2;
    DumpParseTree(kid1(), indent);
    IndentNewLine(indent);
    DumpParseTree(kid2(), indent);
    IndentNewLine(indent);
    DumpParseTree(kid3(), indent);
    fprintf(stderr, ")");
}

void
FunctionNode::dump(int indent)
{
    const char* name = parseNodeNames[getKind()];
    fprintf(stderr, "(%s ", name);
    indent += strlen(name) + 2;
    DumpParseTree(body(), indent);
    fprintf(stderr, ")");
}

void
ModuleNode::dump(int indent)
{
    const char* name = parseNodeNames[getKind()];
    fprintf(stderr, "(%s ", name);
    indent += strlen(name) + 2;
    DumpParseTree(body(), indent);
    fprintf(stderr, ")");
}

void
ListNode::dump(int indent)
{
    const char* name = parseNodeNames[getKind()];
    fprintf(stderr, "(%s [", name);
    if (ParseNode* listHead = head()) {
        indent += strlen(name) + 3;
        DumpParseTree(listHead, indent);
        for (ParseNode* item : contentsFrom(listHead->pn_next)) {
            IndentNewLine(indent);
            DumpParseTree(item, indent);
        }
    }
    fprintf(stderr, "])");
}

template <typename CharT>
static void
DumpName(const CharT* s, size_t len)
{
    if (len == 0)
        fprintf(stderr, "#<zero-length name>");

    for (size_t i = 0; i < len; i++) {
        char16_t c = s[i];
        if (c > 32 && c < 127)
            fputc(c, stderr);
        else if (c <= 255)
            fprintf(stderr, "\\x%02x", unsigned(c));
        else
            fprintf(stderr, "\\u%04x", unsigned(c));
    }
}

void
NameNode::dump(int indent)
{
    switch (getKind()) {
      case PNK_STRING:
      case PNK_TEMPLATE_STRING:
      case PNK_OBJECT_PROPERTY_NAME: {
        atom()->dumpCharsNoNewline();
        return;
      }
      
      case PNK_NAME:
      case PNK_PRIVATE_NAME: // atom() already includes the '#', no need to specially include it.
      case PNK_PROPERTYNAME: {
        if (!atom()) {
            fprintf(stderr, "#<null name>");
        } else if (getOp() == JSOP_GETARG && atom()->length() == 0) {
            // Dump destructuring parameter.
            static const char ZeroLengthPrefix[] = "(#<zero-length name> ";
            fprintf(stderr, ZeroLengthPrefix);
            DumpParseTree(initializer(), indent + strlen(ZeroLengthPrefix));
            fputc(')', stderr);
        } else {
            JS::AutoCheckCannotGC nogc;
            if (atom()->hasLatin1Chars())
                DumpName(atom()->latin1Chars(nogc), atom()->length());
            else
                DumpName(atom()->twoByteChars(nogc), atom()->length());
        }
        return;
      }

      case PNK_LABEL: {
        const char* name = parseNodeNames[getKind()];
        fprintf(stderr, "(%s ", name);
        atom()->dumpCharsNoNewline();
        indent += strlen(name) + atom()->length() + 2;
        DumpParseTree(initializer(), indent);
        fprintf(stderr, ")");
        return;
      }

      default: {
        const char* name = parseNodeNames[getKind()];
        fprintf(stderr, "(%s ", name);
        indent += strlen(name) + 2;
        DumpParseTree(initializer(), indent);
        fprintf(stderr, ")");
        return;
      }
    }
}

void
LexicalScopeNode::dump(int indent)
{
    const char* name = parseNodeNames[getKind()];
    fprintf(stderr, "(%s [", name);
    int nameIndent = indent + strlen(name) + 3;
    if (!isEmptyScope()) {
        LexicalScope::Data* bindings = scopeBindings();
        for (uint32_t i = 0; i < bindings->length; i++) {
            JSAtom* name = bindings->trailingNames[i].name();
            JS::AutoCheckCannotGC nogc;
            if (name->hasLatin1Chars())
                DumpName(name->latin1Chars(nogc), name->length());
            else
                DumpName(name->twoByteChars(nogc), name->length());
            if (i < bindings->length - 1)
                IndentNewLine(nameIndent);
        }
    }
    fprintf(stderr, "]");
    indent += 2;
    IndentNewLine(indent);
    DumpParseTree(scopeBody(), indent);
    fprintf(stderr, ")");
}
#endif

ObjectBox::ObjectBox(JSObject* object, ObjectBox* traceLink)
  : object(object),
    traceLink(traceLink),
    emitLink(nullptr)
{
    MOZ_ASSERT(!object->is<JSFunction>());
    MOZ_ASSERT(object->isTenured());
}

ObjectBox::ObjectBox(JSFunction* function, ObjectBox* traceLink)
  : object(function),
    traceLink(traceLink),
    emitLink(nullptr)
{
    MOZ_ASSERT(object->is<JSFunction>());
    MOZ_ASSERT(asFunctionBox()->function() == function);
    MOZ_ASSERT(object->isTenured());
}

FunctionBox*
ObjectBox::asFunctionBox()
{
    MOZ_ASSERT(isFunctionBox());
    return static_cast<FunctionBox*>(this);
}

/* static */ void
ObjectBox::TraceList(JSTracer* trc, ObjectBox* listHead)
{
    for (ObjectBox* box = listHead; box; box = box->traceLink)
        box->trace(trc);
}

void
ObjectBox::trace(JSTracer* trc)
{
    TraceRoot(trc, &object, "parser.object");
}

void
FunctionBox::trace(JSTracer* trc)
{
    ObjectBox::trace(trc);
    if (enclosingScope_)
        TraceRoot(trc, &enclosingScope_, "funbox-enclosingScope");
}

bool
js::frontend::IsAnonymousFunctionDefinition(ParseNode* pn)
{
    // ES 2017 draft
    // 12.15.2 (ArrowFunction, AsyncArrowFunction).
    // 14.1.12 (FunctionExpression).
    // 14.4.8 (GeneratorExpression).
    // 14.6.8 (AsyncFunctionExpression)
    if (pn->is<FunctionNode>() &&
        !pn->as<FunctionNode>().funbox()->function()->explicitName())
        return true;

    // 14.5.8 (ClassExpression)
    if (pn->is<ClassNode>() && !pn->as<ClassNode>().names())
        return true;

    return false;
}
