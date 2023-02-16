#ifndef VISITOR_H
#define VISITOR_H

#include "BalanceParserBaseVisitor.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "../Utilities.h"
#include "../models/BalanceProperty.h"
#include "../models/BalanceType.h"
#include "../models/BalanceValue.h"

#include "clang/Driver/Driver.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Driver/Compilation.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Value.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/IR/ValueSymbolTable.h"

#include "llvm/IR/BasicBlock.h"

using namespace antlrcpptest;
using namespace llvm;

Constant *geti8StrVal(Module &M, char const *str, Twine const &name, bool addNull);

class BalanceVisitor : public BalanceParserBaseVisitor
{
public:
    std::any visitWhileStatement(BalanceParser::WhileStatementContext *ctx) override;
    std::any visitForStatement(BalanceParser::ForStatementContext *ctx) override;
    std::any visitMemberAssignment(BalanceParser::MemberAssignmentContext *ctx) override;
    std::any visitMemberIndexExpression(BalanceParser::MemberIndexExpressionContext *ctx) override;
    std::any visitArrayLiteral(BalanceParser::ArrayLiteralContext *ctx) override;
    std::any visitIfStatement(BalanceParser::IfStatementContext *ctx) override;
    std::any visitVariableExpression(BalanceParser::VariableExpressionContext *ctx) override;
    std::any visitNewAssignment(BalanceParser::NewAssignmentContext *ctx) override;
    std::any visitExistingAssignment(BalanceParser::ExistingAssignmentContext *ctx) override;
    std::any visitRelationalExpression(BalanceParser::RelationalExpressionContext *ctx) override;
    std::any visitAdditiveExpression(BalanceParser::AdditiveExpressionContext *ctx) override;
    std::any visitNoneLiteral(BalanceParser::NoneLiteralContext *ctx) override;
    std::any visitNumericLiteral(BalanceParser::NumericLiteralContext *ctx) override;
    std::any visitBooleanLiteral(BalanceParser::BooleanLiteralContext *ctx) override;
    std::any visitDoubleLiteral(BalanceParser::DoubleLiteralContext *ctx) override;
    std::any visitStringLiteral(BalanceParser::StringLiteralContext *ctx) override;
    std::any visitFunctionCall(BalanceParser::FunctionCallContext *ctx) override;
    std::any visitReturnStatement(BalanceParser::ReturnStatementContext *ctx) override;
    std::any visitFunctionDefinition(BalanceParser::FunctionDefinitionContext *ctx) override;
    std::any visitLambdaExpression(BalanceParser::LambdaExpressionContext *ctx) override;
    std::any visitMemberAccessExpression(BalanceParser::MemberAccessExpressionContext *ctx) override;
    std::any visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) override;
    std::any visitClassInitializerExpression(BalanceParser::ClassInitializerExpressionContext *ctx) override;
    std::any visitMultiplicativeExpression(BalanceParser::MultiplicativeExpressionContext *ctx) override;
    std::any visitMapInitializerExpression(BalanceParser::MapInitializerExpressionContext *ctx) override;
    std::any visitGenericType(BalanceParser::GenericTypeContext *ctx) override;
    std::any visitSimpleType(BalanceParser::SimpleTypeContext *ctx) override;
    std::any visitNoneType(BalanceParser::NoneTypeContext *ctx) override;
    std::any visitRange(BalanceParser::RangeContext *ctx) override;

    BalanceValue * visitAndLoad(ParserRuleContext * ctx);
    BalanceValue * visitFunctionCall__print(BalanceParser::FunctionCallContext *ctx);
};

#endif