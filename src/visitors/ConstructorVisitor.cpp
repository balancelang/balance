#include "../headers/ConstructorVisitor.h"

#include "../headers/Visitor.h"
#include "../headers/Package.h"
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
#include "antlr4-runtime.h"

#include "config.h"

#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "BalanceParserBaseVisitor.h"

#include <typeinfo>
#include <typeindex>

using namespace antlrcpptest;
using namespace std;

extern BalancePackage *currentPackage;

void createDefaultConstructor(StructType *classValue)
{
    std::string constructorName = classValue->getName().str() + "_constructor";
    vector<Type *> functionParameterTypes;

    // TODO: Constructor should return Type of class?
    Type *returnType = getBuiltinType("None");

    ArrayRef<Type *> parametersReference{classValue->getPointerTo()};
    FunctionType *functionType = FunctionType::get(returnType, parametersReference, false);
    Function *function = Function::Create(functionType, Function::ExternalLinkage, constructorName, currentPackage->currentModule->module);
    currentPackage->currentModule->currentClass->constructor = function;

    // Add parameter names
    Function::arg_iterator args = function->arg_begin();
    llvm::Value *thisValue = args++;
    thisValue->setName("this");

    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, constructorName + "_body", function);
    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    for (auto const &x : currentPackage->currentModule->currentClass->properties)
    {
        BalanceProperty * property = x.second;
        Type *propertyType = property->type;

        Value *initialValue;
        if (propertyType->isIntegerTy(1))
        {
            initialValue = ConstantInt::get(getBuiltinType("Bool"), 0, true);
        }
        else if (propertyType->isIntegerTy(32))
        {
            initialValue = ConstantInt::get(getBuiltinType("Int"), 0, true);
        }
        else if (propertyType->isFloatingPointTy())
        {
            initialValue = ConstantFP::get(getBuiltinType("Double"), 0.0);
        }
        // // TODO: Handle String and nullable types

        int intIndex = property->index;
        auto zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
        auto index = ConstantInt::get(*currentPackage->context, llvm::APInt(32, intIndex, true));
        Type *structType = thisValue->getType()->getPointerElementType();

        auto ptr = currentPackage->currentModule->builder->CreateGEP(structType, thisValue, {zero, index});
        currentPackage->currentModule->builder->CreateStore(initialValue, ptr);
    }

    currentPackage->currentModule->builder->CreateRetVoid();

    bool hasError = verifyFunction(*function);
    if (hasError) {
        // TODO: Throw error
        std::cout << "Error verifying default constructor for class: " << classValue->getName().str() << std::endl;
        currentPackage->currentModule->module->print(llvm::errs(), nullptr);
        exit(1);
    }
    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

// Creates default empty argument constructor
std::any ConstructorVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) {
    string text = ctx->getText();
    string className = ctx->className->getText();

    BalanceClass *bclass = currentPackage->currentModule->getClass(className);
    currentPackage->currentModule->currentClass = bclass;

    if (bclass->constructor == nullptr) {
        createDefaultConstructor(currentPackage->currentModule->currentClass->structType);
    }

    currentPackage->currentModule->currentClass = nullptr;
    return nullptr;
}

// TODO: Allow custom constructor generation, which will be handled in a future visit function here
// std::any ConstructorVisitor::visitClassConstructor(BalanceParser::ClassConstructorContext *ctx) { ... }