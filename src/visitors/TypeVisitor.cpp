#include "TypeVisitor.h"
#include "../Package.h"

using namespace antlr4;

extern BalancePackage *currentPackage;

std::string TypeVisitor::getText(antlr4::ParserRuleContext *ctx) {
    int a = ctx->start->getStartIndex();
    int b = ctx->stop->getStopIndex();
    antlr4::misc::Interval * interval = new antlr4::misc::Interval((size_t) a, (size_t) b);
    return currentPackage->currentModule->antlrStream->getText(*interval);
}

// std::any TypeVisitor::visitWhileStatement(BalanceParser::WhileStatementContext *ctx) { return std::any(); }
std::any TypeVisitor::visitIfStatement(BalanceParser::IfStatementContext *ctx) {
    std::string text = this->getText(ctx);
    BalanceScopeBlock *scope = currentPackage->currentModule->currentScope;

    BalanceTypeString * condition = any_cast<BalanceTypeString *>(visit(ctx->expression()));
    if (condition->base != "Bool") {
        currentPackage->currentModule->addTypeError(ctx, "If statement condition must be bool: " + this->getText(ctx->expression()));
    }

    currentPackage->currentModule->currentScope = new BalanceScopeBlock(nullptr, scope);
    visit(ctx->ifblock);

    currentPackage->currentModule->currentScope = new BalanceScopeBlock(nullptr, scope);
    if (ctx->elseblock) {
        visit(ctx->elseblock);
    }

    // Reset scope
    currentPackage->currentModule->currentScope = scope;
    return std::any();
}
// std::any TypeVisitor::visitIfBlock(BalanceParser::IfBlockContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitReturnStatement(BalanceParser::ReturnStatementContext *ctx) { return std::any(); }
std::any TypeVisitor::visitNewAssignment(BalanceParser::NewAssignmentContext *ctx) {
    std::string text = this->getText(ctx);
    std::string variableName = ctx->IDENTIFIER()->getText();

    BalanceTypeString *tryVal = currentPackage->currentModule->currentScope->typeSymbolTable[variableName];
    if (tryVal != nullptr) {
        currentPackage->currentModule->addTypeError(ctx, "Duplicate variable name: " + variableName);
        return std::any();
    }

    BalanceTypeString * value = any_cast<BalanceTypeString *>(visit(ctx->expression()));

    currentPackage->currentModule->currentScope->typeSymbolTable[variableName] = value;

    return std::any();
}
// std::any TypeVisitor::visitExistingAssignment(BalanceParser::ExistingAssignmentContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitMemberAssignment(BalanceParser::MemberAssignmentContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitMemberAccessExpression(BalanceParser::MemberAccessExpressionContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitAdditiveExpression(BalanceParser::AdditiveExpressionContext *ctx) { return std::any(); }
std::any TypeVisitor::visitRelationalExpression(BalanceParser::RelationalExpressionContext *ctx) {
    std::string text = this->getText(ctx);

    BalanceTypeString * lhs = any_cast<BalanceTypeString *>(visit(ctx->lhs));
    BalanceTypeString * rhs = any_cast<BalanceTypeString *>(visit(ctx->rhs));

    if (!lhs->equalTo(rhs)) {
        currentPackage->currentModule->addTypeError(ctx, "Types are not equal in relational expression: " + text + ", " + lhs->toString() + " != " + rhs->toString());
    }

    return new BalanceTypeString("Bool");
}
// std::any TypeVisitor::visitLiteralExpression(BalanceParser::LiteralExpressionContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitMemberIndexExpression(BalanceParser::MemberIndexExpressionContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitLambdaExpression(BalanceParser::LambdaExpressionContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitMultiplicativeExpression(BalanceParser::MultiplicativeExpressionContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitFunctionCallExpression(BalanceParser::FunctionCallExpressionContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitClassInitializerExpression(BalanceParser::ClassInitializerExpressionContext *ctx) { return std::any(); }
std::any TypeVisitor::visitVariable(BalanceParser::VariableContext *ctx) {
    std::string variableName = ctx->IDENTIFIER()->getText();
    BalanceTypeString *tryVal = currentPackage->currentModule->getTypeValue(variableName);
    if (tryVal == nullptr) {
        currentPackage->currentModule->addTypeError(ctx, "Unknown variable: " + variableName);
        return new BalanceTypeString("Unknown");
    }
    return tryVal;
}
// std::any TypeVisitor::visitParameterList(BalanceParser::ParameterListContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitSimpleType(BalanceParser::SimpleTypeContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitGenericType(BalanceParser::GenericTypeContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitTypeList(BalanceParser::TypeListContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitParameter(BalanceParser::ParameterContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitArgumentList(BalanceParser::ArgumentListContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitArgument(BalanceParser::ArgumentContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitFunctionCall(BalanceParser::FunctionCallContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitFunctionDefinition(BalanceParser::FunctionDefinitionContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitClassInitializer(BalanceParser::ClassInitializerContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitClassElement(BalanceParser::ClassElementContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitClassProperty(BalanceParser::ClassPropertyContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitLambda(BalanceParser::LambdaContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitReturnType(BalanceParser::ReturnTypeContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitBlock(BalanceParser::BlockContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitFunctionBlock(BalanceParser::FunctionBlockContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitLiteral(BalanceParser::LiteralContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitArrayLiteral(BalanceParser::ArrayLiteralContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitListElements(BalanceParser::ListElementsContext *ctx) { return std::any(); }

std::any TypeVisitor::visitNumericLiteral(BalanceParser::NumericLiteralContext *ctx) {
    return new BalanceTypeString("Int");
}

std::any TypeVisitor::visitStringLiteral(BalanceParser::StringLiteralContext *ctx) {
    return new BalanceTypeString("String");
}

std::any TypeVisitor::visitBooleanLiteral(BalanceParser::BooleanLiteralContext *ctx) {
    return new BalanceTypeString("Bool");
}

std::any TypeVisitor::visitDoubleLiteral(BalanceParser::DoubleLiteralContext *ctx) {
    return new BalanceTypeString("Double");
}
