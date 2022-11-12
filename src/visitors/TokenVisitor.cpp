#include "TokenVisitor.h"

void TokenVisitor::addToken(antlr4::Token * token, SemanticTokenType type) {
    unsigned int line = token->getLine() - 1;
    unsigned int columnStart = token->getCharPositionInLine();
    unsigned int length = token->getText().size();

    return this->addToken(line, columnStart, length, type);
}

void TokenVisitor::addToken(unsigned int line, unsigned int columnStart, unsigned int length, SemanticTokenType type) {
    unsigned int previousLine = 0;
    unsigned int previousStart = 0;
    unsigned int tokenModifiers = 0;

    if (!this->tokens.empty()) {
        previousLine = this->previousLine;
        if (line == previousLine) {
            previousStart = this->previousColumn;
        }
    }

    SemanticToken semanticToken = { line - previousLine, columnStart - previousStart, length, type, tokenModifiers };
    this->tokens.push_back(semanticToken);

    this->previousLine = line;
    this->previousColumn = columnStart;
}


std::any TokenVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) {
    std::string text = ctx->getText();

    // class keyword
    this->addToken(ctx->CLASS()->getSymbol(), SemanticTokenType::ls_keyword);

    // class name
    this->addToken(ctx->className, SemanticTokenType::ls_class);

    // class elements
    visitChildren(ctx);

    return std::any();
}

std::any TokenVisitor::visitClassInitializer(BalanceParser::ClassInitializerContext *ctx) {
    this->addToken(ctx->NEW()->getSymbol(), SemanticTokenType::ls_keyword);
    this->addToken(ctx->IDENTIFIER()->getSymbol(), SemanticTokenType::ls_class);

    visit(ctx->argumentList());
    return std::any();
}

std::any TokenVisitor::visitImportStatement(BalanceParser::ImportStatementContext *ctx) {
    this->addToken(ctx->FROM()->getSymbol(), SemanticTokenType::ls_keyword);
    this->addToken(ctx->IMPORT()->getSymbol(), SemanticTokenType::ls_keyword);
    return std::any();
}

std::any TokenVisitor::visitWhileStatement(BalanceParser::WhileStatementContext *ctx) {
    this->addToken(ctx->WHILE()->getSymbol(), SemanticTokenType::ls_keyword);
    visit(ctx->expression());
    visit(ctx->ifBlock());
    return std::any();
}

std::any TokenVisitor::visitIfStatement(BalanceParser::IfStatementContext *ctx) {
    this->addToken(ctx->IF()->getSymbol(), SemanticTokenType::ls_keyword);

    visit(ctx->expression());

    visit(ctx->ifblock);

    if (ctx->elseblock) {
        this->addToken(ctx->ELSE()->getSymbol(), SemanticTokenType::ls_keyword);
        visit(ctx->elseblock);
    }
    return std::any();
}

std::any TokenVisitor::visitReturnStatement(BalanceParser::ReturnStatementContext *ctx) {
    this->addToken(ctx->RETURN()->getSymbol(), SemanticTokenType::ls_keyword);
    if (ctx->expression()) {
        visit(ctx->expression());
    }
    return std::any();
}

std::any TokenVisitor::visitNewAssignment(BalanceParser::NewAssignmentContext *ctx) {
    this->addToken(ctx->VAR()->getSymbol(), SemanticTokenType::ls_keyword);
    this->addToken(ctx->IDENTIFIER()->getSymbol(), SemanticTokenType::ls_variable);

    visit(ctx->expression());
    return std::any();
}

std::any TokenVisitor::visitVariable(BalanceParser::VariableContext *ctx) {
    this->addToken(ctx->IDENTIFIER()->getSymbol(), SemanticTokenType::ls_variable);
    return std::any();
}

std::any TokenVisitor::visitFunctionCall(BalanceParser::FunctionCallContext *ctx) {
    this->addToken(ctx->IDENTIFIER()->getSymbol(), SemanticTokenType::ls_function);
    visit(ctx->argumentList());
    return std::any();
}

std::any TokenVisitor::visitFunctionDefinition(BalanceParser::FunctionDefinitionContext *ctx) {
    this->addToken(ctx->DEF()->getSymbol(), SemanticTokenType::ls_keyword);
    this->addToken(ctx->IDENTIFIER()->getSymbol(), SemanticTokenType::ls_method);

    visit(ctx->parameterList());
    if (ctx->returnType()) {
        visit(ctx->returnType());
    }
    visit(ctx->functionBlock());

    return std::any();
}

std::any TokenVisitor::visitExistingAssignment(BalanceParser::ExistingAssignmentContext *ctx) {
    this->addToken(ctx->IDENTIFIER()->getSymbol(), SemanticTokenType::ls_variable);
    visit(ctx->expression());
    return std::any();
}

std::any TokenVisitor::visitSimpleType(BalanceParser::SimpleTypeContext *ctx) {
    if (ctx->IDENTIFIER()) {
        this->addToken(ctx->IDENTIFIER()->getSymbol(), SemanticTokenType::ls_type);
    }

    if (ctx->NONE()) {
        this->addToken(ctx->NONE()->getSymbol(), SemanticTokenType::ls_type);
    }

    return std::any();
}

std::any TokenVisitor::visitGenericType(BalanceParser::GenericTypeContext *ctx) {
    this->addToken(ctx->IDENTIFIER()->getSymbol(), SemanticTokenType::ls_type);

    visit(ctx->typeList());
    return std::any();
}

std::any TokenVisitor::visitStringLiteral(BalanceParser::StringLiteralContext *ctx) {
    auto token = ctx->STRING()->getSymbol();
    unsigned int line = token->getLine() - 1;
    unsigned int columnStart = token->getCharPositionInLine();
    unsigned int length = token->getText().size();

    // Grammar removes the quotes, so we add them again here
    this->addToken(line, columnStart, length, SemanticTokenType::ls_string);
    return std::any();
}

std::any TokenVisitor::visitBooleanLiteral(BalanceParser::BooleanLiteralContext *ctx) {
    if (ctx->TRUE()) {
        this->addToken(ctx->TRUE()->getSymbol(), SemanticTokenType::ls_keyword);
    }
    if (ctx->FALSE()) {
        this->addToken(ctx->FALSE()->getSymbol(), SemanticTokenType::ls_keyword);
    }
    return std::any();
}

std::any TokenVisitor::visitNoneLiteral(BalanceParser::NoneLiteralContext *ctx) {
    this->addToken(ctx->NONE()->getSymbol(), SemanticTokenType::ls_type);
    return std::any();
}

std::any TokenVisitor::visitDoubleLiteral(BalanceParser::DoubleLiteralContext *ctx) {
    this->addToken(ctx->DOUBLE()->getSymbol(), SemanticTokenType::ls_number);
    return std::any();
}

std::any TokenVisitor::visitNumericLiteral(BalanceParser::NumericLiteralContext *ctx) {
    this->addToken(ctx->DECIMAL_INTEGER()->getSymbol(), SemanticTokenType::ls_number);
    return std::any();
}

std::any TokenVisitor::visitClassProperty(BalanceParser::ClassPropertyContext *ctx) {
    visit(ctx->type);
    this->addToken(ctx->IDENTIFIER()->getSymbol(), SemanticTokenType::ls_property);
    return std::any();
}

std::any TokenVisitor::visitParameter(BalanceParser::ParameterContext *ctx) {
    visit(ctx->type);
    this->addToken(ctx->IDENTIFIER()->getSymbol(), SemanticTokenType::ls_variable);
    return std::any();
}
