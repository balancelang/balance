#include "Builtins.h"
#include "BalancePackage.h"

#include "builtins/natives/Int64.h"
#include "builtins/File.h"
#include "builtins/Int.h"
#include "builtins/String.h"
#include "builtins/Bool.h"
#include "builtins/Double.h"
#include "builtins/Array.h"
#include "builtins/Lambda.h"
#include "builtins/Any.h"
#include "builtins/Type.h"

#include "models/BalanceType.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;

void registerNativeTypes() {
    BalanceModule * backup = currentPackage->currentModule;
    currentPackage->currentModule = currentPackage->builtinModules["builtins"];
    for (BuiltinType * builtinType : currentPackage->nativeTypes) {
        builtinType->registerType();
    }
    currentPackage->currentModule = backup;
}

void finalizeNativeTypes() {
    BalanceModule * backup = currentPackage->currentModule;
    currentPackage->currentModule = currentPackage->builtinModules["builtins"];
    for (BuiltinType * builtinType : currentPackage->nativeTypes) {
        builtinType->finalizeType();
    }
    currentPackage->currentModule = backup;
}


void registerBuiltinTypes() {
    BalanceModule * backup = currentPackage->currentModule;
    currentPackage->currentModule = currentPackage->builtinModules["builtins"];
    for (BuiltinType * builtinType : currentPackage->builtinTypes) {
        builtinType->registerType();
    }
    currentPackage->currentModule = backup;
}

void finalizeBuiltinTypes() {
    BalanceModule * backup = currentPackage->currentModule;
    currentPackage->currentModule = currentPackage->builtinModules["builtins"];
    for (BuiltinType * builtinType : currentPackage->builtinTypes) {
        builtinType->finalizeType();
    }
    currentPackage->currentModule = backup;
}

void registerBuiltinMethods() {
    BalanceModule * backup = currentPackage->currentModule;
    currentPackage->currentModule = currentPackage->builtinModules["builtins"];
    for (BuiltinType * builtinType : currentPackage->builtinTypes) {
        builtinType->registerMethods();
    }
    currentPackage->currentModule = backup;
}

void finalizeBuiltinMethods() {
    BalanceModule * backup = currentPackage->currentModule;
    currentPackage->currentModule = currentPackage->builtinModules["builtins"];
    for (BuiltinType * builtinType : currentPackage->builtinTypes) {
        builtinType->finalizeMethods();
    }
    currentPackage->currentModule = backup;
}

void registerBuiltinFunctions() {
    BalanceModule * backup = currentPackage->currentModule;
    currentPackage->currentModule = currentPackage->builtinModules["builtins"];
    for (BuiltinType * builtinType : currentPackage->builtinTypes) {
        builtinType->registerFunctions();
    }

    // Register all non-class functions
    registerFunction__print();
    // registerFunction__open();

    currentPackage->currentModule = backup;
}

void finalizeBuiltinFunctions() {
    BalanceModule * backup = currentPackage->currentModule;
    currentPackage->currentModule = currentPackage->builtinModules["builtins"];
    for (BuiltinType * builtinType : currentPackage->builtinTypes) {
        builtinType->finalizeFunctions();
    }

    // Finalize all non-class functions
    finalizeFunction__print();
    // finalizeFunction__open();

    currentPackage->currentModule = backup;
}

void registerFunction__print() {
    // TODO: One day we might change this to "Any" and then do toString on it
    BalanceType * stringType = currentPackage->currentModule->getType("String");
    BalanceParameter * contentParameter = new BalanceParameter(stringType, "content");
    std::vector<BalanceParameter *> parameters = {
        contentParameter
    };
    BalanceFunction * bfunction = new BalanceFunction(currentPackage->currentModule, nullptr, "print", parameters, currentPackage->currentModule->getType("None"));
    currentPackage->currentModule->addFunction(bfunction);
}

