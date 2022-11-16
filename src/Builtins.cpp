#include "Builtins.h"
#include "BalancePackage.h"
#include "builtins/File.h"
#include "builtins/Int.h"
#include "builtins/String.h"
#include "builtins/Bool.h"
#include "builtins/Double.h"
#include "builtins/Array.h"

#include "models/BalanceTypeString.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;

void createFunction__print()
{
    // Create forward declaration of printf
    llvm::FunctionType * printfFunctionType = llvm::FunctionType::get(llvm::IntegerType::getInt32Ty(*currentPackage->context), llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0), true);
    FunctionCallee printfFunction = currentPackage->currentModule->module->getOrInsertFunction("printf", printfFunctionType);

    // TODO: One day we might change this to "Any" and then do toString on it
    BalanceParameter * contentParameter = new BalanceParameter(new BalanceTypeString("String"), "content");
    contentParameter->type = getBuiltinType(new BalanceTypeString("String"));

    // Create BalanceFunction
    std::vector<BalanceParameter *> parameters = {
        contentParameter
    };
    BalanceFunction * bfunction = new BalanceFunction("print", parameters, new BalanceTypeString("None"));
    currentPackage->currentModule->functions["print"] = bfunction;

    // Create llvm::Function
    ArrayRef<Type *> parametersReference({
        contentParameter->type
    });

    Type * returnType = llvm::Type::getVoidTy(*currentPackage->context);
    FunctionType *functionType = FunctionType::get(returnType, parametersReference, false);

    llvm::Function * printFunc = Function::Create(functionType, Function::ExternalLinkage, "print", currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, "print_body", printFunc);

    bfunction->function = printFunc;
    bfunction->returnType = getBuiltinType(new BalanceTypeString("None"));

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = printFunc->arg_begin();
    llvm::Value *stringStructPointer = args++;

    BalanceClass * stringClass = currentPackage->builtins->getClass(new BalanceTypeString("String"));
    Value * zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    Value * stringPointerIndex = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringClass->properties["stringPointer"]->index, true));
    Value * stringPointerValue = currentPackage->currentModule->builder->CreateGEP(stringClass->structType, stringStructPointer, {zero, stringPointerIndex});
    Value * stringValue = currentPackage->currentModule->builder->CreateLoad(stringPointerValue);

    // CreateCall to printf with stringPointer as argument.
    ArrayRef<Value *> arguments({
        stringValue,
    });
    currentPackage->currentModule->builder->CreateCall(printfFunction, arguments);

    // Create newline TODO: Make this a parameter?
    ArrayRef<Value *> newlineArguments({
        geti8StrVal(*currentPackage->currentModule->module, "\n", "newline", true),
    });
    currentPackage->currentModule->builder->CreateCall(printfFunction, newlineArguments);

    currentPackage->currentModule->builder->CreateRetVoid();

    bool hasError = verifyFunction(*bfunction->function, &llvm::errs());
    if (hasError) {
        throw std::runtime_error("Error verifying function: " + bfunction->name);
    }

    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

void createFunction__open()
{
    // Create forward declaration of fopen
    llvm::FunctionType * functionType = llvm::FunctionType::get(llvm::PointerType::get(llvm::Type::getInt32Ty(*currentPackage->context), 0), llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0), false);
    currentPackage->currentModule->module->getOrInsertFunction("fopen", functionType);

    BalanceParameter * pathParameter = new BalanceParameter(new BalanceTypeString("String"), "path");
    pathParameter->type = getBuiltinType(new BalanceTypeString("String"));

    BalanceParameter * modeParameter = new BalanceParameter(new BalanceTypeString("String"), "mode");
    modeParameter->type = getBuiltinType(new BalanceTypeString("String"));

    std::vector<BalanceParameter *> parameters = {
        pathParameter,
        modeParameter
    };
    BalanceFunction * bfunction = new BalanceFunction("open", parameters, new BalanceTypeString("File"));
    BalanceClass * fileClass = currentPackage->builtins->getClass(new BalanceTypeString("File"));
    bfunction->returnType = fileClass->structType->getPointerTo();
    currentPackage->currentModule->functions["open"] = bfunction;
}

void createFunctions() {
    createFunction__print();
    createFunction__open();
}

void createTypes() {
    createType__String();
    createType__Int();
    createType__Bool();
    createType__Double();
    createType__File();
    // createType__Array(); // Arrays are lazily created with their generic types
}

void createBuiltins() {
    BalanceModule * bmodule = new BalanceModule("builtins", false);
    bmodule->initializeModule();
    currentPackage->builtins = bmodule;
    currentPackage->currentModule = bmodule;

    // TODO: Check if this works creating types multiple times with multiple modules
    createTypes();
    createFunctions();

    bmodule->builder->CreateRet(ConstantInt::get(*currentPackage->context, APInt(32, 0)));
}