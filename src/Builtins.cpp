#include "Builtins.h"
#include "BalancePackage.h"
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

void createFunction__print()
{
    // Create forward declaration of printf
    llvm::FunctionType * printfFunctionType = llvm::FunctionType::get(llvm::IntegerType::getInt32Ty(*currentPackage->context), llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0), true);
    FunctionCallee printfFunction = currentPackage->currentModule->module->getOrInsertFunction("printf", printfFunctionType);

    // TODO: One day we might change this to "Any" and then do toString on it
    BalanceParameter * contentParameter = new BalanceParameter(currentPackage->currentModule->getType("String"), "content");

    // Create BalanceFunction
    std::vector<BalanceParameter *> parameters = {
        contentParameter
    };
    BalanceFunction * bfunction = new BalanceFunction("print", parameters, currentPackage->currentModule->getType("None"));
    currentPackage->currentModule->addFunction(bfunction);

    // Create llvm::Function
    ArrayRef<Type *> parametersReference({
        contentParameter->balanceType->getReferencableType()
    });

    Type * returnType = llvm::Type::getVoidTy(*currentPackage->context);
    FunctionType *functionType = FunctionType::get(returnType, parametersReference, false);

    llvm::Function * printFunc = Function::Create(functionType, Function::ExternalLinkage, "print", currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, "print_body", printFunc);

    bfunction->function = printFunc;

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

    bool hasError = verifyFunction(*bfunction->function, &llvm::errs());
    if (hasError) {
        throw std::runtime_error("Error verifying function: " + bfunction->name);
    }

    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

void createFunction__open()
{
    BalanceParameter * pathParameter = new BalanceParameter(currentPackage->currentModule->getType("String"), "path");
    BalanceParameter * modeParameter = new BalanceParameter(currentPackage->currentModule->getType("String"), "mode");

    std::vector<BalanceParameter *> parameters = {
        pathParameter,
        modeParameter
    };
    BalanceType * fileType = currentPackage->currentModule->getType("File");
    BalanceFunction * bfunction = new BalanceFunction("open", parameters, fileType);

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
    ArrayRef<Type *> parametersReference({
        pathParameter->balanceType->getReferencableType(),
        modeParameter->balanceType->getReferencableType()
    });
    FunctionType *functionType = FunctionType::get(bfunction->returnType->getReferencableType(), parametersReference, false);
    llvm::Function * openFunction = Function::Create(functionType, Function::ExternalLinkage, bfunction->name, currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, bfunction->name + "_body", openFunction);

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
    currentPackage->currentModule->builder->CreateCall(fileType->initializer, argumentsReference); // TODO: should it have a constructor?

    // Get reference to 0th property (filePointer) and assign
    int intIndex = fileType->properties["filePointer"]->index;
    auto index = ConstantInt::get(*currentPackage->context, llvm::APInt(32, intIndex, true));

    auto ptr = currentPackage->currentModule->builder->CreateGEP(fileType->getInternalType(), pointer, {zero, index});
    currentPackage->currentModule->builder->CreateStore(filePointer, ptr);

    currentPackage->currentModule->builder->CreateRet(pointer);

    // Assign the llvm function to the bfunction
    bfunction->function = openFunction;
    currentPackage->currentModule->addFunction(bfunction);

    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

void createType__None() {
    BalanceType * btype = new BalanceType(currentPackage->currentModule, "None", Type::getVoidTy(*currentPackage->context));
    btype->isSimpleType = true;
    currentPackage->currentModule->addType(btype);
}

void createType__FatPointer() {
    // TODO: Make sure you can't instantiate this from balance
    BalanceType * btype = new BalanceType(currentPackage->currentModule, "FatPointer");

    currentPackage->currentModule->addType(btype);
    BalanceType * pointerType = new BalanceType(currentPackage->currentModule, "pointer", llvm::Type::getInt64PtrTy(*currentPackage->context));
    pointerType->isSimpleType = true;
    btype->properties["thisPointer"] = new BalanceProperty("thisPointer", pointerType, false);
    btype->properties["vtablePointer"] = new BalanceProperty("vtablePointer", pointerType, false);

    currentPackage->currentModule->currentType = btype;
    StructType *structType = StructType::create(*currentPackage->context, "FatPointer");
    ArrayRef<Type *> propertyTypesRef({
        btype->properties["thisPointer"]->balanceType->getInternalType(),
        btype->properties["vtablePointer"]->balanceType->getInternalType()
    });
    structType->setBody(propertyTypesRef, false);
    btype->hasBody = true;
    btype->internalType = structType;

    // TODO: Might not be needed
    createDefaultConstructor(currentPackage->currentModule, btype);

    currentPackage->currentModule->currentType = nullptr;
}

void createFunctions() {
    createFunction__print();
    createFunction__open();

    // Type functions
    createFunctions__Int();
    createFunctions__Any();
}

void createTypes() {
    // eventually split this up in first creating the types, and then creating their functions etc.
    createType__Int();

    createType__Any();
    createType__None();

    createType__String();
    createType__Bool();
    createType__Double();
    createType__File();

    createType__Type();
    createType__FatPointer();

    // Generic versions are lazily created with their generic types
    createType__Array(nullptr);
    createType__Lambda({});
}

void createBuiltinTypes() {
    BalanceModule * bmodule = new BalanceModule("builtins", {}, false);
    currentPackage->builtinModules["builtins"] = bmodule;
    currentPackage->currentModule = bmodule;

    createTypes();
}

void createBuiltinFunctions() {
    createFunctions();

    currentPackage->builtinModules["builtins"]->builder->CreateRet(ConstantInt::get(*currentPackage->context, APInt(32, 0)));
}