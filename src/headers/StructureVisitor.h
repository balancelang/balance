#ifndef STRUCTURE_VISITOR_H
#define STRUCTURE_VISITOR_H

#include "BalanceParserBaseVisitor.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"

#include "llvm/IR/BasicBlock.h"

using namespace antlrcpptest;
using namespace llvm;
using namespace std;


class StructureVisitor : public BalanceParserBaseVisitor
{
public:
    std::any visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) override;
    std::any visitFunctionDefinition(BalanceParser::FunctionDefinitionContext *ctx) override;
    std::any visitClassProperty(BalanceParser::ClassPropertyContext *ctx) override;
};

#endif