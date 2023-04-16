#include "Int64Pointer.h"

#include "../../BalancePackage.h"
#include "../../models/BalanceType.h"

extern BalancePackage *currentPackage;

void Int64PointerType::registerType() {
    this->balanceType = new BalanceType(currentPackage->currentModule, "Int64Pointer");
    this->balanceType->isSimpleType = true;
    currentPackage->currentModule->addType(this->balanceType);
}

void Int64PointerType::finalizeType() {
    this->balanceType->internalType = llvm::Type::getInt64PtrTy(*currentPackage->context);
}

void Int64PointerType::registerMethods() {

}

void Int64PointerType::finalizeMethods() {

}

void Int64PointerType::registerFunctions() {

}

void Int64PointerType::finalizeFunctions() {

}

