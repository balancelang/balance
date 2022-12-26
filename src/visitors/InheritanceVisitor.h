#ifndef INHERITANCE_VISITOR_H
#define INHERITANCE_VISITOR_H

#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "BalanceParserBaseVisitor.h"

#include "LibLsp/lsp/textDocument/SemanticTokens.h"

#include "llvm/IR/BasicBlock.h"

using namespace antlrcpptest;
using namespace llvm;
using namespace std;

struct InheritanceVisitorException : public std::exception
{
};

class InheritanceVisitor : public BalanceParserBaseVisitor {
public:
    std::any visitClassExtendsImplements(BalanceParser::ClassExtendsImplementsContext *ctx) override;
    std::any visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) override;
};

#endif