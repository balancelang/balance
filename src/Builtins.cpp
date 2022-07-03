#include "Builtins.h"
#include "Package.h"
#include "builtins/File.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;

void createFunction__print()
{
    // Create forward declaration of printf
    llvm::FunctionType * printfFunctionType = llvm::FunctionType::get(llvm::IntegerType::getInt32Ty(*currentPackage->context), llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0), true);
    FunctionCallee printfFunction = currentPackage->currentModule->module->getOrInsertFunction("printf", printfFunctionType);

    // Create BalanceFunction
    std::vector<BalanceParameter *> parameters = {
        new BalanceParameter("String", "content")       // TODO: One day we might change this to "Any" and then do toString on it
    };
    BalanceFunction * bfunction = new BalanceFunction("print", parameters, "None");
    currentPackage->currentModule->functions["print"] = bfunction;

    // Create llvm::Function
    ArrayRef<Type *> parametersReference({
        getBuiltinType("String")
    });

    Type * returnType = llvm::Type::getVoidTy(*currentPackage->context);
    FunctionType *functionType = FunctionType::get(returnType, parametersReference, false);

    llvm::Function * printFunc = Function::Create(functionType, Function::ExternalLinkage, "print", currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, "print_body", printFunc);

    bfunction->function = printFunc;
    bfunction->returnType = getBuiltinType("None");

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = printFunc->arg_begin();
    llvm::Value *stringPointer = args++;

    // CreateCall to printf with stringPointer as argument.
    ArrayRef<Value *> arguments({
        stringPointer,
    });
    currentPackage->currentModule->builder->CreateCall(printfFunction, arguments);
    currentPackage->currentModule->builder->CreateRetVoid();

    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

void createFunction__open()
{
    // Create forward declaration of fopen
    llvm::FunctionType * functionType = llvm::FunctionType::get(llvm::PointerType::get(llvm::Type::getInt32Ty(*currentPackage->context), 0), llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0), false);
    currentPackage->currentModule->module->getOrInsertFunction("fopen", functionType);
}

void createFunctions() {
    createFunction__print();
    createFunction__open();
}

void createTypes() {
    createType__File();
}

void createBuiltins() {
    BalanceModule * bmodule = new BalanceModule("builtins", false);
    currentPackage->builtins = bmodule;
    currentPackage->currentModule = bmodule;

    // TODO: Check if this works creating types multiple times with multiple modules
    createTypes();
    createFunctions();

    bmodule->builder->CreateRet(ConstantInt::get(*currentPackage->context, APInt(32, 0)));
}