void finalizeFunction__print()
{
    BalanceFunction * printFunction = currentPackage->currentModule->getFunctionsByName("print")[0];
    // Create forward declaration of printf
    llvm::FunctionType * printfFunctionType = llvm::FunctionType::get(llvm::IntegerType::getInt32Ty(*currentPackage->context), llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0), true);
    FunctionCallee printfFunction = currentPackage->currentModule->module->getOrInsertFunction("printf", printfFunctionType);

    // Create llvm::Function
    llvm::Function * printFunc = Function::Create(printFunction->getLlvmFunctionType(), Function::ExternalLinkage, "print", currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, "print_body", printFunc);

    printFunction->setLlvmFunction(printFunc);

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = printFunc->arg_begin();
    llvm::Value *stringStructPointer = args++;

    // Print "None" if null, or the non-null string
    BasicBlock *thenBlock = BasicBlock::Create(*currentPackage->context, "then", printFunc);
    BasicBlock *elseBlock = BasicBlock::Create(*currentPackage->context, "else", printFunc);
    BasicBlock *mergeBlock = BasicBlock::Create(*currentPackage->context, "ifcont", printFunc);

    // check if null
    Value * nullValue = ConstantPointerNull::get((PointerType *) stringStructPointer->getType());
    Value * expression = currentPackage->currentModule->builder->CreateICmpEQ(nullValue, stringStructPointer);

    currentPackage->currentModule->builder->CreateCondBr(expression, thenBlock, elseBlock);
    currentPackage->currentModule->builder->SetInsertPoint(thenBlock);
    Value * arrayValueNull = geti8StrVal(*currentPackage->currentModule->module, "None", "None", true);
    ArrayRef<Value *> nullCallArguments({
        arrayValueNull
    });
    currentPackage->currentModule->builder->CreateCall(printfFunction, nullCallArguments);
    currentPackage->currentModule->builder->CreateBr(mergeBlock);
    currentPackage->currentModule->builder->SetInsertPoint(elseBlock);

    // when string is not null
    BalanceType * stringType = currentPackage->currentModule->getType("String");
    Value * zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    Value * stringPointerIndex = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringType->properties["stringPointer"]->index, true));
    Value * stringPointerValue = currentPackage->currentModule->builder->CreateGEP(stringType->getInternalType(), stringStructPointer, {zero, stringPointerIndex});
    Value * stringValue = currentPackage->currentModule->builder->CreateLoad(stringPointerValue);

    // CreateCall to printf with stringPointer as argument.
    ArrayRef<Value *> arguments({
        stringValue,
    });
    currentPackage->currentModule->builder->CreateCall(printfFunction, arguments);
    currentPackage->currentModule->builder->CreateBr(mergeBlock);

    currentPackage->currentModule->builder->SetInsertPoint(mergeBlock);
    // Create newline TODO: Make this a parameter?
    ArrayRef<Value *> newlineArguments({
        geti8StrVal(*currentPackage->currentModule->module, "\n", "newline", true),
    });
    currentPackage->currentModule->builder->CreateCall(printfFunction, newlineArguments);
    currentPackage->currentModule->builder->CreateRetVoid();

    bool hasError = verifyFunction(*printFunction->getLlvmFunction(currentPackage->currentModule), &llvm::errs());
    if (hasError) {
        throw std::runtime_error("Error verifying function: " + printFunction->name);
    }

    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

void registerFunction__open() {
    BalanceType * stringType = currentPackage->currentModule->getType("String");
    BalanceType * fileType = currentPackage->currentModule->getType("File");
    BalanceParameter * pathParameter = new BalanceParameter(stringType, "path");
    BalanceParameter * modeParameter = new BalanceParameter(stringType, "mode");
    std::vector<BalanceParameter *> parameters = {
        pathParameter,
        modeParameter
    };
    BalanceFunction * bfunction = new BalanceFunction(currentPackage->currentModule, nullptr, "open", parameters, fileType);
    currentPackage->currentModule->addFunction(bfunction);
}

