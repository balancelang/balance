#ifndef VISITOR_H
#define VISITOR_H

#include "BalanceParserBaseVisitor.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"
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
#include "llvm/IR/IRBuilder.h"
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
#include "llvm/IR/IRBuilder.h"
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

class ScopeBlock;
class BalanceClass;
class BalanceFunction;
class BalanceModule;

Value *anyToValue(any anyVal);
Value *getValue(string variableName);
void setValue(string variableName, Value *value);
Type * getBuiltinType(string typeString);
Constant *geti8StrVal(Module &M, char const *str, Twine const &name);
void LogError(string errorMessage);

struct TypeAndIndex
{
    int index;
    Type *type;
};

class ScopeBlock
{
public:
    BasicBlock *block;
    ScopeBlock *parent;
    map<string, Value *> symbolTable;
    ScopeBlock(BasicBlock *block, ScopeBlock *parent)
    {
        this->block = block;
        this->parent = parent;
    }
};

class BalanceClass
{
public:
    string name;
    map<string, TypeAndIndex> properties;
    map<string, Function *> methods;
    Function *constructor;
    StructType *structType;
    BalanceModule *module;
    bool finalized;
    BalanceClass(string name)
    {
        this->name = name;
    }
};

class BalanceParameter
{
public:
    string type;
    string name;

    BalanceParameter(string type, string name) {
        this->type = type;
        this->name = name;
    }
}

class BalanceFunction
{
public:
    string name;
    string returnType;
    vector<BalanceParameter> parameters;
    Function * function;
    BalanceModule *module;

    BalanceFunction(string name) {
        this->name = name;
    }
};

class BalanceModule
{
public:
    string path;
    string filePath;
    string name;
    bool isEntrypoint;

    // Overview of accessible properties of a module
    map<string, BalanceClass *> classes;
    map<string, BalanceFunction *> functions;
    map<string, Value *> globals;

    ScopeBlock * rootScope;
    ScopeBlock * currentScope;

    llvm::Module * module;

    bool finalized;
    bool finishedDiscovery;

    BalanceModule(string path, bool isEntrypoint)
    {
        this->path = path;
        this->isEntrypoint = isEntrypoint;
        this->filePath = this->path + ".bl";

        if (this->path.find('/') != std::string::npos) {
            this->name = this->path.substr(this->path.find_last_of("/"), this->path.size());
        } else {
            this->name = this->path;
        }
    }

    Value * getValue(std::string name) {
        map<string, BalanceClass *>::iterator classIterator = this->classes.find(name);
        if (classIterator != this->classes.end() ) {
            return (Value *) classIterator->second->structType;
        }

        map<string, BalanceFunction *>::iterator functionIterator = this->functions.find(name);
        if (functionIterator != this->functions.end()) {
            return (Value *) functionIterator->second;
        }

        return nullptr;
    }
};

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
    any visitImportStatement(BalanceParser::ImportStatementContext *ctx) override;
};

#endif