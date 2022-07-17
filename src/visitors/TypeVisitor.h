#ifndef TYPE_VISITOR_H
#define TYPE_VISITOR_H

#include "BalanceParserBaseVisitor.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"

#include "llvm/IR/BasicBlock.h"

using namespace antlrcpptest;
using namespace llvm;
using namespace std;


class TypeVisitor : public BalanceParserBaseVisitor
{
public:
    std::any visitImportStatement(BalanceParser::ImportStatementContext *ctx) override;
    std::any visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) override;
    std::any visitFunctionDefinition(BalanceParser::FunctionDefinitionContext *ctx) override;
    std::any visitClassProperty(BalanceParser::ClassPropertyContext *ctx) override;
};

#endif