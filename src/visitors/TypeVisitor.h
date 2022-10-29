#ifndef TYPE_VISITOR_H
#define TYPE_VISITOR_H

#include "BalanceParserBaseVisitor.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"

using namespace antlrcpptest;
using namespace std;


class TypeVisitor : public BalanceParserBaseVisitor
{
public:
    std::string getText(antlr4::ParserRuleContext *ctx);
    std::any visitWhileStatement(BalanceParser::WhileStatementContext *ctx) override;
    std::any visitIfStatement(BalanceParser::IfStatementContext *ctx) override;
    std::any visitReturnStatement(BalanceParser::ReturnStatementContext *ctx) override;
    std::any visitNewAssignment(BalanceParser::NewAssignmentContext *ctx) override;
    std::any visitExistingAssignment(BalanceParser::ExistingAssignmentContext *ctx) override;
    // std::any visitMemberAssignment(BalanceParser::MemberAssignmentContext *ctx) override;
    std::any visitMemberAccessExpression(BalanceParser::MemberAccessExpressionContext *ctx) override;
    std::any visitAdditiveExpression(BalanceParser::AdditiveExpressionContext *ctx) override;
    std::any visitRelationalExpression(BalanceParser::RelationalExpressionContext *ctx) override;
    // std::any visitMemberIndexExpression(BalanceParser::MemberIndexExpressionContext *ctx) override;
    // std::any visitMultiplicativeExpression(BalanceParser::MultiplicativeExpressionContext *ctx) override;
    std::any visitVariable(BalanceParser::VariableContext *ctx) override;
    // std::any visitParameterList(BalanceParser::ParameterListContext *ctx) override;
    // std::any visitSimpleType(BalanceParser::SimpleTypeContext *ctx) override;
    std::any visitGenericType(BalanceParser::GenericTypeContext *ctx) override;
    // std::any visitTypeList(BalanceParser::TypeListContext *ctx) override;
    // std::any visitParameter(BalanceParser::ParameterContext *ctx) override;
    // std::any visitArgumentList(BalanceParser::ArgumentListContext *ctx) override;
    // std::any visitArgument(BalanceParser::ArgumentContext *ctx) override;
    std::any visitFunctionCall(BalanceParser::FunctionCallContext *ctx) override;
    std::any visitFunctionDefinition(BalanceParser::FunctionDefinitionContext *ctx) override;
    std::any visitClassInitializer(BalanceParser::ClassInitializerContext *ctx) override;
    std::any visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) override;
    std::any visitClassProperty(BalanceParser::ClassPropertyContext *ctx) override;
    std::any visitLambda(BalanceParser::LambdaContext *ctx) override;
    // std::any visitReturnType(BalanceParser::ReturnTypeContext *ctx) override;
    // std::any visitBlock(BalanceParser::BlockContext *ctx) override;
    // std::any visitFunctionBlock(BalanceParser::FunctionBlockContext *ctx) override;
    // std::any visitLiteral(BalanceParser::LiteralContext *ctx) override;
    std::any visitArrayLiteral(BalanceParser::ArrayLiteralContext *ctx) override;
    // std::any visitListElements(BalanceParser::ListElementsContext *ctx) override;
    std::any visitNumericLiteral(BalanceParser::NumericLiteralContext *ctx) override;
    std::any visitStringLiteral(BalanceParser::StringLiteralContext *ctx) override;
    std::any visitBooleanLiteral(BalanceParser::BooleanLiteralContext *ctx) override;
    std::any visitDoubleLiteral(BalanceParser::DoubleLiteralContext *ctx) override;
    std::any visitNoneLiteral(BalanceParser::NoneLiteralContext *ctx) override;
};

#endif