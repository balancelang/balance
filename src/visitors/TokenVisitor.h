#ifndef TOKEN_VISITOR_H
#define TOKEN_VISITOR_H

#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "BalanceParserBaseVisitor.h"

#include "LibLsp/lsp/textDocument/SemanticTokens.h"

#include "llvm/IR/BasicBlock.h"

using namespace antlrcpptest;
using namespace llvm;
using namespace std;

class TokenVisitor : public BalanceParserBaseVisitor {
  public:
    vector<SemanticToken> tokens;
    unsigned int previousLine = 0;
    unsigned int previousColumn = 0;

    void addToken(antlr4::Token *token, SemanticTokenType type);
    void addToken(unsigned int line, unsigned int columnStart, unsigned int length, SemanticTokenType type);

    std::any visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) override;
    std::any visitImportStatement(BalanceParser::ImportStatementContext *ctx) override;
    std::any visitWhileStatement(BalanceParser::WhileStatementContext *ctx) override;
    std::any visitIfStatement(BalanceParser::IfStatementContext *ctx) override;
    std::any visitReturnStatement(BalanceParser::ReturnStatementContext *ctx) override;
    std::any visitNewAssignment(BalanceParser::NewAssignmentContext *ctx) override;
    std::any visitExistingAssignment(BalanceParser::ExistingAssignmentContext *ctx) override;
    std::any visitVariable(BalanceParser::VariableContext *ctx) override;
    std::any visitSimpleType(BalanceParser::SimpleTypeContext *ctx) override;
    std::any visitGenericType(BalanceParser::GenericTypeContext *ctx) override;
    std::any visitParameter(BalanceParser::ParameterContext *ctx) override;
    std::any visitFunctionCall(BalanceParser::FunctionCallContext *ctx) override;
    std::any visitFunctionDefinition(BalanceParser::FunctionDefinitionContext *ctx) override;
    std::any visitClassInitializer(BalanceParser::ClassInitializerContext *ctx) override;
    std::any visitClassProperty(BalanceParser::ClassPropertyContext *ctx) override;
    std::any visitNumericLiteral(BalanceParser::NumericLiteralContext *ctx) override;
    std::any visitStringLiteral(BalanceParser::StringLiteralContext *ctx) override;
    std::any visitBooleanLiteral(BalanceParser::BooleanLiteralContext *ctx) override;
    std::any visitDoubleLiteral(BalanceParser::DoubleLiteralContext *ctx) override;
    std::any visitNoneLiteral(BalanceParser::NoneLiteralContext *ctx) override;
    std::any visitInterfaceDefinition(BalanceParser::InterfaceDefinitionContext *ctx) override;
    std::any visitFunctionSignature(BalanceParser::FunctionSignatureContext *ctx) override;
    std::any visitClassExtendsImplements(BalanceParser::ClassExtendsImplementsContext *ctx) override;
};

#endif