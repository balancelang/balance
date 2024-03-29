#ifndef STRUCTURE_VISITOR_H
#define STRUCTURE_VISITOR_H

#include "BalanceParserBaseVisitor.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"
#include <exception>

#include "llvm/IR/BasicBlock.h"

using namespace antlrcpptest;
using namespace llvm;
using namespace std;



struct StructureVisitorException : public std::exception
{
};

class StructureVisitor : public BalanceParserBaseVisitor
{
public:
    std::any visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) override;
    std::any visitClassProperty(BalanceParser::ClassPropertyContext *ctx) override;
    std::any visitNoneType(BalanceParser::NoneTypeContext *ctx) override;
    std::any visitSimpleType(BalanceParser::SimpleTypeContext *ctx) override;
    std::any visitLambdaType(BalanceParser::LambdaTypeContext *ctx) override;
    std::any visitGenericType(BalanceParser::GenericTypeContext *ctx) override;
    std::any visitInterfaceDefinition(BalanceParser::InterfaceDefinitionContext *ctx) override;
    std::any visitFunctionSignature(BalanceParser::FunctionSignatureContext *ctx) override;
    std::any visitArrayLiteral(BalanceParser::ArrayLiteralContext *ctx) override;
    std::any visitNumericLiteral(BalanceParser::NumericLiteralContext *ctx) override;
    std::any visitStringLiteral(BalanceParser::StringLiteralContext *ctx) override;
    std::any visitBooleanLiteral(BalanceParser::BooleanLiteralContext *ctx) override;
    std::any visitDoubleLiteral(BalanceParser::DoubleLiteralContext *ctx) override;
    std::any visitNoneLiteral(BalanceParser::NoneLiteralContext *ctx) override;
    std::any visitLambdaExpression(BalanceParser::LambdaExpressionContext *ctx) override;
};

#endif