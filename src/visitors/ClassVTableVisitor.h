#ifndef CLASS_V_TABLE_VISITOR_H
#define CLASS_V_TABLE_VISITOR_H

#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "BalanceParserBaseVisitor.h"

#include "LibLsp/lsp/textDocument/SemanticTokens.h"

#include "llvm/IR/BasicBlock.h"

using namespace antlrcpptest;
using namespace llvm;
using namespace std;

class ClassVTableVisitor : public BalanceParserBaseVisitor {
public:
    std::any visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) override;
};

#endif