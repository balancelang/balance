#include "String.h"
#include "../models/BalanceType.h"
#include "../BalancePackage.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;

void StringType::registerType() {
    this->balanceType = new BalanceType(currentPackage->currentModule, "String");
    currentPackage->currentModule->addType(this->balanceType);
}

void StringType::finalizeType() {
    BalanceType * intType = currentPackage->currentModule->getType("Int");      // TODO: Int32
    this->balanceType->properties["typeId"] = new BalanceProperty("typeId", intType, 0, false);

    BalanceType * int8PointerType = currentPackage->currentModule->getType("Int8Pointer");
    this->balanceType->properties["stringPointer"] = new BalanceProperty("stringPointer", int8PointerType, 1, false);

    BalanceType * int64Type = currentPackage->currentModule->getType("Int64");
    this->balanceType->properties["length"] = new BalanceProperty("length", int64Type, 2, true);

    StructType *structType = StructType::create(*currentPackage->context, "String");
    ArrayRef<Type *> propertyTypesRef({
        this->balanceType->properties["typeId"]->balanceType->getReferencableType(),
        // Pointer to the String
        int8PointerType->getInternalType(),
        // Size of the string
        int64Type->getInternalType()
    });
    structType->setBody(propertyTypesRef, false);
    this->balanceType->internalType = structType;
    this->balanceType->hasBody = true;
}

void StringType::registerMethods() {
    this->registerMethod_String_toString();
}

void StringType::finalizeMethods() {
    this->finalizeMethod_String_toString();
}

void StringType::registerFunctions() {
    this->registerMethod_String_toString();
}

void StringType::finalizeFunctions() {

}

void StringType::registerMethod_String_toString() {
    std::string functionName = "toString";
    BalanceType * stringType = currentPackage->currentModule->getType("String");

    BalanceParameter * valueParameter = new BalanceParameter(stringType, "value");
    std::vector<BalanceParameter *> parameters = {
        valueParameter
    };
    BalanceFunction * bfunction = new BalanceFunction(currentPackage->currentModule, stringType, functionName, parameters, stringType);
    stringType->addMethod(functionName, bfunction);
}

void StringType::finalizeMethod_String_toString() {
    BalanceFunction * toStringFunction = this->balanceType->getMethod("toString");
    // Create forward declaration of memcpy
    // void * memcpy ( void * destination, const void * source, size_t num );
    ArrayRef<Type *> memcpyParams({
        llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0),
        llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0),
        llvm::Type::getInt64Ty(*currentPackage->context),
    });
    llvm::Type * memcpyReturnType = llvm::Type::getInt32Ty(*currentPackage->context);
    llvm::FunctionType * memcpyDeclarationType = llvm::FunctionType::get(memcpyReturnType, memcpyParams, false);
    currentPackage->builtinModules["builtins"]->module->getOrInsertFunction("memcpy", memcpyDeclarationType);

    llvm::Function * intToStringFunc = Function::Create(toStringFunction->getLlvmFunctionType(), Function::ExternalLinkage, toStringFunction->getFullyQualifiedFunctionName(), currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, toStringFunction->getFunctionName() + "_body", intToStringFunc);
    toStringFunction->setLlvmFunction(intToStringFunc);

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = intToStringFunc->arg_begin();
    llvm::Value * stringValue = args++;

    currentPackage->currentModule->builder->CreateRet(stringValue);
    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}
