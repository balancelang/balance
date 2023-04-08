#include "FatPointer.h"

#include "../BalancePackage.h"

extern BalancePackage *currentPackage;

void FatPointerType::registerType() {
    this->balanceType = new BalanceType(currentPackage->currentModule, "FatPointer");
    currentPackage->currentModule->addType(this->balanceType);
}

void FatPointerType::finalizeType() {
    BalanceType * int64PointerType = currentPackage->currentModule->getType("Int64Pointer");

    this->balanceType->properties["thisPointer"] = new BalanceProperty("thisPointer", int64PointerType, false);
    this->balanceType->properties["vtablePointer"] = new BalanceProperty("vtablePointer", int64PointerType, false);

    StructType *structType = StructType::create(*currentPackage->context, "FatPointer");
    ArrayRef<Type *> propertyTypesRef({
        this->balanceType->properties["thisPointer"]->balanceType->getInternalType(),
        this->balanceType->properties["vtablePointer"]->balanceType->getInternalType()
    });
    structType->setBody(propertyTypesRef, false);
    this->balanceType->hasBody = true;
    this->balanceType->internalType = structType;
}

void FatPointerType::registerMethods() {

}

void FatPointerType::finalizeMethods() {
    // createDefaultConstructor(currentPackage->currentModule, btype);
}

void FatPointerType::registerFunctions() {

}

void FatPointerType::finalizeFunctions() {

}
