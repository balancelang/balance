#ifndef GENERIC_TYPE_REGISTRATION_VISITOR_H
#define GENERIC_TYPE_REGISTRATION_VISITOR_H

#include "BalanceParserBaseVisitor.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"
#include <exception>

#include "llvm/IR/BasicBlock.h"

using namespace antlrcpptest;
using namespace llvm;
using namespace std;



struct GenericTypeRegistrationVisitorException : public std::exception
{
};

class GenericTypeRegistrationVisitor : public BalanceParserBaseVisitor
{
public:
    std::any visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) override;
    std::any visitClassGenerics(BalanceParser::ClassGenericsContext *ctx) override;
    std::any visitGenericType(BalanceParser::GenericTypeContext *ctx) override;
    std::any visitSimpleType(BalanceParser::SimpleTypeContext *ctx) override;
};

#endif