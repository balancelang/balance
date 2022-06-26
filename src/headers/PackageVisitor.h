#ifndef PACKAGE_VISITOR_H
#define PACKAGE_VISITOR_H

#include "BalanceParserBaseVisitor.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"

#include "llvm/IR/BasicBlock.h"

using namespace antlrcpptest;
using namespace llvm;
using namespace std;


class PackageVisitor : public BalanceParserBaseVisitor
{
public:
    std::any visitImportStatement(BalanceParser::ImportStatementContext *ctx) override;
};

#endif