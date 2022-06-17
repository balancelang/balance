#ifndef VISITOR_H
#define VISITOR_H

#include "BalanceParserBaseVisitor.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"

#include "llvm/IR/BasicBlock.h"

using namespace antlrcpptest;
using namespace llvm;
using namespace std;

Value *anyToValue(any anyVal);
Value *getValue(string variableName);
void setValue(string variableName, Value *value);
Type * getBuiltinType(string typeString);
Constant *geti8StrVal(Module &M, char const *str, Twine const &name);
void LogError(string errorMessage);

class BalanceVisitor : public BalanceParserBaseVisitor
{
public:
    any visitWhileStatement(BalanceParser::WhileStatementContext *ctx) override;
    any visitMemberAssignment(BalanceParser::MemberAssignmentContext *ctx) override;
    any visitMemberIndexExpression(BalanceParser::MemberIndexExpressionContext *ctx) override;
    any visitArrayLiteral(BalanceParser::ArrayLiteralContext *ctx) override;
    any visitIfStatement(BalanceParser::IfStatementContext *ctx) override;
    any visitArgument(BalanceParser::ArgumentContext *ctx) override;
    any visitVariableExpression(BalanceParser::VariableExpressionContext *ctx) override;
    any visitNewAssignment(BalanceParser::NewAssignmentContext *ctx) override;
    any visitExistingAssignment(BalanceParser::ExistingAssignmentContext *ctx) override;
    any visitRelationalExpression(BalanceParser::RelationalExpressionContext *ctx) override;
    any visitAdditiveExpression(BalanceParser::AdditiveExpressionContext *ctx) override;
    any visitNumericLiteral(BalanceParser::NumericLiteralContext *ctx) override;
    any visitBooleanLiteral(BalanceParser::BooleanLiteralContext *ctx) override;
    any visitDoubleLiteral(BalanceParser::DoubleLiteralContext *ctx) override;
    any visitStringLiteral(BalanceParser::StringLiteralContext *ctx) override;
    any visitFunctionCall(BalanceParser::FunctionCallContext *ctx) override;
    any visitReturnStatement(BalanceParser::ReturnStatementContext *ctx) override;
    any visitFunctionDefinition(BalanceParser::FunctionDefinitionContext *ctx) override;
    any visitLambdaExpression(BalanceParser::LambdaExpressionContext *ctx) override;
    any visitMemberAccessExpression(BalanceParser::MemberAccessExpressionContext *ctx) override;
    any visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) override;
    any visitClassProperty(BalanceParser::ClassPropertyContext *ctx) override;
    any visitClassInitializerExpression(BalanceParser::ClassInitializerExpressionContext *ctx) override;
    any visitMultiplicativeExpression(BalanceParser::MultiplicativeExpressionContext *ctx) override;
};

class ScopeBlock
{
public:
    BasicBlock *block;
    ScopeBlock *parent;
    map<string, llvm::Value *> symbolTable;
    ScopeBlock(BasicBlock *block, ScopeBlock *parent)
    {
        this->block = block;
        this->parent = parent;
    }
};

#endif