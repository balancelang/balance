#include "Any.h"
#include "../Builtins.h"
#include "../BalancePackage.h"

#include "../models/BalanceType.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;

void AnyType::registerType() {
    this->balanceType = new BalanceType(currentPackage->currentModule, "Any");
    currentPackage->currentModule->addType(this->balanceType);
}

void AnyType::finalizeType() {
    BalanceType * intType = currentPackage->currentModule->getType("Int");
    this->balanceType->properties["typeId"] = new BalanceProperty("typeId", intType, 0, false);

    StructType *structType = StructType::create(*currentPackage->context, "Any");
    ArrayRef<Type *> propertyTypesRef({
        this->balanceType->properties["typeId"]->balanceType->getReferencableType(),
    });
    structType->setBody(propertyTypesRef, false);
    this->balanceType->hasBody = true;

    this->balanceType->internalType = structType;
}

void AnyType::registerMethods() {
    this->registerMethod_getType();
}

void AnyType::finalizeMethods() {
    this->finalizeMethod_getType();
}

void AnyType::registerFunctions() {

}

void AnyType::finalizeFunctions() {

}

void AnyType::registerMethod_getType() {
    std::string functionName = "getType";
    BalanceType * typeType = currentPackage->currentModule->getType("Type");
    std::vector<BalanceParameter *> parameters = {
        new BalanceParameter(this->balanceType, "this")
    };
    BalanceFunction * bfunction = new BalanceFunction(currentPackage->currentModule, this->balanceType, functionName, parameters, typeType);
    this->balanceType->addMethod(functionName, bfunction);
}

void AnyType::finalizeMethod_getType() {
    BalanceFunction * getTypeBalanceFunction = this->balanceType->getMethod("getType");
    BalanceType * anyType = currentPackage->currentModule->getType("Any");
    BalanceType * stringType = currentPackage->currentModule->getType("String");
    BalanceType * typeType = currentPackage->currentModule->getType("Type");

    llvm::Function * getTypeFunction = Function::Create(getTypeBalanceFunction->getLlvmFunctionType(), Function::ExternalLinkage, getTypeBalanceFunction->getFullyQualifiedFunctionName(), currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, getTypeBalanceFunction->name + "_body", getTypeFunction);

    getTypeBalanceFunction->setLlvmFunction(getTypeFunction);

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = getTypeFunction->arg_begin();
    llvm::Value *thisPointer = args++;

    // load typeId of 'this'
    BalanceProperty * typeIdProperty = anyType->getProperty("typeId");
    auto zeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto typeIdIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, typeIdProperty->index, true));
    auto typeIdPointer = currentPackage->currentModule->builder->CreateGEP(anyType->getInternalType(), thisPointer, {zeroValue, typeIdIndexValue});
    Value * typeIdValue = (Value *)currentPackage->currentModule->builder->CreateLoad(typeIdPointer);

    // lookup TypeInfo in typeInfoTable
    auto typeInfoPointer = currentPackage->currentModule->builder->CreateGEP(currentPackage->typeInfoTable, {zeroValue, typeIdValue});

    // create destination 'Type' struct, extract type info and return
    auto typeMemoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),       // input type?
        typeType->getInternalType(),                            // output type, which we get pointer to?
        ConstantExpr::getSizeOf(typeType->getInternalType()),   // size, matches input type?
        nullptr, nullptr, "");
    currentPackage->currentModule->builder->Insert(typeMemoryPointer);

    ArrayRef<Value *> argumentsReference{typeMemoryPointer};
    currentPackage->currentModule->builder->CreateCall(typeType->getInitializer()->getLlvmFunction(currentPackage->currentModule), argumentsReference);

    // Create pointer for destination Type.typeId
    auto dstTypeIdZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto dstTypeIdIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, typeType->properties["typeId"]->index, true));
    auto dstTypeIdGEP = currentPackage->currentModule->builder->CreateGEP(typeType->getInternalType(), typeMemoryPointer, {dstTypeIdZeroValue, dstTypeIdIndexValue});

    // Create pointer for source TypeInfo.typeId
    auto srcTypeIdZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto srcTypeIdIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto srcTypeIdGEP = currentPackage->currentModule->builder->CreateGEP(currentPackage->typeInfoStructType, typeInfoPointer, {srcTypeIdZeroValue, srcTypeIdIndexValue});
    Value * srcTypeIdValue = (Value *)currentPackage->currentModule->builder->CreateLoad(typeIdPointer);
    currentPackage->currentModule->builder->CreateStore(srcTypeIdValue, dstTypeIdGEP);

    // Create pointer for destination Type.name (String *)
    auto nameZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto nameIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, typeType->properties["name"]->index, true));
    auto nameGEP = currentPackage->currentModule->builder->CreateGEP(typeType->getInternalType(), typeMemoryPointer, {nameZeroValue, nameIndexValue});

    // Allocate string
    auto stringMemoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),       // input type?
        stringType->getInternalType(),                          // output type, which we get pointer to?
        ConstantExpr::getSizeOf(stringType->getInternalType()), // size, matches input type?
        nullptr, nullptr, "");
    currentPackage->currentModule->builder->Insert(stringMemoryPointer);

    ArrayRef<Value *> stringArgumentsReference{stringMemoryPointer};
    currentPackage->currentModule->builder->CreateCall(stringType->getInitializer()->getLlvmFunction(currentPackage->currentModule), stringArgumentsReference);
    auto pointerZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto pointerIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringType->properties["stringPointer"]->index, true));
    auto pointerGEP = currentPackage->currentModule->builder->CreateGEP(stringType->getInternalType(), stringMemoryPointer, {pointerZeroValue, pointerIndexValue});
    auto sizeZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto sizeIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringType->properties["length"]->index, true));
    auto sizeGEP = currentPackage->currentModule->builder->CreateGEP(stringType->getInternalType(), stringMemoryPointer, {sizeZeroValue, sizeIndexValue});

    // Create pointer for source TypeInfo.name (i8*)
    auto srcNameZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto srcNameIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 1, true));
    auto srcNameGEP = currentPackage->currentModule->builder->CreateGEP(currentPackage->typeInfoStructType, typeInfoPointer, {srcNameZeroValue, srcNameIndexValue});
    Value * srcNameValue = (Value *)currentPackage->currentModule->builder->CreateLoad(srcNameGEP);

    currentPackage->currentModule->builder->CreateStore(srcNameValue, pointerGEP);
    // currentPackage->currentModule->builder->CreateStore(srcNameValue, sizeGEP);

    currentPackage->currentModule->builder->CreateStore(stringMemoryPointer, nameGEP);
    currentPackage->currentModule->builder->CreateRet(typeMemoryPointer);
    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}
