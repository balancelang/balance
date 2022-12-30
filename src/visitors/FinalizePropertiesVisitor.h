#ifndef FINALIZE_PROPERTIES_VISITOR_H
#define FINALIZE_PROPERTIES_VISITOR_H

#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "BalanceParserBaseVisitor.h"

#include "LibLsp/lsp/textDocument/SemanticTokens.h"

#include "llvm/IR/BasicBlock.h"

using namespace antlrcpptest;
using namespace llvm;
using namespace std;

struct FinalizePropertiesVisitorException : public std::exception
{
};

class FinalizePropertiesVisitor : public BalanceParserBaseVisitor {
public:
    std::any visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) override;
};

#endif