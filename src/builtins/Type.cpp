#include "Type.h"
#include "../Builtins.h"
#include "../BalancePackage.h"

#include "../models/BalanceType.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;

void TypeBalanceType::registerType() {
    this->balanceType = new BalanceType(currentPackage->currentModule, "Type");
    currentPackage->currentModule->addType(this->balanceType);
}

void TypeBalanceType::finalizeType() {
    BalanceType * stringType = currentPackage->currentModule->getType("String");
    BalanceType * intType = currentPackage->currentModule->getType("Int");
    this->balanceType->properties["typeId"] = new BalanceProperty("typeId", intType, 0, false);
    this->balanceType->properties["name"] = new BalanceProperty("name", stringType, 1);

    StructType *structType = StructType::create(*currentPackage->context, "Type");
    ArrayRef<Type *> propertyTypesRef({
        this->balanceType->properties["typeId"]->balanceType->getReferencableType(),
        this->balanceType->properties["name"]->balanceType->getReferencableType(),
    });
    structType->setBody(propertyTypesRef, false);
    this->balanceType->internalType = structType;
    this->balanceType->hasBody = true;

    createDefaultConstructor(currentPackage->currentModule, this->balanceType);
}

void TypeBalanceType::registerMethods() {

}

void TypeBalanceType::finalizeMethods() {
    // createDefaultToStringMethod(this->balanceType);
}

void TypeBalanceType::registerFunctions() {

}

void TypeBalanceType::finalizeFunctions() {

}