void finalizeFunction__open()
{
    BalanceFunction * bfunction = currentPackage->currentModule->getFunctionsByName("open")[0];
    BalanceType * fileType = currentPackage->currentModule->getType("File");

    // Build llvm function and assign to bfunction
    ArrayRef<Type *> fopenParametersReference({
        llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0),
        llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0)
    });

    // Create forward declaration of fopen
    llvm::FunctionType * fopenfunctionType = FunctionType::get(
        llvm::PointerType::get(llvm::Type::getInt32Ty(*currentPackage->context), 0),
        fopenParametersReference,
        false);
    currentPackage->currentModule->module->getOrInsertFunction("fopen", fopenfunctionType);
    Function *fopenFunc = currentPackage->builtinModules["builtins"]->module->getFunction("fopen");

    // Create llvm::Function
    llvm::Function * openFunction = Function::Create(bfunction->getLlvmFunctionType(), Function::ExternalLinkage, bfunction->getFunctionName(), currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, bfunction->getFunctionName() + "_body", openFunction);

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    BalanceType *stringType = currentPackage->currentModule->getType("String");

    Value *zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto functionArgs = openFunction->arg_begin();

    Value * pathStringValue = functionArgs++;
    Value * modeStringValue = functionArgs++;

    Value *stringPointerIndex = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringType->properties["stringPointer"]->index, true));
    Value *pathPointerValue = currentPackage->currentModule->builder->CreateGEP(stringType->getInternalType(), pathStringValue, {zero, stringPointerIndex});
    Value *pathValue = currentPackage->currentModule->builder->CreateLoad(pathPointerValue);
    Value *modePointerValue = currentPackage->currentModule->builder->CreateGEP(stringType->getInternalType(), modeStringValue, {zero, stringPointerIndex});
    Value *modeValue = currentPackage->currentModule->builder->CreateLoad(modePointerValue);

    auto args = ArrayRef<Value *>({pathValue, modeValue});
    Value *filePointer = currentPackage->currentModule->builder->CreateCall(fopenFunc, args);

    // Create File struct which holds this and return pointer to the struct
    auto structSize = ConstantExpr::getSizeOf(fileType->getInternalType());
    auto pointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),
        fileType->getInternalType(),
        structSize,
        nullptr, nullptr, "");
    currentPackage->currentModule->builder->Insert(pointer);

    ArrayRef<Value *> argumentsReference{pointer};
    currentPackage->currentModule->builder->CreateCall(fileType->getInitializer()->getLlvmFunction(currentPackage->currentModule), argumentsReference); // TODO: should it have a constructor?

    // Get reference to 0th property (filePointer) and assign
    int intIndex = fileType->properties["filePointer"]->index;
    auto index = ConstantInt::get(*currentPackage->context, llvm::APInt(32, intIndex, true));

    auto ptr = currentPackage->currentModule->builder->CreateGEP(fileType->getInternalType(), pointer, {zero, index});
    currentPackage->currentModule->builder->CreateStore(filePointer, ptr);

    currentPackage->currentModule->builder->CreateRet(pointer);

    // Assign the llvm function to the bfunction
    bfunction->setLlvmFunction(openFunction);
    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

// void createFunctions() {
//     // Type functions
//     // createFunctions__Int();
//     createFunctions__String();
//     createFunctions__Bool();
//     createFunctions__Double();
//     createFunctions__File();
//     createFunctions__Type();

//     // Any is handled else-where

//     createFunction__print();
//     createFunction__open();
// }

// void createBuiltinTypes() {
//     BalanceModule * bmodule = new BalanceModule("builtins", {}, false);
//     currentPackage->builtinModules["builtins"] = bmodule;
//     currentPackage->currentModule = bmodule;

//     // Create non native types
//     // createType__Int();

//     createType__Any();
//     createType__None();

//     createType__String();
//     createType__Bool();
//     createType__Double();
//     createType__File();

//     createType__Type();

//     // Generic versions are lazily created with their generic types
//     createType__Array(nullptr);
//     createType__Lambda({});
// }
