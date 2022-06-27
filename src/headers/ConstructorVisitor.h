#ifndef CONSTRUCTOR_VISITOR_H
#define CONSTRUCTOR_VISITOR_H

#include "BalanceParserBaseVisitor.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"

#include "llvm/IR/BasicBlock.h"

using namespace antlrcpptest;
using namespace llvm;
using namespace std;


class ConstructorVisitor : public BalanceParserBaseVisitor
{
public:
    std::any visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx);
};

#endif