#ifndef LLVM_TYPE_VISITOR_H
#define LLVM_TYPE_VISITOR_H

#include "BalanceParserBaseVisitor.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"

#include "llvm/IR/BasicBlock.h"

using namespace antlrcpptest;
using namespace llvm;
using namespace std;


class LLVMTypeVisitor : public BalanceParserBaseVisitor
{
public:
    std::any visitImportStatement(BalanceParser::ImportStatementContext *ctx) override;
    std::any visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) override;
    std::any visitFunctionSignature(BalanceParser::FunctionSignatureContext *ctx) override;
    std::any visitClassProperty(BalanceParser::ClassPropertyContext *ctx) override;
    std::any visitInterfaceDefinition(BalanceParser::InterfaceDefinitionContext *ctx) override;

    std::any visitGenericType(BalanceParser::GenericTypeContext *ctx) override;
    std::any visitSimpleType(BalanceParser::SimpleTypeContext *ctx) override;
    std::any visitNoneType(BalanceParser::NoneTypeContext *ctx) override;
    std::any visitLambdaType(BalanceParser::LambdaTypeContext *ctx) override;
};

#endif