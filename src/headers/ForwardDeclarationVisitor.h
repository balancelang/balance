#ifndef FORWARD_DECLARATION_VISITOR_H
#define FORWARD_DECLARATION_VISITOR_H

#include "BalanceParserBaseVisitor.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"

#include "llvm/IR/BasicBlock.h"

using namespace antlrcpptest;
using namespace llvm;
using namespace std;


class ForwardDeclarationVisitor : public BalanceParserBaseVisitor
{
public:
    std::any visitImportStatement(BalanceParser::ImportStatementContext *ctx) override;
};

#endif