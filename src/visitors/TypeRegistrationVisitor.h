#ifndef TYPE_REGISTRATION_VISITOR_H
#define TYPE_REGISTRATION_VISITOR_H

#include "BalanceParserBaseVisitor.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"
#include <exception>

#include "llvm/IR/BasicBlock.h"

using namespace antlrcpptest;
using namespace llvm;
using namespace std;



struct TypeRegistrationVisitorException : public std::exception
{
};

class TypeRegistrationVisitor : public BalanceParserBaseVisitor
{
public:
    std::any visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) override;
    std::any visitInterfaceDefinition(BalanceParser::InterfaceDefinitionContext *ctx) override;
};

#endif