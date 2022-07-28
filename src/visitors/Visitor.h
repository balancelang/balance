#ifndef VISITOR_H
#define VISITOR_H

#include "BalanceParserBaseVisitor.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "../Utilities.h"
#include "../models/BalanceProperty.h"
#include "../models/BalanceTypeString.h"

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
using namespace std;

llvm::Value *anyToValue(any anyVal);
Type *getBuiltinType(BalanceTypeString * typeString);
Constant *geti8StrVal(Module &M, char const *str, Twine const &name, bool addNull);
void LogError(string errorMessage);

class BalanceVisitor : public BalanceParserBaseVisitor
{
public:
    any visitWhileStatement(BalanceParser::WhileStatementContext *ctx) override;
    any visitMemberAssignment(BalanceParser::MemberAssignmentContext *ctx) override;
    any visitMemberIndexExpression(BalanceParser::MemberIndexExpressionContext *ctx) override;
    any visitArrayLiteral(BalanceParser::ArrayLiteralContext *ctx) override;
    any visitIfStatement(BalanceParser::IfStatementContext *ctx) override;
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
    any visitClassInitializerExpression(BalanceParser::ClassInitializerExpressionContext *ctx) override;
    any visitMultiplicativeExpression(BalanceParser::MultiplicativeExpressionContext *ctx) override;
};

#endif